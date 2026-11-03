//
// WebEventServiceTest.cpp
//
// Copyright (c) 2025, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebEventServiceTest.h"
#include "Poco/OSP/WebEvent/WebEventRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
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
#include "Poco/Net/Socket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Format.h"
#include "Poco/URI.h"
#include "Poco/Stopwatch.h"
#include "Poco/Clock.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <istream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
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
		const Poco::Net::Socket& socket, SignalState::Ptr pState):
		WebEventTaskNotification(service),
		_socket(socket),
		_pState(pState)
	{
	}

	void execute() override
	{
		_socket.impl()->close();
		_pState->ready.set();
	}

private:
	Poco::Net::Socket _socket;
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

	void execute() override
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

	void execute() override
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
	using Subscriber = WebEventServiceImpl::Subscriber;
	using WebEventServiceImpl::shutdown;
	using WebEventServiceImpl::workerQueue;
	using WebEventServiceImpl::mainQueue;
	using WebEventServiceImpl::enqueueMain;
	using WebEventServiceImpl::socketMap;

	struct SubscriberStats
	{
		std::size_t pending = 0;
		Poco::UInt64 dropped = 0;
		Poco::UInt64 enqueued = 0;
		Poco::Clock lastActive;
		bool found = false;
	};

	[[nodiscard]] bool corruptFirstSocket(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		Subscriber::Ptr pSubscriber;
		if (!firstSubscriber(pSubscriber, timeoutMs) || pSubscriber.isNull()) return false;
		enqueueMain(new CorruptSocketNotification(*this, *pSubscriber->pWebSocket, pState));
		return pState->ready.tryWait(timeoutMs);
	}

	[[nodiscard]] bool waitForMainThread(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		enqueueMain(new SignalNotification(*this, pState));
		return pState->ready.tryWait(timeoutMs);
	}

	[[nodiscard]] bool waitForWorkerThread(long timeoutMs = 5000)
	{
		SignalState::Ptr pState = new SignalState;
		workerQueue().enqueueNotification(new SignalNotification(*this, pState));
		return pState->ready.tryWait(timeoutMs);
	}

	~TestableWebEventServiceImpl() override
	{
		if (_pDestroyed != nullptr) _pDestroyed->set();
	}

	void setDestroyedEvent(Poco::Event* pEvent)
		/// The event set by the destructor, if any.
	{
		_pDestroyed = pEvent;
	}

	[[nodiscard]] SubscriberStats inspectFirstSubscriber()
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

	void execute() override
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
	enqueueMain(new InspectSubscriberNotification(*this, pState));
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

	void execute() override
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
	enqueueMain(new FirstSubscriberNotification(*this, pState));
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


WebEventServiceImpl::Config serviceConfig(int workerCount)
	/// The default configuration with the given number of workers.
{
	WebEventServiceImpl::Config config;
	config.workerCount = workerCount;
	return config;
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
		if (which == MAIN)
		{
			service.enqueueMain(new BlockerNotification(service, _pState));
		}
		else
		{
			service.workerQueue().enqueueNotification(new BlockerNotification(service, _pState));
		}
		_parked = _pState->started.tryWait(timeoutMs);
	}

	~ThreadBlock()
	{
		release();
	}

	[[nodiscard]] bool parked() const
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


class WebEventTestRequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	/// Serves "/webevent" with the production request handler, as the web
	/// server does.
{
public:
	explicit WebEventTestRequestHandlerFactory(Poco::AutoPtr<TestableWebEventServiceImpl> pService,
		Poco::OSP::WebEvent::WebEventRequestHandler::OriginSetPtr pAllowedOrigins = nullptr):
		_pService(std::move(pService)),
		_pAllowedOrigins(std::move(pAllowedOrigins))
	{
		// The X-Test-User header stands in for a session lookup.
		_pService->setUserResolver([](const Poco::Net::HTTPServerRequest& request)
		{
			return request.get("X-Test-User", "");
		});
	}

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(
		const Poco::Net::HTTPServerRequest& request) override
	{
		if (Poco::URI(request.getURI()).getPath() == "/webevent")
		{
			return new Poco::OSP::WebEvent::WebEventRequestHandler(_pService, _pAllowedOrigins);
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
	Poco::OSP::WebEvent::WebEventRequestHandler::OriginSetPtr _pAllowedOrigins;
};


struct LocalService
	/// A dedicated service + HTTP server for tests needing a non-default
	/// worker count or per-subscriber queue limit.
{
	LocalService(int workerCount, int maxPendingFrames, bool clientNotify = false)
	{
		WebEventServiceImpl::Config config = serviceConfig(workerCount);
		config.maxPendingFrames = maxPendingFrames;
		config.clientNotify = clientNotify;
		pService = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
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
		if (pService != nullptr)
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


struct RequestCounter
{
	void onRequest(const Poco::Net::HTTPServerRequest*&)
	{
		++count;
	}

	std::atomic<int> count{0};
};


struct UnsubscriptionRecorder
{
	void onUnsubscription(const std::string& subject)
	{
		Poco::FastMutex::ScopedLock lock(mutex);
		subjects.push_back(subject);
	}

	[[nodiscard]] std::vector<std::string> recorded()
	{
		Poco::FastMutex::ScopedLock lock(mutex);
		return subjects;
	}

	Poco::FastMutex mutex;
	std::vector<std::string> subjects;
};


struct RejectingRequestHandler
	/// Rejects every upgrade request that carries an X-Test-Reject header.
{
	void onRequest(const Poco::Net::HTTPServerRequest*& pRequest)
	{
		if (pRequest->has("X-Test-Reject"))
		{
			pRequest->response().setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
		}
	}
};


struct WebSocketErrorRequestHandler
	/// Fails every upgrade request with a WebSocket error that is not one of
	/// the handshake errors.
{
	[[noreturn]] void onRequest(const Poco::Net::HTTPServerRequest*&)
	{
		throw Poco::Net::WebSocketException("delegate failed", Poco::Net::WebSocket::WS_ERR_PAYLOAD_TOO_BIG);
	}
};


struct ThrowingNotificationHandler
	/// Fails every notification, with a standard or a Poco exception.
{
	[[noreturn]] void onNotification(const WebEventService::NotificationEvent&)
	{
		if (throwStd) throw std::runtime_error("delegate failed");
		throw Poco::IllegalStateException("delegate failed");
	}

	std::atomic<bool> throwStd{true};
};


Poco::Net::WebSocket connectWebSocket(Poco::UInt16 port, const std::string& username = std::string())
{
	Poco::Net::HTTPClientSession session("127.0.0.1", port);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
	if (!username.empty()) request.set("X-Test-User", username);
	Poco::Net::HTTPResponse response;
	return Poco::Net::WebSocket(session, request, response);
}


void sendText(Poco::Net::WebSocket& ws, const std::string& text)
{
	ws.sendFrame(text.data(), static_cast<int>(text.size()), Poco::Net::WebSocket::FRAME_TEXT);
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


WebEventService::NotificationEvent receiveNotification(Poco::Net::WebSocket& ws)
	/// Receives the next frame and parses it as a NOTIFY message.
	///
	/// Frames leave a subscriber's queue in order, so receiving a marker
	/// sent last as the next frame shows that nothing was queued before it,
	/// without waiting for a frame that is expected not to come.
{
	std::vector<char> buffer(2048);
	int flags = 0;
	int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	if (received <= 0) throw Poco::IOException("No notification frame received");
	return parseNotificationFrame(std::string(buffer.data(), received));
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
	CppUnit::TestCase(name)
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

	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), serviceConfig(1));
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
	if (_pService != nullptr)
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
	if (_pService == nullptr) return;
	_pService->stop();
	if (_pServer != nullptr)
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
	siblingSocket.setReceiveTimeout(Poco::Timespan(5, 0));

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

	// Once the main thread has handled the notification completely, the
	// sibling's delegate has not fired, and the first frame the sibling
	// receives is the one sent to its own subject afterwards.
	assertTrue(testableService(_pService).waitForMainThread());
	assertFalse(_siblingEvent.tryWait(0));
	_pService->notify(unrelatedSubject, "marker");
	WebEventService::NotificationEvent siblingNotification = receiveNotification(siblingSocket);
	assertEquals(unrelatedSubject, siblingNotification.first);
	assertEquals(std::string("marker"), siblingNotification.second);

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
	// This makes the fd invalid while the poll set still holds the socket,
	// which neither epoll nor poll() then reports as ready or in error.
	TestableWebEventServiceImpl& testable = testableService(_pService);
	assertEqual(1, _pService->subscriberCount(subject));
	assertTrue(testable.corruptFirstSocket());

	// Also close the client side cleanly.
	try { staleSocket.close(); } catch (...) {}

	// The main thread finds the socket whose fd is now
	// POCO_INVALID_SOCKET, and removeBadSocketsImpl() removes it.
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
	// And nothing behind it: the next frame is a later notification.
	_pService->notify(parentSubject + ".marker", "marker");
	notification = receiveNotification(ws);
	assertEquals(parentSubject + ".marker", notification.first);

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

	// An enqueue after the stop has begun. It is never waited on and may
	// never run, so its state belongs to the notification.
	local.pService->enqueueMain(new SignalNotification(*local.pService, new SignalState));

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
		closed = n == 0 || (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE;
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
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent?page=/app/auth/", Poco::Net::HTTPMessage::HTTP_1_1);
	request.set("X-Test-User", "alice");
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


void WebEventServiceTest::testClientPingIsAnsweredWithPong()
	/// A PING is answered with a PONG carrying the same payload, and neither
	/// a PING nor an empty one ends the subscription.
{
	const std::string subject("com.example.parent.child.leaf");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	for (const std::string& payload: {std::string("hi"), std::string()})
	{
		ws.sendFrame(payload.data(), static_cast<int>(payload.size()),
			static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING));
		std::array<char, 64> buffer{};
		int flags = 0;
		int n = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
		assertEqual(static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG), flags & Poco::Net::WebSocket::FRAME_OP_BITMASK);
		assertEquals(payload, std::string(buffer.data(), n));
	}

	_pService->notify(subject, "after");
	WebEventService::NotificationEvent notification = receiveNotification(ws);
	assertEquals(std::string("after"), notification.second);
	assertEqual(1, _pService->subscriberCount(subject));

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testThrowingDelegateDoesNotStopDelivery()
	/// A delegate that throws, a standard or a Poco exception, keeps neither
	/// the notification from the subscriber nor the event thread from
	/// handling the requests that follow.
{
	const std::string subject("com.example.throwing");

	// Declared ahead of the service so that they outlive the event thread
	// firing into them, however this test leaves.
	SubscriptionWaiter waiter;
	ThrowingNotificationHandler thrower;
	LocalService local(1, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);
	local.pService->subjectNotified(subject) += delegate(&thrower, &ThrowingNotificationHandler::onNotification);
	local.pService->notificationSent += delegate(&thrower, &ThrowingNotificationHandler::onNotification);

	Poco::Net::WebSocket ws = connectWebSocket(local.port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, subject);
	assertTrue(waiter.event.tryWait(2000));

	for (bool throwStd: {true, false})
	{
		thrower.throwStd = throwStd;
		local.pService->notify(subject, "payload");
		WebEventService::NotificationEvent notification = receiveNotification(ws);
		assertEquals(subject, notification.first);

		sendSubscribe(ws, subject + (throwStd ? ".std" : ".poco"));
		assertTrue(waiter.event.tryWait(2000));
	}

	local.pService->notificationSent -= delegate(&thrower, &ThrowingNotificationHandler::onNotification);
	local.pService->subjectNotified(subject) -= delegate(&thrower, &ThrowingNotificationHandler::onNotification);
	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);
	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testNotificationIsHandedOnWithoutPollDelay()
	/// A queued notification wakes the main thread from its wait for the
	/// sockets. With a poll timeout of two seconds, a notification that
	/// waited for the timeout would take up to two seconds; woken, it takes
	/// milliseconds, also on a loaded machine.
{
	const std::string subject("com.example.parent.child.leaf");

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.pollTimeout = std::chrono::seconds(2);
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl), serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	Poco::Net::WebSocket ws = connectWebSocket(port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, subject);
	assertTrue(waitUntil([&]{ return pImpl->subscriberCount(subject) == 1; }, 5000));

	for (int i = 0; i < 3; ++i)
	{
		// The main thread has handled everything queued and waits again.
		assertTrue(pImpl->waitForMainThread());
		Poco::Stopwatch sw;
		sw.start();
		pImpl->notify(subject, "x");
		receiveNotification(ws);
		assertTrue(sw.elapsed() < 500*Poco::Timespan::MILLISECONDS);
	}

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	pImpl->stop();
	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testCrossOriginUpgradeIsRejected()
	/// A browser connects with the cookies of its user whatever page asks it
	/// to, so an upgrade from a page of another origin is rejected before
	/// any delegate sees it, unless that origin is allowed. Clients that
	/// send no Origin are not browsers and are accepted.
{
	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), serviceConfig(1));
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl,
			Poco::OSP::WebEvent::WebEventRequestHandler::parseAllowedOrigins("https://app.example")),
		serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	auto connect = [&](const std::string& origin)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
		if (!origin.empty()) request.set("Origin", origin);
		Poco::Net::HTTPResponse response;
		try
		{
			Poco::Net::WebSocket ws(session, request, response);
			ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		}
		catch (Poco::Net::WebSocketException&)
		{
		}
		return static_cast<int>(response.getStatus());
	};
	RequestCounter counter;
	pImpl->requestReceived += delegate(&counter, &RequestCounter::onRequest);

	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), connect("http://evil.example"));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), connect("null"));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), connect(Poco::format("http://127.0.0.1:%hu.evil.example", port)));
	// The scheme is that of the connection, and a missing port is the
	// default one of the scheme, not any port.
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), connect(Poco::format("https://127.0.0.1:%hu", port)));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), connect("http://127.0.0.1"));
	assertEqual(0, counter.count.load());
	assertTrue(pImpl->listSubscribers().empty());

	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), connect(Poco::format("http://127.0.0.1:%hu", port)));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), connect("https://APP.example"));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), connect(""));
	assertEqual(3, counter.count.load());

	pImpl->requestReceived -= delegate(&counter, &RequestCounter::onRequest);
	pImpl->stop();
	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testAllowedOriginsAreParsedTrimmedAndLowered()
	/// The value of osp.web.event.allowedOrigins is a comma-separated list;
	/// the entries are compared in lower case without surrounding blanks.
{
	auto pOrigins = Poco::OSP::WebEvent::WebEventRequestHandler::parseAllowedOrigins(" https://App.Example ,, http://b.example:8080,");
	assertTrue(pOrigins != nullptr);
	assertEqual(2, static_cast<int>(pOrigins->size()));
	assertEqual(1, static_cast<int>(pOrigins->count("https://app.example")));
	assertEqual(1, static_cast<int>(pOrigins->count("http://b.example:8080")));
	assertTrue(Poco::OSP::WebEvent::WebEventRequestHandler::parseAllowedOrigins("")->empty());
	stopService();
}


