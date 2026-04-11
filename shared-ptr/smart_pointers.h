#include <utility>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <functional>
#include <exception>

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

namespace detail {

struct ControlBlockBase {
    size_t shared_count_;
    size_t weak_count_;

    ControlBlockBase(size_t shared_refs, size_t weak_refs_extra)
        : shared_count_(shared_refs), weak_count_(weak_refs_extra) {}

    virtual ~ControlBlockBase() = default;

    virtual void dispose_object() = 0;

    virtual void destroy_self() = 0;
    
    void increment_shared_count() noexcept {
        shared_count_++;
    }

    void decrement_shared_count_and_check_destroy() noexcept {
        if (!--shared_count_) {
            dispose_object();
        }
        if (!weak_count_ && !shared_count_) {
            destroy_self();
        }
    }

    void increment_weak_count() noexcept {
        weak_count_++;
    }

    void decrement_weak_count_and_check_destroy() noexcept {
        if (!--weak_count_ && !shared_count_) {
            destroy_self();
        }
    }
};


template <typename PtrType, typename Deleter, typename Alloc>
class ControlBlockRegular : public ControlBlockBase {
    PtrType* managed_ptr_; 
    Deleter object_deleter_; 
    Alloc control_block_allocator_;

public:
    template <typename ActualPtrType, typename D, typename A>
    ControlBlockRegular(ActualPtrType* ptr, D&& deleter, A&& alloc)
        : ControlBlockBase(1,0), 
          managed_ptr_(ptr),
          object_deleter_(std::forward<D>(deleter)),
          control_block_allocator_(std::forward<A>(alloc))
    {}

    void dispose_object() override {
        if (managed_ptr_) {
            object_deleter_(managed_ptr_);
            managed_ptr_ = nullptr; 
        }
    }

    void destroy_self() override {
        using CBAllocator = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockRegular<PtrType, Deleter, Alloc>>;
        CBAllocator alloc_copy = control_block_allocator_;
        this->~ControlBlockRegular(); 
        std::allocator_traits<CBAllocator>::deallocate(alloc_copy, this, 1);
    }
};


template <typename T, typename Alloc>
class ControlBlockMakeShared : public ControlBlockBase {
    Alloc object_and_block_allocator_;
    alignas(alignof(T)) std::byte storage_[sizeof(T)];

    T* get_object_ptr() noexcept { return reinterpret_cast<T*>(&storage_); }
    const T* get_object_ptr() const noexcept { return reinterpret_cast<const T*>(&storage_); }
public:
    template <typename A, typename... Args>
    ControlBlockMakeShared(A&& alloc, Args&&... args)
        : ControlBlockBase(1,0), object_and_block_allocator_(std::forward<A>(alloc)) {
        std::allocator_traits<Alloc>::construct(object_and_block_allocator_, get_object_ptr(), std::forward<Args>(args)...);
    }

    T* get_constructed_object() { return get_object_ptr(); }

    void dispose_object() override {
        std::allocator_traits<Alloc>::destroy(object_and_block_allocator_, get_object_ptr());
    }

    void destroy_self() override {
        using CBAllocator = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockMakeShared<T, Alloc>>;
        CBAllocator cb_alloc_copy(object_and_block_allocator_); 
        
        this->~ControlBlockMakeShared();
        std::allocator_traits<CBAllocator>::deallocate(cb_alloc_copy, this, 1);
    }

    ~ControlBlockMakeShared() noexcept {
        return;
    }
};


template <typename Y, typename U, typename T_esft>
void try_init_enable_shared_from_ptr(Y* raw_ptr, const SharedPtr<U>& shared_ptr_instance) {
    if constexpr (std::is_base_of_v<EnableSharedFromThis<T_esft>, Y>) {
        if constexpr (std::is_convertible_v<U*, T_esft*>) {
            EnableSharedFromThis<T_esft>* esft = static_cast<EnableSharedFromThis<T_esft>*>(raw_ptr);
            if (esft && esft->weak_this_for_esft_.expired()) {
                esft->weak_this_for_esft_ = shared_ptr_instance; 
            }
        }
    }
}

} // namespace detail


