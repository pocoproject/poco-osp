//
// WebEventServiceTest.cpp
//
// Copyright (c) 2025, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebEventServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/Delegate.h"
#include "Poco/SharedPtr.h"
#include "Poco/AutoPtr.h"
#include "Poco/RefCountedObject.h"
#include "Poco/Exception.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Timespan.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Format.h"
#include "Poco/Stopwatch.h"
#include "Poco/Clock.h"
#include "Poco/Thread.h"
#include <string>
#include <thread>
#include <utility>
#include <vector>


using Poco::delegate;
using Poco::OSP::WebEvent::WebEventService;
using Poco::OSP::WebEvent::WebEventServiceImpl;
using Poco::OSP::WebEvent::WebEventTaskNotification;


namespace
{

struct SignalState: public Poco::RefCountedObject
	/// What a queued notification reports back through. Reference counted
	/// and held by the notification as well as by the waiter, because a
	/// wait that expires returns while the notification is still queued:
	/// an event on the waiter's stack would be gone by the time the
	/// notification ran.
{
	using Ptr = Poco::AutoPtr<SignalState>;

	Poco::Event ready;
};


class CorruptSocketNotification: public WebEventTaskNotification
{
public:
	CorruptSocketNotification(WebEventServiceImpl& service,
		Poco::Net::Socket::SocketList& socketList, SignalState::Ptr pState):
		WebEventTaskNotification(service),
		_socketList(socketList),
		_pState(pState)
	{
	}

	void execute()
	{
		if (!_socketList.empty())
			_socketList.front().impl()->close();
		_pState->ready.set();
	}

private:
	Poco::Net::Socket::SocketList& _socketList;
	SignalState::Ptr _pState;
};


class SignalNotification: public WebEventTaskNotification
{
public:
	SignalNotification(WebEventServiceImpl& service, SignalState::Ptr pState):
		WebEventTaskNotification(service),
		_pState(pState)
	{
	}

	void execute()
	{
		_pState->ready.set();
	}

private:
	SignalState::Ptr _pState;
};


struct BlockState: public Poco::RefCountedObject
	/// The events a parked thread reports through, shared with whoever
	/// parked it. Reference counted for the same reason as SignalState:
	/// a block that is given up on, or never picked up at all, leaves its
	/// notification queued.
{
	using Ptr = Poco::AutoPtr<BlockState>;

	Poco::Event started;
	Poco::Event proceed;
		/// Named away from release(), which every reference counted object
		/// already has.
	Poco::Event finished;
};


class BlockerNotification: public WebEventTaskNotification
	/// Parks the executing thread until released, and reports when it
	/// has left; used to make queues fill deterministically.
{
public:
	BlockerNotification(WebEventServiceImpl& service, BlockState::Ptr pState):
		WebEventTaskNotification(service),
		_pState(pState)
	{
	}

	void execute()
	{
		_pState->started.set();
		_pState->proceed.wait();
		_pState->finished.set();
	}

private:
	BlockState::Ptr _pState;
};


class TestableWebEventServiceImpl: public WebEventServiceImpl
{
public:
	using WebEventServiceImpl::WebEventServiceImpl;
	using WebEventServiceImpl::addSubscriber;
	using WebEventServiceImpl::Subscriber;
	using WebEventServiceImpl::shutdown;
	using WebEventServiceImpl::workerQueue;
	using WebEventServiceImpl::mainQueue;
	using WebEventServiceImpl::socketMap;

	struct SubscriberStats
	{
		std::size_t pending = 0;
		Poco::UInt64 dropped = 0;
		Poco::UInt64 enqueued = 0;
		Poco::Clock lastActive;
		bool found = false;
	};

	bool corruptFirstSocket(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		mainQueue().enqueueNotification(
			new CorruptSocketNotification(*this, socketList(), pState));
		return pState->ready.tryWait(timeoutMs);
	}

	bool waitForMainThread(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		mainQueue().enqueueNotification(new SignalNotification(*this, pState));
		return pState->ready.tryWait(timeoutMs);
	}

	bool waitForWorkerThread(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		workerQueue().enqueueNotification(new SignalNotification(*this, pState));
		return pState->ready.tryWait(timeoutMs);
	}

	~TestableWebEventServiceImpl()
	{
		if (_pDestroyed) _pDestroyed->set();
	}

	void setDestroyedEvent(Poco::Event* pEvent)
		/// The event set by the destructor, if any.
	{
		_pDestroyed = pEvent;
	}

	SubscriberStats inspectFirstSubscriber()
		/// Must run on the main thread (socket map access).
	{
		SubscriberStats stats;
		if (!socketMap().empty())
		{
			Subscriber::Ptr pSubscriber = socketMap().begin()->second;
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			stats.pending = pSubscriber->pending.size();
			stats.dropped = pSubscriber->droppedFrames;
			stats.enqueued = pSubscriber->enqueuedFrames;
			stats.lastActive = pSubscriber->lastActive;
			stats.found = true;
		}
		return stats;
	}

	bool firstSubscriberStats(SubscriberStats& stats, long timeoutMs = 5000);
	bool firstSubscriber(Subscriber::Ptr& pSubscriber, long timeoutMs = 5000);

private:
	Poco::Event* _pDestroyed = nullptr;
};


struct StatsState: public SignalState
	/// Carries the statistics back as well, so that a wait which expires
	/// leaves nothing of the caller's behind for the notification to fill
	/// in later.
{
	using Ptr = Poco::AutoPtr<StatsState>;

	TestableWebEventServiceImpl::SubscriberStats stats;
};


class InspectSubscriberNotification: public WebEventTaskNotification
{
public:
	InspectSubscriberNotification(TestableWebEventServiceImpl& service, StatsState::Ptr pState):
		WebEventTaskNotification(service),
		_testable(service),
		_pState(pState)
	{
	}

	void execute()
	{
		_pState->stats = _testable.inspectFirstSubscriber();
		_pState->ready.set();
	}

private:
	TestableWebEventServiceImpl& _testable;
	StatsState::Ptr _pState;
};


bool TestableWebEventServiceImpl::firstSubscriberStats(SubscriberStats& stats, long timeoutMs)
{
	StatsState::Ptr pState = new StatsState;
	mainQueue().enqueueNotification(new InspectSubscriberNotification(*this, pState));
	if (!pState->ready.tryWait(timeoutMs)) return false;
	stats = pState->stats;
	return true;
}


struct SubscriberState: public SignalState
	/// Carries the subscriber back, for the same reason as StatsState.
{
	using Ptr = Poco::AutoPtr<SubscriberState>;

	TestableWebEventServiceImpl::Subscriber::Ptr pSubscriber;
};


class FirstSubscriberNotification: public WebEventTaskNotification
{
public:
	FirstSubscriberNotification(TestableWebEventServiceImpl& service, SubscriberState::Ptr pState):
		WebEventTaskNotification(service),
		_testable(service),
		_pState(pState)
	{
	}

