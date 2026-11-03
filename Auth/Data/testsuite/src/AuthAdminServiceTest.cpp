//
// AuthAdminServiceTest.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "AuthAdminServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/SQLite/Connector.h"
#include "Poco/NotificationCenter.h"
#include "Poco/NObserver.h"
#include "Poco/Format.h"
#include "Poco/File.h"
#include <iostream>


using namespace Poco::OSP::Auth;
using namespace Poco::OSP::Auth::Data;
using namespace Poco::Data;
using namespace Poco::Data::Keywords;


bool AuthAdminServiceTest::_firstRun(true);


AuthAdminServiceTest::AuthAdminServiceTest(const std::string& name, bool testInlineAttributes):
	CppUnit::TestCase(name),
	_testName(name),
	_testInlineAttributes(testInlineAttributes)
{
}


AuthAdminServiceTest::~AuthAdminServiceTest()
{
}


void AuthAdminServiceTest::testUsers()
{
	_pAAS->addUser("user1", "user");
	_pAAS->addUser("user2", "resu");
	_pAAS->addUser("admin1", "admin");

	std::set<std::string> users;
	_pAAS->users(users);
	assert (users.size() == 3);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
	assert (users.find("admin1") != users.end());

	_pAAS->removeUser("admin1");
	_pAAS->users(users);
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
}


void AuthAdminServiceTest::testManyUsers()
{
	for (int i = 0; i < 100; i++)
	{
		std::cout << '.' << std::flush;
		_pAAS->addUser(Poco::format("user%02d", i), "s3cr3t");
	}

	std::set<std::string> users;
	_pAAS->users(users, 0, 10);
	assert (users.size() == 10);
	assert (users.find("user00") != users.end());
	assert (users.find("user09") != users.end());

	_pAAS->users(users, 10, 10);
	assert (users.size() == 10);
	assert (users.find("user10") != users.end());
	assert (users.find("user19") != users.end());

	_pAAS->users(users, 90, 10);
	assert (users.size() == 10);
	assert (users.find("user90") != users.end());
	assert (users.find("user99") != users.end());

	_pAAS->users(users, 100, 10);
	assert (users.size() == 0);
}


