	// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

///\file Defines the Callback template class and void specialization.

#pragma once

#ifndef NYTL_INCLUDE_CALLBACK
#define NYTL_INCLUDE_CALLBACK

#include <nytl/connection.hpp> // nytl::BasicConnection

#include <functional> // std::function
#include <vector> // std::vector
#include <utility> // std::move
#include <cstdint> // std::uintptr_t

namespace nytl {

// First declaration - undefined.
// Signature must have the format ReturnType(Args...)
template<typename Signature, typename ID = ConnectionID> class Callback;
template<typename Signature> using TrackedCallback = Callback<Signature, TrackedConnectionID>;

// TODO: C++17: do this with cnstexpr if in callback
namespace detail {

template<typename Ret>
struct CallbackCall {
	template<typename CB, typename CallIter, typename Slots, typename... Args>
	static std::vector<Ret> call(CB& cb, CallIter*& iter, const Slots& slots, Args... args)
	{
		CallIter stackIter;
		stackIter.above = iter;
		iter = &stackIter;
		auto& idx = stackIter.current;

		std::vector<Ret> ret;
		ret.reserve(slots.size());

		while(idx < slots.size()) {
			const auto& slot = slots[idx];
			++idx;
			// auto f = slot.func;
			ret.push_back(slot.func({cb, slot.id}, std::forward<Args>(args)...));
		}

		iter = stackIter.above;
		return ret;
	}
};

template<>
struct CallbackCall<void> {
	template<typename CB, typename CallIter, typename Slots, typename... Args>
	static void call(CB& cb, CallIter*& iter, const Slots& slots, Args... args)
	{
		CallIter stackIter;
		stackIter.above = iter;
		iter = &stackIter;
		auto& idx = stackIter.current;

		std::cout << "callbegin " << &slots << "\n";
		while(idx < slots.size()) {
			const auto& slot = slots[idx];
			std::cout << "\tcall: " << idx << ": " << slot.id.value << ": " << &slot << ": " << &slots[idx] << "\n";
			++idx;

			if(slot.id.value == 2) {
				auto f = slot.func;
				f({cb, slot.id}, std::forward<Args>(args)...);
			} else {
				slot.func({cb, slot.id}, std::forward<Args>(args)...);
			}
		}

		std::cout << "callend " << &slots << "\n";

		iter = stackIter.above;
	}
};

}

/// \brief Represents a Callback for which listener functions can be registered.
///
/// Used for registering functions that should be called when the Callback is triggered.
/// This is intented as more lightweight, easier, more dynmaic and
/// macro-free options to the signal-slot mechanism used by many c++ libraries.
/// The template parameter Signature indicated the return types registered functions should have
/// and the parameters they take.
///
/// Registering a Callback function returns a unique connection id which can either
/// totally ignored, dealt with manually or wrapped into a nytl::Connection guard.
/// The returned id can be only be used to unregister the function.
/// Any object that can be represented by a std::function can be registered at a Callback object,
/// so it is impossible to unregister a function only by knowing its function
/// (std::function cannot be compared for equality), that is why unique ids are used to
/// unregister/check the registered functions.
///
/// Registered functions that should be called if the Callback is activated must have the
/// signature Ret(Args...). Alternatively, the function can have the signature
/// Ret(nytl::Connection, Args...) in which case it will receive an additional connection parameter
/// that can then be used to disconnect the callback connection for the called function from
/// withtin itself.
/// This (as well as adding a new callback function from inside a callback function of the
/// same callback object) can be done safely. Calls to the callback can also safely be nested,
/// so e.g. a registered callback function can trigger the callback again from within (but should
/// not do so too often or it will result in a stack overflow).
///
/// The class is not designed threadsafe, if one thread calls e.g. call() while another
/// one calls add() it may cause undefined behaviour.
/// \module function
template<typename Ret, typename... Args, typename CID>
class Callback<Ret(Args...), CID> : public BasicConnectable<CID> {
public:
	using ID = CID;
	using Signature = Ret(Args...);
	using Conn = BasicConnection<BasicConnectable<ID>, ID>;

	~Callback()
	{
		for(auto& slot : slots_)
			slot.id.reset();
	}

	/// \brief Registers a new function in the same way add does.
	/// \returns A unique connection id for the registered function which can be used to
	/// unregister it.
	template<typename F>
	Conn operator+=(F&& func)
	{
		return add(std::forward<F>(func));
	}

	/// \brief Resets all registered function and sets the given one as only Callback function.
	/// \returns A unique connection id for the registered function which can be used to
	/// unregister it.
	template<typename F>
	Conn operator=(F&& func)
	{
		clear();
		return add(std::forward<F>(func));
	}