	void execute()
	{
		if (!_testable.socketMap().empty())
			_pState->pSubscriber = _testable.socketMap().begin()->second;
		_pState->ready.set();
	}

private:
	TestableWebEventServiceImpl& _testable;
	SubscriberState::Ptr _pState;
};


bool TestableWebEventServiceImpl::firstSubscriber(Subscriber::Ptr& pSubscriber, long timeoutMs)
	/// Fetches the first subscriber on the main thread (socket map access).
{
	SubscriberState::Ptr pState = new SubscriberState;
	mainQueue().enqueueNotification(new FirstSubscriberNotification(*this, pState));
	if (!pState->ready.tryWait(timeoutMs)) return false;
	pSubscriber = pState->pSubscriber;
	return true;
}


TestableWebEventServiceImpl& testableService(Poco::AutoPtr<WebEventServiceImpl>& pService)
	/// A service as its test double: a borrowed reference, so the owner
	/// keeps the only owning one and the teardown count stays exact.
{
	return dynamic_cast<TestableWebEventServiceImpl&>(*pService);
}


template <typename Predicate>
bool waitUntil(Predicate pred, long timeoutMs)
	/// Polls pred until it holds or the deadline passes.
{
	Poco::Stopwatch sw;
	sw.start();
	while (!pred())
	{
		if (sw.elapsed() / 1000 >= timeoutMs) return false;
		Poco::Thread::sleep(10);
	}
	return true;
}


class ThreadBlock
	/// Parks a worker or the main thread of a service until released.
	/// The destructor releases the thread and waits until it has left the
	/// block, so the events never die under a parked thread, not even
	/// when an assertion throws. Declare after the service it parks.
{
public:
	enum Which
	{
		WORKER,
		MAIN
	};

	explicit ThreadBlock(TestableWebEventServiceImpl& service, Which which = WORKER, long timeoutMs = 5000):
		_pState(new BlockState)
	{
		Poco::NotificationQueue& queue = which == MAIN ? service.mainQueue() : service.workerQueue();
		queue.enqueueNotification(new BlockerNotification(service, _pState));
		_parked = _pState->started.tryWait(timeoutMs);
	}

	~ThreadBlock()
	{
		release();
	}

	bool parked() const
		/// Whether a thread picked the block up in time.
	{
		return _parked;
	}

	void release()
		/// Releases the parked thread and waits until it has left the block.
		/// The wait is bounded, and the events outlive this object, so a
		/// thread that stays parked cannot be left holding dead storage.
	{
		if (_released) return;
		_released = true;
		_pState->proceed.set();
		if (_parked) _pState->finished.tryWait(5000);
	}

private:
	BlockState::Ptr _pState;
	bool _parked = false;
	bool _released = false;
};


class WebEventTestRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
	explicit WebEventTestRequestHandler(Poco::AutoPtr<TestableWebEventServiceImpl> pService):
		_pService(std::move(pService))
	{
	}

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
	{
		try
		{
			if (request.getURI() != "/webevent")
			{
				response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
				response.setContentLength(0);
				response.send();
				return;
			}

			const Poco::Net::HTTPServerRequest* pRequest = &request;
			_pService->requestReceived(pRequest);
			if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
			{
				response.setContentLength(0);
				response.send();
				return;
			}

			Poco::SharedPtr<Poco::Net::WebSocket> pWS = new Poco::Net::WebSocket(request, response);
			// Identity headers stand in for the production user resolver and
			// page query parameter (see WebEventRequestHandler).
			_pService->addSubscriber(pWS,
				request.get("X-Test-User", ""),
				request.get("X-Test-Page", ""),
				request.clientAddress().toString());
		}
		catch (Poco::Exception& exc)
		{
			response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
			response.setReason(exc.displayText());
			response.setContentLength(0);
			response.send();
		}
	}

private:
	Poco::AutoPtr<TestableWebEventServiceImpl> _pService;
};


class WebEventTestRequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
{
public:
	explicit WebEventTestRequestHandlerFactory(Poco::AutoPtr<TestableWebEventServiceImpl> pService):
		_pService(std::move(pService))
	{
	}

	Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override
	{
		if (request.getURI() == "/webevent")
		{
			return new WebEventTestRequestHandler(_pService);
		}

		class NotFoundHandler: public Poco::Net::HTTPRequestHandler
		{
		public:
			void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response) override
			{
				response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
				response.setContentLength(0);
				response.send();
			}
		};

		return new NotFoundHandler;
	}

private:
	Poco::AutoPtr<TestableWebEventServiceImpl> _pService;
};


struct LocalService
	/// A dedicated service + HTTP server for tests needing a non-default
	/// worker count or per-subscriber queue limit.
{
	LocalService(int workerCount, int maxPendingFrames)
	{
		pService = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), 0, workerCount, maxPendingFrames);
		Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
		port = serverSocket.address().port();
		pServer = std::make_unique<Poco::Net::HTTPServer>(
			new WebEventTestRequestHandlerFactory(pService), pool, serverSocket, new Poco::Net::HTTPServerParams);
		pServer->start();
	}

	~LocalService()
	{
		stop();
	}

	int stop()
		/// Stops the service, then the server and its threads, and returns
		/// the reference count the service is left with before the last
		/// reference is dropped. It must be 1: anything more is a reference
		/// still held by a server or service thread. Idempotent.
	{
		if (pService)
		{
			pService->stop();
			pServer->stopAll(true);
			pServer->stop();
			pool.joinAll();
			pServer.reset();
			refCount = pService->referenceCount();
			pService = nullptr;
		}
		return refCount;
	}

	Poco::ThreadPool pool;
	Poco::AutoPtr<TestableWebEventServiceImpl> pService;
	std::unique_ptr<Poco::Net::HTTPServer> pServer;
	Poco::UInt16 port = 0;
	int refCount = 0;
};


struct SubscriptionWaiter
{
	void onSubscription(const std::string&)
	{
		event.set();
	}

	Poco::Event event;
};


struct SlowNotificationHandler
	/// Makes each notification take long enough that a backlog queued in one
	/// go keeps the event thread busy across several keepalive sweeps.
{
	void onNotification(const WebEventService::NotificationEvent&)
	{
		Poco::Thread::sleep(1);
	}
};


Poco::Net::WebSocket connectWebSocket(Poco::UInt16 port, const std::string& username = std::string())
{
	Poco::Net::HTTPClientSession session("127.0.0.1", port);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	if (!username.empty()) request.set("X-Test-User", username);
	Poco::Net::HTTPResponse response;
	return Poco::Net::WebSocket(session, request, response);
}


void sendSubscribe(Poco::Net::WebSocket& ws, const std::string& subject)
{
	std::string frame("SUBSCRIBE ");
	frame += subject;
	frame += " WebEvent/1.0\r\n";
	ws.sendFrame(frame.data(), static_cast<int>(frame.size()), Poco::Net::WebSocket::FRAME_TEXT);
}


WebEventService::NotificationEvent parseNotificationFrame(const std::string& frame)
{
	const std::string headerPrefix("NOTIFY ");
	const std::string versionSuffix(" WebEvent/1.0");
	auto headerEnd = frame.find("\r\n");
	std::string header = headerEnd == std::string::npos ? frame : frame.substr(0, headerEnd);
	std::string body = headerEnd == std::string::npos ? "" : frame.substr(headerEnd + 2);

	if (header.compare(0, headerPrefix.size(), headerPrefix) != 0)
		throw Poco::InvalidArgumentException("Unexpected notification header: " + header);

	auto subjectEnd = header.rfind(versionSuffix);
	if (subjectEnd == std::string::npos || subjectEnd <= headerPrefix.size())
		throw Poco::InvalidArgumentException("Malformed notification header: " + header);

	std::string subject = header.substr(headerPrefix.size(), subjectEnd - headerPrefix.size());
	return WebEventService::NotificationEvent(subject, body);
}


Poco::Net::StreamSocket connectRawWebSocket(Poco::UInt16 port)
	/// Performs the WebSocket handshake by hand and returns the bare
	/// stream socket, so a test can write a frame in pieces.
{
	Poco::Net::HTTPClientSession session("127.0.0.1", port);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	request.set("Connection", "Upgrade");
	request.set("Upgrade", "websocket");
	request.set("Sec-WebSocket-Version", "13");
	request.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
	session.setKeepAlive(true);
	session.sendRequest(request);
	Poco::Net::HTTPResponse response;
	session.receiveResponse(response);
	if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS)
		throw Poco::IOException("WebSocket handshake failed: " + response.getReason());
	return session.detachSocket();
}


std::string maskedTextFrame(const std::string& payload)
	/// A complete client-to-server text frame (FIN, masked) for a short payload.
{
	poco_assert (payload.size() < 126);
	const char mask[4] = {0x12, 0x34, 0x56, 0x78};
	std::string frame;
	frame += static_cast<char>(0x81);
	frame += static_cast<char>(0x80 | payload.size());
	frame.append(mask, 4);
	for (std::size_t i = 0; i < payload.size(); ++i)
	{
		frame += static_cast<char>(payload[i] ^ mask[i % 4]);
	}
	return frame;
}