void AuthAdminServiceTest::testUsersWithPermission()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	std::set<std::string> perms1;
	perms1.insert("perm1");
	std::set<std::string> perms2;
	perms2.insert("perm2");

	_pAAS->grantPermissionsToUser("user1", perms1);
	_pAAS->grantPermissionsToUser("user2", perms2);
	_pAAS->grantPermissionsToUser("user3", perms1);
	_pAAS->grantPermissionsToUser("user4", perms2);

	std::set<std::string> users;
	_pAAS->usersWithPermission(users, "perm1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithPermission(users, "perm2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testUsersWithRole()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	_pAAS->addRole("role1");
	_pAAS->addRole("role2");

	_pAAS->assignRoleToUser("user1", "role1");
	_pAAS->assignRoleToUser("user2", "role2");
	_pAAS->assignRoleToUser("user3", "role1");
	_pAAS->assignRoleToUser("user4", "role2");

	std::set<std::string> users;
	_pAAS->usersWithRole(users, "role1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithRole(users, "role2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testUsersWithAttribute()
{
	_pAAS->addUser("user1", "s3cr3t");
	_pAAS->addUser("user2", "s3cr3t");
	_pAAS->addUser("user3", "s3cr3t");
	_pAAS->addUser("user4", "s3cr3t");

	_pAAS->setUserAttribute("user1", "attr", "value1");
	_pAAS->setUserAttribute("user2", "attr", "value2");
	_pAAS->setUserAttribute("user3", "attr", "value1");
	_pAAS->setUserAttribute("user4", "attr", "value2");

	std::set<std::string> users;
	_pAAS->usersWithAttribute(users, "attr", "value1");
	assert (users.size() == 2);
	assert (users.find("user1") != users.end());
	assert (users.find("user3") != users.end());

	users.clear();
	_pAAS->usersWithAttribute(users, "attr", "value2");
	assert (users.size() == 2);
	assert (users.find("user2") != users.end());
	assert (users.find("user4") != users.end());
}


void AuthAdminServiceTest::testRoles()
{
	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	std::set<std::string> roles;
	_pAAS->roles(roles);
	assert (roles.size() == 2);
	assert (roles.find("user") != roles.end());
	assert (roles.find("admin") != roles.end());

	_pAAS->removeRole("admin");
	roles.clear();
	_pAAS->roles(roles);
	assert (roles.size() == 1);
	assert (roles.find("user") != roles.end());
}


void AuthAdminServiceTest::testPermissions()
{
	std::set<std::string> perms;

	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	perms.clear();
	perms.insert("perm1");
	perms.insert("perm2");
	_pAAS->grantPermissionsToRole("user", perms);

	perms.clear();
	perms.insert("admin");
	_pAAS->grantPermissionsToRole("admin", perms);

	perms.clear();
	perms.insert("perm3");
	_pAAS->grantPermissionsToUser("user1", perms);

	_pAAS->assignRoleToUser("user1", "user");
	_pAAS->assignRoleToUser("user2", "user");
	_pAAS->assignRoleToUser("admin1", "admin");
	_pAAS->assignRoleToUser("admin1", "user");

	std::set<std::string> roles;
	_pAAS->rolesForUser("user1", roles);
	assert (roles.size() == 1);
	assert (roles.find("user") != roles.end());

	roles.clear();
	_pAAS->rolesForUser("admin1", roles);
	assert (roles.size() == 2);
	assert (roles.find("user") != roles.end());
	assert (roles.find("admin") != roles.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("user1", perms);
	assert (perms.size() == 3);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());
	assert (perms.find("perm3") != perms.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("user2", perms);
	assert (perms.size() == 2);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("admin1", perms);
	assert (perms.size() == 3);
	assert (perms.find("perm1") != perms.end());
	assert (perms.find("perm2") != perms.end());
	assert (perms.find("admin") != perms.end());

	_pAAS->removeRoleFromUser("admin1", "user");
	roles.clear();
	_pAAS->rolesForUser("admin1", roles);
	assert (roles.size() == 1);
	assert (roles.find("admin") != roles.end());

	perms.clear();
	_pAAS->effectivePermissionsForUser("admin1", perms);
	assert (perms.size() == 1);
	assert (perms.find("admin") != perms.end());
}


void AuthAdminServiceTest::testAuthenticate()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	assert (_pAAS->authenticate("user1", "pass1"));
	assert (_pAAS->authenticate("user2", "pass2"));
	assert (_pAAS->authenticate("admin1", "pass3"));
	assert (_pAAS->verifyCredentials("user1", "pass1"));

	assert (!_pAAS->authenticate("admin1", "pass1"));
	assert (!_pAAS->authenticate("admin2", "pass3"));
	assert (!_pAAS->verifyCredentials("admin1", "pass1"));
}


void AuthAdminServiceTest::testAuthenticateNotification()
{
	_pAAS->addUser("userNOT", "passNOT");

	auto& nc = Poco::NotificationCenter::defaultCenter();

	nc.addObserver(
		Poco::NObserver<AuthAdminServiceTest, Poco::OSP::Auth::AuthNotification>(*this, &AuthAdminServiceTest::handleNotification)
	);

	assert (_pAAS->authenticate("userNOT", "passNOT"));
	assert (!_pAAS->authenticate("userNOT", "passWRONG"));

	assert(_notificationCounter == 1);

	nc.removeObserver(
		Poco::NObserver<AuthAdminServiceTest, Poco::OSP::Auth::AuthNotification>(*this, &AuthAdminServiceTest::handleNotification)
	);
}


void AuthAdminServiceTest::testAuthenticateV1()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS->addUser("user1", "pass1");
	pAAS->addUser("user2", "pass2");
	pAAS->addUser("admin1", "pass3");

	assert (pAAS->authenticate("user1", "pass1"));
	assert (pAAS->authenticate("user2", "pass2"));
	assert (pAAS->authenticate("admin1", "pass3"));

	assert (!pAAS->authenticate("admin1", "pass1"));
	assert (!pAAS->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV1ToV2()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS1 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS1->addUser("user1", "pass1");
	pAAS1->addUser("user2", "pass2");
	pAAS1->addUser("admin1", "pass3");

	assert (pAAS1->authenticate("user1", "pass1"));
	assert (pAAS1->authenticate("user2", "pass2"));
	assert (pAAS1->authenticate("admin1", "pass3"));

	assert (!pAAS1->authenticate("admin1", "pass1"));
	assert (!pAAS1->authenticate("admin2", "pass3"));

	// check hashes are stored as V1 (MD5)
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash == "49ecb36e877c78513407a5d36d9459a5");

	authParams.hashVersion = 2;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS2->authenticate("user1", "pass1"));
	assert (pAAS2->authenticate("user2", "pass2"));
	assert (pAAS2->authenticate("admin1", "pass3"));

	assert (!pAAS2->authenticate("admin1", "pass1"));
	assert (!pAAS2->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V2
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=2;") == 0);
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV1ToV3()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 1;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS1 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS1->addUser("user1", "pass1");
	pAAS1->addUser("user2", "pass2");
	pAAS1->addUser("admin1", "pass3");

	assert (pAAS1->authenticate("user1", "pass1"));
	assert (pAAS1->authenticate("user2", "pass2"));
	assert (pAAS1->authenticate("admin1", "pass3"));

	assert (!pAAS1->authenticate("admin1", "pass1"));
	assert (!pAAS1->authenticate("admin2", "pass3"));

	// check hashes are stored as V1 (MD5)
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash == "49ecb36e877c78513407a5d36d9459a5");

	authParams.hashVersion = 3;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V3
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV2ToV3()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS2 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS2->addUser("user1", "pass1");
	pAAS2->addUser("user2", "pass2");
	pAAS2->addUser("admin1", "pass3");

	assert (pAAS2->authenticate("user1", "pass1"));
	assert (pAAS2->authenticate("user2", "pass2"));
	assert (pAAS2->authenticate("admin1", "pass3"));

	assert (!pAAS2->authenticate("admin1", "pass1"));
	assert (!pAAS2->authenticate("admin2", "pass3"));

	// check hashes are stored as V2
	Poco::Data::Session session("SQLite", "auth.db");

	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=2;") == 0);

	authParams.hashVersion = 3;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V3
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthenticateUpgradeFromV3ToV4()
{
	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 3;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS3 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS3->addUser("user1", "pass1");
	pAAS3->addUser("user2", "pass2");
	pAAS3->addUser("admin1", "pass3");

	assert (pAAS3->authenticate("user1", "pass1"));
	assert (pAAS3->authenticate("user2", "pass2"));
	assert (pAAS3->authenticate("admin1", "pass3"));

	assert (!pAAS3->authenticate("admin1", "pass1"));
	assert (!pAAS3->authenticate("admin2", "pass3"));

	// check hashes are stored as V3
	Poco::Data::Session session("SQLite", "auth.db");

	std::string hash;
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=3;") == 0);

	assert (_pAAS->authenticate("user1", "pass1"));
	assert (_pAAS->authenticate("user2", "pass2"));
	assert (_pAAS->authenticate("admin1", "pass3"));

	assert (!_pAAS->authenticate("admin1", "pass1"));
	assert (!_pAAS->authenticate("admin2", "pass3"));

	// check hashes have been upgraded to V4
	session << "SELECT password FROM auth_user WHERE username = 'user1'", into(hash), now;
	assert (hash.compare(0, 4, "v=4;") == 0);

	authParams.hashVersion = 4;
	AuthAdminServiceImpl::Ptr pAAS4 = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	assert (pAAS4->authenticate("user1", "pass1"));
	assert (pAAS4->authenticate("user2", "pass2"));
	assert (pAAS4->authenticate("admin1", "pass3"));

	assert (!pAAS4->authenticate("admin1", "pass1"));
	assert (!pAAS4->authenticate("admin2", "pass3"));
}


void AuthAdminServiceTest::testAuthorize()
{
	std::set<std::string> perms;

	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");
	_pAAS->addUser("admin1", "pass3");

	_pAAS->addRole("user");
	_pAAS->addRole("admin");

	perms.clear();
	perms.insert("perm1");
	perms.insert("perm2");
	_pAAS->grantPermissionsToRole("user", perms);

	perms.clear();
	perms.insert("admin");
	_pAAS->grantPermissionsToRole("admin", perms);

	perms.clear();
	perms.insert("perm3");
	_pAAS->grantPermissionsToUser("user1", perms);

	_pAAS->assignRoleToUser("user1", "user");
	_pAAS->assignRoleToUser("user2", "user");
	_pAAS->assignRoleToUser("admin1", "admin");
	_pAAS->assignRoleToUser("admin1", "user");

	assert (_pAAS->authorize("user1", "perm1"));
	assert (_pAAS->authorize("user1", "perm2"));
	assert (_pAAS->authorize("user1", "perm3"));

	assert (!_pAAS->authorize("user1", "admin"));
	assert (!_pAAS->authorize("user1", "perm4"));

	assert (_pAAS->authorize("user2", "perm1"));
	assert (_pAAS->authorize("user2", "perm2"));
	assert (!_pAAS->authorize("user2", "perm3"));
	assert (!_pAAS->authorize("user2", "admin"));

	assert (_pAAS->authorize("admin1", "perm1"));
	assert (_pAAS->authorize("admin1", "perm2"));
	assert (_pAAS->authorize("admin1", "admin"));
	assert (!_pAAS->authorize("admin1", "perm4"));
}


void AuthAdminServiceTest::testAttributes()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");

	std::set<std::string> attrs;
	_pAAS->attributesForUser("user1", attrs);

	assert (attrs.empty());

	_pAAS->setUserAttribute("user1", "attr1", "value1");
	_pAAS->setUserAttribute("user1", "attr2", "value2");

	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 2);
	assert (attrs.find("attr1") != attrs.end());
	assert (attrs.find("attr2") != attrs.end());

	_pAAS->setUserAttribute("user1", "attr3", "value3");

	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 3);
	assert (attrs.find("attr1") != attrs.end());
	assert (attrs.find("attr2") != attrs.end());
	assert (attrs.find("attr3") != attrs.end());

	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");
	assert (_pAAS->getUserAttribute("user1", "attr2") == "value2");
	assert (_pAAS->getUserAttribute("user1", "attr3") == "value3");

	_pAAS->removeUserAttribute("user1", "attr3");

	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");
	assert (_pAAS->getUserAttribute("user1", "attr2") == "value2");
	assert (_pAAS->getUserAttribute("user1", "attr3", "undefined") == "undefined");

	_pAAS->removeUserAttribute("user1", "attr2");
	assert (_pAAS->getUserAttribute("user1", "attr2", "undefined") == "undefined");

	attrs.clear();
	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 1);
	assert (attrs.find("attr1") != attrs.end());

	_pAAS->setUserAttribute("user2", "attr1", "value3");
	attrs.clear();
	_pAAS->attributesForUser("user2", attrs);
	assert (attrs.size() == 1);
	assert (attrs.find("attr1") != attrs.end());

	assert (_pAAS->getUserAttribute("user2", "attr1") == "value3");
	assert (_pAAS->getUserAttribute("user1", "attr1") == "value1");

	std::vector<std::string> users;
	users = _pAAS->findUsersByAttribute("attr1", "value1");
	assert (users.size() == 1 && users[0] == "user1");
	users = _pAAS->findUsersByAttribute("attr1", "value3");
	assert (users.size() == 1 && users[0] == "user2");
	users = _pAAS->findUsersByAttribute("attr1", "valueX");
	assert (users.empty());
}


