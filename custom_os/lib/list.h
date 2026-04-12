#pragma once

#include <cstddef>
#include <cstdint>

#include "kernel/panic.h"

struct ListNode {
    ListNode* next = nullptr;
    ListNode* prev = nullptr;

    void Insert(ListNode* p, ListNode* n) {
        p->next = this;
        prev = p;
        next = n;
        n->prev = this;
    }

    void Remove() {
        prev->next = next;
        next->prev = prev;
    }
};

template <typename T, typename P, P T::* Field>
inline T* ContainerOf(P* node) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(node) - (size_t)&(reinterpret_cast<T*>(0)->*Field));
}

template <typename T, ListNode T::* Field>
class ListHead {
private:
    ListNode head_;

public:
    class Iterator {
    private:
        ListNode* ptr_;

    public:
        Iterator(ListNode* ptr) : ptr_(ptr) {}

        Iterator& operator++() {
            ptr_ = ptr_->next;
            return *this;
        }

        T* Value() {
            return ContainerOf<T, ListNode, Field>(ptr_);
        }

        bool operator!=(const Iterator& other) const {
            return ptr_ != other.ptr_;
        }

        T& operator*() {
            return *Value();
        }

        T* operator->() {
            return Value();
        }
    };

    ListHead() {
        head_.next = &head_;
        head_.prev = &head_;
    }

    void InsertLast(T& obj) {
        (obj.*Field).Insert(head_.prev, &head_);
    }

    void InsertFirst(T& obj) {
        (obj.*Field).Insert(&head_, head_.next);
    }

    bool Empty() const {
        return &head_ == head_.next;
    }

    T& First() {
        BUG_ON(Empty());
        return *ContainerOf<T, ListNode, Field>(head_.next);
    }

    const T& First() const {
        BUG_ON(Empty());
        return *ContainerOf<T, ListNode, Field>(head_.next);
    }

    Iterator begin() {
        return Iterator(head_.next);
    }

    Iterator end() {
        return Iterator(&head_);
    }
};
