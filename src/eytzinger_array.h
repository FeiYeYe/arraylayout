/*
 * eytzinger_array.h
 *
 *  Created on: 2015-04-24
 *      Author: morin
 */

#ifndef FBS_EYTZINGER_ARRAY_H_
#define FBS_EYTZINGER_ARRAY_H_

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <cassert>

#include <bitset>

#include "base_array.h"

namespace fbs {

template<typename T, typename I, bool aligned=false>
class eytzinger_array : public base_array<T,I> {
protected:
	using base_array<T,I>::a;
	using base_array<T,I>::n;

	// We always prefetch multiplier*i + offset
	static const I multiplier = 64/sizeof(T);
	static const I offset = multiplier + multiplier/2 - 1;

	template<typename ForwardIterator>
	ForwardIterator copy_data(ForwardIterator a0, I i);

	I _branchy_search(T x) const ;

	template<bool prefetch>
	I _branchfree_search(T x) const ;

public:
	eytzinger_array() { };

	template<typename ForwardIterator>
	eytzinger_array(ForwardIterator a0, I n0);

	~eytzinger_array();

	I branchfree_search(T x) const { return _branchfree_search<false>(x); };
	I branchfree_prefetch_search(T x) const { return _branchfree_search<true>(x); };
	I search(T x) const { return _branchy_search(x); };
};

// An Eytzinger array with branch-free searches
template<typename T, typename I, bool aligned=false>
class eytzinger_array_bf : public eytzinger_array<T,I,aligned> {
protected:
	using eytzinger_array<T,I,aligned>::branchfree_search;

public:
	template<typename ForwardIterator>
	eytzinger_array_bf(ForwardIterator a0, I n0)
		: eytzinger_array<T,I,aligned>(a0, n0) {}
	I search(T x) const { return branchfree_search(x); };
};

// An Eytzinger array with branch-free searches and prefetching
template<typename T, typename I, bool aligned=false>
class eytzinger_array_bfp : public eytzinger_array<T,I,aligned> {
protected:
	using eytzinger_array<T,I,aligned>::branchfree_prefetch_search;

public:
	template<typename ForwardIterator>
	eytzinger_array_bfp(ForwardIterator a0, I n0)
		: eytzinger_array<T,I,aligned>(a0, n0) {}
	I search(T x) const { return branchfree_prefetch_search(x); };
};

// An Eytzinger array with branch-free searches and masked prefetching
template<typename T, typename I, bool aligned=false>
class eytzinger_array_bfpm : public eytzinger_array<T,I,aligned> {
protected:
	using eytzinger_array<T,I,aligned>::a;
	using eytzinger_array<T,I,aligned>::n;
	using eytzinger_array<T,I,aligned>::multiplier;
	using eytzinger_array<T,I,aligned>::offset;
	I mask;

public:
	template<typename ForwardIterator>
	eytzinger_array_bfpm(ForwardIterator a0, I n0)
		: eytzinger_array<T,I,aligned>(a0, n0) {
		for (mask = 1; mask <= n; mask <<= 1) {}
		mask--;
	}
	I search(T x) const;
};


template<typename T, typename I, bool aligned>
template<typename ForwardIterator>
ForwardIterator eytzinger_array<T,I,aligned>::copy_data(ForwardIterator a0, I i) {

	if (i >= n) return a0;

	// visit left child
	a0 = copy_data(a0, 2*i+1);

	// put data at the root
	a[i] = *a0++;

	// visit right child
	a0 = copy_data(a0, 2*i+2);

	return a0;
}


template<typename T, typename I, bool aligned>
template<typename ForwardIterator>
eytzinger_array<T,I,aligned>::eytzinger_array(ForwardIterator a0, I n0) {
	if (n0 > std::numeric_limits<I>::max()/2) {
		std::ostringstream ss;
		ss << "array length " << n0 << " is too big, use a larger I class";
		throw std::out_of_range(ss.str());
	}
	n = n0;
	if (aligned) {
		assert(posix_memalign((void **)&a, 64, sizeof(T) * (n+1)) == 0);
		a++;
	} else {
		a = new T[n];
	}
	copy_data(a0, 0);
}

template<typename T, typename I, bool aligned>
eytzinger_array<T,I,aligned>::~eytzinger_array() {
	if (aligned) {
		free(a-1);
	} else {
		delete[] a;
	}
}

// Branchy code with no prefetching
template<typename T, typename I, bool aligned>
I __attribute__ ((noinline)) eytzinger_array<T,I,aligned>::_branchy_search(T x) const {
	I i = 0;
	while (i < n) {
		if (x < a[i]) {
			i = 2*i + 1;
		} else if (x > a[i]) {
			i = 2*i + 2;
		} else {
			return i;
		}
	}
	I j = (i+1) >> __builtin_ffs(~(i+1));
	return (j == 0) ? n : j-1;
}

// Branch-free code with or without prefetching
template<typename T, typename I, bool aligned>
template<bool prefetch>
I __attribute__ ((noinline)) eytzinger_array<T,I,aligned>::_branchfree_search(T x) const {
	I i = 0;
	while (i < n) {
		if (prefetch) __builtin_prefetch(a+(multiplier*i + offset));
		i = (x <= a[i]) ? (2*i + 1) : (2*i + 2);
	}
	I j = (i+1) >> __builtin_ffs(~(i+1));
	return (j == 0) ? n : j-1;
}

template<typename T, typename I, bool aligned>
I __attribute__ ((noinline)) eytzinger_array_bfpm<T,I,aligned>::search(T x) const {
	I i = 0;
	while (i < n) {
		__builtin_prefetch(a+((multiplier*i + offset)&mask));
		i = (x <= a[i]) ? (2*i + 1) : (2*i + 2);
	}
	I j = (i+1) >> __builtin_ffs(~(i+1));
	return (j == 0) ? n : j-1;
}

} // namespace fbs

#endif /* FBS_EYTZINGER_ARRAY_H_ */