void AuthAdminServiceTest::testAttributesForUser()
{
	_pAAS->addUser("user1", "pass1");
	_pAAS->addUser("user2", "pass2");

	_pAAS->setUserAttribute("user1", "attr1", "value1");
	_pAAS->setUserAttribute("user1", "attr2", "value2");

	_pAAS->setUserAttribute("user2", "attr1", "value3");

	std::map<std::string, std::string> attrs;
	_pAAS->attributesForUser("user1", attrs);
	assert (attrs.size() == 2);
	assert (attrs["attr1"] == "value1");
	assert (attrs["attr2"] == "value2");

	attrs.clear();
	_pAAS->attributesForUser("user2", attrs);
	assert (attrs.size() == 1);
	assert (attrs["attr1"] == "value3");
}


void AuthAdminServiceTest::testLockOut()
{
	_pAAS->addUser("user1", "user");

	assert (_pAAS->authenticate("user1", "user"));
	assert (!_pAAS->isUserLockedOut("user1"));

	std::map<std::string, std::string> attrs;
	int flags = _pAAS->attributesForUser("user1", attrs);
	assert ((flags & AuthAdminService::USER_LOCKED_OUT) == 0);

	_pAAS->lockOutUser("user1", Poco::DateTime() + Poco::Timespan(600, 0));

	assert (_pAAS->isUserLockedOut("user1"));

	flags = _pAAS->attributesForUser("user1", attrs);
	assert ((flags & AuthAdminService::USER_LOCKED_OUT) != 0);
	assert (!_pAAS->authenticate("user1", "user"));

	_pAAS->unlockOutUser("user1");

	assert (!_pAAS->isUserLockedOut("user1"));
	assert (_pAAS->authenticate("user1", "user"));
}


