/*
 * kvec_ext.h
 *
 * Using kvec to implement several simple and useful utilities.
 *
 *  Created on: 4 May, 2015
 *      Author: xiao
 */

#ifndef UTILS_ADT_KVEC_EXT_H
#define UTILS_ADT_KVEC_EXT_H

#include "Util/DataStructure/kvec.h"

/*
 * Always specify the length to kvec_str functions when the input char* is from
 * StringRef. Because StringRef is not always \0 terminated.
 */
template <int initN = 8> class kvec_str : public kvec<char> {
public:
  kvec_str() : kvec<char>(initN) {}

  kvec_str(const char *str) : kvec<char>() { fill(str); }

  kvec_str(const char *str, int len) : kvec<char>() { fill(str, len); }

  virtual ~kvec_str() {}

  // Append a tailing zero to make it a c-style string
  const char *c_str() {
    if (this->n == this->m)
      this->resize(this->m + 1);

    if (this->n > 0 && this->back() != '\0')
      this->a[this->n] = 0;

    return this->a;
  }

  // Expose the internal buffer to outside and intendedly to be modified
  char *data() { return this->a; }

  // Replace the content of kvec by \p str
  void fill(const char *str, int len = -1) {
    if (len == -1)
      len = std::strlen(str);
    if (this->m < len)
      this->resize(len);
    std::strcpy(this->a, str);
    this->n = len;
  }

  int find(char c) {
    int i = 0;
    for (; i < this->n; ++i) {
      if (this->a[i] == c)
        break;
    }
    return i == this->n ? -1 : i;
  }

  int rfind(char c) {
    int i = this->n - 1;
    for (; i > -1; --i) {
      if (this->a[i] == c)
        break;
    }
    return i;
  }

  // Find the \p pstr in current string vector from back
  // Return -1 if \p pstr is not found
  int rfind(const char *pstr) {
    int plen = std::strlen(pstr);
    int i = this->n - plen;

    for (; i > -1; --i) {
      if (std::strncmp(this->a[i], pstr, plen) == 0)
        break;
    }

    return i;
  }

  // Erase the substr from [start_pos, n)
  void erase_substr(int start_pos) {
    if (start_pos >= 0) {
      this->n = start_pos;
    }
  }

  // Append \p str to the kvec_str content
  void append_str(const char *str, int len = -1) {
    if (len == -1)
      len = std::strlen(str);
    if (this->n + len > this->m) {
      int new_size = 2 * this->m + len;
      this->resize(new_size);
    }
    std::strcpy(this->a + this->n, str);
    this->n += len;
  }
};

#endif /* UTILS_ADT_KVEC_EXT_H */
