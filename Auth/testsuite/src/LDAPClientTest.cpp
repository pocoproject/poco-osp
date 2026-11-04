//
// LDAPClientTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LDAPClientTest.h"


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)


#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Auth/LDAPClient.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Stopwatch.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>


using Poco::OSP::Auth::LDAPClient;
using namespace std::string_literals;


namespace
{
	class ClosingConnection: public Poco::Net::TCPServerConnection
		/// Closes every connection without answering.
	{
	public:
		explicit ClosingConnection(const Poco::Net::StreamSocket& socket):
			Poco::Net::TCPServerConnection(socket)
		{
		}

		void run() override
		{
		}
	};


	std::string ldapURI(Poco::UInt16 port)
	{
		return "ldap://127.0.0.1:"s + std::to_string(port);
	}


	struct Entry
	{
		std::string dn;
		std::vector<std::pair<std::string, std::vector<std::string>>> attributes;
	};


	struct Directory
		/// Answers of the scripted LDAP server and a record of the requests.
	{
		unsigned bindResultCode = 0;
		unsigned searchResultCode = 0;
		std::vector<Entry> entries;

		std::vector<std::string> boundDNs;
		std::vector<std::string> requestedAttributes;
			/// The attributes requested by the last search.

		Poco::FastMutex mutex;
	};


	// Minimal BER encoding and decoding of LDAP messages (RFC 4511), enough for
	// simple binds and searches with short-form tags.

	constexpr unsigned char TAG_INTEGER = 0x02;
	constexpr unsigned char TAG_OCTET_STRING = 0x04;
	constexpr unsigned char TAG_ENUMERATED = 0x0A;
	constexpr unsigned char TAG_SEQUENCE = 0x30;
	constexpr unsigned char TAG_SET = 0x31;
	constexpr unsigned char TAG_BIND_REQUEST = 0x60;
	constexpr unsigned char TAG_BIND_RESPONSE = 0x61;
	constexpr unsigned char TAG_SEARCH_REQUEST = 0x63;
	constexpr unsigned char TAG_SEARCH_RESULT_ENTRY = 0x64;
	constexpr unsigned char TAG_SEARCH_RESULT_DONE = 0x65;
	constexpr std::size_t LONG_LENGTH = 0x80;
		/// Set in the first length byte if the following bytes hold the length.
	constexpr std::size_t LONG_LENGTH_COUNT_MASK = 0x7F;
	constexpr unsigned BYTE_MASK = 0xFF;
	constexpr unsigned BITS_PER_BYTE = 8;
	constexpr int SEARCH_REQUEST_FIELDS = 8;
		/// base, scope, deref, size limit, time limit, typesOnly, filter, attributes

	constexpr int RESULT_TIME_LIMIT_EXCEEDED = 3;
	constexpr int RESULT_SIZE_LIMIT_EXCEEDED = 4;
	constexpr int RESULT_CONSTRAINT_VIOLATION = 19;
	constexpr int RESULT_INAPPROPRIATE_AUTHENTICATION = 48;
	constexpr int RESULT_INVALID_CREDENTIALS = 49;
	constexpr int RESULT_INSUFFICIENT_ACCESS_RIGHTS = 50;
	constexpr int RESULT_BUSY = 51;
	constexpr int RESULT_UNAVAILABLE = 52;
	constexpr int RESULT_UNWILLING_TO_PERFORM = 53;
	constexpr int RESULT_OTHER = 80;


	std::string encodeLength(std::size_t length)
	{
		if (length < LONG_LENGTH) return std::string(1, static_cast<char>(length));
		std::string bytes;
		while (length > 0)
		{
			bytes.insert(bytes.begin(), static_cast<char>(length & BYTE_MASK));
			length >>= BITS_PER_BYTE;
		}
		return std::string(1, static_cast<char>(LONG_LENGTH | bytes.size())) + bytes;
	}


	std::string tlv(unsigned char tag, const std::string& value)
	{
		return std::string(1, static_cast<char>(tag)) + encodeLength(value.size()) + value;
	}