void WebEventServiceTest::testClientNotifyIsDroppedByDefault()
	/// A client's NOTIFY reaches neither the other subscribers nor the
	/// server's delegates unless client notifications are enabled. A
	/// system.ping is still answered, to the sender only, and once per ping.
{
	const std::string subject("com.example.parent");
	const std::string notifiedSubject("com.example.parent.child.leaf");

	_pService->notificationSent += delegate(this, &WebEventServiceTest::onNotificationSent);
	_notificationHandlerAdded = true;
	_pService->subjectNotified(notifiedSubject) += delegate(this, &WebEventServiceTest::onChildNotified);
	_childHandlerAdded = true;
	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::WebSocket listener = connectWebSocket(_port);
	listener.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(listener, subject);
	assertTrue(_parentSubscriptionEvent.tryWait(2000));

	Poco::Net::WebSocket sender = connectWebSocket(_port);
	sender.setReceiveTimeout(Poco::Timespan(5, 0));
	sendText(sender, "NOTIFY " + notifiedSubject + " WebEvent/1.0\r\ninjected");
	sendText(sender, "NOTIFY system.ping WebEvent/1.0\r\necho1");
	sendText(sender, "NOTIFY system.ping WebEvent/1.0\r\necho2");

	// The sender's frames are handled in order, so once both pongs are in,
	// the NOTIFY before them has been handled too.
	WebEventService::NotificationEvent pong = receiveNotification(sender);
	assertEquals(std::string("system.pong"), pong.first);
	assertEquals(std::string("echo1"), pong.second);
	pong = receiveNotification(sender);
	assertEquals(std::string("system.pong"), pong.first);
	assertEquals(std::string("echo2"), pong.second);
	assertTrue(testableService(_pService).waitForMainThread());

	assertFalse(_notificationEvent.tryWait(0));
	assertFalse(_childEvent.tryWait(0));
	_pService->notify(subject + ".marker", "marker");
	WebEventService::NotificationEvent notification = receiveNotification(listener);
	assertEquals(subject + ".marker", notification.first);

	for (auto* pWS: {&listener, &sender})
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


void WebEventServiceTest::testClientNotifyReachesSubscribersWhenEnabled()
	/// With client notifications enabled, a client's NOTIFY is passed on
	/// like a call to notify(): to the subscribers and to the delegates.
{
	const std::string subject("com.example.parent");
	const std::string notifiedSubject("com.example.parent.child.leaf");

	SubscriptionWaiter waiter;
	LocalService local(1, 0, true);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);
	local.pService->notificationSent += delegate(this, &WebEventServiceTest::onNotificationSent);
	local.pService->subjectNotified(notifiedSubject) += delegate(this, &WebEventServiceTest::onChildNotified);

	Poco::Net::WebSocket listener = connectWebSocket(local.port);
	listener.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(listener, subject);
	assertTrue(waiter.event.tryWait(2000));

	Poco::Net::WebSocket sender = connectWebSocket(local.port);
	sendText(sender, "NOTIFY " + notifiedSubject + " WebEvent/1.0\r\nfrom a client");

	WebEventService::NotificationEvent notification = receiveNotification(listener);
	assertEquals(notifiedSubject, notification.first);
	assertEquals(std::string("from a client"), notification.second);
	assertTrue(_notificationEvent.tryWait(2000));
	assertTrue(_childEvent.tryWait(2000));
	assertEquals(notifiedSubject, _lastChildNotification.first);

	local.pService->subjectNotified(notifiedSubject) -= delegate(this, &WebEventServiceTest::onChildNotified);
	local.pService->notificationSent -= delegate(this, &WebEventServiceTest::onNotificationSent);
	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);
	for (auto* pWS: {&listener, &sender})
	{
		try
		{
			pWS->shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
			pWS->close();
		}
		catch (...) {}
	}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testConnectionLimitHoldsForConcurrentUpgrades()
	/// The connection limit counts upgrades that are not yet subscribers:
	/// with the main thread held, none of them is added, and the upgrade
	/// beyond the limit is still rejected. A slot is free again once its
	/// subscriber has gone.
{
	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.maxSockets = 2;
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl), serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	auto upgrade = [&](std::unique_ptr<Poco::Net::WebSocket>& pWS)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
		Poco::Net::HTTPResponse response;
		try
		{
			pWS = std::make_unique<Poco::Net::WebSocket>(session, request, response);
		}
		catch (Poco::Net::WebSocketException&)
		{
		}
		return static_cast<int>(response.getStatus());
	};

	std::unique_ptr<Poco::Net::WebSocket> pFirst;
	std::unique_ptr<Poco::Net::WebSocket> pSecond;
	std::unique_ptr<Poco::Net::WebSocket> pThird;
	{
		ThreadBlock block(*pImpl, ThreadBlock::MAIN);
		assertTrue(block.parked());
		assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), upgrade(pFirst));
		assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), upgrade(pSecond));
		assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE), upgrade(pThird));
	}
	assertTrue(waitUntil([&]{ return pImpl->listSubscribers().size() == 2; }, 5000));

	pFirst->shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
	assertTrue(waitUntil([&]{ return pImpl->listSubscribers().size() == 1; }, 5000));
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), upgrade(pThird));

	for (auto* pWS: {pFirst.get(), pSecond.get(), pThird.get()})
	{
		try
		{
			pWS->close();
		}
		catch (...) {}
	}
	pImpl->stop();
	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testUpgradeAfterStopIsClosed()
	/// An upgrade that arrives after the service has stopped is closed
	/// instead of being left open with nobody reading it, and gives its
	/// connection slot back: with a limit of one connection, a second
	/// upgrade is still accepted by the web server.
{
	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.maxSockets = 1;
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl), serverSocket, new Poco::Net::HTTPServerParams);
	server.start();
	pImpl->stop();

	for (int i = 0; i < 2; ++i)
	{
		Poco::Net::WebSocket ws = connectWebSocket(port);
		ws.setReceiveTimeout(Poco::Timespan(5, 0));
		bool closed = false;
		try
		{
			std::array<char, 64> buffer{};
			int flags = 0;
			closed = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags) == 0;
		}
		catch (Poco::TimeoutException&)
		{
		}
		catch (Poco::Net::NetException&)
		{
			closed = true;
		}
		assertTrue(closed);
	}

	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testSubscribingBeyondSubjectLimitClosesConnection()
	/// One connection can subscribe to at most DEFAULT_MAX_SUBJECTS subjects;
	/// a SUBSCRIBE beyond that is a protocol violation.
{
	LocalService local(1, 0);

	Poco::Net::WebSocket ws = connectWebSocket(local.port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	std::string subjects;
	for (int i = 0; i < WebEventServiceImpl::DEFAULT_MAX_SUBJECTS; ++i)
	{
		if (i > 0) subjects += ',';
		subjects += Poco::format("com.example.limit.s%d", i);
	}
	sendSubscribe(ws, subjects);
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount("com.example.limit.s0") == 1; }, 5000));
	assertTrue(local.pService->waitForMainThread());
	assertEqual(1, local.pService->subscriberCount(Poco::format("com.example.limit.s%d", WebEventServiceImpl::DEFAULT_MAX_SUBJECTS - 1)));

	sendSubscribe(ws, "com.example.limit.extra");
	std::array<char, 256> buffer{};
	int flags = 0;
	int n = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	assertTrue(n >= 0);
	assertEqual(static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CLOSE), flags & Poco::Net::WebSocket::FRAME_OP_BITMASK);
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount("com.example.limit.s0") == 0; }, 5000));
	assertEqual(0, local.pService->subscriberCount("com.example.limit.extra"));

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testWorkerCountIsDefaultedAndLimited()
	/// A worker count of zero or less starts defaultWorkerCount() workers,
	/// and a count above MAX_WORKER_COUNT starts that many: as many tasks
	/// can be parked at once, and not one more.
{
	const int defaultCount = std::min(WebEventServiceImpl::defaultWorkerCount(), WebEventServiceImpl::MAX_WORKER_COUNT);
	const std::vector<std::pair<int, int>> cases =
	{
		{0, defaultCount},
		{-1, defaultCount},
		{1000, WebEventServiceImpl::MAX_WORKER_COUNT}
	};
	for (const auto& c: cases)
	{
		Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), serviceConfig(c.first));
		{
			std::vector<std::unique_ptr<ThreadBlock>> blocks;
			for (int i = 0; i < c.second; ++i)
			{
				blocks.push_back(std::make_unique<ThreadBlock>(*pImpl));
				assertTrue(blocks.back()->parked());
			}
			ThreadBlock extra(*pImpl, ThreadBlock::WORKER, 200);
			assertFalse(extra.parked());
		}
		pImpl->stop();
	}
}


