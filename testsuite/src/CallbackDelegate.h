//
// CallbackDelegate.h
//
// Definition of the CallbackDelegate class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef CallbackDelegate_INCLUDED
#define CallbackDelegate_INCLUDED


#include "Poco/OSP/BundleEvent.h"
#include "Poco/AbstractDelegate.h"
#include <functional>
#include <memory>


class CallbackDelegate: public Poco::AbstractDelegate<Poco::OSP::BundleEvent>
	/// Calls a function for a bundle event without the lock that
	/// Poco::Delegate holds during the call, so that the function may
	/// wait for another thread that fires the same event.
{
public:
	using Callback = std::function<void(Poco::OSP::BundleEvent&)>;

	CallbackDelegate() = delete;
	explicit CallbackDelegate(Callback callback);
	CallbackDelegate(const CallbackDelegate& other);
	CallbackDelegate(CallbackDelegate&& other) = delete;
	~CallbackDelegate() override;

	CallbackDelegate& operator = (const CallbackDelegate&) = delete;
	CallbackDelegate& operator = (CallbackDelegate&&) = delete;

	bool notify(const void* sender, Poco::OSP::BundleEvent& event) override;
	Poco::AbstractDelegate<Poco::OSP::BundleEvent>* clone() const override;
	void disable() override;
		/// Does nothing: the delegate is removed only when the
		/// event is not fired.
	bool equals(const Poco::AbstractDelegate<Poco::OSP::BundleEvent>& other) const override;
		/// Returns true iff other is this delegate or one of its clones.

private:
	Callback _callback;
	std::shared_ptr<int> _pKey;
};


#endif // CallbackDelegate_INCLUDED