template <typename T>
class EnableSharedFromThis {
protected:
    constexpr EnableSharedFromThis() noexcept = default;
    EnableSharedFromThis(const EnableSharedFromThis&) noexcept = default;
    EnableSharedFromThis& operator=(const EnableSharedFromThis&) noexcept = default;
    ~EnableSharedFromThis() = default;

public:
    SharedPtr<T> shared_from_this() {
        if (weak_this_for_esft_.expired()) {
            throw std::bad_weak_ptr();
        }
        return weak_this_for_esft_.lock();
    }

    SharedPtr<const T> shared_from_this() const {
        if (weak_this_for_esft_.expired()) {
            throw std::bad_weak_ptr();
        }
        return weak_this_for_esft_.lock();
    }

private:
    template <typename Y, typename U, typename T_esft>
    friend void detail::try_init_enable_shared_from_ptr(Y* raw_ptr, const SharedPtr<U>& sp);
    
    WeakPtr<T> weak_this_for_esft_; 
};


template <typename T>
class SharedPtr {
public:
    using element_type = T; 
    using weak_type = WeakPtr<T>;

private:
    element_type* ptr_ = nullptr;
    detail::ControlBlockBase* control_block_ = nullptr;

    template <typename U> 
    friend class SharedPtr;
    template <typename U> 
    friend class WeakPtr;
    template <typename Y, typename U, typename T_esft>
    friend void detail::try_init_enable_shared_from_ptr(Y* raw_ptr, const SharedPtr<U>& sp);
    
    template <typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(const Alloc& alloc, Args&&... args);

    template <typename U, typename... Args>
    friend SharedPtr<U> makeShared(Args&&... args);

    SharedPtr(element_type* p, detail::ControlBlockBase* cb) : ptr_(p), control_block_(cb) {
        if (!control_block_) { 
            ptr_ = nullptr;
        }
    }
    
    template<typename Y>
    void init_esft_if_needed(Y* p_raw) {
        if (p_raw) {
            detail::try_init_enable_shared_from_ptr<Y, T, std::remove_const<Y>>(p_raw, *this);
        }
    }

public:
    constexpr SharedPtr() noexcept = default;
    constexpr SharedPtr(std::nullptr_t) noexcept {}

    template <typename Y>
    explicit SharedPtr(Y* p)
    requires (std::is_convertible_v<Y*, T*> && !std::is_array_v<Y>) {
        using Deleter = std::default_delete<Y>;
        using CBAlloc = std::allocator<char>;
        using CBType = detail::ControlBlockRegular<Y, Deleter, CBAlloc>;
        
        CBAlloc cb_default_alloc;
        typename std::allocator_traits<CBAlloc>:: template rebind_alloc<CBType> cb_alloc(cb_default_alloc);

        try {
            if (p == nullptr) {
                return;
            }
            auto* temp_cb = std::allocator_traits<decltype(cb_alloc)>::allocate(cb_alloc, 1);
            new(temp_cb) CBType(p, Deleter{}, cb_default_alloc);

            control_block_ = temp_cb;
            ptr_ = p;
            init_esft_if_needed(p);
        } catch (...) {
            Deleter{}(p);
            throw;
        }
    }

    template <typename Y, typename Deleter>
    SharedPtr(Y* p, Deleter d)
    requires (std::is_convertible_v<Y*, T*>
        && !std::is_array_v<Y>
        && std::is_invocable_v<Deleter&, Y*>) {
        using CBAlloc = std::allocator<char>;
        using CBType = detail::ControlBlockRegular<Y, Deleter, CBAlloc>;
        
        CBAlloc cb_default_alloc;
        typename std::allocator_traits<CBAlloc>:: template rebind_alloc<CBType> cb_alloc(cb_default_alloc);

        try {
            auto* temp_cb = std::allocator_traits<decltype(cb_alloc)>::allocate(cb_alloc, 1);
            new(temp_cb) CBType(p, std::forward<Deleter>(d), cb_default_alloc);
            control_block_ = temp_cb;
            ptr_ = p;
            init_esft_if_needed(p);
        } catch (...) {
            std::forward<Deleter>(d)(p); 
            throw;
        }
    }