void WebEventServiceTest::testInvalidMessagesCloseTheConnection()
	/// A message that is not one valid WebEvent text frame is a protocol
	/// violation: the server closes the connection and removes the
	/// subscriber without acting on the message.
{
	struct Case
	{
		std::string text;
		int flags;
	};
	const int text = static_cast<int>(Poco::Net::WebSocket::FRAME_TEXT);
	const std::vector<Case> cases =
	{
		{"HELLO com.example.invalid WebEvent/1.0", text},
		{"SUBSCRIBE com.example.invalid WebEvent/2.0", text},
		{"SUBSCRIBE com.example.invalid,,com.example.other WebEvent/1.0", text},
		{"SUBSCRIBE com.example.invalid, WebEvent/1.0", text},
		{"SUBSCRIBE com.example.invalid WebEvent/1.0", static_cast<int>(Poco::Net::WebSocket::FRAME_OP_TEXT)},
		{"SUBSCRIBE com.example.invalid." + std::string(5000, 'x') + " WebEvent/1.0", text}
	};

	for (const auto& c: cases)
	{
		Poco::Net::WebSocket ws = connectWebSocket(_port);
		ws.setReceiveTimeout(Poco::Timespan(5, 0));
		assertTrue(waitUntil([&]{ return _pService->listSubscribers().size() == 1; }, 5000));
		ws.sendFrame(c.text.data(), static_cast<int>(c.text.size()), c.flags);

		std::array<char, 256> buffer{};
		int flags = 0;
		ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
		assertEqual(static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CLOSE), flags & Poco::Net::WebSocket::FRAME_OP_BITMASK);
		assertTrue(waitUntil([&]{ return _pService->listSubscribers().empty(); }, 5000));
		assertEqual(0, _pService->subscriberCount("com.example.invalid"));
		assertEqual(0, _pService->subscriberCount(""));
		try
		{
			ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
			ws.close();
		}
		catch (...) {}
	}
	stopService();
}


