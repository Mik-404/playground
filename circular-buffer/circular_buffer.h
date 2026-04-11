#include <array>
#include <compare>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stdint.h>

static const size_t DYNAMIC_CAPACITY = std::numeric_limits<size_t>::max();

template <typename T, size_t Capacity = DYNAMIC_CAPACITY>
class CircularBuffer;

struct EmptyCapacity {};

namespace iterator_trap {
    
    template <typename T, bool IsConst, size_t Capacity>
    class base_iterator {
        constexpr static bool isDynamicCapacity () noexcept {
            return DYNAMIC_CAPACITY == Capacity;
        }

        using CapacityStorage = typename std::conditional_t<
            (isDynamicCapacity()),
            size_t,
            EmptyCapacity
        >;

    public:

        using iterator_category = std::random_access_iterator_tag;
        using ptr_type = std::conditional_t <IsConst, const T*, T*>;
        using ref_type = std::conditional_t <IsConst, const T&, T&>;
        using value_type = std::conditional_t <IsConst, const T, T>;

    private:
        friend CircularBuffer<T, Capacity>;
        friend base_iterator<T, false, Capacity>;

        ptr_type ptr_;
        int64_t index_;
        int64_t start_;
        int64_t sz_;
        CapacityStorage capacity_;
        bool is_end_;

        base_iterator (ptr_type ptr, size_t start, size_t sz, bool is_end, size_t capacity = 0): 
            ptr_(ptr), index_(0), start_(start), sz_(sz), is_end_(is_end) {
            if constexpr(isDynamicCapacity()) {
                capacity_ = capacity;
            }
            if (is_end) {
                index_ = sz_;
            }
        }

        ptr_type get_ptr () const {
            return ptr_;
        }
    public:
        ref_type operator*() const { return *ptr_; }
        ptr_type operator->() const { return ptr_; }

        base_iterator& operator++ () {
            if (is_end_) {
                return *this;
            }
            index_++;
            ptr_++;
            if constexpr(isDynamicCapacity()) {
                if (size_t(index_ + start_) == capacity_) {
                    ptr_ -= capacity_;
                }
            } else {
                if (size_t(index_ + start_) == Capacity) {
                    ptr_ -= Capacity;
                }
            }
            if (index_ == sz_) {
                is_end_ = true;             
            }
            return *this;
        }

        base_iterator operator++ (int) {
            base_iterator copy = *this;
            ++(*this);
            return copy;
        }

        base_iterator& operator+= (int other) {
            if (other < 0) {
                return (*this -= (-other));
            }
            if (this->is_end_) {
                return *this;
            }
            if (this->index_ + other >= this->sz_) {
                other = this->sz_ - this->index_;
            }
            this->index_ += other;
            this->ptr_ += other;
            
            if constexpr(isDynamicCapacity()) {
                if (size_t(this->index_ + this->start_) >= this->capacity_) {
                    this->ptr_ -= this->capacity_;
                }
            } else {
                if (size_t(this->index_ + this->start_) >= Capacity) {
                    this->ptr_ -= Capacity;
                }
            }
            if (this->index_ == this->sz_) {
                this->is_end_ = true;             
            }
            return *this;
        }

        base_iterator& operator-= (int other) {
            if (other < 0) {
                return (*this += (-other));
            }

            if (this->index_ == 0) {
                return *this;
            }
            this->is_end_ = false;
            if (this->index_ - other <= 0) {
                other = this->index_;
            }
            this->index_ -= other;
            this->ptr_ -= other;
            if constexpr(isDynamicCapacity()) {
                if (size_t(this->index_ + this->start_) < this->capacity_ && size_t(this->start_ + other + this->index_) >= this->capacity_) {
                    this->ptr_ += this->capacity_;
                }
            } else {
                if (size_t(this->index_ + this->start_) < Capacity && size_t(this->start_ + other + this->index_) >= Capacity) {
                    this->ptr_ += Capacity;
                }
            }
            return *this;
        }

        base_iterator& operator-- () {
            if (index_ == 0) {
                return *this;
            }
            is_end_ = false;
            index_--;
            ptr_--;
            if constexpr(isDynamicCapacity()) {
                if (size_t(index_ + start_) == capacity_ - 1) {
                    ptr_ += capacity_;
                }
            } else {
                if (index_ + start_ == Capacity - 1) {
                    ptr_ += Capacity;
                }
            }
            return *this;
        }

        base_iterator operator-- (int) {
            base_iterator result (*this);
            --(*this);
            return result;
        }
        
        friend std::strong_ordering operator<=>(const base_iterator& a, const base_iterator& b) {
            return a.index_ <=> b.index_;
        }

        bool operator== (const base_iterator& other) const {
            return this->index_ == other.index_;
        }

        operator base_iterator<T, true, Capacity> () const {
            if constexpr(isDynamicCapacity()) {
                return base_iterator<T, true, Capacity>(ptr_, start_, sz_, is_end_, capacity_);
            }
            return base_iterator<T, true, Capacity>(ptr_, start_, sz_, is_end_);

        }