void AuthAdminServiceTest::testLockOutAfterLoginFailures()
{
	Poco::Data::Session session("SQLite", "auth.db");
	std::string hash;
	session << "ALTER TABLE auth_user ADD auth_failures INTEGER NOT NULL DEFAULT 0", now;

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 2;
	authParams.saltLength = 8;
	authParams.pbkdf2Iterations = 10000;
	authParams.credentialsSalt = "poco";
	authParams.maxFailures = 3;
	authParams.lockOutTime = 1;

	AuthAdminServiceImpl::LDAPParams ldapParams;
	AuthAdminServiceImpl::Ptr pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);

	pAAS->addUser("user1", "pass1");

	// Successful login attempt after two failures
	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (pAAS->authenticate("user1", "pass1"));
	assert (!pAAS->isUserLockedOut("user1"));

	// Lock out after three failures
	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (!pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "bad"));
	assert (pAAS->isUserLockedOut("user1"));

	assert (!pAAS->authenticate("user1", "pass1"));

	Poco::Thread::sleep(1500);

	assert (!pAAS->isUserLockedOut("user1"));
	assert (pAAS->authenticate("user1", "pass1"));
}


void AuthAdminServiceTest::testReset()
{
	_pAAS->addUser("user1", "user");
	_pAAS->addUser("user2", "resu");
	_pAAS->addUser("admin1", "admin");

	_pAAS->reset();

	std::set<std::string> users;
	_pAAS->users(users);
	assert (users.size() == 3);
	assert (users.find("user1") != users.end());
	assert (users.find("user2") != users.end());
	assert (users.find("admin1") != users.end());
}


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
void AuthAdminServiceTest::testExternalRole()
{
	_pAAS->addRole("extRole");
	_pAAS->addRole("localRole");
	_pAAS->addRole("undefRole");

	_pAAS->setRoleAttribute("extRole", AuthAdminService::ROLE_ATTR_LDAP, "true");
	_pAAS->setRoleAttribute("localRole", AuthAdminService::ROLE_ATTR_LDAP, "false");

	assert(_pAAS->isExternalRole("extRole"));
	assert(!_pAAS->isExternalRole("localRole"));
	assert(!_pAAS->isExternalRole("undefRole"));
}
#endif