void WebEventServiceTest::testSubjectListKeepsCommasInBrackets()
	/// A comma inside brackets belongs to the subject name; a closing
	/// bracket without an opening one does not keep the next comma.
{
	Poco::Net::WebSocket ws = connectWebSocket(_port);
	sendSubscribe(ws, "x.a],x.b,x.c[1,2]");
	assertTrue(waitUntil([&]{ return _pService->subscriberCount("x.c[1,2]") == 1; }, 5000));
	assertEqual(1, _pService->subscriberCount("x.a]"));
	assertEqual(1, _pService->subscriberCount("x.b"));

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	stopService();
}


void WebEventServiceTest::testUnsubscribeRemovesSubjects()
	/// UNSUBSCRIBE removes the subjects it names, and "*" removes all; the
	/// unsubscription event fires once per subject removed, and the
	/// connection stays open.
{
	const std::string a("com.example.unsubscribe.a");
	const std::string b("com.example.unsubscribe.b");
	const std::string c("com.example.unsubscribe.c");

	UnsubscriptionRecorder recorder;
	LocalService local(1, 0);
	local.pService->unsubscriptionRequestReceived += delegate(&recorder, &UnsubscriptionRecorder::onUnsubscription);

	Poco::Net::WebSocket ws = connectWebSocket(local.port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, a + "," + b + "," + c);
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(c) == 1; }, 5000));

	sendText(ws, "UNSUBSCRIBE " + a + " WebEvent/1.0");
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(a) == 0; }, 5000));
	assertEqual(1, local.pService->subscriberCount(b));
	assertEqual(1, local.pService->subscriberCount(c));

	sendText(ws, "UNSUBSCRIBE * WebEvent/1.0");
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(b) == 0 && local.pService->subscriberCount(c) == 0; }, 5000));
	assertTrue(local.pService->waitForMainThread());

	std::vector<std::string> unsubscribed = recorder.recorded();
	std::sort(unsubscribed.begin(), unsubscribed.end());
	assertEqual(3, static_cast<int>(unsubscribed.size()));
	assertEquals(a, unsubscribed[0]);
	assertEquals(b, unsubscribed[1]);
	assertEquals(c, unsubscribed[2]);
	assertEqual(1, static_cast<int>(local.pService->listSubscribers().size()));

	local.pService->unsubscriptionRequestReceived -= delegate(&recorder, &UnsubscriptionRecorder::onUnsubscription);
	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testUpgradeRequestHandlerChecks()
	/// The request handler: a requestReceived delegate can reject an
	/// upgrade, an unsupported protocol version gets 400 with the version
	/// supported, the page comes from the "page" query parameter before
	/// the Referer, and a user resolver that throws, or none, leaves the
	/// connection without a user.
{
	RejectingRequestHandler rejecter;
	_pService->requestReceived += delegate(&rejecter, &RejectingRequestHandler::onRequest);

	auto rawUpgrade = [&](const std::string& version, bool reject, Poco::Net::HTTPResponse& response)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", _port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
		request.set("Connection", "Upgrade");
		request.set("Upgrade", "websocket");
		request.set("Sec-WebSocket-Version", version);
		request.set("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
		if (reject) request.set("X-Test-Reject", "1");
		// Otherwise the session replaces the Connection header.
		session.setKeepAlive(true);
		session.sendRequest(request);
		std::istream& body = session.receiveResponse(response);
		return std::string(std::istreambuf_iterator<char>(body), std::istreambuf_iterator<char>());
	};

	Poco::Net::HTTPResponse rejected;
	assertTrue(rawUpgrade(Poco::Net::WebSocket::WEBSOCKET_VERSION, true, rejected).empty());
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), static_cast<int>(rejected.getStatus()));

	Poco::Net::HTTPResponse badVersion;
	rawUpgrade("8", false, badVersion);
	assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST), static_cast<int>(badVersion.getStatus()));
	assertEquals(Poco::Net::WebSocket::WEBSOCKET_VERSION, badVersion.get("Sec-WebSocket-Version"));
	assertTrue(_pService->listSubscribers().empty());

	_pService->setUserResolver([](const Poco::Net::HTTPServerRequest& request) -> std::string
	{
		if (request.get("X-Test-User", "") == "throw") throw std::runtime_error("no session");
		return request.get("X-Test-User", "");
	});
	auto connect = [&](const std::string& uri, const std::string& referer, const std::string& username)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", _port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, uri, Poco::Net::HTTPMessage::HTTP_1_1);
		request.set("Referer", referer);
		request.set("X-Test-User", username);
		Poco::Net::HTTPResponse response;
		Poco::Net::WebSocket ws(session, request, response);
		assertTrue(waitUntil([&]{ return _pService->listSubscribers().size() == 1; }, 5000));
		WebEventService::SubscriberInfo info = _pService->listSubscribers()[0];
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		assertTrue(waitUntil([&]{ return _pService->listSubscribers().empty(); }, 5000));
		return info;
	};

	WebEventService::SubscriberInfo info = connect("/webevent?page=/from/query", "/from/referer", "alice");
	assertEquals(std::string("/from/query"), info.page);
	assertEquals(std::string("alice"), info.username);

	info = connect("/webevent", "/from/referer", "throw");
	assertEquals(std::string("/from/referer"), info.page);
	assertEquals(std::string(), info.username);

	_pService->setUserResolver(nullptr);
	info = connect("/webevent", "/from/referer", "alice");
	assertEquals(std::string(), info.username);

	_pService->requestReceived -= delegate(&rejecter, &RejectingRequestHandler::onRequest);
	stopService();
}


