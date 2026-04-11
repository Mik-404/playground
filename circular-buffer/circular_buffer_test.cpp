#include "circular_buffer.h"

#include <cassert>
#include <vector>
#include <ranges>
#include <algorithm>

enum class WhenCapacityKnown {
  KNOWN_AT_COMPILETIME,
  KNOWN_AT_RUNTIME
};

// Acts like a factory for the CircularBuffer
template<typename T, WhenCapacityKnown When, std::size_t Capacity>
auto create_buffer() {
  constexpr std::size_t TemplatedCapacity =
      When == WhenCapacityKnown::KNOWN_AT_COMPILETIME ? Capacity : DYNAMIC_CAPACITY;

  return CircularBuffer<T, TemplatedCapacity>(Capacity);
}


// Here we have some helper functions for the actual tests ('assert_{something}')

template<typename Buffer>
void assert_size_and_capacity(
    const Buffer& buffer,
    std::size_t expected_capacity,
    std::size_t expected_size
)
{
  assert(buffer.size() == expected_size);
  assert(buffer.capacity() == expected_capacity);
  assert(buffer.empty() == (expected_size == 0));
  assert(buffer.full() == (expected_size == expected_capacity));
}

template<typename Buffer, typename U, std::size_t ExpectedContentsSize>
void assert_contents(const Buffer& buffer, const U (&expected_contents)[ExpectedContentsSize]) {
  assert(buffer.size() == ExpectedContentsSize);

  for (std::size_t i = 0; i < ExpectedContentsSize; ++i) {
    assert(buffer[i] == expected_contents[i]);
  }
}

template<typename Buffer, typename Range>
void assert_contents(const Buffer& buffer, Range&& range) {
  std::size_t i = 0;

  for (const auto& value: range) {
    assert(buffer[i] == value);
    ++i;
  }

  assert(buffer.size() == i);
}

namespace TestsByPetialetia {

template<WhenCapacityKnown When>
void test_empty() {
  auto buffer = create_buffer<int, When, 32>();
  assert_size_and_capacity(buffer, 32, 0);
}

// This struct is a wrapper for an int value (allocated by a call to 'new')
struct HeapInt {
public:
  HeapInt() = delete;

  HeapInt(int value): ptr(new int(value))
  {
  }

  HeapInt(const HeapInt& other): ptr(new int(*other.ptr))
  {
  }

  HeapInt& operator=(const HeapInt& other) {
    auto copy = other;
    swap(copy);

    return *this;
  }

  void swap(HeapInt& other) {
    std::swap(ptr, other.ptr);
  }

  friend bool operator==(const HeapInt& left, int right) {
    return *left.ptr == right;
  }

  friend bool operator==(const HeapInt& left, const HeapInt& right) {
    return *left.ptr == *right.ptr;
  }

  ~HeapInt() {
    delete ptr;
  }

private:
  int* ptr;
};

template<WhenCapacityKnown When>
void test_push_and_pop() {
  constexpr std::size_t capacity = 4;

  auto buffer = create_buffer<HeapInt, When, capacity>();

  buffer.push_back(2);
  assert_size_and_capacity(buffer, capacity, 1);
  assert_contents(buffer, {2});

  buffer.push_front(1);
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {1, 2});

  buffer.push_back(3);
  buffer.push_front(0);
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {0, 1, 2, 3});

  buffer.push_back(4);
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {1, 2, 3, 4});

  buffer.push_front(0);
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {0, 1, 2, 3});

  buffer.pop_back();
  assert_size_and_capacity(buffer, capacity, 3);
  assert_contents(buffer, {0, 1, 2});

  buffer.pop_front();
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {1, 2});

  buffer.pop_back();
  buffer.pop_front();
  assert_size_and_capacity(buffer, capacity, 0);

  buffer.push_front(42);
  assert_size_and_capacity(buffer, capacity, 1);
  assert_contents(buffer, {42});

  for (std::size_t i = 0; i < 10; ++i) {
    buffer.push_back(i);
    buffer.push_front(i);
  }

  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {9, 0, 42, 0});

  buffer.pop_front();
  buffer.pop_back();
  buffer.pop_front();
  assert_size_and_capacity(buffer, capacity, 1);
  assert_contents(buffer, {42});
}

