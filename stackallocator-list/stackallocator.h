#include <cstdint>
#include <list>
#include <type_traits>

template <size_t N>
class StackStorage {
    std::byte storage__[N];
    void* last_free__ = storage__;

public:
    inline void* get_ptr() const noexcept  {
        return last_free__;
    }

    inline const std::byte* get_start() const noexcept {
        return storage__;
    }

    inline void update_position_on_allocate(size_t count) noexcept {
        last_free__ = reinterpret_cast<void*> (reinterpret_cast<uintptr_t> (last_free__) + count);
    }
    inline void update_position_on_deallocate(void* ptr, size_t count) noexcept {
        if (reinterpret_cast<void*> (reinterpret_cast<uintptr_t> (last_free__) + count) == last_free__) [[unlikely]] {
            last_free__ = ptr;
        }
    }
};

template <typename T, size_t N>
class StackAllocator {

    template <typename U, size_t M>
    friend class StackAllocator;

    StackStorage<N>* ptr__ = nullptr;
public:
    using value_type = T;

    template <typename U>
    inline StackAllocator (StackAllocator<U, N> other) noexcept: ptr__(other.ptr__) {};

    inline StackAllocator (StackStorage<N>& storage) noexcept: ptr__(&storage) {};

    inline T* allocate(size_t count) {
        uintptr_t cur_addess = reinterpret_cast<uintptr_t>((*ptr__).get_ptr());
        uintptr_t start_address = reinterpret_cast<uintptr_t>((*ptr__).get_start());
        size_t space = N - (cur_addess - start_address);
        
        void* aligned_ptr = reinterpret_cast<void*> (cur_addess);
        
        if (!std::align(alignof(T), 
                    count * sizeof(T), 
                    aligned_ptr, 
                    space)) [[unlikely]] 
        {
            throw std::bad_alloc();
        }

        size_t required_size = count * sizeof(T);
        if (reinterpret_cast<uintptr_t>(aligned_ptr) + required_size - start_address > N) [[unlikely]] {
            throw std::bad_alloc();
        }

        (*ptr__).update_position_on_allocate(
            reinterpret_cast<uintptr_t>(aligned_ptr) - cur_addess + required_size
        );

        return static_cast<T*>(aligned_ptr);
    }

    inline void deallocate (T* ptr, size_t count) noexcept {
        (*ptr__).update_position_on_deallocate(ptr, count * sizeof(T));
    }

    template <typename U, typename... Args>
    inline void construct(U* ptr, Args&&... args) noexcept(noexcept(U(std::forward<Args>(args)...))) {
        new(ptr) U(std::forward<Args>(args)...);
    }

    template <typename U>
    inline void destroy(U* ptr) noexcept {
        ptr->~U();
    }

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

    template <typename U, size_t Cap>
    inline bool operator== ([[maybe_unused]]const StackAllocator<U, Cap>& other) noexcept {
        return false;
    }

    inline bool operator== (const StackAllocator& other) noexcept {
        return ptr__ == other.ptr__;
    }

    template <typename U, size_t Cap>
    inline bool operator!= (const StackAllocator<U, Cap>& other) noexcept {
        return ! (*this == other);
    }
};

template <typename T, typename Allocator = std::allocator<T>>
class List {

    struct BaseNode {
        BaseNode* right = this;
        BaseNode* left = this;

        BaseNode () noexcept = default;

        BaseNode (BaseNode* prev, BaseNode* next) noexcept
        : right(prev), left(next) {}

        BaseNode& change_node_ (const BaseNode& other) noexcept {
            right = other.right;
            left = other.left;
            other.right->left = this;
            other.left->right = this;
            return *this;
        }
    };

    struct Node : BaseNode {
        T value;

        template <typename... Args>
        Node (BaseNode* prev, BaseNode* next, Args&&... args)
        noexcept (noexcept(T(std::forward<Args>(args)...)))
        : value (std::forward<Args>(args)...) {
            this->right = prev;
            this->left = next;
            prev->left = this;
            next->right = this;
        }

        ~Node() noexcept {
            this->right->left = this->left;
            this->left->right = this->right;
        }
    };

    using AllocatorForNode = typename std::allocator_traits<Allocator>:: template rebind_alloc<Node>;
    using AllocatorTraits = std::allocator_traits<AllocatorForNode>;

    BaseNode fakeNode__;
    size_t sz__;
    [[no_unique_address]] AllocatorForNode allocator__;

    void clear_nodes() noexcept {
        while (!empty()) {
            pop_front();
        }
    }

    template <bool IsConst>
    class base_iterator {
        friend class List;
        using base_node_ptr = std::conditional_t <IsConst, const BaseNode*, BaseNode*>; 
        using node_ptr = std::conditional_t <IsConst, const Node*, Node*>; 
        base_node_ptr ptr__;

        base_iterator (base_node_ptr ptr)
        : ptr__(ptr) {}
        
        BaseNode* get_ptr () const noexcept{
            return const_cast<BaseNode*> (ptr__);
        }
    
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t <IsConst, const T*, T*>;
        using reference = std::conditional_t <IsConst, const T&, T&>;