void AuthAdminServiceTest::testRoleMappings()
{
	// Setup: create users, roles, and permissions
	_pAAS->addUser("ldapuser", "pass1");
	_pAAS->addUser("localuser", "pass2");

	_pAAS->addRole("cn=ldap_operators,dc=example,dc=com");
	_pAAS->addRole("cn=ldap_viewers,dc=example,dc=com");
	_pAAS->addRole("operators");
	_pAAS->addRole("cn=ldap_admins,dc=example,dc=com");
	_pAAS->addRole("admins");

	// Grant permissions to internal roles
	std::set<std::string> perms;
	perms.insert("app.devices.read");
	perms.insert("app.devices.write");
	perms.insert("hmi");
	_pAAS->grantPermissionsToRole("operators", perms);

	perms.clear();
	perms.insert("app.auth.admin");
	perms.insert("app.sql.admin");
	_pAAS->grantPermissionsToRole("admins", perms);

	// Grant an LDAP permission to the external role (simulates rolePermissionAttribute = cn)
	perms.clear();
	perms.insert("LDAP_Operators");
	_pAAS->grantPermissionsToRole("cn=ldap_operators,dc=example,dc=com", perms);

	// Assign LDAP role to user
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_operators,dc=example,dc=com");

	// --- User with no roles/mappings should have no permissions ---
	perms.clear();
	_pAAS->effectivePermissionsForUser("localuser", perms);
	assert (perms.size() == 0);

	// --- Without mapping: user only has the LDAP permission ---
	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 1);
	assert (perms.find("LDAP_Operators") != perms.end());
	assert (perms.find("app.devices.read") == perms.end());

	// --- Test setRoleMapping: map external LDAP role to internal role ---
	_pAAS->setRoleMapping("cn=ldap_operators,dc=example,dc=com", "operators");

	// Verify mapping was stored
	std::string internalRole;
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole == "operators");

	// With mapping: user gets both LDAP permission + mapped role permissions
	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 4);
	assert (perms.find("LDAP_Operators") != perms.end());
	assert (perms.find("app.devices.read") != perms.end());
	assert (perms.find("app.devices.write") != perms.end());
	assert (perms.find("hmi") != perms.end());

	// --- Test authorize works through the mapping ---
	assert (_pAAS->authorize("ldapuser", "app.devices.read"));
	assert (_pAAS->authorize("ldapuser", "hmi"));
	assert (!_pAAS->authorize("ldapuser", "app.auth.admin"));

	// --- Test allRoleMappings ---
	std::map<std::string, std::string> mappings;
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 1);
	assert (mappings["cn=ldap_operators,dc=example,dc=com"] == "operators");

	// --- Test adding a second mapping ---
	_pAAS->setRoleMapping("cn=ldap_admins,dc=example,dc=com", "admins");
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_admins,dc=example,dc=com");

	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	assert (perms.size() == 6); // LDAP_Operators + app.devices.read + app.devices.write + hmi + app.auth.admin + app.sql.admin
	assert (perms.find("app.auth.admin") != perms.end());
	assert (perms.find("app.sql.admin") != perms.end());

	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 2);

	// --- Test double-mapping to same internal role: no permission duplication ---
	_pAAS->setRoleMapping("cn=ldap_viewers,dc=example,dc=com", "operators");
	_pAAS->assignRoleToUser("ldapuser", "cn=ldap_viewers,dc=example,dc=com");

	perms.clear();
	_pAAS->effectivePermissionsForUser("ldapuser", perms);
	// operators perms (app.devices.read, app.devices.write, hmi) should appear once even though
	// two external roles map to operators. UNION in SQL deduplicates.
	assert (perms.size() == 6); // still 6, not 9
	assert (perms.find("app.devices.read") != perms.end());

	// --- Test overwrite mapping: change operators -> admins ---
	_pAAS->setRoleMapping("cn=ldap_operators,dc=example,dc=com", "admins");
	internalRole.clear();
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole == "admins");

	// --- Test removeRoleMapping ---
	_pAAS->removeRoleMapping("cn=ldap_operators,dc=example,dc=com");
	internalRole.clear();
	_pAAS->roleMappingForExternal("cn=ldap_operators,dc=example,dc=com", internalRole);
	assert (internalRole.empty());

	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 2); // ldap_admins + ldap_viewers remain

	// --- Test that removing the internal role cascades to remove mappings ---
	_pAAS->removeRole("admins");
	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 1); // only ldap_viewers -> operators remains
	assert (mappings.find("cn=ldap_viewers,dc=example,dc=com") != mappings.end());

	// --- Test that removing the external role cascades to remove the mapping ---
	_pAAS->removeRole("cn=ldap_viewers,dc=example,dc=com");
	mappings.clear();
	_pAAS->allRoleMappings(mappings);
	assert (mappings.size() == 0);

	// --- Test query for non-existent mapping returns empty ---
	internalRole = "should-be-cleared";
	_pAAS->roleMappingForExternal("nonexistent", internalRole);
	assert (internalRole.empty());
}