bool receivesCloseFrame(Poco::Net::StreamSocket& raw)
	/// Reads the first frame the server sends and tells whether it is a
	/// CLOSE. The two header bytes need not arrive together, so it reads
	/// until both are in or the connection ends.
{
	char buffer[64];
	int n = 0;
	while (n < 2)
	{
		int rc = raw.receiveBytes(buffer + n, static_cast<int>(sizeof(buffer)) - n);
		if (rc <= 0) break;
		n += rc;
	}
	return n >= 2 && (buffer[0] & 0x0f) == 0x08;
}


int unreadBytes(Poco::Net::WebSocket& ws)
	/// Bytes still in the kernel's receive queue of the socket, leaving
	/// out what the WebSocket has already pulled into its own buffer.
{
	return ws.impl()->Poco::Net::SocketImpl::available();
}


class FrameRest
	/// The tail of a frame sent in pieces. Sent on request, or by the
	/// destructor if the test unwinds first: a server read stalled on a
	/// half-sent header must be completed before the client goes away.
{
public:
	FrameRest(Poco::Net::StreamSocket& socket, const std::string& bytes):
		_socket(socket),
		_bytes(bytes)
	{
	}

	~FrameRest()
	{
		try
		{
			send();
		}
		catch (...)
		{
		}
	}

	void send()
	{
		if (_sent) return;
		_sent = true;
		_socket.sendBytes(_bytes.data(), static_cast<int>(_bytes.size()));
	}

private:
	Poco::Net::StreamSocket& _socket;
	std::string _bytes;
	bool _sent = false;
};

} // namespace


WebEventServiceTest::WebEventServiceTest(const std::string& name):
	CppUnit::TestCase(name),
	_port(0),
	_notificationHandlerAdded(false),
	_childHandlerAdded(false),
	_parentHandlerAdded(false),
	_subscriptionHandlerAdded(false),
	_siblingHandlerAdded(false)
{
}


WebEventServiceTest::~WebEventServiceTest() = default;


void WebEventServiceTest::setUp()
{
	_notificationEvent.reset();
	_childEvent.reset();
	_parentEvent.reset();
	_siblingEvent.reset();
	_childSubscriptionEvent.reset();
	_parentSubscriptionEvent.reset();
	_siblingSubscriptionEvent.reset();
	_recoverySubscriptionEvent1.reset();
	_recoverySubscriptionEvent2.reset();
	_recoverySubscriptionCount = 0;
	_notificationCount = 0;
	_lastNotification = WebEventService::NotificationEvent();
	_lastChildNotification = WebEventService::NotificationEvent();
	_lastParentNotification = WebEventService::NotificationEvent();
	_lastSiblingNotification = WebEventService::NotificationEvent();

	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), 0, 1);
	_pService = pImpl;

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	_port = serverSocket.address().port();

	Poco::Net::HTTPRequestHandlerFactory::Ptr pFactory = new WebEventTestRequestHandlerFactory(pImpl);
	Poco::Net::HTTPServerParams::Ptr pParams = new Poco::Net::HTTPServerParams;
	_pServer = std::make_unique<Poco::Net::HTTPServer>(pFactory, _pool, serverSocket, pParams);
	_pServer->start();

	_notificationHandlerAdded = false;
	_childHandlerAdded = false;
	_parentHandlerAdded = false;
	_subscriptionHandlerAdded = false;
	_siblingHandlerAdded = false;
}


void WebEventServiceTest::tearDown()
{
	if (_pService)
	{
		if (_notificationHandlerAdded)
			_pService->notificationSent -= delegate(this, &WebEventServiceTest::onNotificationSent);
		if (_childHandlerAdded)
			_pService->subjectNotified("com.example.parent.child.leaf") -= delegate(this, &WebEventServiceTest::onChildNotified);
		_childHandlerAdded = false;
		if (_parentHandlerAdded)
			_pService->subjectNotified("com.example.parent") -= delegate(this, &WebEventServiceTest::onParentNotified);
		_parentHandlerAdded = false;
		if (_subscriptionHandlerAdded)
			_pService->subscriptionRequestReceived -= delegate(this, &WebEventServiceTest::onSubscriptionReceived);
		_subscriptionHandlerAdded = false;
		if (_siblingHandlerAdded)
			_pService->subjectNotified("com.example.parent.sibling") -= delegate(this, &WebEventServiceTest::onSiblingNotified);
		_siblingHandlerAdded = false;
	}
	stopService(false);
}


void WebEventServiceTest::stopService(bool check)
	/// Stops the service, then the server and its threads, in that order.
	/// With check, the service must be left with exactly one reference:
	/// anything more is a reference still held by a server or service
	/// thread. Every test ends with the check; tearDown repeats the
	/// sequence without it, for tests that failed before their end.
{
	if (!_pService) return;
	_pService->stop();
	if (_pServer)
	{
		_pServer->stopAll(true);
		_pServer->stop();
		_pool.joinAll();
		_pServer.reset();
	}
	int refCount = _pService->referenceCount();
	if (check)
	{
		assertEqual(1, refCount);
	}
	_pService = nullptr;
}