template<WhenCapacityKnown When>
void test_indexing() {
  constexpr std::size_t capacity = 4;
  auto buffer = create_buffer<HeapInt, When, capacity>();

  buffer.push_back(1);
  buffer.push_front(0);

  buffer[1] = 1337;
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {0, 1337});

  buffer[0] = 42;
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {42, 1337});

  buffer.push_back(2);
  buffer.push_back(3);

  buffer[2] = 314;
  buffer[3] = 2718;
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {42, 1337, 314, 2718});

  const auto& const_buffer = buffer;
  const auto& value = const_buffer[0];
  buffer[0] = 96;
  assert(value == 96);
}

template<WhenCapacityKnown When>
void test_at() {
  constexpr std::size_t capacity = 32;
  auto buffer = create_buffer<HeapInt, When, capacity>();

  buffer.push_back(1);
  buffer.push_front(0);

  assert(buffer[0] == 0);
  assert(buffer.at(0) == buffer[0]);
  assert(buffer[1] == 1);
  assert(buffer.at(1) == buffer[1]);

  for (std::size_t i = 2; i < capacity; ++i) {
    try {
      buffer.at(i);
      assert(false);
    } catch(std::out_of_range&) {
    }
  }

  buffer.at(0) = 42;
  buffer.at(1) = 1337;
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {42, 1337});
}

template<WhenCapacityKnown When>
void test_copy_and_assignment() {
  constexpr std::size_t capacity = 4;
  auto buffer = create_buffer<HeapInt, When, capacity>();

  buffer.push_back(2);
  buffer.push_back(3);

  auto other_buffer = buffer;
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {2, 3});
  assert_size_and_capacity(other_buffer, capacity, 2);
  assert_contents(other_buffer, {2, 3});

  other_buffer.push_front(1);
  other_buffer.push_front(0);
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {2, 3});
  assert_size_and_capacity(other_buffer, capacity, 4);
  assert_contents(other_buffer, {0, 1, 2, 3});

  buffer = other_buffer;
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {0, 1, 2, 3});
  assert_size_and_capacity(other_buffer, capacity, 4);
  assert_contents(other_buffer, {0, 1, 2, 3});
}

void test_constructor_exception() {
  try {
    CircularBuffer<int, 32>(42);
    assert(false);
  } catch(std::invalid_argument&) {
  }
}

// Acts like a wrapper for any type
template<typename T>
struct Value {
  T value;

  operator T() const {
    return value;
  }
};