	std::string encodeInteger(unsigned char tag, unsigned value)
	{
		std::string bytes;
		while (value > 0)
		{
			bytes.insert(bytes.begin(), static_cast<char>(value & BYTE_MASK));
			value >>= BITS_PER_BYTE;
		}
		// The integer is signed and has at least one byte.
		if (bytes.empty() || (static_cast<unsigned char>(bytes[0]) & LONG_LENGTH) != 0)
			bytes.insert(bytes.begin(), '\0');
		return tlv(tag, bytes);
	}


	bool readElement(const std::string& buffer, std::size_t& pos, unsigned char& tag, std::string& value)
	{
		if (pos + 2 > buffer.size()) return false;
		tag = static_cast<unsigned char>(buffer[pos++]);
		std::size_t length = static_cast<unsigned char>(buffer[pos++]);
		if ((length & LONG_LENGTH) != 0)
		{
			const std::size_t count = length & LONG_LENGTH_COUNT_MASK;
			if (count > sizeof(std::size_t) || pos + count > buffer.size()) return false;
			length = 0;
			for (std::size_t i = 0; i < count; ++i)
				length = (length << BITS_PER_BYTE) | static_cast<unsigned char>(buffer[pos++]);
		}
		if (length > buffer.size() - pos) return false;
		value = buffer.substr(pos, length);
		pos += length;
		return true;
	}


	bool receiveExactly(Poco::Net::StreamSocket& socket, std::string& buffer, std::size_t length)
	{
		std::string chunk(length, '\0');
		std::size_t received = 0;
		while (received < length)
		{
			const int n = socket.receiveBytes(&chunk[received], static_cast<int>(length - received));
			if (n <= 0) return false;
			received += static_cast<std::size_t>(n);
		}
		buffer += chunk;
		return true;
	}


	bool receiveMessage(Poco::Net::StreamSocket& socket, std::string& message)
	{
		std::string header;
		if (!receiveExactly(socket, header, 2)) return false;
		std::size_t length = static_cast<unsigned char>(header[1]);
		if ((length & LONG_LENGTH) != 0)
		{
			std::string lengthBytes;
			if (!receiveExactly(socket, lengthBytes, length & LONG_LENGTH_COUNT_MASK)) return false;
			length = 0;
			for (char ch: lengthBytes) length = (length << BITS_PER_BYTE) | static_cast<unsigned char>(ch);
		}
		message.clear();
		return receiveExactly(socket, message, length);
	}


	std::string ldapResult(unsigned messageID, unsigned char tag, unsigned resultCode)
	{
		const std::string result = encodeInteger(TAG_ENUMERATED, resultCode) + tlv(TAG_OCTET_STRING, ""s) +
			tlv(TAG_OCTET_STRING, ""s);
		return tlv(TAG_SEQUENCE, encodeInteger(TAG_INTEGER, messageID) + tlv(tag, result));
	}


	std::string searchEntry(unsigned messageID, const Entry& entry)
	{
		std::string attributes;
		for (const auto& [name, values]: entry.attributes)
		{
			std::string encodedValues;
			for (const auto& v: values) encodedValues += tlv(TAG_OCTET_STRING, v);
			attributes += tlv(TAG_SEQUENCE, tlv(TAG_OCTET_STRING, name) + tlv(TAG_SET, encodedValues));
		}
		const std::string objectEntry = tlv(TAG_OCTET_STRING, entry.dn) + tlv(TAG_SEQUENCE, attributes);
		return tlv(TAG_SEQUENCE, encodeInteger(TAG_INTEGER, messageID) + tlv(TAG_SEARCH_RESULT_ENTRY, objectEntry));
	}


	unsigned decodeInteger(const std::string& value)
	{
		unsigned result = 0;
		for (char ch: value) result = (result << BITS_PER_BYTE) | static_cast<unsigned char>(ch);
		return result;
	}