        friend base_iterator operator+ (int left, const base_iterator& right) {
            base_iterator result (right);
            result += left;
            return result;
        }
    
        friend base_iterator operator+ (const base_iterator& left, int right) {
            base_iterator result (left);
            result += right;
            return result;
        }
    
        friend base_iterator operator- (const base_iterator& left, int right) {
            base_iterator result (left);
            result -= right;
            return result;
        }
    
        friend int64_t operator- (const base_iterator& left, const base_iterator& right) {
            return left.index_ - right.index_;
        }
    };
}

template <typename T, size_t Capacity>
class CircularBuffer {

    constexpr static bool isDynamicCapacity () noexcept {
        return DYNAMIC_CAPACITY == Capacity;
    }

    using CapacityStorage = typename std::conditional_t<
        (isDynamicCapacity()),
        size_t,
        EmptyCapacity
    >;

    using MemoryStorage = typename std::conditional_t<
        (isDynamicCapacity()),
        EmptyCapacity,
        std::array<char, Capacity * sizeof(T)>
    >;

public:
    using iterator = iterator_trap::base_iterator<T, false, Capacity>;
    using const_iterator = iterator_trap::base_iterator<T, true, Capacity>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    

    iterator begin() {
        if constexpr(isDynamicCapacity()) {
            return iterator(arr_ + start_, start_, sz_, false, capacity_);
        }
        return iterator(arr_ + start_, start_, sz_, false);
    }

    iterator end() {
        if constexpr(isDynamicCapacity()) {
            return iterator(arr_ + (start_ + sz_) % capacity(), start_, sz_, true, capacity_);
        }
        return iterator(arr_ + (start_ + sz_) % capacity(), start_, sz_, true);
    }

    const_iterator begin() const {
        if constexpr(isDynamicCapacity()) {
            return const_iterator(arr_ + start_, start_, sz_, false, capacity_);
        }
        return const_iterator(arr_ + start_, start_, sz_, false);
    }

    const_iterator end() const {
        if constexpr(isDynamicCapacity()) {
            return const_iterator(arr_ + (start_ + sz_) % capacity(), start_, sz_, true, capacity_);
        }
        return const_iterator(arr_ + (start_ + sz_) % capacity(), start_, sz_, true);
    }

    const_iterator cbegin() const {
        const_iterator result (begin());
        return result;
    }

    const_iterator cend() const {
        const_iterator result (end());
        return result;
    }

    reverse_iterator rbegin() {
        reverse_iterator result (end());
        return result;
    }

    reverse_iterator rend() {
        reverse_iterator result (begin());
        return result;
    }

    const_reverse_iterator rbegin() const {
        const_reverse_iterator result (end());
        return result;
    }

    const_reverse_iterator rend() const {
        const_reverse_iterator result (begin());
        return result;
    }

    const_reverse_iterator crbegin() const {
        const_reverse_iterator result (end());
        return result;
    }

    const_reverse_iterator crend() const {
        const_reverse_iterator result (begin());
        return result;
    }

    CircularBuffer () noexcept requires (!isDynamicCapacity()) : arr_(reinterpret_cast<T*> (memory_.begin())), sz_(0), start_(0) {}

    explicit CircularBuffer (size_t capacity): sz_(0), start_(0) {
        if (!isDynamicCapacity() && Capacity != capacity) {
            throw std::invalid_argument ("capacity tuudda syuuda");
            return;
        }
        if constexpr (isDynamicCapacity()) {
            arr_ = reinterpret_cast<T*> (new char [capacity * sizeof(T)]),
            capacity_ = capacity;
        } else {
            arr_ = reinterpret_cast<T*> (memory_.begin());
        }
    }

    CircularBuffer (const CircularBuffer<T, Capacity>& other): sz_(other.sz_), start_(0) {
        size_t capacity = Capacity;
        if constexpr(isDynamicCapacity()) {
            capacity_ = other.capacity();
            capacity = capacity_;
            arr_ = reinterpret_cast<T*> (new char[capacity * sizeof(T)]);
        } else {
            arr_ = reinterpret_cast<T*> (memory_.begin());
        }
        size_t index = 0;
        try {
            for (auto iter = other.begin(); iter != other.end(); ++index, ++iter) {
                new(arr_ + index) T(*iter);
            }
        } catch (...) {
            for (size_t old_index = 0; old_index < index; ++old_index) {
                (arr_ + old_index)->~T();
            }
            if (isDynamicCapacity()) {
                delete[] reinterpret_cast<char*> (arr_);
            }
            throw;
        }
    }


    CircularBuffer (CircularBuffer<T, Capacity>&& other): sz_(other.sz_), start_(0) {
        if constexpr(isDynamicCapacity()) {
            capacity_ = other.capacity();
            arr_ = other.arr_;
            other.arr_ = nullptr;
            other.capacity_ = 0;
            other.sz_ = 0;
            other.start_ = 0;
            return;
        }
        arr_ = reinterpret_cast<T*> (memory_.begin());
        size_t index = 0;
        try {
            for (auto iter = other.begin(); iter != other.end(); ++index, ++iter) {
                new(arr_ + index) T(std::move(*iter));
                (iter)->~T();
            }
            other.sz_ = 0;
            other.start_ = 0;
        } catch (...) {
            for (size_t old_index = 0; old_index < index; ++old_index) {
                (arr_ + old_index)->~T();
            }
            if (isDynamicCapacity()) {
                delete[] reinterpret_cast<char*> (arr_);
            }
            throw;
        }
    }