void AuthAdminServiceTest::handleNotification(const Poco::AutoPtr<Poco::OSP::Auth::AuthNotification>& n)
{
	if (n != nullptr)
	{
		std::cout << "Logged in: " << n->userName() << ", " << n->fullName() << std::endl;
		++_notificationCounter;
	}
}


void AuthAdminServiceTest::setUp()
{
	if (_firstRun)
	{
		Poco::Data::SQLite::Connector::registerConnector();
		_firstRun = false;
	}

	Poco::File f("auth.db");
	if (f.exists())
	{
		f.remove();
	}

	Poco::Data::Session session("SQLite", "auth.db");
	if (_testInlineAttributes)
	{
		session <<
			"CREATE TABLE auth_user"
			"("
			"    username VARCHAR(256) PRIMARY KEY, "
			"    password VARCHAR(256) NOT NULL, "
			"    attr1    VARCHAR(256), "
			"    attr2    VARCHAR(256)"
			")",
			now;
	}
	else
	{
		session <<
			"CREATE TABLE auth_user"
			"("
			"    username VARCHAR(256) PRIMARY KEY, "
			"    password VARCHAR(256) NOT NULL"
			")",
			now;
	}
	session <<
		"CREATE TABLE auth_role"
		"("
		"    rolename VARCHAR(256) PRIMARY KEY"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_roles"
		"("
		"    username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,"
		"    rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_permissions"
		"("
		"    username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,"
		"    permission VARCHAR(32) NOT NULL"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_permissions"
		"("
		"    rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE,"
		"    permission VARCHAR(32) NOT NULL"
		")",
		now;
	session <<
		"CREATE TABLE auth_user_attributes"
		"("
		"    username VARCHAR(256), "
		"    attribute VARCHAR(256), "
		"    value VARCHAR(256)"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_attributes"
		"("
		"    rolename VARCHAR(256), "
		"    attribute VARCHAR(256), "
		"    value VARCHAR(256)"
		")",
		now;
	session <<
		"CREATE TABLE auth_role_mappings"
		"("
		"    external_role VARCHAR(256) PRIMARY KEY REFERENCES auth_role(rolename) ON DELETE CASCADE, "
		"    internal_role VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE"
		")",
		now;

	Poco::Path propsPath(false);
	int upCount = 0;
	do
	{
		propsPath.clear();
		upCount++;
		for (int i = 0; i < upCount; i++)
		{
			propsPath.pushDirectory("..");
		}
		propsPath.pushDirectory("bundle");
		propsPath.setFileName("bundle.properties");
	}
	while (upCount < 5 && !Poco::File(propsPath.toString()).exists());

	_pProperties = new Poco::Util::PropertyFileConfiguration(propsPath.toString());

	AuthAdminServiceImpl::AuthParams authParams;
	authParams.dbConnector = "SQLite";
	authParams.dbConnectionString = "auth.db";
	authParams.cacheSize = 100;
	authParams.cacheTimeout = 3600;
	authParams.hashVersion = 4;
	authParams.saltLength = 16;
	authParams.pbkdf2Iterations = 50000;
	authParams.credentialsSalt = "poco";

	AuthAdminServiceImpl::LDAPParams ldapParams;
	if (_testName == "testExternalRole" || _testName == "testRoleMappings")
		ldapParams.uri = "ldap://example.com";	// Simulate LDAP enabled

	_pAAS = new AuthAdminServiceImpl(*_pProperties, Poco::Logger::root(), authParams, ldapParams);
}