void WebEventServiceTest::testNotifyDispatchesToSubjectHierarchy()
{
	const std::string subject("com.example.parent.child.leaf");
	const std::string parentSubject("com.example.parent");
	const std::string payload("{\"value\":42}");

	_pService->notificationSent += delegate(this, &WebEventServiceTest::onNotificationSent);
	_notificationHandlerAdded = true;

	auto& childEvent = _pService->subjectNotified(subject);
	childEvent += delegate(this, &WebEventServiceTest::onChildNotified);
	_childHandlerAdded = true;

	auto& parentEvent = _pService->subjectNotified(parentSubject);
	parentEvent += delegate(this, &WebEventServiceTest::onParentNotified);
	_parentHandlerAdded = true;

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::HTTPClientSession childSession("127.0.0.1", _port);
	Poco::Net::HTTPRequest childRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse childResponse;
	Poco::Net::WebSocket childSocket(childSession, childRequest, childResponse);
	childSocket.setReceiveTimeout(Poco::Timespan(5, 0));

	std::string childSubscribe("SUBSCRIBE ");
	childSubscribe += subject;
	childSubscribe += " WebEvent/1.0\r\n";
	childSocket.sendFrame(childSubscribe.data(), static_cast<int>(childSubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	Poco::Net::HTTPClientSession parentSession("127.0.0.1", _port);
	Poco::Net::HTTPRequest parentRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse parentResponse;
	Poco::Net::WebSocket parentSocket(parentSession, parentRequest, parentResponse);

	std::string parentSubscribe("SUBSCRIBE ");
	parentSubscribe += parentSubject;
	parentSubscribe += " WebEvent/1.0\r\n";
	parentSocket.sendFrame(parentSubscribe.data(), static_cast<int>(parentSubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_parentSubscriptionEvent.tryWait(2000));

	_pService->notify(subject, payload);

	auto readNotification = [this](Poco::Net::WebSocket& ws)
	{
		std::vector<char> buffer(2048);
		int flags = 0;
		int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
		assertTrue(received > 0);
		std::string frame(buffer.data(), received);
		return parseNotificationFrame(frame);
	};

	WebEventService::NotificationEvent childNotification = readNotification(childSocket);
	WebEventService::NotificationEvent parentNotification = readNotification(parentSocket);

	assertTrue(_notificationEvent.tryWait(2000));
	assertTrue(_childEvent.tryWait(2000));
	assertTrue(_parentEvent.tryWait(2000));

	assertEquals(subject, childNotification.first);
	assertEquals(payload, childNotification.second);
	assertEquals(subject, parentNotification.first);
	assertEquals(payload, parentNotification.second);
	assertEquals(subject, _lastNotification.first);
	assertEquals(payload, _lastNotification.second);
	assertEquals(subject, _lastChildNotification.first);
	assertEquals(payload, _lastChildNotification.second);
	assertEquals(subject, _lastParentNotification.first);
	assertEquals(payload, _lastParentNotification.second);

	try
	{
		childSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		childSocket.close();
	}
	catch (...) {}

	try
	{
		parentSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		parentSocket.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testSubjectNotifiedReturnsSameEvent()
{
	auto& event1 = _pService->subjectNotified("sample.subject");
	auto& event2 = _pService->subjectNotified("sample.subject");
	assertTrue(&event1 == &event2);
	stopService();
}


void WebEventServiceTest::testNotifyNoDispatch()
{
	const std::string subject("com.example.parent.child.leaf");
	const std::string unrelatedSubject("com.example.parent.sibling");
	const std::string payload("{\"value\":13}");

	auto& childEvent = _pService->subjectNotified(subject);
	childEvent += delegate(this, &WebEventServiceTest::onChildNotified);
	_childHandlerAdded = true;

	auto& siblingEvent = _pService->subjectNotified(unrelatedSubject);
	siblingEvent += delegate(this, &WebEventServiceTest::onSiblingNotified);
	_siblingHandlerAdded = true;

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::HTTPClientSession childSession("127.0.0.1", _port);
	Poco::Net::HTTPRequest childRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse childResponse;
	Poco::Net::WebSocket childSocket(childSession, childRequest, childResponse);

	std::string childSubscribe("SUBSCRIBE ");
	childSubscribe += subject;
	childSubscribe += " WebEvent/1.0\r\n";
	childSocket.sendFrame(childSubscribe.data(), static_cast<int>(childSubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	Poco::Net::HTTPClientSession siblingSession("127.0.0.1", _port);
	Poco::Net::HTTPRequest siblingRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse siblingResponse;
	Poco::Net::WebSocket siblingSocket(siblingSession, siblingRequest, siblingResponse);
	siblingSocket.setReceiveTimeout(Poco::Timespan(0, 500000));

	std::string siblingSubscribe("SUBSCRIBE ");
	siblingSubscribe += unrelatedSubject;
	siblingSubscribe += " WebEvent/1.0\r\n";
	siblingSocket.sendFrame(siblingSubscribe.data(), static_cast<int>(siblingSubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_siblingSubscriptionEvent.tryWait(2000));

	_pService->notify(subject, payload);

	std::vector<char> buffer(2048);
	int flags = 0;
	int received = childSocket.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	std::string frame(buffer.data(), received);
	WebEventService::NotificationEvent childNotification = parseNotificationFrame(frame);

	assertTrue(_childEvent.tryWait(2000));
	assertEquals(subject, childNotification.first);
	assertEquals(payload, childNotification.second);
	assertFalse(_siblingEvent.tryWait(500));
	Poco::Timespan timeout(500000); // 0.5 seconds
	assertFalse(siblingSocket.poll(timeout, Poco::Net::Socket::SELECT_READ));

	try
	{
		childSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		childSocket.close();
	}
	catch (...) {}

	try
	{
		siblingSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		siblingSocket.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::onNotificationSent(const WebEventService::NotificationEvent& ev)
{
	_lastNotification = ev;
	++_notificationCount;
	_notificationEvent.set();
}


void WebEventServiceTest::onChildNotified(const WebEventService::NotificationEvent& ev)
{
	_lastChildNotification = ev;
	_childEvent.set();
}


void WebEventServiceTest::onParentNotified(const WebEventService::NotificationEvent& ev)
{
	_lastParentNotification = ev;
	_parentEvent.set();
}


void WebEventServiceTest::onSubscriptionReceived(const std::string& subject)
{
	if (subject == "com.example.parent.child.leaf")
		_childSubscriptionEvent.set();
	if (subject == "com.example.parent")
		_parentSubscriptionEvent.set();
	if (subject == "com.example.parent.sibling")
		_siblingSubscriptionEvent.set();
	if (subject == "com.example.badsocket")
	{
		++_recoverySubscriptionCount;
		if (_recoverySubscriptionCount == 1)
			_recoverySubscriptionEvent1.set();
		else
			_recoverySubscriptionEvent2.set();
	}
}


void WebEventServiceTest::onSiblingNotified(const WebEventService::NotificationEvent& ev)
{
	_lastSiblingNotification = ev;
	_siblingEvent.set();
}


void WebEventServiceTest::testBadSocketDescriptorRecovery()
{
	const std::string subject("com.example.badsocket");
	const std::string payload("{\"recovered\":true}");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	// Connect first client and subscribe
	Poco::Net::HTTPClientSession staleSession("127.0.0.1", _port);
	Poco::Net::HTTPRequest staleRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse staleResponse;
	Poco::Net::WebSocket staleSocket(staleSession, staleRequest, staleResponse);

	std::string staleSubscribe("SUBSCRIBE ");
	staleSubscribe += subject;
	staleSubscribe += " WebEvent/1.0\r\n";
	staleSocket.sendFrame(staleSubscribe.data(), static_cast<int>(staleSubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_recoverySubscriptionEvent1.tryWait(2000));

	// Force-close the raw server-side file descriptor behind Poco's back.
	// This makes the fd invalid while _socketList still holds a reference,
	// which is the exact condition causing select() to throw EBADF and
	// spin infinitely in production.
	TestableWebEventServiceImpl& testable = testableService(_pService);
	assertEqual(1, _pService->subscriberCount(subject));
	assertTrue(testable.corruptFirstSocket());

	// Also close the client side cleanly.
	try { staleSocket.close(); } catch (...) {}

	// The realistic recovery path: select() throws because the
	// socket's fd is now POCO_INVALID_SOCKET. The catch block calls
	// removeBadSocketsImpl() which detects and removes the bad socket.
	// We verify recovery by connecting a healthy client - its
	// subscription event is the deterministic sync point (it can only
	// fire after the main thread has recovered and processed the new
	// subscriber's notifications).
	Poco::Net::HTTPClientSession healthySession("127.0.0.1", _port);
	Poco::Net::HTTPRequest healthyRequest(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPResponse healthyResponse;
	Poco::Net::WebSocket healthySocket(healthySession, healthyRequest, healthyResponse);
	healthySocket.setReceiveTimeout(Poco::Timespan(5, 0));

	std::string healthySubscribe("SUBSCRIBE ");
	healthySubscribe += subject;
	healthySubscribe += " WebEvent/1.0\r\n";
	healthySocket.sendFrame(healthySubscribe.data(), static_cast<int>(healthySubscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	assertTrue(_recoverySubscriptionEvent2.tryWait(5000));

	// Stale subscriber removed, healthy subscriber added.
	// The healthy subscription and the bad-fd detection/removal are
	// triggered by separate select() iterations, so ordering between
	// "event 2 fires" and "stale removed" is not strictly defined on
	// all platforms. Poll with a bounded timeout instead of asserting
	// the exact moment.
	{
		Poco::Stopwatch sw;
		sw.start();
		while (_pService->subscriberCount(subject) != 1 && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(1, _pService->subscriberCount(subject));

	// Wait for the worker to finish shutdownImpl() on the dead socket
	// (enqueued by removeSubscriberImpl during recovery).
	assertTrue(testable.waitForWorkerThread());

	// Notify - the healthy client must still receive it
	_pService->notify(subject, payload);

	std::vector<char> buffer(2048);
	int flags = 0;
	int received = healthySocket.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	std::string frame(buffer.data(), received);
	WebEventService::NotificationEvent notification = parseNotificationFrame(frame);

	assertEquals(subject, notification.first);
	assertEquals(payload, notification.second);

	try
	{
		healthySocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		healthySocket.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testOverlappingSubscriptionsEnqueueOneFrame()
	/// A client subscribed to both a subject and one of its ancestors is
	/// visited twice by the hierarchy walk, and must still be sent ONE
	/// frame per notification.
	///
	/// Counted at the queue's door rather than at the socket, because the
	/// queue coalesces by subject and would hide a second enqueue whenever
	/// the first frame is still pending. It is NOT still pending when a
	/// worker sends it before the walk reaches the ancestor - the first
	/// enqueue wakes that worker - and then the same frame goes out twice.
	/// A test that reads the socket only sees that on the unlucky
	/// interleaving; this one sees it every time.
{
	// The subjects the suite's subscription handler knows: it signals the
	// waits below by exact name, so a test of its own invents nothing.
	const std::string parentSubject("com.example.parent");
	const std::string subject("com.example.parent.child.leaf");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	TestableWebEventServiceImpl& testable = testableService(_pService);

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));
	sendSubscribe(ws, parentSubject);
	assertTrue(_parentSubscriptionEvent.tryWait(2000));

	// Parked so nothing is drained: the count is of what the notification
	// put in, whatever the workers would have done with it.
	ThreadBlock block(testable);
	assertTrue(block.parked());

	_pService->notify(subject, "v1");
	assertTrue(testable.waitForMainThread());

	TestableWebEventServiceImpl::SubscriberStats stats;
	assertTrue(testable.firstSubscriberStats(stats));
	assertTrue(stats.found);
	assertEqual(1, static_cast<int>(stats.enqueued));
	assertEqual(1, static_cast<int>(stats.pending));

	block.release();

	std::vector<char> buffer(2048);
	int flags = 0;
	int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	WebEventService::NotificationEvent notification = parseNotificationFrame(std::string(buffer.data(), received));
	assertEquals(subject, notification.first);
	assertEquals(std::string("v1"), notification.second);
	// And nothing behind it.
	assertFalse(ws.poll(Poco::Timespan(500000), Poco::Net::Socket::SELECT_READ));

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testResetSocketIsDispatchedOnce()
	/// A socket that select() reports readable and in error at once must
	/// reach one worker only. Handing it to two, one to read and one to
	/// shut down, has both reading the same socket.
{
	const std::string subject("com.example.parent.child.leaf");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	TestableWebEventServiceImpl& testable = testableService(_pService);

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));
	// the socket is watched again once the subscribe has been processed
	assertTrue(testable.waitForMainThread());

	// Park the worker so whatever the main thread hands it stays queued.
	ThreadBlock block(testable);
	assertTrue(block.parked());

	// A reset makes the socket readable and in error at the same time.
	ws.setLinger(true, 0);
	ws.close();

	assertTrue(waitUntil([&]{ return testable.workerQueue().size() > 0; }, 5000));
	assertTrue(testable.waitForMainThread());
	assertEqual(1, testable.workerQueue().size());

	block.release();
	assertTrue(testable.waitForWorkerThread(15000));
	// The subscriber leaves the map on the main thread, and by which of
	// two routes depends on how the platform reports a reset: at once,
	// when select put the socket in the error list, or on the removal that
	// a failed receive posts back from the worker.
	assertTrue(waitUntil([&]{ return _pService->subscriberCount(subject) == 0; }, 15000));
	stopService();
}


void WebEventServiceTest::testReceiveAndShutdownAreSerialized()
	/// A worker reading a socket and a worker shutting the same socket
	/// down must take turns: the confirmation read of the shutdown and a
	/// receive in flight otherwise share the socket's receive state.
	///
	/// The overlap is forced: the shutdown is queued first and stalls in
	/// its confirmation read on a half-sent frame, then the receive of
	/// that same frame is released on the other worker. Nothing asserts
	/// the race itself; a thread sanitizer build reports it, and both
	/// builds must still reach a clean removal of the subscriber.
{
	const std::string subject("com.example.serialized");

	SubscriptionWaiter waiter;
	LocalService local(2, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	Poco::Net::StreamSocket raw = connectRawWebSocket(local.port);
	raw.setReceiveTimeout(Poco::Timespan(5, 0));
	std::string subscribe = maskedTextFrame("SUBSCRIBE " + subject + " WebEvent/1.0\r\n");
	raw.sendBytes(subscribe.data(), static_cast<int>(subscribe.size()));
	assertTrue(waiter.event.tryWait(2000));

	// Park both workers so both tasks are queued before either runs.
	ThreadBlock block1(*local.pService);
	ThreadBlock block2(*local.pService);
	assertTrue(block1.parked());
	assertTrue(block2.parked());

	TestableWebEventServiceImpl::Subscriber::Ptr pSubscriber;
	assertTrue(local.pService->firstSubscriber(pSubscriber));
	assertTrue(!pSubscriber.isNull());

	// First task: an active shutdown, which waits for the client's CLOSE.
	local.pService->shutdown(pSubscriber, Poco::Net::WebSocket::WS_NORMAL_CLOSE, "", false);

	// Second task: the main thread sees the first two bytes of a frame and
	// hands the socket to a worker to read the rest.
	std::string frame = maskedTextFrame("junk");
	FrameRest rest(raw, frame.substr(2));
	raw.sendBytes(frame.data(), 2);
	assertTrue(waitUntil([&]{ return local.pService->workerQueue().size() >= 2; }, 5000));
	assertTrue(local.pService->waitForMainThread());
	assertEqual(2, local.pService->workerQueue().size());

	// The shutdown runs first: once the client holds its CLOSE frame and
	// the two bytes have left the server socket, that worker is inside the
	// confirmation read, stalled on the rest of the frame.
	block1.release();
	assertTrue(receivesCloseFrame(raw));
	assertTrue(waitUntil([&]{ return unreadBytes(*pSubscriber->pWebSocket) == 0; }, 5000));

	// The receive of the same frame runs on the other worker.
	block2.release();

	// The rest of the frame lets both reads complete; the subscriber must
	// then be gone and both workers idle again.
	rest.send();
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(subject) == 0; }, 15000));
	assertTrue(local.pService->waitForWorkerThread(15000));
	assertTrue(local.pService->waitForWorkerThread(15000));

	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	try
	{
		raw.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testStopReturnsWhenNotificationRacesStop()
	/// A notification enqueued after stop() has begun must not leave the
	/// main thread waiting on its queue with the stop already signalled.
	/// A queue's wake-up flag is cleared by any later enqueue, so the
	/// signal has to be a queued notification of its own.
{
	LocalService local(1, 0);

	// Park the main thread, so the stop cannot complete before the
	// notification lands.
	ThreadBlock block(*local.pService, ThreadBlock::MAIN);
	assertTrue(block.parked());

	Poco::Thread stopper;
	stopper.startFunc([&]{ local.pService->stop(); });
	assertTrue(waitUntil([&]{ return local.pService->stopped(); }, 5000));

	// The enqueue that used to clear the wake-up. It is never waited on and
	// may never run, so its state belongs to the notification.
	local.pService->mainQueue().enqueueNotification(new SignalNotification(*local.pService, new SignalState));

	block.release();
	assertTrue(stopper.tryJoin(5000));
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testLocalServiceTearsDownSynchronously()
	/// Leaving the scope of a service with a subscriber still connected
	/// must have destroyed the service and shut the client's socket
	/// before the scope ends: nothing may outlive it on another thread.
{
	const std::string subject("com.example.teardown");
	Poco::Event destroyed(Poco::Event::EVENT_MANUALRESET);
	std::unique_ptr<Poco::Net::WebSocket> pWS;
	{
		SubscriptionWaiter waiter;
		LocalService local(1, 0);
		local.pService->setDestroyedEvent(&destroyed);
		local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

		pWS = std::make_unique<Poco::Net::WebSocket>(connectWebSocket(local.port));
		sendSubscribe(*pWS, subject);
		assertTrue(waiter.event.tryWait(2000));
		local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);
	}
	assertTrue(destroyed.tryWait(0));

	// the client's socket is shut: its next read ends
	pWS->setReceiveTimeout(Poco::Timespan(2, 0));
	bool closed = false;
	try
	{
		char buffer[64];
		int flags = 0;
		int n = pWS->receiveFrame(buffer, sizeof(buffer), flags);
		closed = n == 0 || (flags & Poco::Net::WebSocket::FRAME_OP_CLOSE) != 0;
	}
	catch (Poco::Exception&)
	{
		closed = true;
	}
	assertTrue(closed);
}


void WebEventServiceTest::testListSubscribers()
{
	assertTrue(_pService->listSubscribers().empty());

	Poco::Net::HTTPClientSession session("127.0.0.1", _port);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	request.set("X-Test-User", "alice");
	request.set("X-Test-Page", "/app/auth/");
	Poco::Net::HTTPResponse response;
	Poco::Net::WebSocket socket(session, request, response);

	// addSubscriber is asynchronous (enqueued by the upgrade handler), so
	// poll for the snapshot to contain the new connection.
	{
		Poco::Stopwatch sw;
		sw.start();
		while (_pService->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}

	WebEventService::SubscriberInfoVec subscribers = _pService->listSubscribers();
	assertEqual(1, static_cast<int>(subscribers.size()));
	assertEquals(std::string("alice"), subscribers[0].username);
	assertEquals(std::string("/app/auth/"), subscribers[0].page);
	assertEquals(std::string("127.0.0.1"), subscribers[0].clientAddress.substr(0, 9));

	// A graceful client close must remove the entry.
	try
	{
		socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		socket.close();
	}
	catch (...) {}

	{
		Poco::Stopwatch sw;
		sw.start();
		while (!_pService->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertTrue(_pService->listSubscribers().empty());
}


void WebEventServiceTest::testCloseFrameIsAnsweredWhileTcpStaysOpen()
{
	// Firefox closes a page's WebSocket by sending a CLOSE frame and then
	// keeping the TCP connection open for 20 seconds, waiting for the
	// server's CLOSE reply (Chromium drops the connection right after its
	// frame, so it never notices a server that ignores it). The frame must
	// be recognised as a close - opcode 0x8 shares a bit with PONG 0xA -
	// answered, and the subscriber removed, all without a FIN from the
	// client.
	assertTrue(_pService->listSubscribers().empty());

	Poco::Net::WebSocket socket = connectWebSocket(_port, "alice");
	sendSubscribe(socket, "test.subject");
	{
		Poco::Stopwatch sw;
		sw.start();
		while (_pService->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(1, static_cast<int>(_pService->listSubscribers().size()));

	// CLOSE frame only; the TCP connection stays open.
	socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "going away");

	// The server answers with its own CLOSE frame ...
	socket.setReceiveTimeout(Poco::Timespan(5, 0));
	char buffer[64];
	int flags = 0;
	int n = socket.receiveFrame(buffer, sizeof(buffer), flags);
	assertTrue(n > 0);
	assertEqual(static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CLOSE), flags & Poco::Net::WebSocket::FRAME_OP_BITMASK);

	// ... and drops the subscriber.
	{
		Poco::Stopwatch sw;
		sw.start();
		while (!_pService->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertTrue(_pService->listSubscribers().empty());

	try { socket.close(); } catch (...) {}
}


void WebEventServiceTest::testListSubscribersThrowsOnTimeout()
{
	// A snapshot the event thread never answers must not come back as a
	// snapshot in which nobody is connected: the caller cannot tell the two
	// apart, and would report every user as having gone away.
	LocalService local(1, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES);
	Poco::Net::WebSocket socket = connectWebSocket(local.port, "alice");

	{
		Poco::Stopwatch sw;
		sw.start();
		while (local.pService->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(1, static_cast<int>(local.pService->listSubscribers().size()));

	ThreadBlock block(*local.pService, ThreadBlock::MAIN);
	assertTrue(block.parked());

	try
	{
		local.pService->listSubscribers();
		fail("unanswered snapshot - must throw");
	}
	catch (Poco::TimeoutException&)
	{
	}

	// Released here rather than by the destructor, so that the snapshot
	// given up on above is seen to complete into storage nobody reads.
	block.release();
	assertTrue(local.pService->waitForMainThread());

	try
	{
		socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		socket.close();
	}
	catch (...) {}
}


void WebEventServiceTest::testKeepaliveReapsSilentSubscribers()
{
	// A dedicated service with a 1-second keepalive: subscribers idle for
	// 0.5s are pinged, subscribers silent for 2s are removed. A client that
	// answers pings must survive well past the reap threshold; one that
	// never answers must be removed - this also guards against the sweep
	// phase-alignment regression, where responsive subscribers whose idle
	// time tracked the sweep period were reaped despite answering.
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl =
		new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), 0, 1, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES, 1);

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	Poco::UInt16 port = serverSocket.address().port();
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl),
		serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	auto connect = [&](const std::string& username, Poco::Net::HTTPResponse& response)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
		request.set("X-Test-User", username);
		return Poco::Net::WebSocket(session, request, response);
	};

	Poco::Net::HTTPResponse responsiveResponse;
	Poco::Net::WebSocket responsiveSocket = connect("responsive", responsiveResponse);
	Poco::Net::HTTPResponse silentResponse;
	Poco::Net::WebSocket silentSocket = connect("silent", silentResponse);

	{
		Poco::Stopwatch sw;
		sw.start();
		while (pImpl->listSubscribers().size() != 2 && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(2, static_cast<int>(pImpl->listSubscribers().size()));

	// Answer pings on the responsive socket for two full reap windows while
	// the silent socket stays quiet.
	responsiveSocket.setReceiveTimeout(Poco::Timespan(0, 200000));
	Poco::Stopwatch sw;
	sw.start();
	while (sw.elapsedSeconds() < 5)
	{
		try
		{
			std::vector<char> buffer(256);
			int flags = 0;
			int n = responsiveSocket.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
			if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_PING)
			{
				responsiveSocket.sendFrame(buffer.data(), n,
					static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG));
			}
		}
		catch (Poco::TimeoutException&)
		{
		}
	}

	WebEventService::SubscriberInfoVec subscribers = pImpl->listSubscribers();
	assertEqual(1, static_cast<int>(subscribers.size()));
	assertEquals(std::string("responsive"), subscribers[0].username);

	try
	{
		responsiveSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		responsiveSocket.close();
	}
	catch (...) {}
	try { silentSocket.close(); } catch (...) {}

	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testActivityIsStampedWhenReadable()
{
	// The keepalive must count a socket as heard from when its bytes arrive,
	// not when a worker gets round to reading them: a worker held up longer
	// than the reap threshold would otherwise have subscribers that are in
	// fact answering removed. With the only worker parked here, nothing but
	// the readability itself can refresh the stamp.
	LocalService local(1, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES);
	Poco::Net::WebSocket socket = connectWebSocket(local.port, "alice");

	TestableWebEventServiceImpl::SubscriberStats before;
	{
		Poco::Stopwatch sw;
		sw.start();
		while (sw.elapsedSeconds() < 5)
		{
			if (local.pService->firstSubscriberStats(before) && before.found) break;
			Poco::Thread::sleep(25);
		}
	}
	assertTrue(before.found);

	ThreadBlock block(*local.pService);
	assertTrue(block.parked());

	sendSubscribe(socket, "test.subject");

	TestableWebEventServiceImpl::SubscriberStats after;
	bool stamped = false;
	{
		Poco::Stopwatch sw;
		sw.start();
		while (sw.elapsedSeconds() < 5)
		{
			if (local.pService->firstSubscriberStats(after) && after.found &&
				after.lastActive > before.lastActive)
			{
				stamped = true;
				break;
			}
			Poco::Thread::sleep(25);
		}
	}
	assertTrue(stamped);

	// The frame is still unread - the worker that would read it has not
	// returned - so the stamp can only have come from the main thread
	// seeing the socket readable.
	assertTrue(!local.pService->waitForWorkerThread(200));

	block.release();
	try
	{
		socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		socket.close();
	}
	catch (...) {}
}


void WebEventServiceTest::testKeepaliveSweepRunsUnderQueueLoad()
{
	// A sweep must not be postponed by a main queue that its producers keep
	// full: a subscriber which has stopped answering has to be reclaimed
	// while the traffic is still flowing, not once it has died down.
	const std::string subject("keepalive.load.subject");
	const int backlog = 4000;

	// Declared ahead of the service so that it outlives the event thread
	// firing into it, however this test leaves.
	SlowNotificationHandler slow;

	// One second between sweeps: idle for half of that and a subscriber is
	// pinged, silent for two of them and it is removed.
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl =
		new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), 0, 2, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES, 1);

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	Poco::UInt16 port = serverSocket.address().port();
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl),
		serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	Poco::Net::WebSocket socket = connectWebSocket(port, "silent");
	sendSubscribe(socket, subject);
	{
		Poco::Stopwatch sw;
		sw.start();
		while (pImpl->subscriberCount(subject) != 1 && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(1, pImpl->subscriberCount(subject));

	// Every notification is made slow, so a backlog queued in one go keeps
	// the event thread occupied for several sweep periods. Load being
	// applied, not a wait for something to happen.
	pImpl->notificationSent += delegate(&slow, &SlowNotificationHandler::onNotification);
	for (int i = 0; i < backlog; i++)
	{
		pImpl->notify("load.subject", "x");
	}

	// The subscriber count is read straight from the subject map rather than
	// through a snapshot, which would queue behind the backlog itself.
	bool reaped = false;
	int stillQueued = 0;
	{
		Poco::Stopwatch sw;
		sw.start();
		while (sw.elapsedSeconds() < 10)
		{
			if (pImpl->subscriberCount(subject) == 0)
			{
				stillQueued = static_cast<int>(pImpl->mainQueue().size());
				reaped = true;
				break;
			}
			Poco::Thread::sleep(25);
		}
	}
	assertTrue(reaped);
	// Reclaimed while the queue was still loaded, which is the point: had it
	// waited for the backlog to drain, this would be zero.
	assertTrue(stillQueued > 0);

	pImpl->notificationSent -= delegate(&slow, &SlowNotificationHandler::onNotification);
	try { socket.close(); } catch (...) {}

	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testUnsentPingDoesNotReap()
{
	// A subscriber is removed for ignoring pings, so a ping that never left
	// must not count against it: with the only worker parked, the keepalive
	// frame stays in the send queue and the peer is never actually asked.
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl =
		new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), 0, 1, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES, 1);

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	Poco::UInt16 port = serverSocket.address().port();
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl),
		serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	Poco::Net::WebSocket socket = connectWebSocket(port, "quiet");
	{
		Poco::Stopwatch sw;
		sw.start();
		while (pImpl->listSubscribers().empty() && sw.elapsedSeconds() < 5)
			Poco::Thread::sleep(25);
	}
	assertEqual(1, static_cast<int>(pImpl->listSubscribers().size()));

	ThreadBlock block(*pImpl);
	assertTrue(block.parked());

	// Held well past the point at which a subscriber that had been pinged
	// and stayed silent would have gone: two reap thresholds over.
	{
		Poco::Stopwatch sw;
		sw.start();
		while (sw.elapsedSeconds() < 4) Poco::Thread::sleep(50);
	}
	assertEqual(1, static_cast<int>(pImpl->listSubscribers().size()));

	block.release();
	try { socket.close(); } catch (...) {}

	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testSnapshotReturnsEmptyWhenStopped()
{
	// A stop can land between a snapshot being asked for and the event
	// thread leaving its loop, after which nothing will ever execute the
	// request. The documented answer for a stopped service is an empty
	// list, and giving it must not take the whole snapshot timeout.
	LocalService local(1, WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES);

	ThreadBlock block(*local.pService, ThreadBlock::MAIN);
	assertTrue(block.parked());

	WebEventService::SubscriberInfoVec snapshot;
	bool threw = false;
	Poco::Event finished;
	std::thread requester([&]
	{
		try
		{
			snapshot = local.pService->listSubscribers();
		}
		catch (Poco::Exception&)
		{
			threw = true;
		}
		finished.set();
	});
	std::thread stopper([&]
	{
		local.pService->stop();
	});

	// Deliberately below the snapshot timeout: waiting that out is the
	// behaviour being ruled out here.
	const bool prompt = finished.tryWait(3000);

	// Released and joined before the assertions, so a failure leaves no
	// thread parked and none unjoined.
	block.release();
	requester.join();
	stopper.join();

	assertTrue(prompt);
	assertTrue(!threw);
	assertTrue(snapshot.empty());
}


void WebEventServiceTest::testSendQueueCoalescing()
{
	const std::string parentSubject("com.example.parent");
	const std::string subject("com.example.parent.child.leaf");

	_pService->notificationSent += delegate(this, &WebEventServiceTest::onNotificationSent);
	_notificationHandlerAdded = true;
	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	TestableWebEventServiceImpl& testable = testableService(_pService);

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));

	// One client subscribed to both an exact subject and an ancestor,
	// so the hierarchy walk hits it twice per notify.
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));
	sendSubscribe(ws, parentSubject);
	assertTrue(_parentSubscriptionEvent.tryWait(2000));

	// Park the single worker so notifies pile up in the subscriber's queue.
	ThreadBlock block(testable);
	assertTrue(block.parked());

	_pService->notify(subject, "v1");
	_pService->notify(subject, "v2");
	_pService->notify(subject, "v3");
	assertTrue(testable.waitForMainThread());
	assertEqual(3, _notificationCount.load());

	block.release();

	// Latest value wins and overlapping subscriptions are deduplicated:
	// exactly one frame, carrying the newest payload.
	std::vector<char> buffer(2048);
	int flags = 0;
	int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	WebEventService::NotificationEvent notification = parseNotificationFrame(std::string(buffer.data(), received));
	assertEquals(subject, notification.first);
	assertEquals(std::string("v3"), notification.second);
	assertFalse(ws.poll(Poco::Timespan(500000), Poco::Net::Socket::SELECT_READ));

	// The drainer token must have recovered: a fresh notify still arrives.
	_pService->notify(subject, "v4");
	received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	notification = parseNotificationFrame(std::string(buffer.data(), received));
	assertEquals(std::string("v4"), notification.second);

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testSendQueueBounded()
{
	const std::string parentSubject("com.example.bound");
	const int maxPendingFrames = 4;
	const int notifyCount = 10;

	SubscriptionWaiter waiter;
	LocalService local(1, maxPendingFrames);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	Poco::Net::WebSocket ws = connectWebSocket(local.port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, parentSubject);
	assertTrue(waiter.event.tryWait(2000));

	ThreadBlock block(*local.pService);
	assertTrue(block.parked());

	// Distinct subjects do not coalesce; the oldest ones must be dropped.
	for (int i = 0; i < notifyCount; ++i)
	{
		local.pService->notify(Poco::format("%s.s%d", parentSubject, i), Poco::format("p%d", i));
	}
	assertTrue(local.pService->waitForMainThread());

	TestableWebEventServiceImpl::SubscriberStats stats;
	assertTrue(local.pService->firstSubscriberStats(stats));
	assertTrue(stats.found);
	assertEqual(maxPendingFrames, static_cast<int>(stats.pending));
	assertEqual(notifyCount - maxPendingFrames, static_cast<int>(stats.dropped));

	block.release();

	// The newest maxPendingFrames frames arrive, in generation order.
	std::vector<char> buffer(2048);
	for (int i = notifyCount - maxPendingFrames; i < notifyCount; ++i)
	{
		int flags = 0;
		int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
		assertTrue(received > 0);
		WebEventService::NotificationEvent notification = parseNotificationFrame(std::string(buffer.data(), received));
		assertEquals(Poco::format("%s.s%d", parentSubject, i), notification.first);
		assertEquals(Poco::format("p%d", i), notification.second);
	}
	assertFalse(ws.poll(Poco::Timespan(500000), Poco::Net::Socket::SELECT_READ));

	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testPendingSendsAfterDisconnect()
{
	const std::string subject("com.example.parent.child.leaf");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	TestableWebEventServiceImpl& testable = testableService(_pService);

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	ThreadBlock block(testable);
	assertTrue(block.parked());

	for (int i = 0; i < 5; ++i)
	{
		_pService->notify(Poco::format("%s.x%d", subject, i), "pending");
	}
	assertTrue(testable.waitForMainThread());

	// Drop the client while its frames are still queued.
	try
	{
		ws.close();
	}
	catch (...) {}

	block.release();

	// The subscriber must be removed and its queued frames released
	// without hanging the worker or the main thread.
	Poco::Stopwatch sw;
	sw.start();
	while (_pService->subscriberCount(subject) != 0 && sw.elapsedSeconds() < 10)
		Poco::Thread::sleep(25);
	assertEqual(0, _pService->subscriberCount(subject));
	assertTrue(testable.waitForWorkerThread(15000));
	assertTrue(testable.waitForMainThread());
	stopService();
}


void WebEventServiceTest::testSlowConsumerDoesNotStallOthers()
{
	const std::string slowSubject("com.example.slow");
	const std::string fastSubject("com.example.fast");
	const std::string payload("{\"fast\":true}");

	SubscriptionWaiter waiter;
	LocalService local(2, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	// The slow client subscribes and never reads.
	Poco::Net::WebSocket slowSocket = connectWebSocket(local.port);
	sendSubscribe(slowSocket, slowSubject);
	assertTrue(waiter.event.tryWait(2000));

	Poco::Net::WebSocket fastSocket = connectWebSocket(local.port);
	fastSocket.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(fastSocket, fastSubject);
	assertTrue(waiter.event.tryWait(2000));

	// Fill the TCP buffers towards the slow client until its drainer
	// blocks in sendFrame; distinct subjects prevent coalescing.
	const std::string bigPayload(131072, 'x');
	for (int i = 0; i < 40; ++i)
	{
		local.pService->notify(Poco::format("%s.s%d", slowSubject, i), bigPayload);
	}

	// The fast client must not wait behind the stalled one.
	local.pService->notify(fastSubject, payload);

	std::vector<char> buffer(2048);
	int flags = 0;
	int received = fastSocket.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	WebEventService::NotificationEvent notification = parseNotificationFrame(std::string(buffer.data(), received));
	assertEquals(fastSubject, notification.first);
	assertEquals(payload, notification.second);

	// Unblock the stalled drainer quickly so teardown does not wait for
	// the full send timeout.
	try
	{
		slowSocket.close();
	}
	catch (...) {}

	Poco::Stopwatch sw;
	sw.start();
	while (local.pService->subscriberCount(slowSubject) != 0 && sw.elapsedSeconds() < 15)
		Poco::Thread::sleep(50);

	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	try
	{
		fastSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		fastSocket.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testNotifyUserDeliversOnlyToUser()
{
	const std::string subject("com.example.parent.child.leaf");
	const std::string payload("{\"user\":\"alice\"}");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::WebSocket aliceSocket = connectWebSocket(_port, "alice");
	aliceSocket.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(aliceSocket, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	Poco::Net::WebSocket bobSocket = connectWebSocket(_port, "bob");
	sendSubscribe(bobSocket, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	_pService->notifyUser("alice", subject, payload);

	std::vector<char> buffer(2048);
	int flags = 0;
	int received = aliceSocket.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(received > 0);
	WebEventService::NotificationEvent notification = parseNotificationFrame(std::string(buffer.data(), received));
	assertEquals(subject, notification.first);
	assertEquals(payload, notification.second);

	assertFalse(bobSocket.poll(Poco::Timespan(500000), Poco::Net::Socket::SELECT_READ));

	for (auto* pWS: {&aliceSocket, &bobSocket})
	{
		try
		{
			pWS->shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
			pWS->close();
		}
		catch (...) {}
	}
	stopService();
}


void WebEventServiceTest::testConcurrentApiAccess()
{
	// Hammers the public API from non-service threads while the main thread
	// dispatches notifications and (un)subscriptions. Deterministic asserts
	// are minimal; the real check is a clean TSan run.
	const std::string subject("com.example.parent.child.leaf");
	const std::string churnSubject("com.example.churn");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	TestableWebEventServiceImpl& testable = testableService(_pService);

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	// an event target on a hammered subject, so notifyImpl fires delegates
	// while another thread inserts neighboring subjects
	_pService->subjectNotified("com.example.hammer.h0") += delegate(this, &WebEventServiceTest::onChildNotified);

	std::atomic<bool> failed{false};
	std::thread counter([&]{
		for (int i = 0; i < 3000; ++i)
		{
			if (_pService->subscriberCount(subject) < 0) failed = true;
		}
	});
	std::thread inserter([&]{
		for (int i = 0; i < 3000; ++i)
		{
			_pService->subjectNotified(Poco::format("com.example.hammer.h%d", i % 64));
		}
	});

	Poco::Net::WebSocket churn = connectWebSocket(_port);
	for (int i = 0; i < 1000; ++i)
	{
		_pService->notify(Poco::format("com.example.hammer.h%d", i % 64), "x");
		if (i % 25 == 0)
		{
			std::string verb = (i % 50 == 0) ? "SUBSCRIBE " : "UNSUBSCRIBE ";
			std::string frame = verb + churnSubject + " WebEvent/1.0\r\n";
			churn.sendFrame(frame.data(), static_cast<int>(frame.size()), Poco::Net::WebSocket::FRAME_TEXT);
		}
	}

	counter.join();
	inserter.join();
	_pService->subjectNotified("com.example.hammer.h0") -= delegate(this, &WebEventServiceTest::onChildNotified);
	assertFalse(failed.load());
	assertTrue(testable.waitForMainThread());
	assertEqual(1, _pService->subscriberCount(subject));

	// hammer subjects are unrelated to the subscription: no frames delivered
	assertFalse(ws.poll(Poco::Timespan(200000), Poco::Net::Socket::SELECT_READ));

	for (auto* pWS: {&ws, &churn})
	{
		try
		{
			pWS->shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
			pWS->close();
		}
		catch (...) {}
	}
	stopService();
}


CppUnit::Test* WebEventServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEventServiceTest");

	CppUnit_addTest(pSuite, WebEventServiceTest, testNotifyDispatchesToSubjectHierarchy);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSubjectNotifiedReturnsSameEvent);
	CppUnit_addTest(pSuite, WebEventServiceTest, testNotifyNoDispatch);
	CppUnit_addTest(pSuite, WebEventServiceTest, testBadSocketDescriptorRecovery);
	CppUnit_addTest(pSuite, WebEventServiceTest, testOverlappingSubscriptionsEnqueueOneFrame);
	CppUnit_addTest(pSuite, WebEventServiceTest, testResetSocketIsDispatchedOnce);
	CppUnit_addTest(pSuite, WebEventServiceTest, testReceiveAndShutdownAreSerialized);
	CppUnit_addTest(pSuite, WebEventServiceTest, testStopReturnsWhenNotificationRacesStop);
	CppUnit_addTest(pSuite, WebEventServiceTest, testLocalServiceTearsDownSynchronously);
	CppUnit_addTest(pSuite, WebEventServiceTest, testListSubscribers);
	CppUnit_addTest(pSuite, WebEventServiceTest, testCloseFrameIsAnsweredWhileTcpStaysOpen);
	CppUnit_addTest(pSuite, WebEventServiceTest, testListSubscribersThrowsOnTimeout);
	CppUnit_addTest(pSuite, WebEventServiceTest, testKeepaliveReapsSilentSubscribers);
	CppUnit_addTest(pSuite, WebEventServiceTest, testActivityIsStampedWhenReadable);
	CppUnit_addTest(pSuite, WebEventServiceTest, testKeepaliveSweepRunsUnderQueueLoad);
	CppUnit_addTest(pSuite, WebEventServiceTest, testUnsentPingDoesNotReap);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSnapshotReturnsEmptyWhenStopped);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSendQueueCoalescing);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSendQueueBounded);
	CppUnit_addTest(pSuite, WebEventServiceTest, testPendingSendsAfterDisconnect);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSlowConsumerDoesNotStallOthers);
	CppUnit_addTest(pSuite, WebEventServiceTest, testNotifyUserDeliversOnlyToUser);
	CppUnit_addTest(pSuite, WebEventServiceTest, testConcurrentApiAccess);

	return pSuite;
}