	class ScriptedConnection: public Poco::Net::TCPServerConnection
		/// Answers binds and searches as the Directory says.
	{
	public:
		ScriptedConnection(const Poco::Net::StreamSocket& socket, Directory& directory):
			Poco::Net::TCPServerConnection(socket),
			_directory(directory)
		{
		}

		void run() override
		{
			std::string message;
			while (receiveMessage(socket(), message))
			{
				std::size_t pos = 0;
				unsigned char tag = 0;
				std::string id;
				std::string operation;
				if (!readElement(message, pos, tag, id) || !readElement(message, pos, tag, operation)) return;
				const unsigned messageID = decodeInteger(id);
				std::string response;
				if (tag == TAG_BIND_REQUEST) // version, name, simple authentication
				{
					std::size_t opPos = 0;
					unsigned char t = 0;
					std::string version;
					std::string name;
					if (!readElement(operation, opPos, t, version) || !readElement(operation, opPos, t, name)) return;
					Poco::FastMutex::ScopedLock lock(_directory.mutex);
					_directory.boundDNs.push_back(name);
					response = ldapResult(messageID, TAG_BIND_RESPONSE, _directory.bindResultCode);
				}
				else if (tag == TAG_SEARCH_REQUEST)
				{
					std::size_t opPos = 0;
					unsigned char t = 0;
					std::string field;
					for (int i = 0; i < SEARCH_REQUEST_FIELDS; ++i)
					{
						if (!readElement(operation, opPos, t, field)) return;
					}
					std::vector<std::string> requested;
					std::size_t attrPos = 0;
					std::string attribute;
					while (readElement(field, attrPos, t, attribute)) requested.push_back(attribute);

					Poco::FastMutex::ScopedLock lock(_directory.mutex);
					_directory.requestedAttributes = requested;
					for (const auto& entry: _directory.entries) response += searchEntry(messageID, entry);
					response += ldapResult(messageID, TAG_SEARCH_RESULT_DONE, _directory.searchResultCode);
				}
				else return;
				socket().sendBytes(response.data(), static_cast<int>(response.size()));
			}
		}

	private:
		Directory& _directory;
	};


	class ScriptedConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	{
	public:
		explicit ScriptedConnectionFactory(Directory& directory):
			_directory(directory)
		{
		}

		Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override
		{
			return new ScriptedConnection(socket, _directory);
		}

	private:
		Directory& _directory;
	};


	class ScriptedLDAPServer
		/// An LDAP server on 127.0.0.1 that answers as the given Directory says.
	{
	public:
		explicit ScriptedLDAPServer(Directory& directory):
			_server(new ScriptedConnectionFactory(directory),
				Poco::Net::ServerSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0)))
		{
			_server.start();
		}

		~ScriptedLDAPServer()
		{
			_server.stop();
		}

		ScriptedLDAPServer(const ScriptedLDAPServer&) = delete;
		ScriptedLDAPServer(ScriptedLDAPServer&&) = delete;
		ScriptedLDAPServer& operator = (const ScriptedLDAPServer&) = delete;
		ScriptedLDAPServer& operator = (ScriptedLDAPServer&&) = delete;

		std::string uri() const
		{
			return ldapURI(_server.port());
		}

	private:
		Poco::Net::TCPServer _server;
	};


	void bindClient(LDAPClient& client, const ScriptedLDAPServer& server)
	{
		client.setUserObjectClass("inetOrgPerson"s);
		client.setPermissionAttributeName("permission"s);
		client.setRoleAttributeName("memberOf"s);
		const Poco::Timespan timeout(10, 0);
		if (!client.initialize(server.uri()) || !client.setTimeout(timeout))
		{
			throw Poco::IOException("cannot initialize the LDAP client");
		}
		if (client.bindDirect("cn=service,dc=example,dc=com"s, "secret"s) != LDAPClient::Result::SUCCESS_RESULT)
		{
			throw Poco::IOException("cannot bind to the scripted LDAP server");
		}
	}
}


LDAPClientTest::LDAPClientTest(const std::string& name): CppUnit::TestCase(name)
{
}


LDAPClientTest::~LDAPClientTest() = default;


void LDAPClientTest::testEmptyOrNulPasswordIsNotSentToServer()
{
	Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
	Poco::Net::TCPServer server(new Poco::Net::TCPServerConnectionFactoryImpl<ClosingConnection>(), socket);
	server.start();

	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	assertTrue (client.initialize(ldapURI(server.port())));
	assertTrue (client.bindDirect("uid=victim,ou=people,dc=example,dc=com"s, ""s) ==
		LDAPClient::Result::WRONG_CREDENTIALS);
	assertTrue (client.bind("victim"s, ""s) == LDAPClient::Result::WRONG_CREDENTIALS);
	assertTrue (client.bindDirect("uid=victim,ou=people,dc=example,dc=com"s, "\0abc"s) ==
		LDAPClient::Result::WRONG_CREDENTIALS);
	assertTrue (client.bindDirect("uid=victim,ou=people,dc=example,dc=com"s, "abc\0def"s) ==
		LDAPClient::Result::WRONG_CREDENTIALS);
	assertTrue (client.bind("vic\0tim"s, "secret"s) == LDAPClient::Result::WRONG_CREDENTIALS);
	std::string userDN;
	assertFalse (client.findUserDN("vic\0tim"s, userDN));
	assertFalse (client.isBound());
	assertEqual (0, server.totalConnections());

	server.stop();
}


