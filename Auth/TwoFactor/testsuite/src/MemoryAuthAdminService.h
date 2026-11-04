//
// MemoryAuthAdminService.h
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef MemoryAuthAdminService_INCLUDED
#define MemoryAuthAdminService_INCLUDED


#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/AutoPtr.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include "Poco/Mutex.h"
#include "Poco/Timestamp.h"
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>


class MemoryAuthAdminService: public Poco::OSP::Auth::AuthAdminService
	/// Keeps user attributes and lock-outs in memory.
	/// The other parts of the AuthAdminService interface are not used by the tests.
{
public:
	using Ptr = Poco::AutoPtr<MemoryAuthAdminService>;

	MemoryAuthAdminService() = default;
	MemoryAuthAdminService(const MemoryAuthAdminService&) = delete;
	MemoryAuthAdminService(MemoryAuthAdminService&&) = delete;
	MemoryAuthAdminService& operator = (const MemoryAuthAdminService&) = delete;
	MemoryAuthAdminService& operator = (MemoryAuthAdminService&&) = delete;
	~MemoryAuthAdminService() override = default;

	void holdReadsOf(const std::string& attribute)
		/// The first read of the given attribute returns its value only
		/// after a second read of it has read the value as well, or after
		/// at most HOLD_TIME, so that two validations that are not
		/// serialised read it before either of them writes it.
	{
		_heldAttribute = attribute;
	}

	[[nodiscard]] bool waitForHeldRead() const
		/// Waits until the first read of the held attribute has read the
		/// value, or for at most HOLD_TIME. Returns true if it has.
	{
		std::unique_lock<std::mutex> lock(_holdMutex);
		return _holdCondition.wait_for(lock, HOLD_TIME, [this]() { return _heldReads >= 1; });
	}

	std::string getUserAttribute(const std::string& username, const std::string& attribute,
		const std::string& deflt) const override
	{
		std::string value(deflt);
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			const auto itUser = _attributes.find(username);
			if (itUser != _attributes.end())
			{
				const auto itAttr = itUser->second.find(attribute);
				if (itAttr != itUser->second.end()) value = itAttr->second;
			}
		}
		if (!_heldAttribute.empty() && attribute == _heldAttribute)
		{
			std::unique_lock<std::mutex> lock(_holdMutex);
			++_heldReads;
			_holdCondition.notify_all();
			_holdCondition.wait_for(lock, HOLD_TIME, [this]() { return _heldReads >= 2; });
		}
		return value;
	}

	void setAuthAttribute(const std::string& username, const std::string& attribute,
		const std::string& value) override
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_attributes[username][attribute] = value;
	}

	void removeAuthAttribute(const std::string& username, const std::string& attribute) override
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_attributes[username].erase(attribute);
	}

	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) override
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_lockOuts[username] = lockOutUntil;
	}

	void unlockOutUser(const std::string& username) override
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_lockOuts.erase(username);
	}

	bool isUserLockedOut(const std::string& username) const override
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		const auto it = _lockOuts.find(username);
		return it != _lockOuts.end() && it->second >= _time;
	}

	Poco::DateTime lockOutUntil(const std::string& username) const
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		return _lockOuts.at(username);
	}

	void setTime(const Poco::DateTime& time)
		/// Sets the time that isUserLockedOut() compares with the end of a lock-out.
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_time = time;
	}

	// AuthService
	bool authenticate(const std::string&, const std::string&) const override
		{ throw Poco::NotImplementedException(); }
	bool authorize(const std::string&, const std::string&) const override { throw Poco::NotImplementedException(); }
	bool authorize(const std::string&, const std::string&, const std::string&) const override
		{ throw Poco::NotImplementedException(); }
	bool userExists(const std::string&) const override { throw Poco::NotImplementedException(); }

	// AuthAdminService
	bool verifyCredentials(const std::string&, const std::string&) override
		{ throw Poco::NotImplementedException(); }
	void changePassword(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
	void permissionsForUser(const std::string&, std::set<std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	void effectivePermissionsForUser(const std::string&, std::set<std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	void grantPermissionsToUser(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void replacePermissionsForUser(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void revokePermissionsFromUser(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	int users(std::set<std::string>&, int, int) const override { throw Poco::NotImplementedException(); }
	int usersWithName(std::set<std::string>&, const std::string&, int, int) const override
		{ throw Poco::NotImplementedException(); }
	int usersWithPermission(std::set<std::string>&, const std::string&, int, int) const override
		{ throw Poco::NotImplementedException(); }
	int usersWithRole(std::set<std::string>&, const std::string&, int, int) const override
		{ throw Poco::NotImplementedException(); }
	int usersWithAttribute(std::set<std::string>&, const std::string&, const std::string&, int, int) const override
		{ throw Poco::NotImplementedException(); }
	bool userEnabled(const std::string&) const override { throw Poco::NotImplementedException(); }
	void addUser(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
	void removeUser(const std::string&) override { throw Poco::NotImplementedException(); }
	void permissionsForRole(const std::string&, std::set<std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	void grantPermissionsToRole(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void replacePermissionsForRole(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void revokePermissionsFromRole(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	bool roleExists(const std::string&) const override { throw Poco::NotImplementedException(); }
	void roles(std::set<std::string>&) const override { throw Poco::NotImplementedException(); }
	void addRole(const std::string&) override { throw Poco::NotImplementedException(); }
	void removeRole(const std::string&) override { throw Poco::NotImplementedException(); }
	void rolesForUser(const std::string&, std::set<std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	void assignRoleToUser(const std::string&, const std::string&) override
		{ throw Poco::NotImplementedException(); }
	void removeRoleFromUser(const std::string&, const std::string&) override
		{ throw Poco::NotImplementedException(); }
	void replaceRolesForUser(const std::string&, const std::set<std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void uncacheUser(const std::string&) override { throw Poco::NotImplementedException(); }
	void setUserAttribute(const std::string&, const std::string&, const std::string&) override
		{ throw Poco::NotImplementedException(); }
	void removeUserAttribute(const std::string&, const std::string&) override
		{ throw Poco::NotImplementedException(); }
	void replaceUserAttributes(const std::string&, const std::map<std::string, std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void updateUserAttributes(const std::string&, const std::map<std::string, std::string>&) override
		{ throw Poco::NotImplementedException(); }
	void attributesForUser(const std::string&, std::set<std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	int attributesForUser(const std::string&, std::map<std::string, std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	std::vector<std::string> findUsersByAttribute(const std::string&, const std::string&) const override
		{ throw Poco::NotImplementedException(); }
	void revokePermission(const std::string&) override { throw Poco::NotImplementedException(); }
	bool isExternalUser(const std::string&) const override { throw Poco::NotImplementedException(); }
	bool isExternalRole(const std::string&) const override { throw Poco::NotImplementedException(); }
	void allRoleMappings(std::map<std::string, std::string>&) const override
		{ throw Poco::NotImplementedException(); }
	void roleMappingForExternal(const std::string&, std::string&) const override
		{ throw Poco::NotImplementedException(); }
	void setRoleMapping(const std::string&, const std::string&) override { throw Poco::NotImplementedException(); }
	void removeRoleMapping(const std::string&) override { throw Poco::NotImplementedException(); }
	void reset() override { throw Poco::NotImplementedException(); }
	void invalidateCaches(int) override { throw Poco::NotImplementedException(); }

private:
	static constexpr std::chrono::seconds HOLD_TIME{1};
		/// Waited for in full when the validations are serialised, as the
		/// second read cannot start then.

	std::map<std::string, std::map<std::string, std::string>> _attributes;
	std::map<std::string, Poco::DateTime> _lockOuts;
	Poco::DateTime _time{Poco::Timestamp(0)};
		/// The start of the epoch, before the end of every lock-out of the
		/// tests, until a test sets the time.
	mutable Poco::FastMutex _mutex;
	std::string _heldAttribute;
	mutable std::mutex _holdMutex;
	mutable std::condition_variable _holdCondition;
	mutable int _heldReads = 0;
};


#endif // MemoryAuthAdminService_INCLUDED