    template <typename Y, typename Deleter, typename Alloc>
    SharedPtr(Y* p, Deleter d, Alloc a)
    requires(std::is_convertible_v<Y*, T*>
        && !std::is_array_v<Y>
        && std::is_invocable_v<Deleter&, Y*>) {
        using CBType = detail::ControlBlockRegular<Y, Deleter, Alloc>;
        typename std::allocator_traits<Alloc>::template rebind_alloc<CBType> cb_alloc(a); 
        try {
            auto* temp_cb = std::allocator_traits<decltype(cb_alloc)>::allocate(cb_alloc, 1);
            new(temp_cb) CBType(p, std::forward<Deleter>(d), a);
            control_block_ = temp_cb;
            ptr_ = p;
            init_esft_if_needed(p);
        } catch (...) {
            std::forward<Deleter>(d)(p);
            throw;
        }
    }


    SharedPtr(const SharedPtr& other) noexcept
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->increment_shared_count();
        }
    }

    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other) noexcept
    requires (std::is_convertible_v<Y*, T*>)
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->increment_shared_count();
        }
    }
    
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, element_type* p) noexcept
      : ptr_(p), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->increment_shared_count();
        }
    }

    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }

    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other) noexcept
    requires (std::is_convertible_v<Y*, T*>)
        : ptr_(other.ptr_), control_block_(other.control_block_) { 
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }

    template<typename Y>
    explicit SharedPtr(const WeakPtr<Y>& r)
    requires (std::is_convertible_v<Y*, T*>) {
        SharedPtr temp = r.lock();
        if (!temp) {
            throw std::bad_weak_ptr();
        }
        ptr_ = temp.ptr_;
        control_block_ = temp.control_block_;
        temp.ptr_ = nullptr;
        temp.control_block_ = nullptr;
    }

    ~SharedPtr() {
        if (control_block_) {
            control_block_->decrement_shared_count_and_check_destroy();
        }
    }

    SharedPtr& operator=(const SharedPtr& other) noexcept {
        SharedPtr(other).swap(*this);
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(const SharedPtr<Y>& other) noexcept
    requires (std::is_convertible_v<Y*, T*>) {
        SharedPtr<T>(other).swap(*this);
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(SharedPtr<Y>&& other) noexcept
    requires (std::is_convertible_v<Y*, T*>) {
        SharedPtr<T>(std::move(other)).swap(*this);
        return *this;
    }
    
    SharedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    void reset() noexcept {
        SharedPtr().swap(*this);
    }

    template <typename Y>
    void reset(Y* p)
    requires (std::is_convertible_v<Y*, T*> && !std::is_array_v<Y>) {
        SharedPtr(p).swap(*this); 
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(control_block_, other.control_block_);
    }

    element_type* get() const noexcept {
        return ptr_;
    }

    T& operator*() const noexcept {
        return *ptr_;
    }

    T* operator->() const noexcept {
        return ptr_;
    }

    long use_count() const noexcept {
        return control_block_ ? control_block_->shared_count_ : 0;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

}; // SharedPtr class


template <typename T>
class WeakPtr {
public:
    using element_type = T;

private:
    element_type* ptr_element_type_ = nullptr; 
    detail::ControlBlockBase* control_block_ = nullptr;

    template <typename U>
    friend class WeakPtr;

    template <typename U>
    friend class SharedPtr;

    template <typename Y, typename U, typename T_esft>
    friend void detail::try_init_enable_shared_from_ptr(Y* raw_ptr, const SharedPtr<U>& sp);


public:
    constexpr WeakPtr() noexcept = default;

    template <typename Y>
    WeakPtr(const SharedPtr<Y>& sp) noexcept
    requires(std::is_convertible_v<Y*, T*>)
        : ptr_element_type_(sp.get()), control_block_(sp.control_block_) {
        if (control_block_) {
            control_block_->increment_weak_count();
        }
    }

    WeakPtr(const WeakPtr& other) noexcept
        : ptr_element_type_(other.ptr_element_type_), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->increment_weak_count();
        }
    }

    template <typename Y>
    WeakPtr(const WeakPtr<Y>& other) noexcept
    requires (std::is_convertible_v<Y*, T*>)
        : ptr_element_type_(other.ptr_element_type_), control_block_(other.control_block_) { 
        if (control_block_) {
            control_block_->increment_weak_count();
        }
    }

    WeakPtr(WeakPtr&& other) noexcept
        : ptr_element_type_(other.ptr_element_type_), control_block_(other.control_block_) {
        other.ptr_element_type_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    template <typename Y>
    WeakPtr(WeakPtr<Y>&& other) noexcept
    requires(std::is_convertible_v<Y*, T*>)
        : ptr_element_type_(other.ptr_element_type_), control_block_(other.control_block_) {
        other.ptr_element_type_ = nullptr;
        other.control_block_ = nullptr;
    }


    ~WeakPtr() {
        if (control_block_) {
            control_block_->decrement_weak_count_and_check_destroy();
        }
    }

    WeakPtr& operator=(const WeakPtr& other) noexcept {
        WeakPtr(other).swap(*this);
        return *this;
    }
    
    template <typename Y>
    WeakPtr& operator=(const WeakPtr<Y>& other) noexcept
    requires(std::is_convertible_v<Y*, T*>) {
        WeakPtr<T>(other).swap(*this);
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& other) noexcept {
        WeakPtr(std::move(other)).swap(*this);
        return *this;
    }
    
    template <typename Y>
    WeakPtr& operator=(WeakPtr<Y>&& other) noexcept
    requires(std::is_convertible_v<Y*, T*>) {
        WeakPtr<T>(std::move(other)).swap(*this);
        return *this;
    }


    template <typename Y>
    WeakPtr& operator=(const SharedPtr<Y>& sp) noexcept
    requires(std::is_convertible_v<Y*, T*>) {
        WeakPtr<T>(sp).swap(*this);
        return *this;
    }

    void reset() noexcept {
        WeakPtr().swap(*this);
    }

    void swap(WeakPtr& other) noexcept {
        std::swap(ptr_element_type_, other.ptr_element_type_);
        std::swap(control_block_, other.control_block_);
    }

    long use_count() const noexcept {
        return control_block_ ? control_block_->shared_count_ : 0;
    }

    bool expired() const noexcept {
        return !control_block_ || control_block_->shared_count_ == 0;
    }

    SharedPtr<T> lock() const {
        if (expired()) {
            throw std::bad_weak_ptr();
        }
        SharedPtr<T> result = SharedPtr<T>(ptr_element_type_, control_block_);
        control_block_->increment_shared_count();
        return result;
    }
}; // WeakPtr class

// makeShared
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    using Alloc = std::allocator<char>;
    using CBType = detail::ControlBlockMakeShared<T, Alloc>;
    
    Alloc block_allocator;
    typename std::allocator_traits<Alloc>:: template rebind_alloc<CBType> cb_alloc(block_allocator);
    
    CBType* cb_ptr = nullptr;
    try {
        cb_ptr = std::allocator_traits<decltype(cb_alloc)>::allocate(cb_alloc, 1);
        new(cb_ptr) CBType(Alloc(), std::forward<Args>(args)...);

        SharedPtr<T> result(cb_ptr->get_constructed_object(), cb_ptr);
        result.init_esft_if_needed(cb_ptr->get_constructed_object());
        return result;
    } catch (...) {
        if (cb_ptr) { 
            std::allocator_traits<decltype(cb_alloc)>::deallocate(cb_alloc, cb_ptr, 1);
        }
        throw;
    }
}

// allocateShared
template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(const Alloc& alloc, Args&&... args) {
    using CBType = detail::ControlBlockMakeShared<T, Alloc>;
    
    typename std::allocator_traits<Alloc>::template rebind_alloc<CBType> block_allocator(alloc);
    
    CBType* cb_ptr = nullptr;
    try {
        cb_ptr = std::allocator_traits<decltype(block_allocator)>::allocate(block_allocator, 1);
        new(cb_ptr) CBType(alloc, std::forward<Args>(args)...);

        SharedPtr<T> result(cb_ptr->get_constructed_object(), cb_ptr);
        result.init_esft_if_needed(cb_ptr->get_constructed_object());
        return result;
    } catch (...) {
        if (cb_ptr) {
            std::allocator_traits<decltype(block_allocator)>::deallocate(block_allocator, cb_ptr, 1);
        }
        throw;
    }
}