template<WhenCapacityKnown When>
void test_iterators() {
  constexpr size_t capacity = 32;
  auto buffer = create_buffer<HeapInt, When, capacity>();

  for (std::size_t i = 0; i < buffer.capacity() / 2; ++i) {
    buffer.push_front(buffer.capacity() / 2 - i - 1);
    buffer.push_back(buffer.capacity() / 2 + i);
  }

  using Iter = typename decltype(buffer)::iterator;

  // Formal checks
  static_assert(std::is_same_v<Iter, decltype(buffer.begin())>);
  static_assert(std::is_same_v<Iter, decltype(buffer.end())>);

  Iter begin_iter = buffer.begin();
  Iter end_iter = buffer.end();

  assert(end_iter - begin_iter == buffer.capacity());
  assert(begin_iter + buffer.capacity() == end_iter);
  assert(end_iter - buffer.capacity() == begin_iter);
  assert(begin_iter < end_iter);

  for (std::size_t i = 0; i < buffer.capacity(); ++i) {
    Iter cur_iter = begin_iter + i;

    assert(*cur_iter == i);
    assert(cur_iter - begin_iter == i);
    assert(cur_iter - i == begin_iter);
    assert(end_iter - cur_iter == buffer.capacity() - i);
    assert(cur_iter + (buffer.capacity() - i) == end_iter);
    assert(begin_iter <= cur_iter);
    assert(cur_iter < end_iter);
    assert(cur_iter == cur_iter);
  }

  using ReverseIter = typename decltype(buffer)::reverse_iterator;

  // Formal checks
  static_assert(std::is_same_v<ReverseIter, decltype(buffer.rbegin())>);
  static_assert(std::is_same_v<ReverseIter, decltype(buffer.rend())>);

  ReverseIter rbegin_iter = buffer.rbegin();
  ReverseIter rend_iter = buffer.rend();

  assert(rend_iter - rbegin_iter == buffer.capacity());
  assert(rbegin_iter + buffer.capacity() == rend_iter);
  assert(rend_iter - buffer.capacity() == rbegin_iter);
  assert(rbegin_iter < rend_iter);

  for (std::size_t i = 0; i < buffer.capacity(); ++i) {
    auto cur_iter = rbegin_iter + i;

    assert(*cur_iter == buffer.capacity() - i - 1);
    assert(cur_iter - rbegin_iter == i);
    assert(cur_iter - i == rbegin_iter);
    assert(rend_iter - cur_iter == buffer.capacity() - i);
    assert(cur_iter + (buffer.capacity() - i) == rend_iter);
    assert(rbegin_iter <= cur_iter);
    assert(cur_iter < rend_iter);
    assert(cur_iter == cur_iter);
  }

  std::vector<HeapInt> storage;

  for (const auto& value : buffer) {
    storage.push_back(value);
  }

  assert(storage.size() == buffer.capacity());
  assert(storage.size() == 32);

  for (std::size_t i = 0; i < storage.size(); ++i) {
    assert(storage[i] == i);
  }

  using ConstIter = typename decltype(buffer)::const_iterator;
  const auto& const_buffer = buffer;

  // Formal checks
  static_assert(std::is_same_v<ConstIter, decltype(buffer.cbegin())>);
  static_assert(std::is_same_v<ConstIter, decltype(buffer.cend())>);
  static_assert(std::is_same_v<ConstIter, decltype(const_buffer.cbegin())>);
  static_assert(std::is_same_v<ConstIter, decltype(const_buffer.cend())>);
  static_assert(std::is_same_v<ConstIter, decltype(const_buffer.begin())>);
  static_assert(std::is_same_v<ConstIter, decltype(const_buffer.end())>);

  static_assert(!std::is_assignable_v<decltype(*const_buffer.begin()), HeapInt>);
  static_assert(std::is_assignable_v<decltype(*buffer.begin()), HeapInt>);
  static_assert(!std::is_assignable_v<decltype(*buffer.cbegin()), HeapInt>);

  static_assert(!std::is_assignable_v<decltype(*const_buffer.end()), HeapInt>);
  static_assert(std::is_assignable_v<decltype(*buffer.end()), HeapInt>);
  static_assert(!std::is_assignable_v<decltype(*buffer.cend()), HeapInt>);

  std::for_each(buffer.begin(), buffer.end(), [](HeapInt& value) { value = 1337; });
  auto expected = std::vector(32, 1337);

  assert_size_and_capacity(buffer, capacity, 32);
  assert_contents(buffer, expected);

  *(buffer.begin() + 10) = 42;
  expected[10] = 42;

  assert_size_and_capacity(buffer, capacity, 32);
  assert_contents(buffer, expected);

  auto values_buffer = create_buffer<Value<int>, When, capacity>();
  values_buffer.push_back({1});
  values_buffer.push_front({0});

  values_buffer.begin()->value = 42;
  (values_buffer.begin() + 1)->value = 1337;
  assert_size_and_capacity(values_buffer, capacity, 2);
  assert_contents(values_buffer, {42, 1337});

  const auto& value_ref_arrow = values_buffer.cbegin()->value;
  const auto& value_ref_star = *values_buffer.cbegin();
  values_buffer.begin()->value = 43;
  assert(value_ref_arrow == 43);
  assert(value_ref_star == 43);

  using ConstReverseIter = typename decltype(buffer)::const_reverse_iterator;

  // Formal checks
  static_assert(std::is_same_v<ConstReverseIter, decltype(buffer.crbegin())>);
  static_assert(std::is_same_v<ConstReverseIter, decltype(buffer.crend())>);
  static_assert(std::is_same_v<ConstReverseIter, decltype(const_buffer.crbegin())>);
  static_assert(std::is_same_v<ConstReverseIter, decltype(const_buffer.crend())>);
  static_assert(std::is_same_v<ConstReverseIter, decltype(const_buffer.rbegin())>);
  static_assert(std::is_same_v<ConstReverseIter, decltype(const_buffer.rend())>);

  static_assert(!std::is_assignable_v<decltype(*const_buffer.rbegin()), HeapInt>);
  static_assert(std::is_assignable_v<decltype(*buffer.rbegin()), HeapInt>);
  static_assert(!std::is_assignable_v<decltype(*buffer.crbegin()), HeapInt>);

  static_assert(!std::is_assignable_v<decltype(*const_buffer.rend()), HeapInt>);
  static_assert(std::is_assignable_v<decltype(*buffer.rend()), HeapInt>);
  static_assert(!std::is_assignable_v<decltype(*buffer.crend()), HeapInt>);
}