void LDAPClientTest::testUnansweredBindTimesOut()
{
	// The connection is queued by the listening socket and never answered.
	Poco::Net::ServerSocket silentServer(Poco::Net::SocketAddress("127.0.0.1"s, 0));

	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	assertTrue (client.initialize(ldapURI(silentServer.address().port())));
	assertTrue (client.setTimeout(Poco::Timespan(1, 0)));

	Poco::Stopwatch stopwatch;
	stopwatch.start();
	assertTrue (client.bindDirect("uid=alice,ou=people,dc=example,dc=com"s, "secret"s) ==
		LDAPClient::Result::SERVER_INACCESSIBLE);
	assertTrue (stopwatch.elapsedSeconds() < 10);
}


void LDAPClientTest::testBindResultsAreMapped()
{
	Directory directory;
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	assertTrue (client.initialize(server.uri()));

	const std::vector<std::pair<int, LDAPClient::Result>> results = {
		{RESULT_INVALID_CREDENTIALS, LDAPClient::Result::WRONG_CREDENTIALS},
		{RESULT_INAPPROPRIATE_AUTHENTICATION, LDAPClient::Result::WRONG_CREDENTIALS},
		{RESULT_INSUFFICIENT_ACCESS_RIGHTS, LDAPClient::Result::WRONG_CREDENTIALS},
		{RESULT_UNWILLING_TO_PERFORM, LDAPClient::Result::WRONG_CREDENTIALS},
		{RESULT_CONSTRAINT_VIOLATION, LDAPClient::Result::WRONG_CREDENTIALS},
		{RESULT_BUSY, LDAPClient::Result::SERVER_INACCESSIBLE},
		{RESULT_UNAVAILABLE, LDAPClient::Result::SERVER_INACCESSIBLE},
		{RESULT_TIME_LIMIT_EXCEEDED, LDAPClient::Result::SERVER_INACCESSIBLE},
		{RESULT_OTHER, LDAPClient::Result::ERROR_RESULT}};
	for (const auto& [code, result]: results)
	{
		{
			Poco::FastMutex::ScopedLock lock(directory.mutex);
			directory.bindResultCode = static_cast<unsigned>(code);
		}
		assertTrue (client.bindDirect("uid=alice,ou=people,dc=example,dc=com"s, "wrong"s) == result);
		assertFalse (client.isBound());
	}

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.bindResultCode = 0;
	}
	assertTrue (client.bindDirect("uid=alice,ou=people,dc=example,dc=com"s, "secret"s) ==
		LDAPClient::Result::SUCCESS_RESULT);
	assertTrue (client.isBound());

	Poco::FastMutex::ScopedLock lock(directory.mutex);
	assertEqual (results.size() + 1, directory.boundDNs.size());
}


void LDAPClientTest::testFindUserDNNeedsExactlyOneEntry()
{
	Directory directory;
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	client.setSearchBase("ou=people,dc=example,dc=com"s);
	bindClient(client, server);

	std::string userDN;
	assertFalse (client.findUserDN("alice"s, userDN));

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.entries = {{"uid=alice,ou=people,dc=example,dc=com"s, {}}};
	}
	assertTrue (client.findUserDN("alice"s, userDN));
	assertEqual ("uid=alice,ou=people,dc=example,dc=com"s, userDN);

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.entries.push_back({"uid=alice,ou=staff,dc=example,dc=com"s, {}});
	}
	try
	{
		[[maybe_unused]] const bool found = client.findUserDN("alice"s, userDN);
		fail("several entries must be reported as an error of the search");
	}
	catch (Poco::IOException&)
	{
	}
}