void AuthAdminServiceTest::tearDown()
{
	_pAAS = 0;
}


CppUnit::Test* AuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthAdminServiceTest");

	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsers);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testManyUsers);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithPermission);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithRole);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testUsersWithAttribute);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRoles);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testPermissions);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticate);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateNotification);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateV1);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV2);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV3);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV3);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAuthorize);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAttributes);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testAttributesForUser);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOut);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLockOutAfterLoginFailures);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testReset);
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testExternalRole);
#endif
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testRoleMappings);

	return pSuite;
}


InlineAttributesAuthAdminServiceTest::InlineAttributesAuthAdminServiceTest(const std::string& name):
	AuthAdminServiceTest(name, true)
{
}


CppUnit::Test* InlineAttributesAuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("InlineAttributesAuthAdminServiceTest");

	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsers);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testManyUsers);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithPermission);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithRole);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testUsersWithAttribute);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testRoles);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testPermissions);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticate);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateNotification);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateV1);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV2);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV1ToV3);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthenticateUpgradeFromV2ToV3);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAuthorize);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAttributes);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testAttributesForUser);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testLockOut);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testLockOutAfterLoginFailures);
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testReset);
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	CppUnit_addTest(pSuite, InlineAttributesAuthAdminServiceTest, testExternalRole);
#endif

	return pSuite;
}
