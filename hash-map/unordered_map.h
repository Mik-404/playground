#include <functional>
#include <iostream>
#include <stdexcept>

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
  using key_type = Key;
  using value_type = Value;
  using hasher = Hash;
  using predicate = KeyEqual;
  using hash_type = size_t;
  using base_unit = std::pair<const key_type, value_type>;
  using moveable_base_unit = std::pair<key_type, value_type>;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;

  struct base_node {
    base_node* right = this;

    base_node() noexcept = default;

    ~base_node() noexcept = default;

    base_node(base_node* prev) noexcept : right(prev) {}

    base_node(base_node&& other) noexcept : right(other.right) {
      other.right = nullptr;
    }

    base_node& operator=(const base_node& other) noexcept {
      right = other.right;
      return *this;
    }

    void swap(base_node& other) noexcept { std::swap(right, other.right); }
  };

  struct node : base_node {
    base_unit content_;
    hash_type hash_;

    node(key_type&& key, value_type&& value, const hasher& hasher_) noexcept(
        noexcept(base_unit(std::move(key), std::move(value))) &&
        noexcept(hasher_(key)))
        : content_(std::move(key), std::move(value)),
          hash_(hasher_(content_.first)) {}

    node(const key_type& key, const value_type& value,
         const hasher& hasher_) noexcept(noexcept(base_unit(key, value)) &&
                                         noexcept(hasher_(key)))
        : content_(key, value), hash_(hasher_(content_.first)) {}

    node(moveable_base_unit&& content, const hasher& hasher_) noexcept(
        noexcept(std::is_nothrow_move_constructible_v<base_unit>) &&
        noexcept(hasher_(content.first)))
        : content_(std::move(content)), hash_(hasher_(content_.first)) {}

    node(const base_unit& content,
         const hasher& hasher_) noexcept(noexcept(base_unit(content)) &&
                                         noexcept(hasher_(content.first)))
        : content_(content), hash_(hasher_(content_.first)) {}

    // template <typename... Args>
    // node (Args&&... args)
    // : content_(std::forward<Args>(args)...) {
    //     hash_ = hasher()(content_.first);
    // }

    node(node&& other) noexcept(noexcept(node(std::move(other.__key),
                                              std::move(other.__value))))
        : base_unit(std::move(other.__key), std::move(other.__value)),
          base_node(std::move(other)) {}

    ~node() noexcept = default;

    void init(base_node* prev, base_node* next) noexcept {
      this->right = next;
      prev->right = this;
    }
  };

  template <bool IsConst>
  class base_iterator {
    friend class UnorderedMap;
    using base_node_ptr =
        std::conditional_t<IsConst, const base_node*, base_node*>;
    using node_ptr = std::conditional_t<IsConst, const node*, node*>;
    base_node_ptr ptr_;

    base_iterator(base_node_ptr ptr) : ptr_(ptr) {}

    base_node* get_ptr() const noexcept { return (ptr_); }

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = base_unit;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<IsConst, const base_unit*, base_unit*>;
    using reference = std::conditional_t<IsConst, const base_unit&, base_unit&>;

    reference operator*() const {
      return static_cast<node_ptr>(ptr_)->content_;
    }
    pointer operator->() const {
      return &(static_cast<node_ptr>(ptr_)->content_);
    }

    base_iterator& operator++() {
      ptr_ = ptr_->right;
      return *this;
    }

    base_iterator operator++(int) {
      base_iterator copy = *this;
      ++(*this);
      return copy;
    }

    bool operator==(const base_iterator& other) const {
      return ptr_ == other.ptr_;
    }

    bool operator!=(const base_iterator& other) const {
      return ptr_ != other.ptr_;
    }

    operator base_iterator<true>() const { return {ptr_}; }
  };

 public:
  using iterator = base_iterator<false>;
  using const_iterator = base_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

 private:
  using insert_result = std::pair<iterator, bool>;
  using alloc = Alloc;
  using alloc_traits = std::allocator_traits<alloc>;
  using node_allocator = typename alloc_traits::template rebind_alloc<node>;
  using node_allocator_traits = std::allocator_traits<node_allocator>;
  using node_ptr_allocator =
      typename alloc_traits::template rebind_alloc<base_node*>;
  using node_ptr_allocator_traits = std::allocator_traits<node_ptr_allocator>;

  void update_begin_() noexcept {
    if (!empty()) {
      bkt_ptr_[bkt_count_]->right =
          (&(fake_node_));
    } else {
      fake_node_.right = (&(fake_node_));
    }
  }

  void add_to_index_(node* new_node, base_node** bkt_holder, size_type mod) noexcept {
    if (bkt_holder[new_node->hash_ % mod]) {
      base_node* prev_node = bkt_holder[new_node->hash_ % mod];
      new_node->right = prev_node->right;
      prev_node->right = new_node;
    } else if (bkt_holder[mod] == &(fake_node_)) {
      bkt_holder[mod] = new_node;
      new_node->right = &(fake_node_);
      fake_node_.right = new_node;
      bkt_holder[new_node->hash_ % mod] = &(fake_node_);
    } else {
      node* node_for_replace = static_cast<node*>(fake_node_.right);
      bkt_holder[node_for_replace->hash_ % mod] = new_node;
      new_node->right = fake_node_.right;
      fake_node_.right = new_node;
      bkt_holder[new_node->hash_ % mod] = &(fake_node_);
    }
  }

  void rehash_(size_t new_sz) noexcept {
    iterator iter = begin();
    iterator iter_end = end();
    node_ptr_allocator temp_alloc(allocator_);
    base_node** new_memory =
        node_ptr_allocator_traits::allocate(temp_alloc, new_sz + 1);
    std::fill(new_memory, new_memory + new_sz, nullptr);

    new_memory[new_sz] = end().get_ptr();
    while (iter != iter_end) {
      node* temp = static_cast<node*>(iter.get_ptr());
      iter++;
      add_to_index_(temp, new_memory, new_sz);
    }
    if (bkt_count_ != 0) {
      node_ptr_allocator_traits::deallocate(temp_alloc, bkt_ptr_,
                                            bkt_count_ + 1);
    }
    bkt_count_ = new_sz;
    bkt_ptr_ = new_memory;
  }

  void rehash_if_need_() noexcept {
    if (bkt_count_ == 0) {
      rehash_(11);
    } else if (load_factor() > mx_load_factor_) {
      rehash_(bkt_count_ * growth_factor_);
    }
  }

  void clear() noexcept {
    iterator iter = begin();
    iterator iter_end = end();
    while (iter != iter_end) {
      node* temp = static_cast<node*>(iter.get_ptr());
      iter++;
      node_allocator_traits::destroy(allocator_, temp);
      node_allocator_traits::deallocate(allocator_, temp, 1);
    }
    node_ptr_allocator temp_alloc(allocator_);
    node_ptr_allocator_traits::deallocate(temp_alloc, bkt_ptr_, bkt_count_ + 1);
    bkt_count_ = 0;
    n_elt_ = 0;
    bkt_ptr_ = nullptr;
    fake_node_.right = &(fake_node_);
  }

  
  size_type get_addr (iterator pos) const noexcept {
    return static_cast<node*>(pos.get_ptr())->hash_ % bkt_count_;
  }


  /*
  ========================
  #########FIELDS#########
  ========================
  */

  base_node fake_node_;
  size_type bkt_count_;
  size_type n_elt_;
  float mx_load_factor_ = 1.0f;
  const size_type growth_factor_ = 2;
  base_node** bkt_ptr_;
  [[no_unique_address]] node_allocator allocator_;
  [[no_unique_address]] hasher hasher_;
  [[no_unique_address]] predicate predicate_;

 public:
  iterator begin() noexcept { return iterator(fake_node_.right); }
  iterator end() noexcept {
    return iterator(&(fake_node_));
  }
  const_iterator begin() const noexcept {
    return const_iterator(fake_node_.right);
  }
  const_iterator end() const noexcept {
    return const_iterator(&(fake_node_));
  }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  UnorderedMap() noexcept
      : fake_node_(), bkt_count_(), n_elt_(), bkt_ptr_() {}

  UnorderedMap(node_allocator ext_alloc) noexcept(
      noexcept(node_allocator(ext_alloc)))
      : fake_node_(), bkt_count_(), n_elt_(), allocator_(ext_alloc) {}

  UnorderedMap(const UnorderedMap& other)
      : fake_node_(),
        bkt_count_(),
        n_elt_(),
        allocator_(
            node_allocator_traits ::select_on_container_copy_construction(
                other.get_allocator())) {
      node* new_node = nullptr;
      try {
      rehash_(other.bkt_count_);
      auto iter = other.begin();
      while (n_elt_ < other.n_elt_) {
        new_node = node_allocator_traits::allocate(allocator_, 1);
        node_allocator_traits::construct(allocator_, new_node, *iter, hasher_);
        add_to_index_(new_node, bkt_ptr_, bkt_count_);
        n_elt_++;
        iter++;
      }
    } catch (...) {
      node_allocator_traits::deallocate(allocator_, new_node, 1);
      clear();
      throw;
    }
  }

  UnorderedMap& operator=(const UnorderedMap& other) {
    if (this != &(other)) {
      constexpr bool copy_storage_ =
          node_allocator_traits::propagate_on_container_copy_assignment::value;
      node_allocator newalloc = copy_storage_ ? other.allocator_ : allocator_;
      UnorderedMap temp_obj = other;
      this->swap(temp_obj);
      allocator_ = newalloc;
    }
    return *this;
  }

  UnorderedMap(UnorderedMap&& other) noexcept(noexcept(node_allocator(
      node_allocator_traits::propagate_on_container_move_assignment::value
          ? std::move(other.allocator_)
          : node_allocator())))
      : bkt_count_(other.bkt_count_),
        n_elt_(other.n_elt_),
        bkt_ptr_(other.bkt_ptr_),
        allocator_(
            node_allocator_traits::propagate_on_container_move_assignment::value
                ? std::move(other.allocator_)
                : node_allocator()),
        hasher_(std::move(other.hasher_)),
        predicate_(std::move(other.predicate_)) {
    if (bkt_ptr_) {
      fake_node_.right = other.fake_node_.right;
      bkt_ptr_[bkt_count_]->right = &(fake_node_);
    }
    other.n_elt_ = 0;
    other.bkt_count_ = 0;
    other.bkt_ptr_ = nullptr;
    other.hasher_ = hasher();
    other.predicate_ = predicate();
    other.fake_node_.right = &(other.fake_node_);
  }

  UnorderedMap& operator=(UnorderedMap&& other) noexcept(
      noexcept(UnorderedMap(std::move(other))) &&
      noexcept(std::is_nothrow_move_assignable_v<node_allocator>)) {
    if (this != &(other)) {
      constexpr bool copy_storage_ =
          node_allocator_traits::propagate_on_container_move_assignment::value;
      node_allocator newalloc = copy_storage_ ? other.allocator_ : allocator_;
      UnorderedMap temp_obj = std::move(other);
      this->swap(temp_obj);
      allocator_ = newalloc;
    }
    return *this;
  }

  Value& operator[](key_type&& key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    return emplace(std::move(key), value_type()).first->second;
  }

  Value& operator[](const key_type& key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    return emplace(key, value_type()).first->second;
  }

  Value& at(const Key& key) {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("Key not found");
    }
    return it->second;
  }

  const Value& at(const Key& key) const {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("Key not found");
    }
    return it->second;
  }

  iterator erase(iterator pos) {
    if (pos == end()) return pos;

    base_node* prev =
        bkt_ptr_[get_addr(pos)];
    while (prev->right != pos.get_ptr()) {
      prev = prev->right;
    }
    prev->right = pos.get_ptr()->right;
    
    if (std::next(pos) == end()) {
      bkt_ptr_[bkt_count_] = prev;
    } else if (get_addr(pos) != get_addr(std::next(pos))) {
      bkt_ptr_[get_addr(std::next(pos))] = prev;
    }

    if (get_addr(pos) !=
        static_cast<node*>(prev)->hash_ % bkt_count_) {
      bkt_ptr_[get_addr(pos)] =
          nullptr;
    }
    node_allocator_traits::destroy(allocator_,
                                   static_cast<node*>(pos.get_ptr()));
    node_allocator_traits::deallocate(allocator_,
                                      static_cast<node*>(pos.get_ptr()), 1);
    n_elt_--;
    // Возвращаем итератор на следующий элемент
    return iterator(prev->right);
  }

  iterator erase(iterator first, iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return last;
  }

  iterator find(const key_type& key) noexcept(noexcept(hasher_(key))) {
    hash_type hash = hasher_(key);

    if (bkt_count_ == 0) return end();

    if (!bkt_ptr_[hash % bkt_count_]) return end();
    iterator current(bkt_ptr_[hash % bkt_count_]);
    while (std::next(current) != end() &&
           get_addr(std::next(current)) ==
               hash % bkt_count_ &&
           !predicate_(key, std::next(current)->first)) {
      current++;
    }
    if (std::next(current) != end() &&
        get_addr(std::next(current)) ==
            hash % bkt_count_) {
      return std::next(current);
    }
    return end();
  }

  insert_result insert(const moveable_base_unit& new_elem) {
    iterator temp_iter = find(new_elem.first);
    if (temp_iter != end()) {
      return insert_result(temp_iter, false);
    }
    node* new_node = node_allocator_traits::allocate(allocator_, 1);
    try {
      node_allocator_traits::construct(allocator_, new_node, new_elem, hasher_);
    } catch (...) {
      node_allocator_traits::deallocate(allocator_, new_node, 1);
      throw;
    }
    rehash_if_need_();
    add_to_index_(new_node, bkt_ptr_, bkt_count_);
    n_elt_++;
    return insert_result(find(new_elem.first), true);
  }

  insert_result insert(moveable_base_unit&& new_elem) {
    iterator temp_iter = find(new_elem.first);
    if (temp_iter != end()) {
      return insert_result(temp_iter, false);
    }
    rehash_if_need_();
    node* new_node = node_allocator_traits::allocate(allocator_, 1);
    node_allocator_traits::construct(allocator_, new_node, std::move(new_elem),
                                     hasher_);
    add_to_index_(new_node, bkt_ptr_, bkt_count_);
    n_elt_++;
    return insert_result(find(new_elem.first), true);
  }

  template <typename InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(std::forward<decltype(*first)>(*first));
    }
  }

  template <typename... Args>
  insert_result emplace(Args&&... args) {
    node* new_node = node_allocator_traits::allocate(allocator_, 1);
    try {
      node_allocator_traits::construct(allocator_, new_node,
                                       std::forward<Args>(args)..., hasher_);
    } catch (...) {
      node_allocator_traits::deallocate(allocator_, new_node, 1);
      throw;
    }
    iterator temp_iter = find(new_node->content_.first);
    if (temp_iter != end()) {
      node_allocator_traits::destroy(allocator_, new_node);
      node_allocator_traits::deallocate(allocator_, new_node, 1);
      return insert_result(temp_iter, false);
    }
    rehash_if_need_();
    add_to_index_(new_node, bkt_ptr_, bkt_count_);
    n_elt_++;
    return insert_result(find(new_node->content_.first), true);
  }

  ~UnorderedMap() noexcept { clear(); }

  void reserve(size_type new_sz) { rehash_(new_sz); }

  void swap(UnorderedMap& other) noexcept {
    constexpr bool __swap_storage =
        node_allocator_traits::propagate_on_container_swap::value;
    if constexpr (__swap_storage) {
      std::swap(allocator_, other.allocator_);
    }
    std::swap(n_elt_, other.n_elt_);
    fake_node_.swap(other.fake_node_);
    std::swap(bkt_count_, other.bkt_count_);
    std::swap(mx_load_factor_, other.mx_load_factor_);
    std::swap(bkt_ptr_, other.bkt_ptr_);
    std::swap(hasher_, other.hasher_);
    std::swap(predicate_, other.predicate_);
    update_begin_();
    other.update_begin_();
  }

  float max_load_factor() const noexcept { return mx_load_factor_; }

  void max_load_factor(float new_mx_load_factor) noexcept {
    mx_load_factor_ = new_mx_load_factor;
  }

  float load_factor() const noexcept {
    return float(n_elt_) / float(bkt_count_);
  }

  alloc get_allocator() const noexcept { return alloc(allocator_); }

  size_t size() const noexcept { return n_elt_; }

  [[nodiscard]] bool empty() const noexcept { return n_elt_ == 0; }
};