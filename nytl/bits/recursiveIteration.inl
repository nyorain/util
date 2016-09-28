// Copyright (c) 2016 nyorain 
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#ifndef NYTL_INCLUDE_RECURSIVEITERATION_INL
#define NYTL_INCLUDE_RECURSIVEITERATION_INL

#include <iterator>

namespace nytl
{

///\ingroup utility
///Iterator for recursively iterate over a object with multiple sub-iterators.
template<typename T, typename I = typename T::iterator>
class RecursiveIterator : public std::iterator<std::bidirectional_iterator_tag, T, std::size_t>
{
public:
	I it_;
	RecursiveIterator<T, I> child_;
	bool onParent_ {1};

public:
	RecursiveIterator(const I& it) : it_(it) {}

	auto operator->() -> decltype(&(*it_)) { return onParent_ ? &(*it_) : &(*child_); }
	auto operator*() -> decltype(*it_) { return onParent_ ? *it_ : *child_; }

	RecursiveIterator& operator++()
	{
		if(onParent_)
		{
			child_ = it_->recursive_begin();
			if(child_ == it_->recursive_end())
			{
				++it_;
			}
			else
			{
				onParent_ = 0;
			}
		}
		else if(child_++ == it_->recursive_end())
		{
			++it_;
			onParent_ = 1;
		}

		return *this;
	}

	RecursiveIterator operator++(int)
	{
		auto cop = *this;
		++(*this);
		return cop;
	}

	RecursiveIterator& operator--()
	{
		if(onParent_)
		{
			--it_;

			child_ = it_->recusive_end();
			if(child_ == it_->recursive_begin()) //empty
			{
				--it_;
			}
			else
			{
				--child_;
				onParent_ = 0;
			}
		}
		else if(child_ != it_->recursive_begin())
		{
			--child_;
		}
		else
		{
			--it_;
			onParent_ = 1;
		}

		return *this;
	}

	RecursiveIterator operator--(int)
	{
		auto cop = *this;
		--(*this);
		return cop;
	}
};

///\copydoc RecursiveIterator
///Const-version for the recursive iterator.
template<typename T>
using ConstRecursiveIterator = RecursiveIterator<T, typename T::const_iterator>;


///\ingroup utility
///\brief Allows recursive iteration (like range-based for loop) over an object with sub-iterators.
template<typename T>
class RecursiveIteration
{
public:
	using iterator = RecursiveIterator<T>;
	using const_iterator = ConstRecursiveIterator<T>;

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	T* object_;

public:
	RecursiveIteration(T& object) : object_(&object) {}

	auto begin() -> decltype(object_->recursive_begin()) { object_->recursive_begin(); }
	auto cbegin() -> decltype(object_->recursive_cbegin()) { object_->recursive_cbegin(); }
	auto rbegin() -> decltype(object_->recursive_rbegin()) { object_->recursive_rbegin(); }
	auto crbegin() -> decltype(object_->recursive_crbegin()) { object_->recursive_crbegin(); }

	auto end() -> decltype(object_->recursive_end()) { object_->recursive_end(); }
	auto cend() -> decltype(object_->recursive_cend()) { object_->recursive_cend(); }
	auto rend() -> decltype(object_->recursive_rend()) { object_->recursive_rend(); }
	auto crend() -> decltype(object_->recursive_crend()) { object_->recursive_crend(); }
};

}

#endif //header guard