    ~CircularBuffer () noexcept {
        for (auto iter = begin(); iter != end(); ++iter) {
            (iter)->~T();
        }
        if constexpr(isDynamicCapacity()) {
            delete[] reinterpret_cast<char*> (arr_);
        }
    }

    void swap (CircularBuffer& other) {
        if constexpr (isDynamicCapacity()) {
            std::swap(capacity_, other.capacity_);
            std::swap(arr_, other.arr_);
            std::swap(start_, other.start_);
            std::swap(sz_, other.sz_);    
        } else {
            CircularBuffer<T, Capacity> copy(std::move(other));
            for (auto iter = other.begin(); iter != other.end(); ++iter) {
                iter->~T();
            }
            other.sz_ = 0;
            other.start_ = 0;
            size_t index = 0;
            for (auto iter = begin(); iter != end(); ++index, ++iter) {
                new(other.arr_ + index) T(std::move(*iter));
                other.sz_++;
            }
            
            for (auto iter = begin(); iter != end(); ++iter) {
                iter->~T();
            }
            sz_ = 0;
            start_ = 0;
            index = 0;
            for (auto iter = copy.begin(); iter != copy.end(); ++index, ++iter) {
                new(arr_ + index) T(std::move(*iter));
                sz_++;
            }
        }
    }

    CircularBuffer& operator= (CircularBuffer other) {
        swap(other);
        return *this;
    }

    T& at (int64_t pos) {
        if (pos < 0 || pos >= int64_t(sz_)) {
            throw std::out_of_range("pupupu, You've gone beyond the array");
        }
        return *(begin() + pos);
    }

    const T& at (int64_t pos) const {
        if (pos < 0 || pos >= sz_) {
            throw std::out_of_range("pupupu, You've gone beyond the array");
        }
        return *(cbegin() + pos);
    }

    T& operator[] (int64_t pos) noexcept {
        return *(begin() + pos);
    }

    const T& operator[] (int64_t pos) const noexcept {
        const_iterator iter = cbegin();
        iter+=pos;
        return *(iter);
    }

    bool full () const noexcept {
        return sz_ == capacity();
    } 

    bool empty () const noexcept {
        return sz_ == 0;
    } 

    size_t size() const noexcept {
        return sz_;
    }

    size_t capacity() const noexcept {
        if constexpr (isDynamicCapacity()) {
            return capacity_;
        }
        return Capacity;
    }

    void push_back (const T& new_elem) {
        if (full()) {
            auto pos = begin();
            (*pos) = new_elem;
            start_ = (start_ + 1) % capacity();
        } else {
            new(end().get_ptr()) T(new_elem);
            sz_++;
        }
    }

    void push_front (const T& new_elem) {
        if (full()) {
            auto pos = end() - 1;
            (*pos) = new_elem;
            start_ = (start_ - 1 + capacity()) % capacity();
        } else {
            
            start_ = (start_ - 1 + capacity()) % capacity();
            sz_++;
            try {
                new(begin().get_ptr()) T(new_elem);
            } catch (...) {
                start_ = (start_ + 1) % capacity();
                sz_--;
                throw;
            }
        }
    }

    void pop_back() {
        if (!empty()) {
            auto pos = std::prev(end());
            (pos)->~T();
            sz_--;
        }
    }

    void pop_front() {
        if (!empty()) {
            auto pos = begin();
            (pos)->~T();
            sz_--;
            start_++;
        }
    }

    void insert (CircularBuffer::iterator pos, const T& new_elem) {
        if (full() && pos == begin()) {
            return;
        }
        if (empty()) {
            if (pos == begin()) {
                push_back(new_elem);
            }
            return;
        }
        CircularBuffer temp_obj(*this);
        auto iter = temp_obj.end();
        if (full()) {
            temp_obj.pop_front();
        }
        for (;iter > pos; iter--) {
            new(iter.get_ptr()) T(*std::prev(iter));
            (std::prev(iter))->~T();
        }
        new(iter.get_ptr()) T(new_elem);
        temp_obj.sz_++;
        swap(temp_obj);
    }

    void erase (CircularBuffer::iterator pos) {
        if (empty()) {
            throw std::logic_error("Removing from an empty container");
        }
        CircularBuffer temp_obj(capacity());
        for (auto iter = begin(); iter != end(); iter++) {
            if (pos != iter) {
                temp_obj.push_back(*iter);
            }
        }
        swap(temp_obj);
    }

private:
    alignas(T) MemoryStorage memory_;
    T* arr_;
    size_t sz_;
    size_t start_;
    CapacityStorage capacity_;
};