void WebEventServiceTest::testZeroPingIntervalDisablesKeepalive()
	/// With a ping interval of 0 the keepalive sweep does not run: an idle
	/// subscriber is neither pinged nor removed. A sweep that ran with a
	/// zero interval would ping at once.
	/// Timing-dependent by nature: it checks that nothing arrives within a
	/// fixed time.
{
	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.pingInterval = std::chrono::seconds(0);
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl), serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	Poco::Net::WebSocket ws = connectWebSocket(port);
	assertTrue(waitUntil([&]{ return pImpl->listSubscribers().size() == 1; }, 5000));
	assertFalse(ws.poll(Poco::Timespan(2, 0), Poco::Net::Socket::SELECT_READ));
	assertEqual(1, static_cast<int>(pImpl->listSubscribers().size()));

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	pImpl->stop();
	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testOtherWebSocketErrorIsAnswered()
	/// A WebSocket error other than a failed handshake, here one thrown by a
	/// requestReceived delegate, is answered with 500 and an empty body,
	/// and the connection slot of the upgrade is given back.
{
	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	const Poco::UInt16 port = serverSocket.address().port();
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.maxSockets = 1;
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);
	Poco::Net::HTTPServer server(new WebEventTestRequestHandlerFactory(pImpl), serverSocket, new Poco::Net::HTTPServerParams);
	server.start();

	WebSocketErrorRequestHandler failer;
	pImpl->requestReceived += delegate(&failer, &WebSocketErrorRequestHandler::onRequest);
	for (int i = 0; i < 2; ++i)
	{
		Poco::Net::HTTPClientSession session("127.0.0.1", port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent", Poco::Net::HTTPMessage::HTTP_1_1);
		Poco::Net::HTTPResponse response;
		try
		{
			Poco::Net::WebSocket ws(session, request, response);
			fail("upgrade must be refused");
		}
		catch (Poco::Net::WebSocketException&)
		{
		}
		assertEqual(static_cast<int>(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR), static_cast<int>(response.getStatus()));
		assertEqual(0, static_cast<int>(response.getContentLength()));
	}
	pImpl->requestReceived -= delegate(&failer, &WebSocketErrorRequestHandler::onRequest);

	pImpl->stop();
	server.stopAll(true);
	server.stop();
}