void LDAPClientTest::testFailedSearchIsReported()
{
	Directory directory;
	directory.searchResultCode = RESULT_SIZE_LIMIT_EXCEEDED;
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	bindClient(client, server);

	assertFalse (client.fetchOwnProperties("uid=alice,ou=people,dc=example,dc=com"s));
	std::string userDN;
	try
	{
		[[maybe_unused]] const bool found = client.findUserDN("alice"s, userDN);
		fail("a failed search for the user must throw");
	}
	catch (Poco::IOException&)
	{
	}
}


void LDAPClientTest::testAttributeValuesKeepNulCharacters()
{
	const std::string binaryValue("a\0b", 3);
	Directory directory;
	directory.entries = {{"uid=alice,ou=people,dc=example,dc=com"s,
		{{"displayName"s, {"Alice"s}}, {"photo"s, {binaryValue}}, {"permission"s, {"read"s, "write"s}}}}};
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	client.setExtraAttributesMap({{"photo"s, "photo"s}});
	bindClient(client, server);

	assertTrue (client.fetchOwnProperties("uid=alice,ou=people,dc=example,dc=com"s));
	assertEqual ("Alice"s, client.fullName());
	assertTrue (client.permissions() == LDAPClient::Permissions({"read"s, "write"s}));
	const auto itPhoto = client.attributes().find("photo"s);
	assertTrue (itPhoto != client.attributes().end());
	assertTrue (itPhoto->second == binaryValue);
}


void LDAPClientTest::testAtMostEightExtraAttributesAreRequested()
{
	Directory directory;
	directory.entries = {{"uid=alice,ou=people,dc=example,dc=com"s, {}}};
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	LDAPClient::AttributesNameMap extraAttributes;
	constexpr int CONFIGURED_COUNT = 10;
	for (int i = 0; i < CONFIGURED_COUNT; ++i)
	{
		extraAttributes.emplace_back("extra"s + std::to_string(i), "extra"s + std::to_string(i));
	}
	client.setExtraAttributesMap(extraAttributes);
	bindClient(client, server);

	assertTrue (client.fetchOwnProperties("uid=alice,ou=people,dc=example,dc=com"s));
	const std::vector<std::string> expected = {"displayName"s, "permission"s, "memberOf"s,
		"extra0"s, "extra1"s, "extra2"s, "extra3"s, "extra4"s, "extra5"s, "extra6"s, "extra7"s};
	Poco::FastMutex::ScopedLock lock(directory.mutex);
	assertTrue (directory.requestedAttributes == expected);
}


void LDAPClientTest::testAccountNameIsReadInAccountMode()
{
	const std::string aliceDN("uid=alice,ou=people,dc=example,dc=com");
	Directory directory;
	directory.entries = {{aliceDN, {{"sAMAccountName"s, {"Alice"s}}}}};
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	client.setSearchBase("ou=people,dc=example,dc=com"s);
	bindClient(client, server);
	const auto accountNameRequested = [&directory]()
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		const auto& requested = directory.requestedAttributes;
		return std::find(requested.begin(), requested.end(), "sAMAccountName"s) != requested.end();
	};

	assertTrue (client.fetchOwnProperties(aliceDN));
	assertTrue (client.accountName().empty());
	assertFalse (accountNameRequested());

	client.setMode(LDAPClient::Mode::ACCOUNT_NAME);
	std::string userDN;
	assertTrue (client.findUserDN("alice@example.com"s, userDN));
	assertEqual ("Alice"s, client.accountName());
	assertTrue (accountNameRequested());
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.entries = {{aliceDN, {}}};
	}
	assertTrue (client.findUserDN("alice@example.com"s, userDN));
	assertTrue (client.accountName().empty());

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.entries = {{aliceDN, {{"sAMAccountName"s, {"Alice"s}}}}};
	}
	assertTrue (client.fetchOwnProperties(aliceDN));
	assertEqual ("Alice"s, client.accountName());
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.searchResultCode = RESULT_SIZE_LIMIT_EXCEEDED;
	}
	assertFalse (client.fetchOwnProperties(aliceDN));
	assertTrue (client.accountName().empty());
}


