/*
 * A fast, lightweight, and feature added vector implementation for trivial POD
 * data.
 *
 * 2010-04-29 (0.2.0): Better support for C++, by xiao
 * 2014 (0.3.0): C++ version is significantly enhanced, by xiao
 * 2015 (0.4.0): Optimized for Clearblue, by xiao
 */

#ifndef KVEC_H
#define KVEC_H

// Minimize the number of imported headers because kvec might be referred
// frequently
#ifndef __GNUC__
#include <cstring>
#endif

#include <cassert>
#include <initializer_list>
#include <type_traits>

// The template parameter \p T must have default constructor.
template <typename T> class kvec {
protected:
  const int TAIL_BUF_SIZE = 3;
  const int DOUBLE_GROW_UPPER = 524288;

  int n, m;
  T *a;

public:
  kvec() : n(0), m(0), a(nullptr) {
    // Unlike std::vector, kvec is empty at start
    __pod_check();
  }

  kvec(int sz) : kvec() {
    __pod_check();
    resize(sz);
  }

  kvec(const kvec<T> &other) : kvec() {
    __pod_check();
    copy(other);
  }

  void __pod_check() {
    /*
     * Trigger a compile error if type check fails.
     */
    static_assert(std::is_trivial<T>::value,
                  "T is a non-trivial POD, please use std::vector instead.");
  }

  // Moving constructor, called when the parameter is a reference to a temporary
  // kvec
  kvec(kvec<T> &&other) { move(other); }

  kvec(std::initializer_list<T> init_list) : kvec() {
    int sz = init_list.size();
    if (sz) {
      resize(sz);
      int i = 0;
      for (auto &it : init_list)
        a[i++] = it;
      n += sz;
    }
  }

  ~kvec() {
    if (a != nullptr)
      delete[] a;
  }

  kvec<T> &operator=(const kvec<T> &other) {
    copy(other);
    return *this;
  }

  void copy(const kvec<T> &other) {
    int sz = other.size();
    preserve(sz, 0);

    for (int i = 0; i < sz; ++i) {
      a[i] = other[i];
    }

    n = sz;
  }

  // Move the content of \p other kvec to this kvec
  // This is much efficient than copying two vectors
  void move(kvec<T> &other) {
    n = 0;
    m = 0;
    if (a != nullptr) {
      delete[] a;
      a = nullptr;
    }

    swap(other);
  }

  // Make this vector as a \p N-elements C-array
  void as_array(int N) {
    n = 0;
    if (N > m)
      resize(N);
    n = N;
  }

  // Swap the contents with another vector
  void swap(kvec<T> &other) {
    int nn = other.n;
    int mm = other.m;
    T *aa = other.a;

    other.n = n;
    other.m = m;
    other.a = a;

    n = nn;
    m = mm;
    a = aa;
  }

  // Swap two elements at indices \p i and \p j
  // Note that we don't check the validity of \p i and \p j
  void swap(int i, int j) {
    T t = a[j];
    a[j] = a[i];
    a[i] = t;
  }

  // Reverse the elements
  // The element type should have implemented copy constructor
  void reverse(int i = 0, int end_pos = -1) {
    int j = (end_pos == -1 ? n - 1 : end_pos);

    while (i < j) {
      swap(i, j);
      i++;
      j--;
    }
  }

  // copy_offset is the offset into the new memory
  void resize(int new_size, int copy_offset = 0) {
    assert(new_size >= 0 && "Cannot resize to less than 0 elements.");
    new_size += TAIL_BUF_SIZE; // Append a small buffer to avoid occasional
                               // buffer overflow
    T *b = new T[new_size];

    if (a == nullptr) {
      a = b;
    } else {
      int original_size = n;

      // Determine the copied size
      int cp_elems =
          (new_size - copy_offset < original_size ? (new_size - copy_offset)
                                                  : original_size);

      int cp_bytes = sizeof(T) * cp_elems;
#ifdef __GNUC__
      __builtin_memcpy(&b[copy_offset], a, cp_bytes);
#else
      std::memcpy(&b[copy_offset], a, cp_bytes);
#endif

      delete[] a;
      a = b;
    }

    m = new_size;
  }

  // Abort directly when range-check fails
  T &operator[](int inx) const {
    assert((inx >= 0 && inx < n) && "kvec operator[] range-check failed");
    return a[inx];
  }

  T &at(int inx) { return operator[](inx); }

  T &back() {
    assert((n > 0) && "kvec back(): vector is empty");
    return a[n - 1];
  }

  void set(int inx, T &v) { operator[](inx) = v; }

  typedef T *iterator;
  iterator begin() { return a; }
  iterator end() { return a + n; }

  typedef const T *const_iterator;
  const_iterator begin() const { return a; }
  const_iterator end() const { return a + n; }

  int size() const { return n; }
  bool empty() const { return n == 0; }
  int capacity() const { return m; }
  int vacants() const { return m - n; }

  void push_back(const T &x) {
    if (n == m) {
      int s = m << 1;
      if (m >= DOUBLE_GROW_UPPER)
        // Avoid wasting much memory when the buffer goes too large
        // Just slowdown the growing speed
        s -= (m >> 1);

      resize(s);
    }

    a[n++] = x;
  }

  // Add another vector to the end
  void push_back(const kvec<T> &other) {
    int sz = other.size();
    preserve(sz);

    for (int i = 0; i < sz; ++i) {
      a[n + i] = other[i];
    }

    n += sz;
  }

  // Push back \p sz number of empty elements
  void push_empty(int sz) {
    preserve(sz);
    n += sz;
  }

  T &pop_back() {
    assert((n > 0) && "kvec pop_back(): vector is empty");
    return a[--n];
  }

  void clear() { n = 0; }

  // Insert an element \p x at the front
  // This operation takes O(n) time to shift elements
  void insert_front(const T &x) {
    if (n == m) {
      int s = m << 1;
      if (m >= DOUBLE_GROW_UPPER)
        // Avoid waste much memory when the buffer goes too large
        s -= (m >> 1);

      // Directly shift one position during buffer resize
      resize(s, 1);
    } else {
      // We copy ourselves, -_-
      for (int i = n; i > 0; --i) {
        a[i] = a[i - 1];
      }
    }

    a[0] = x;
  }

  // Emulate the set interface
  // Facilitate the data structure replacement, :>
  void insert(const T &x) { push_back(x); }
  void insert(const kvec<T> &other) { push_back(other); }

  iterator find(const T &x) {
    for (int i = 0; i < n; ++i)
      if (a[i] == x)
        return a + i;
    return end();
  }

private:
  // Guarantee there are at least \p sz free elements starting from \p i^th
  // position
  void preserve(int sz, int i = -1) {
    if (i == -1)
      i = n;
    if (i + sz > m) {
      // Reserve slightly more elements than claimed
      resize(i + sz + sz / 2);
    }
  }
};

#endif
