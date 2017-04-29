// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

/// \file Defines the Connection and Connectable classes needed e.g. for callbacks.

#pragma once

#ifndef NYTL_INCLUDE_CONNECTION_HPP
#define NYTL_INCLUDE_CONNECTION_HPP

#include <iostream> // std::cerr
#include <exception> // std::exception
#include <memory> // std::shared_ptr

namespace nytl {

/// \brief Interface for classes that can be connected to in some way.
/// An example (in nytl) is nytl::Callback.
/// The way for obtaining such a connection if class-defined, this interface defines
/// only a common way to disconnect again, which can then be used by the connection classes.
/// Using this abstraction makes e.g. the Connection class possible as generic
/// connection, seperated from the type of the class it has a connection for.
template <typename ID>
class ConnectableT {
public:
	virtual ~ConnectableT() = default;
	virtual bool disconnect(const ID& id) = 0;
};

/// \brief Associates a BasicConnectable implementation with one of its connection ids.
/// Note that this does not automatically destroy the connection, nor does is
/// track the lifetime of the BasicConnectable implementation object.
template <typename C, typename ID>
class ConnectionT {
public:
	ConnectionT() noexcept = default;
	ConnectionT(C& connectable, ID id);
	~ConnectionT() = default;

	ConnectionT(const ConnectionT&) noexcept = default;
	ConnectionT& operator=(const ConnectionT&) noexcept = default;

	void disconnect();
	bool connected() const noexcept;

	C* connectable() const { return connectable_; }
	ID id() const { return id_; }

protected:
	C* connectable_ {};
	ID id_ {};
};

/// \brief RAII wrapper around a connection id.
/// Note that this does not observe the lifetime of the object the connection id
/// was received from. Destroying the associated Connectable object during the lifetime
/// of the Connection object without then explicitly releasing the Connection id results
/// in undefined behaviour. Same as BasicConnection, but owns the connection
/// it holds, i.e. disconnects it on destruction. Therefore there should never be multiple
/// BasicConnectionGuards for the same connection id. If there exists a connection guard
/// for a connection this connection should not be disconnected in any other way than
/// the destruction of the guard (except the guard is explicitly released).
/// \reqruies Type 'C' shall be disconnectable, i.e. implement disconnect() member function.
/// \reqruies Type 'ID' shall be default and copy constructable/assignable.
template<typename C, typename ID>
class UniqueConnectionT {
public:
	UniqueConnectionT() = default;
	UniqueConnectionT(C& conn, ID id);
	UniqueConnectionT(ConnectionT<C, ID> lhs);
	~UniqueConnectionT();

	UniqueConnectionT(UniqueConnectionT&& lhs) noexcept;
	UniqueConnectionT& operator=(UniqueConnectionT&& lhs) noexcept;

	void disconnect();
	bool connected() const noexcept;
	C* connectable() const { return conn_; }
	ID id() const { return id_; }

	/// Releases ownership of the associated connection and returns its id.
	/// After the call this object will be empty.
	ID release();

protected:
	C* connectable_ {};
	ID id_ {};
};

/// Default ID for a connection that is entirely defined over its value.
struct ConnectionID {
	std::size_t value;

	constexpr void reset() { value = 0; }
	constexpr bool valid() const { return value; }
};

/// Shares the id value between all connections so that disconnections from
/// another connection or the callback itself can be observed.
struct TrackedConnectionID {
	std::shared_ptr<std::size_t> value;

	TrackedConnectionID() = default;
	TrackedConnectionID(std::size_t val) : value(std::make_shared<std::size_t>(val)) {}

	void reset() { if(value) *value = 0; value.reset(); }
	bool valid() const { return value && *value; }
};


using Connectable = ConnectableT<ConnectionID>;
using Connection = ConnectionT<Connectable, ConnectionID>;
using UniqueConnection = UniqueConnectionT<Connectable, ConnectionID>;

using TrackedConnectable = ConnectableT<TrackedConnectionID>;
using TrackedConnection = ConnectionT<TrackedConnectable, TrackedConnectionID>;
using TrackedUniqueConnection = UniqueConnectionT<TrackedConnectable, TrackedConnectionID>;



// - implementation -
template<typename C, typename ID>
ConnectionT<C, ID>::ConnectionT(C& connectable, ID id)
	: connectable_(&connectable), id_(id)
{
}

template<typename C, typename ID>
void ConnectionT<C, ID>::disconnect()
{
	if(connectable_) connectable_->disconnect(id_);
	connectable_ = {};
	id_ = {};
}

// UniqueConnection
template<typename C, typename ID> UniqueConnectionT<C, ID>&
UniqueConnectionT<C, ID>::operator=(UniqueConnectionT&& lhs) noexcept
{
	try {
		disconnect();
	} catch(const std::exception& error) {
		std::cerr << "nytl::UniqueConnectionT: disconnect failed: " << error.what() << "\n";
	}

	conn_ = lhs.conn_;
	id_ = lhs.id_;
	lhs.conn_ = {};
	lhs.id_ = {};
	return *this;
}

template<typename C, typename ID>
UniqueConnectionT<C, ID>::~UniqueConnectionT()
{
	try {
		disconnect();
	} catch(const std::exception& error) {
		std::cerr << "nytl::~UniqueConnectionT: disconnect failed: " << error.what() << "\n";
	}
}

template<typename C, typename ID>
UniqueConnectionT<C, ID>::UniqueConnectionT(UniqueConnectionT&& lhs) noexcept
	: connectable_(lhs.connectable_), id_(lhs.id_)
{
	lhs.id_ = {};
	lhs.connectable_ = {};
}

template<typename C, typename ID>
UniqueConnectionT(C& conn, ID id) : conn_(&conn), id_(id) {}
UniqueConnectionT(ConnectionT<C, ID> lhs) : conn_(lhs.connectable()), id_(lhs.id()) {}

void disconnect() { if(conn_) conn_->disconnect(id_); conn_ = {}; id_ = {}; }
C* connectable() const { return conn_; }
ID id() const { return id_; }

ID release() { auto cpy = id_; id_ = {}; conn_ = {}; return cpy; }

// ConnectionIDs
constexpr bool operator==(ConnectionID a, ConnectionID b) { return a.value == b.value; }
constexpr bool operator!=(ConnectionID a, ConnectionID b) { return a.value != b.value; }

bool operator==(const TrackedConnectionID& a, const TrackedConnectionID& b)
{
	return a.value == b.value;
}
bool operator!=(const TrackedConnectionID& a, const TrackedConnectionID& b)
{
	return a.value != b.value;
}

} // namespace nytl

#endif // header guard