void LDAPClientTest::testSeveralAccountNamesAreDirectoryError()
{
	const std::string aliceDN("uid=alice,ou=people,dc=example,dc=com");
	Directory directory;
	directory.entries = {{aliceDN, {{"sAMAccountName"s, {"alice"s, "bob"s}}}}};
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	client.setMode(LDAPClient::Mode::ACCOUNT_NAME);
	client.setSearchBase("ou=people,dc=example,dc=com"s);
	bindClient(client, server);

	std::string userDN;
	try
	{
		[[maybe_unused]] const bool found = client.findUserDN("alice"s, userDN);
		fail("an entry with several account names must be reported as a directory error");
	}
	catch (Poco::IOException&)
	{
	}
	try
	{
		[[maybe_unused]] const bool fetched = client.fetchOwnProperties(aliceDN);
		fail("an entry with several account names must be reported as a directory error");
	}
	catch (Poco::IOException&)
	{
	}
	assertTrue (client.accountName().empty());
}


void LDAPClientTest::testPrincipalNamesAreReadInAccountMode()
{
	const std::string aliceDN("uid=alice,ou=people,dc=example,dc=com");
	Directory directory;
	directory.entries = {{aliceDN, {
		{"sAMAccountName"s, {"alice"s}},
		{"userPrincipalName"s, {"alice@example.com"s}},
		{"msDS-PrincipalName"s, {"COMPANY\\alice"s}}}}};
	ScriptedLDAPServer server(directory);
	LDAPClient client(Poco::Logger::get("LDAPClientTest"s));
	client.setSearchBase("ou=people,dc=example,dc=com"s);
	bindClient(client, server);
	const auto principalNameRequested = [&directory]()
	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		const auto& requested = directory.requestedAttributes;
		return std::find(requested.begin(), requested.end(), "msDS-PrincipalName"s) != requested.end();
	};

	assertTrue (client.fetchOwnProperties(aliceDN));
	assertTrue (client.userPrincipalName().empty());
	assertTrue (client.principalName().empty());
	assertFalse (principalNameRequested());

	client.setMode(LDAPClient::Mode::ACCOUNT_NAME);
	std::string userDN;
	assertTrue (client.findUserDN("alice"s, userDN));
	assertEqual ("alice@example.com"s, client.userPrincipalName());
	assertEqual ("COMPANY\\alice"s, client.principalName());
	assertTrue (principalNameRequested());
	assertTrue (client.bind("alice@example.com"s, "secret"s) == LDAPClient::Result::SUCCESS_RESULT);
	assertTrue (client.fetchOwnProperties());
	assertEqual ("alice"s, client.accountName());
	assertEqual ("alice@example.com"s, client.userPrincipalName());
	assertEqual ("COMPANY\\alice"s, client.principalName());

	{
		Poco::FastMutex::ScopedLock lock(directory.mutex);
		directory.entries = {{aliceDN, {
			{"userPrincipalName"s, {"alice@example.com"s, "alice@other.example.com"s}},
			{"msDS-PrincipalName"s, {"COMPANY\\alice"s, "OTHER\\alice"s}}}}};
	}
	assertTrue (client.fetchOwnProperties(aliceDN));
	assertTrue (client.userPrincipalName().empty());
	assertTrue (client.principalName().empty());
}


void LDAPClientTest::setUp()
{
}


void LDAPClientTest::tearDown()
{
}


CppUnit::Test* LDAPClientTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LDAPClientTest");

	CppUnit_addTest(pSuite, LDAPClientTest, testEmptyOrNulPasswordIsNotSentToServer);
	CppUnit_addTest(pSuite, LDAPClientTest, testUnansweredBindTimesOut);
	CppUnit_addTest(pSuite, LDAPClientTest, testBindResultsAreMapped);
	CppUnit_addTest(pSuite, LDAPClientTest, testFindUserDNNeedsExactlyOneEntry);
	CppUnit_addTest(pSuite, LDAPClientTest, testFailedSearchIsReported);
	CppUnit_addTest(pSuite, LDAPClientTest, testAttributeValuesKeepNulCharacters);
	CppUnit_addTest(pSuite, LDAPClientTest, testAtMostEightExtraAttributesAreRequested);
	CppUnit_addTest(pSuite, LDAPClientTest, testAccountNameIsReadInAccountMode);
	CppUnit_addTest(pSuite, LDAPClientTest, testSeveralAccountNamesAreDirectoryError);
	CppUnit_addTest(pSuite, LDAPClientTest, testPrincipalNamesAreReadInAccountMode);

	return pSuite;
}


#endif // POCO_OSP_AUTH_ENABLE_LDAP