        reference operator*() const { return (static_cast<node_ptr> (ptr__))->value; }
        pointer operator->() const { return &((static_cast<node_ptr> (ptr__))->value); }

        base_iterator& operator++ () {
            ptr__ = ptr__->left;
            return *this;
        }

        base_iterator operator++ (int) {
            base_iterator copy = *this;
            ++(*this);
            return copy;
        }

        base_iterator& operator-- () {
            ptr__ = ptr__->right;
            return *this;
        }

        base_iterator operator-- (int) {
            base_iterator copy = *this;
            --(*this);
            return copy;
        }

        bool operator== (const base_iterator& other) const {
            return ptr__ == other.ptr__;
        }

        bool operator!= (const base_iterator& other) const {
            return ptr__ != other.ptr__;
        }

        operator base_iterator<true> () const {
            return {ptr__};
        }
    };

public:

    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() noexcept { return iterator(fakeNode__.left); }
    iterator end() noexcept { return iterator(&fakeNode__); }
    const_iterator begin() const noexcept { return const_iterator(fakeNode__.left); }
    const_iterator end() const noexcept { return const_iterator(&fakeNode__); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    List (AllocatorForNode alloc = AllocatorForNode()) noexcept(noexcept(AllocatorForNode(alloc)))
    : fakeNode__(), sz__(0), allocator__(alloc) {}

    explicit 
    List (size_t size, const T& other, AllocatorForNode alloc = AllocatorForNode())
    : List(alloc) {
        try {
            while (sz__ < size) {
                push_back(other);
            }
        } catch (...) {
            while (sz__ > 0) {
                pop_back();
            }
            throw;
        }
    }

    explicit 
    List (size_t size, AllocatorForNode alloc = AllocatorForNode())
    : List(alloc) {
        try {
            while (sz__ < size) {
                Node* new_node = AllocatorTraits::allocate(allocator__, 1);
                try{
                    AllocatorTraits::construct(allocator__, new_node, fakeNode__.right, &fakeNode__);
                    sz__++;
                } catch (...) {
                    AllocatorTraits::deallocate(allocator__, new_node, 1);
                    throw;
                }
            }
        } catch (...) {
            while (sz__ > 0) {
                pop_back();
            }
            throw;
        }
    }

    List (const List& other)
    : fakeNode__(), sz__(0), allocator__(AllocatorTraits
    ::select_on_container_copy_construction(other.get_allocator())) {
        try {
            auto iter = other.begin();
            while (sz__ < other.sz__) {
                push_back(*iter);
                iter++;
            }
        } catch (...) {
            while (sz__ > 0) {
                pop_back();
            }
            throw;
        }
        
    }

    List& operator= 
    (const List& other)
    noexcept(noexcept(AllocatorForNode(other.allocator__))
    && noexcept(AllocatorForNode(allocator__))
    && noexcept(List(other))) {
        constexpr bool __copy_storage =
            AllocatorTraits::propagate_on_container_copy_assignment::value;
        AllocatorForNode new_allocator__ = __copy_storage
            ? other.allocator__ : allocator__;
        List tempObj = other;
        tempObj.swap(*this);
        allocator__ = new_allocator__;
        return *this;
    }

    ~List() noexcept {
        clear_nodes();
    }

    void swap (List& other) noexcept {
        constexpr bool __swap_storage =
                AllocatorTraits::propagate_on_container_swap::value;
        if constexpr (__swap_storage) {
            std::swap(allocator__, other.allocator__);
        }
        BaseNode temp_node;
        temp_node.change_node_(other.fakeNode__);
        other.fakeNode__.change_node_(fakeNode__);
        fakeNode__.change_node_(temp_node);
        std::swap (sz__, other.sz__);
    }

    void push_back(const T& new_elem) {
        insert(end(), new_elem);
    }

    void push_front(const T& new_elem) {
        insert(begin(), new_elem);
    }

    void pop_back() noexcept {
        erase(std::prev(end()));
    }

    void pop_front() noexcept {
        erase(begin());
    }

    void insert(const_iterator pos, const T& other) {
        Node* new_node = AllocatorTraits::allocate(allocator__, 1);
        try{
            AllocatorTraits::construct(allocator__, new_node, std::prev(pos).get_ptr(), pos.get_ptr(), other);
            sz__++;
        } catch (...) {
            AllocatorTraits::deallocate(allocator__, new_node, 1);
            throw;
        }
    }

    void erase(const_iterator pos) noexcept {
        AllocatorTraits::destroy(allocator__, static_cast<Node*>(pos.get_ptr()));
        AllocatorTraits::deallocate(allocator__, static_cast<Node*>(pos.get_ptr()), 1);
        sz__--;
    }

    Allocator get_allocator() const noexcept {
        return Allocator(allocator__);
    }

    size_t size() const noexcept { 
        return sz__;
    }
    bool empty() const noexcept {
        return sz__ == 0;
    }
};