template<WhenCapacityKnown When>
void test_erase() {
  constexpr size_t capacity = 32;
  auto buffer = create_buffer<HeapInt, When, capacity>();

  buffer.push_front(1);
  buffer.push_front(0);
  buffer.push_back(42);
  buffer.push_back(2);
  buffer.push_back(3);

  buffer.erase(buffer.begin() + 2);
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, {0, 1, 2, 3});

  buffer.erase(--buffer.end());
  assert_size_and_capacity(buffer, capacity, 3);
  assert_contents(buffer, {0, 1, 2});

  buffer.erase(buffer.begin());
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, {1, 2});

  buffer.push_front(0);

  for (std::size_t i = 3; i < buffer.capacity(); ++i) {
    buffer.push_back(i);
  }

  buffer.erase(buffer.begin() + 9);

  assert_size_and_capacity(buffer, capacity, 31);
  assert_contents(buffer, std::views::iota(0, 32) | std::views::filter([](int i) { return i != 9; }));
}

template<WhenCapacityKnown When>
void test_insert() {
  constexpr size_t capacity = 32;
  auto buffer = create_buffer<HeapInt, When, capacity>();
  std::vector<int> expected{};

  buffer.insert(buffer.begin(), 2);
  expected.insert(expected.begin(), 2);
  assert_size_and_capacity(buffer, capacity, 1);
  assert_contents(buffer, expected);

  buffer.insert(buffer.begin(), 0);
  expected.insert(expected.begin(), 0);
  assert_size_and_capacity(buffer, capacity, 2);
  assert_contents(buffer, expected);

  buffer.insert(buffer.begin() + 1, 1);
  expected.insert(expected.begin() + 1, 1);
  assert_size_and_capacity(buffer, capacity, 3);
  assert_contents(buffer, expected);

  buffer.insert(buffer.end(), 3);
  expected.insert(expected.end(), 3);
  assert_size_and_capacity(buffer, capacity, 4);
  assert_contents(buffer, expected);

  for (std::size_t i = 4; i < buffer.capacity(); ++i) {
    size_t offset_from_end = i % 10;

    buffer.insert(buffer.end() - offset_from_end, i);
    expected.insert(expected.end() - offset_from_end, i);
    assert_size_and_capacity(buffer, capacity, i + 1);
    assert_contents(buffer, expected);
  }

  for (std::size_t i = buffer.size(); i > 0; --i) {
    auto value = i + 100;

    buffer.insert(buffer.begin() + i, value);
    expected.erase(expected.begin());
    expected.insert(expected.begin() + (i - 1), value);

    assert_size_and_capacity(buffer, capacity, 32);
    assert_contents(buffer, expected);
  }

  buffer.insert(buffer.begin(), 1337); // if full(), insert on begin() should do nothing
  assert_size_and_capacity(buffer, capacity, 32);
  assert_contents(buffer, expected);
}