	/// \brief Registers a new Callback function.
	/// \returns A unique connection id for the registered function which can be used to
	/// unregister it.
	Conn add(std::function<Ret(Args...)> func)
	{
		emplace();
		slots_.back().func = [f = std::move(func)](Conn, Args... args) {
			return f(std::forward<Args>(args)...);
		};

		return {*this, slots_.back().id};
	}

	/// \brief Registers a new Callback function with additional connection parameter.
	/// \returns A unique connection id for the registered function which can be used to
	/// unregister it.
	Conn add(std::function<Ret(Conn, Args...)> func)
	{
		emplace();
		slots_.back().func = std::move(func);
		return {*this, slots_.back().id};
	}

	/// Calls all registered functions and returns a Vector with the returned objects.
	auto call(Args... a)
	{
		// we need the implementation with a CallIter instead of e.g. a range-based
		// for loop, since slots can be removed are added from the called functions.
		// See disconnect function.
		// The whole mechanism works by pushing a CallIter on the stack from this function,
		// storing a pointer to it inside the class and using this point from
		// disconnect to potentially modify the current iteration index.
		// If this function was called in a nested way all existent CallIters will be
		// signaled (as in a linked list, except only on the stack).
		// Provide almost no overhead when nested calls or removes are not used
		// The caller only pays for what he uses and simply triggering the callback
		// is not that expensive.

		// CallIter iter;
		// iter.above = callIter_;
		// callIter_ = &iter;
		// auto& idx = iter.current;
		//
		// if constexpr(std::is_same_v<Ret, void>) {
		// 	while(idx < slots_.size()) {
		// 		auto& slot = slots_[idx];
		// 		++idx;
		// 		slot.func({*this, slot.id}, std::forward<Args>(a)...);
		// 	}
		//
		// 	callIter_ = iter.above;
		// 	return;
		// } else {
		// 	std::vector<Ret> ret;
		// 	ret.reserve(slots_.size());
		//
		// 	while(idx < slots_.size()) {
		// 		auto& slot = slots_[idx];
		// 		++idx;
		// 		ret.push_back(slot.func({*this, slot.id}, std::forward<Args>(a)...));
		// 	}
		//
		// 	callIter_ = iter.above;
		// 	return ret;
		// }

		// std::cout << "!CALL! " << &slots_ << "\n";
		return detail::CallbackCall<Ret>::call(*this, callIter_, slots_, std::forward<Args>(a)...);
	}

	/// Clears all registered functions.
	void clear()
	{
		for(auto& slot : slots_)
			slot.id.reset();

		slots_.clear();
	}

	/// Operator version of call. Calls all registered functions and return their returned objects.
	auto operator() (Args... a)
	{
		return call(std::forward<Args>(a)...);
	}

	/// Removes the callback function registered with the given id.
	/// Returns whether the function could be found. If the id is invalid or the
	/// associated function was already removed, returns false.
	bool disconnect(const ID& id) override
	{
		if(id == ID {} || !slots_) return false;

		// first
		if(slots_->id == id) {
			if(callIter_) callIter_->checkErase(slots_);

			slots_->id.reset();
			auto tmp = slots_;
			slots_ = slots_->next;
			delete tmp;
		}

		// iterate
		for(auto it = slots_; it && it->next; it = it->next) {
			if(it->next->id == id) {
				it->id.reset();

				if(callIter_)
					callIter_->checkErase(it - slots_.begin());
				return true;
			}
		}

		return false;
	}

protected:
	void emplace()
	{
		auto id = ++highestID_;

		if(!slots_) {
			slots_ = new CallbackSlot();
			slots_->id = id;
			return;
		}

		auto slot = slots_;
		while(slot->next) slot = slot->next;
		slot->next = new CallbackSlot();
		slot->next->id = id;
	}

	// Represents one registered callback function with id
	struct CallbackSlot {
 		ID id;
		std::function<Ret(Conn, Args...)> func;
		CallbackSlot* next;
	};

	// Represents one call function on the stack.
	// Implemented this way to allow nesting calls to the same object as well
	// as being able to add/disconnect functions from within a call function.
	// Done without any memory allocation by just using the stack.
	struct CallIter {
		CallbackSlot* next {};
		CallIter* above {}; // optional pointer to the CallIter from the above iteration

		// Will be called from diconnect to signal that the entry with the given id
		// has been disconnected. This will update the current value if needed and signal
		// the CallIter above (if there is one).
		void checkErase(CallbackSlot* slot)
		{
			if(slot == next) next = slot->next;
			if(above) above->checkErase(slot);
		}
	};

	std::size_t highestID_ {};
	CallbackSlot* slots_ {};
	CallIter* callIter_ {}; // pointer to iter inside the lowest (last called) this->call
};

} // namespace nytl

#endif // header guard