void WebEventServiceTest::testNotifyUserWithoutUserSendsNothing()
	/// notifyUser() with an empty user name reaches nobody, also not the
	/// connections that belong to no user.
{
	const std::string subject("com.example.parent.child.leaf");

	_pService->subscriptionRequestReceived += delegate(this, &WebEventServiceTest::onSubscriptionReceived);
	_subscriptionHandlerAdded = true;

	Poco::Net::WebSocket ws = connectWebSocket(_port);
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	sendSubscribe(ws, subject);
	assertTrue(_childSubscriptionEvent.tryWait(2000));

	_pService->notifyUser("", subject, "for nobody");
	_pService->notify(subject, "marker");
	WebEventService::NotificationEvent notification = receiveNotification(ws);
	assertEquals(std::string("marker"), notification.second);

	try
	{
		ws.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		ws.close();
	}
	catch (...) {}
	stopService();
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
	// Timing-dependent by nature: it runs for a fixed five seconds against
	// the one-second sweep.
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.pingInterval = std::chrono::seconds(1);
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);

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
	WebEventServiceImpl::Config config = serviceConfig(2);
	config.pingInterval = std::chrono::seconds(1);
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);

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
	// Timing-dependent by nature: what it checks is that nothing happens
	// within a fixed four seconds.
	WebEventServiceImpl::Config config = serviceConfig(1);
	config.pingInterval = std::chrono::seconds(1);
	Poco::AutoPtr<TestableWebEventServiceImpl> pImpl = new TestableWebEventServiceImpl(Poco::OSP::BundleContext::Ptr(), config);

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

	// The drainer token must have recovered: a fresh notify still arrives,
	// and as the next frame, so nothing was left queued behind v3.
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
	// Nothing else: the next frame is a later notification.
	local.pService->notify(parentSubject + ".marker", "marker");
	WebEventService::NotificationEvent marker = receiveNotification(ws);
	assertEquals(parentSubject + ".marker", marker.first);

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

	// The fast client must not wait behind the stalled one, which would
	// hold it for the ten seconds of the send timeout.
	Poco::Stopwatch fastWatch;
	fastWatch.start();
	local.pService->notify(fastSubject, payload);
	WebEventService::NotificationEvent notification = receiveNotification(fastSocket);
	assertTrue(fastWatch.elapsedSeconds() < 2);
	assertEquals(fastSubject, notification.first);
	assertEquals(payload, notification.second);

	// Unblock the stalled drainer quickly so teardown does not wait for
	// the full send timeout.
	try
	{
		slowSocket.close();
	}
	catch (...) {}

	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(slowSubject) == 0; }, 15000));

	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	try
	{
		fastSocket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		fastSocket.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testSlowReaderIsRemovedAfterSendTimeout()
	/// A subscriber that takes a frame a few bytes at a time restarts the
	/// timeout of every send call, and would hold a worker for as long as
	/// it keeps reading. It is removed once the frame has not been sent
	/// within the send timeout of ten seconds.
	/// Timing-dependent by nature: it runs for at least the send timeout.
{
	const std::string subject("com.example.slow");
	const std::string bigPayload(std::size_t{32}*1024*1024, 'x');

	SubscriptionWaiter waiter;
	LocalService local(1, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	Poco::Net::WebSocket ws = connectWebSocket(local.port);
	sendSubscribe(ws, subject);
	assertTrue(waiter.event.tryWait(2000));
	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);
#if defined(POCO_OS_FAMILY_WINDOWS)
	// Windows takes a frame of any size into the buffers of a socket that has
	// a send buffer, so that the send returns at once; it blocks only without one.
	TestableWebEventServiceImpl::Subscriber::Ptr pSubscriber;
	assertTrue(local.pService->firstSubscriber(pSubscriber));
	pSubscriber->pWebSocket->setSendBufferSize(0);
#endif

	local.pService->notify(subject, bigPayload);

	// A few kilobytes every tenth of a second: far too slow for the frame,
	// but progress for every send call.
	ws.setReceiveTimeout(Poco::Timespan(0, 100000));
	bool removed = false;
	Poco::Stopwatch sw;
	sw.start();
	while (!removed && sw.elapsedSeconds() < 20)
	{
		try
		{
			std::array<char, 4096> buffer{};
			if (ws.impl()->Poco::Net::SocketImpl::receiveBytes(buffer.data(), static_cast<int>(buffer.size())) <= 0) break;
		}
		catch (Poco::TimeoutException&)
		{
		}
		catch (Poco::Net::NetException&)
		{
			break;
		}
		Poco::Thread::sleep(100);
		removed = local.pService->subscriberCount(subject) == 0;
	}
	assertTrue(waitUntil([&]{ return local.pService->subscriberCount(subject) == 0; }, 5000));
	assertTrue(sw.elapsedSeconds() < 20);

	try
	{
		ws.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testTricklingSenderIsRemovedAfterReceiveTimeout()
	/// A client that sends a frame one byte at a time restarts the timeout
	/// of every receive call, and would hold a worker for as long as it
	/// keeps sending. It is removed once the frame has not been received
	/// within ten seconds.
	/// Timing-dependent by nature: it runs for at least that time.
{
	const std::string subject("com.example.trickle");

	SubscriptionWaiter waiter;
	LocalService local(1, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	Poco::Net::StreamSocket raw = connectRawWebSocket(local.port);
	const std::string subscribe = maskedTextFrame("SUBSCRIBE " + subject + " WebEvent/1.0\r\n");
	raw.sendBytes(subscribe.data(), static_cast<int>(subscribe.size()));
	assertTrue(waiter.event.tryWait(2000));
	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	// The header first, then one byte every half second: far too slow for
	// the frame, but within the timeout of every receive call.
	const std::string frame = maskedTextFrame(std::string(100, 'x'));
	raw.sendBytes(frame.data(), 6);
	bool removed = false;
	Poco::Stopwatch sw;
	sw.start();
	for (std::size_t i = 6; i < frame.size() && !removed && sw.elapsedSeconds() < 20; ++i)
	{
		try
		{
			raw.sendBytes(&frame[i], 1);
		}
		catch (Poco::Exception&)
		{
		}
		removed = waitUntil([&]{ return local.pService->subscriberCount(subject) == 0; }, 500);
	}
	assertTrue(removed);
	assertTrue(sw.elapsedSeconds() < 20);

	try
	{
		raw.close();
	}
	catch (...) {}
	assertEqual(1, local.stop());
}


void WebEventServiceTest::testTricklingCloseDoesNotHoldShutdown()
	/// The wait for a client's CLOSE confirmation has one deadline of two
	/// seconds, which a client that sends its CLOSE a byte at a time cannot
	/// extend: the only worker is free again, and another subscriber's
	/// notification is delivered, within about three seconds (the deadline
	/// and the one-second check that enforces it).
	/// Timing-dependent by nature: it waits for the deadline.
{
	const std::string trickleSubject("com.example.trickle");
	const std::string otherSubject("com.example.other");

	SubscriptionWaiter waiter;
	LocalService local(1, 0);
	local.pService->subscriptionRequestReceived += delegate(&waiter, &SubscriptionWaiter::onSubscription);

	Poco::Net::StreamSocket raw = connectRawWebSocket(local.port);
	raw.setReceiveTimeout(Poco::Timespan(5, 0));
	const std::string subscribe = maskedTextFrame("SUBSCRIBE " + trickleSubject + " WebEvent/1.0\r\n");
	raw.sendBytes(subscribe.data(), static_cast<int>(subscribe.size()));
	assertTrue(waiter.event.tryWait(2000));

	Poco::Net::WebSocket other = connectWebSocket(local.port);
	other.setReceiveTimeout(Poco::Timespan(10, 0));
	sendSubscribe(other, otherSubject);
	assertTrue(waiter.event.tryWait(2000));
	local.pService->subscriptionRequestReceived -= delegate(&waiter, &SubscriptionWaiter::onSubscription);

	// An invalid message: the server closes the connection actively and
	// waits for the client's CLOSE.
	const std::string invalid = maskedTextFrame("HELLO " + trickleSubject + " WebEvent/1.0");
	raw.sendBytes(invalid.data(), static_cast<int>(invalid.size()));
	assertTrue(receivesCloseFrame(raw));

	// A CLOSE with the largest control payload, a byte every fifth of a
	// second: it would take more than 25 seconds to arrive.
	std::string close;
	close += static_cast<char>(0x88);
	close += static_cast<char>(0x80 | 125);
	close.append(4, '\x5a');
	close.append(125, 'x');
	std::atomic<bool> stopTrickle{false};
	std::thread trickler([&]
	{
		for (std::size_t i = 0; i < close.size() && !stopTrickle; ++i)
		{
			try
			{
				raw.sendBytes(&close[i], 1);
			}
			catch (Poco::Exception&)
			{
				break;
			}
			Poco::Thread::sleep(200);
		}
	});

	Poco::Stopwatch sw;
	sw.start();
	local.pService->notify(otherSubject, "through");
	bool delivered = false;
	try
	{
		delivered = receiveNotification(other).second == "through";
	}
	catch (Poco::Exception&)
	{
	}
	const Poco::Timestamp::TimeDiff elapsed = sw.elapsed();
	stopTrickle = true;
	trickler.join();

	assertTrue(delivered);
	assertTrue(elapsed < 4*Poco::Timespan::SECONDS);

	try
	{
		other.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
		other.close();
		raw.close();
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
	bobSocket.setReceiveTimeout(Poco::Timespan(5, 0));
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

	// The first frame bob receives is the one sent to him afterwards.
	_pService->notifyUser("bob", subject, "marker");
	notification = receiveNotification(bobSocket);
	assertEquals(std::string("marker"), notification.second);

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
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
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

	// hammer subjects are unrelated to the subscription: the first frame
	// delivered is a later notification of the subscribed subject
	_pService->notify(subject, "marker");
	WebEventService::NotificationEvent marker = receiveNotification(ws);
	assertEquals(subject, marker.first);
	assertEquals(std::string("marker"), marker.second);

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
	CppUnit_addTest(pSuite, WebEventServiceTest, testClientPingIsAnsweredWithPong);
	CppUnit_addTest(pSuite, WebEventServiceTest, testThrowingDelegateDoesNotStopDelivery);
	CppUnit_addTest(pSuite, WebEventServiceTest, testNotificationIsHandedOnWithoutPollDelay);
	CppUnit_addTest(pSuite, WebEventServiceTest, testCrossOriginUpgradeIsRejected);
	CppUnit_addTest(pSuite, WebEventServiceTest, testAllowedOriginsAreParsedTrimmedAndLowered);
	CppUnit_addTest(pSuite, WebEventServiceTest, testClientNotifyIsDroppedByDefault);
	CppUnit_addTest(pSuite, WebEventServiceTest, testClientNotifyReachesSubscribersWhenEnabled);
	CppUnit_addTest(pSuite, WebEventServiceTest, testConnectionLimitHoldsForConcurrentUpgrades);
	CppUnit_addTest(pSuite, WebEventServiceTest, testUpgradeAfterStopIsClosed);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSubscribingBeyondSubjectLimitClosesConnection);
	CppUnit_addTest(pSuite, WebEventServiceTest, testWorkerCountIsDefaultedAndLimited);
	CppUnit_addTest(pSuite, WebEventServiceTest, testInvalidMessagesCloseTheConnection);
	CppUnit_addTest(pSuite, WebEventServiceTest, testSubjectListKeepsCommasInBrackets);
	CppUnit_addTest(pSuite, WebEventServiceTest, testUnsubscribeRemovesSubjects);
	CppUnit_addTest(pSuite, WebEventServiceTest, testUpgradeRequestHandlerChecks);
	CppUnit_addTest(pSuite, WebEventServiceTest, testOtherWebSocketErrorIsAnswered);
	CppUnit_addTest(pSuite, WebEventServiceTest, testNotifyUserWithoutUserSendsNothing);
	CppUnit_addTest(pSuite, WebEventServiceTest, testZeroPingIntervalDisablesKeepalive);
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
	CppUnit_addTest(pSuite, WebEventServiceTest, testSlowReaderIsRemovedAfterSendTimeout);
	CppUnit_addTest(pSuite, WebEventServiceTest, testTricklingSenderIsRemovedAfterReceiveTimeout);
	CppUnit_addTest(pSuite, WebEventServiceTest, testTricklingCloseDoesNotHoldShutdown);
	CppUnit_addTest(pSuite, WebEventServiceTest, testNotifyUserDeliversOnlyToUser);
	CppUnit_addTest(pSuite, WebEventServiceTest, testConcurrentApiAccess);

	return pSuite;
}