// Emulates a class which can throw from a copy constructor
template<typename T>
struct Explosive {
  bool should_explode;
  T value;

  Explosive() = delete;

  Explosive(bool should_explode, const T& value): should_explode(should_explode), value(value) {
  }

  Explosive(const Explosive& other): should_explode(other.should_explode), value(other.value) {
    if (other.should_explode)
      throw "boom";
  }

  Explosive& operator=(const Explosive& other) {
    Explosive copy = other;
    swap(copy);

    return *this;
  }

  void swap(Explosive& other) {
    std::swap(should_explode, other.should_explode);
  }

  operator T() const {
    return value;
  }
};

template<WhenCapacityKnown When>
void test_exceptions() {
  constexpr std::size_t capacity = 32;
  auto buffer = create_buffer<Explosive<std::size_t>, When, capacity>();

  for (std::size_t i = 0; i < buffer.capacity(); ++i) {
    try {
      if (i % 2 == 0) {
        buffer.push_front({false, 42});
        buffer.push_back({true, 1337});
      } else {
        buffer.push_back({false, 42});
        buffer.push_front({true, 1337});
      }

      assert(false);
    } catch (...) {
      assert_size_and_capacity(buffer, capacity, i + 1);
      assert_contents(buffer, std::vector(i + 1, 42ul));
    }
  }

  for (std::size_t i = 0; i < 100; ++i) {
    try {
      if (i % 2 == 0) {
        buffer.push_back({true, 1337});
      } else {
        buffer.push_front({true, 1337});
      }

      assert(false);
    } catch (...) {
      assert_size_and_capacity(buffer, capacity, 32);
      assert_contents(buffer, std::vector(32, 42ul));
    }
  }
}

} // namespace TestsByPetialetia

namespace TestsByNemoumbra {

template<typename Known, typename Unknown>
void assert_iter_size() {
  // Nemo: Tbh, it's a terrible check, but a better one would likely require static analysis
  static_assert(sizeof(typename Known::iterator) < sizeof(typename Unknown::iterator));
  static_assert(sizeof(typename Known::reverse_iterator) < sizeof(typename Unknown::reverse_iterator));
  static_assert(sizeof(typename Known::const_iterator) < sizeof(typename Unknown::const_iterator));
  static_assert(sizeof(typename Known::const_reverse_iterator) < sizeof(typename Unknown::const_reverse_iterator));
}

void test_iterators_size() {
  // If capacity is known at compiletime, there is no need to store it as field in iterator
  assert_iter_size<CircularBuffer<int, 32>, CircularBuffer<int>>();
}

template<typename Iter>
void assert_correct_tag() {
  static_assert(std::is_same_v<std::random_access_iterator_tag, typename Iter::iterator_category>);
}

template<WhenCapacityKnown When>
void test_iterator_tags() {
  using Buffer = decltype(create_buffer<int, When, 32>());

  assert_correct_tag<typename Buffer::iterator>();
  assert_correct_tag<typename Buffer::reverse_iterator>();
  assert_correct_tag<typename Buffer::const_iterator>();
  assert_correct_tag<typename Buffer::const_reverse_iterator>();
}

} // namespace TestsByNemoumbra

int main() {
  TestsByPetialetia::test_empty<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_empty<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_push_and_pop<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_push_and_pop<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_indexing<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_indexing<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_at<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_at<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_copy_and_assignment<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_copy_and_assignment<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_constructor_exception();

  TestsByPetialetia::test_iterators<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_iterators<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByNemoumbra::test_iterator_tags<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByNemoumbra::test_iterator_tags<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByNemoumbra::test_iterators_size();

  TestsByPetialetia::test_erase<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_erase<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_insert<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_insert<WhenCapacityKnown::KNOWN_AT_RUNTIME>();

  TestsByPetialetia::test_exceptions<WhenCapacityKnown::KNOWN_AT_COMPILETIME>();
  TestsByPetialetia::test_exceptions<WhenCapacityKnown::KNOWN_AT_RUNTIME>();
}
