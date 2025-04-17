/*
 * kpair is a trivial POD version of std::pair. Of course, the types for member
 * fields should also be POD. It is designed to safely work with kvec.
 *
 *  Created on: 26 Jul, 2015
 *      Author: xiao
 */

#ifndef UTILS_ADT_KPAIR_H
#define UTILS_ADT_KPAIR_H

#include <cstddef> // for __GLIBCXX__

template <typename T1, typename T2> struct kpair {
  T1 first;
  T2 second;
};

template <typename T1, typename T2>
bool operator<(const kpair<T1, T2> &_p1, const kpair<T1, T2> &_p2) {
  return (_p1.first < _p2.first) ||
         ((_p1.first == _p2.first) && (_p1.second < _p2.second));
}

template <typename T1, typename T2>
bool operator<=(const kpair<T1, T2> &_p1, const kpair<T1, T2> &_p2) {
  return !(_p2 < _p1);
}

template <typename T1, typename T2>
bool operator>(const kpair<T1, T2> &_p1, const kpair<T1, T2> &_p2) {
  return _p2 < _p1;
}

template <typename T1, typename T2>
bool operator>=(const kpair<T1, T2> &_p1, const kpair<T1, T2> &_p2) {
  return !(_p1 < _p2);
}

template <typename T1, typename T2>
bool operator==(const kpair<T1, T2> &_p1, const kpair<T1, T2> &_p2) {
  return _p1.first == _p2.first && _p1.second == _p2.second;
}

// Modified from std_pair in gcc 4.8
#if __cplusplus >= 201103L and defined(__GLIBCXX__)

// NB: DR 706.
template <class _T1, class _T2>
constexpr kpair<typename std::__decay_and_strip<_T1>::__type,
                typename std::__decay_and_strip<_T2>::__type>
k_make_pair(_T1 &&__x, _T2 &&__y) {
  typedef typename std::__decay_and_strip<_T1>::__type __ds_type1;
  typedef typename std::__decay_and_strip<_T2>::__type __ds_type2;
  typedef kpair<__ds_type1, __ds_type2> __pair_type;
  return __pair_type{std::forward<_T1>(__x), std::forward<_T2>(__y)};
}
#else

template <class _Tp> struct __make_kpair_return_impl { typedef _Tp type; };

template <class _Tp>
struct __make_kpair_return_impl<std::reference_wrapper<_Tp>> {
  typedef _Tp &type;
};

template <class _Tp> struct __make_kpair_return {
  typedef
      typename __make_kpair_return_impl<typename std::decay<_Tp>::type>::type
          type;
};

template <class _T1, class _T2>
inline _LIBCPP_INLINE_VISIBILITY kpair<typename __make_kpair_return<_T1>::type,
                                       typename __make_kpair_return<_T2>::type>
k_make_pair(_T1 &&__t1, _T2 &&__t2) {
  return kpair<typename __make_kpair_return<_T1>::type,
               typename __make_kpair_return<_T2>::type>{
      _VSTD::forward<_T1>(__t1), _VSTD::forward<_T2>(__t2)};
}

#endif

#endif /* UTILS_ADT_KPAIR_H */
