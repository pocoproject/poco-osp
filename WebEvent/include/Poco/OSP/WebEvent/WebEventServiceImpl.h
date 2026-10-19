//
// WebEventServiceImpl.h
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventServiceImpl
//
// Definition of the WebEventServiceImpl interface.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_WebEvent_WebEventServiceImpl_INCLUDED
#define OSP_WebEvent_WebEventServiceImpl_INCLUDED


#include "Poco/OSP/WebEvent/WebEventService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/RefCountedObject.h"
#include "Poco/Clock.h"
#include "Poco/Logger.h"
#include "Poco/AutoPtr.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"
#include <atomic>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <unordered_map>
#include <memory>


namespace Poco {
namespace OSP {
namespace WebEvent {


class OSPWebEvent_API WebEventServiceImpl;


class OSPWebEvent_API WebEventTaskNotification: public Poco::Notification
{
public:
	using Ptr = Poco::AutoPtr<WebEventTaskNotification>;

	WebEventTaskNotification(WebEventServiceImpl& service):
		_service(service)
	{
	}

	virtual ~WebEventTaskNotification()
	{
	}

	virtual void execute() = 0;

protected:
	WebEventServiceImpl& _service;
};


class OSPWebEvent_API WebEventServiceImpl: public WebEventService
{
public:
	using Ptr = Poco::AutoPtr<WebEventServiceImpl>;

	static constexpr int DEFAULT_MAX_PENDING_FRAMES = 256;

	static int defaultWorkerCount();

	WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext = nullptr, int maxSockets = 0, int workerCount = defaultWorkerCount(), int maxPendingFrames = DEFAULT_MAX_PENDING_FRAMES, int pingIntervalSeconds = 60);
		/// Creates the WebEventServiceImpl.
		///
		/// maxPendingFrames limits the number of outbound frames queued
		/// per subscriber; 0 means unlimited.
		///
		/// Every pingIntervalSeconds, subscribers that have not sent anything
		/// for at least half that long are sent a WebSocket PING frame
		/// (browsers reply with a PONG automatically); subscribers silent for
		/// twice the interval are considered dead and removed. Pass 0 to
		/// disable the keepalive sweep.

	~WebEventServiceImpl();
		/// Destroys the WebEventServiceImpl, stopping it first.

	void stop();
		/// Stops the service: no further notifications or subscribers are
		/// accepted, every subscriber's socket is shut down, and the main
		/// and worker threads are joined before this returns. Idempotent.
		///
		/// Must not be called from a delegate of the service's events or
		/// from one of its own threads, which would join themselves.

	bool stopped() const;
		/// Returns true once stop() has begun.

	// WebEventService
	Poco::BasicEvent<const NotificationEvent>& subjectNotified(const std::string& subject);
	void notify(const std::string& subjectName, const std::string& data);
	void notifyUser(const std::string& username, const std::string& subjectName, const std::string& data);
	void setUserResolver(UserResolver resolver);
	int subscriberCount(const std::string& subject);
	SubscriberInfoVec listSubscribers();

	std::string resolveUser(const Poco::Net::HTTPServerRequest& request) const;
		/// Invokes the installed user resolver for the given WebSocket
		/// upgrade request; returns an empty string if no resolver is
		/// installed or the resolver throws.

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

protected:
	struct Subscriber: public Poco::RefCountedObject
	{
		using Ptr = Poco::AutoPtr<Subscriber>;

		struct PendingFrame
		{
			std::string subject;
				/// Published subject, used as coalescing key.
				/// An empty subject is never coalesced (pong replies).
			std::shared_ptr<const std::string> pFrame;
				/// Complete NOTIFY frame, shared across subscribers.
			int flags = Poco::Net::WebSocket::FRAME_TEXT;
				/// WebSocket frame flags; keepalive PINGs use a control opcode.
		};
		using PendingList = std::list<PendingFrame>;

		Poco::SharedPtr<Poco::Net::WebSocket> pWebSocket;
		std::set<std::string> subjectNames;
		std::string username;
		std::string page;
		std::string clientAddress;
		Poco::Clock lastActive;
			/// Main-thread only: refreshed whenever the socket is found
			/// readable, and whenever it is re-armed for reading after a
			/// received frame (see watchSocketImpl()). Monotonic, so that a
			/// correction of the system clock cannot mark every subscriber
			/// dead at once, nor postpone the keepalive indefinitely.

		Poco::FastMutex queueMutex;
			/// Guards the queue state below; never held across I/O.
		PendingList pending;
		std::unordered_map<std::string, PendingList::iterator> bySubject;
		bool draining = false;
			/// A SendNotification for this subscriber is queued or executing.
		bool closed = false;
			/// No further enqueues; the drainer stops.
		Poco::UInt64 enqueuedFrames = 0;
			/// Frames accepted for this subscriber, counted before any
			/// coalescing: one notification must add exactly one, however
			/// many of its subscriptions the subject matched.
		Poco::UInt64 droppedFrames = 0;
		Poco::Clock lastDropWarning{0};
		Poco::Clock lastPingSent{0};
			/// When a keepalive PING was last handed to the socket, as
			/// opposed to merely queued. Written by the worker that sends
			/// it, read by the sweep, both under queueMutex.
		bool pingPending = false;
			/// A keepalive PING is queued for this subscriber but has not
			/// left yet, so the sweep does not queue another.

		Poco::FastMutex writeMutex;
			/// Serializes socket writes: drainer sendFrame vs. shutdownImpl close frame.
		Poco::FastMutex readMutex;
			/// Serializes socket reads and close(): receiveImpl vs. the
			/// confirmation read in shutdownImpl. Lock order is writeMutex
			/// before readMutex; receiveImpl takes readMutex only.
	};
	using SubjectMap = std::multimap<std::string, Subscriber::Ptr>;
	using SocketMap = std::map<Poco::Net::Socket, Subscriber::Ptr>;

	void preflightRequest(Poco::Net::HTTPServerRequest& request);
	void runMain();
	void runWorker();
	void addSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username = std::string(), const std::string& page = std::string(), const std::string& clientAddress = std::string());
	void removeSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown);
	void subscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void unsubscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void enqueueSend(Subscriber::Ptr pSubscriber, const std::string& subject, std::shared_ptr<const std::string> pFrame, int flags = Poco::Net::WebSocket::FRAME_TEXT);
	void receive(Subscriber::Ptr pSubscriber);
	void shutdown(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive);

	void notifyImpl(const std::string& subjectName, const std::string& data);
	void notifyUserImpl(const std::string& username, const std::string& subjectName, const std::string& data);
	std::vector<Subscriber::Ptr> collectSubscribers(const std::string& subjectName, const std::string& username = std::string());
		/// Returns, once each, the subscribers of subjectName and of every
		/// ancestor subject, restricted to username when it is not empty.
		/// Must run on the main thread.
	void addSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress);
	void listSubscribersImpl(SubscriberInfoVec& subscribers);
	void removeSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown);
	void subscribeImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void unsubscribeImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void sendPending(Subscriber::Ptr pSubscriber);
	void pingIdleSubscribersImpl();
	bool pingSweepDueImpl() const;
		/// Returns true if a keepalive sweep is due. Read by the main
		/// thread between notifications, so that a queue which stays busy
		/// cannot hold off the sweep indefinitely.
	void receiveImpl(Subscriber::Ptr pSubscriber);
	void shutdownImpl(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive);
	void stopImpl();
		/// Runs on the main thread once its loop has ended: closes out
		/// every subscriber and clears the socket and subject maps.
	void watchSocketImpl(const Poco::Net::Socket& socket);
	void unwatchSocketImpl(const Poco::Net::Socket& socket);
	void removeBadSocketsImpl();
	Poco::Net::Socket::SocketList& socketList();
	SocketMap& socketMap();
	Poco::NotificationQueue& mainQueue();
	Poco::NotificationQueue& workerQueue();

private:
	static const std::string SUBSCRIBE;
	static const std::string UNSUBSCRIBE;
	static const std::string NOTIFY;
	static const std::string VERSION;
	static const std::string SYSTEM_PING;
	static const std::string SYSTEM_PONG;

	enum
	{
		WEBEVENT_RECEIVE_TIMEOUT = 5,
		WEBEVENT_SEND_TIMEOUT = 10,
		WEBEVENT_SHUTDOWN_TIMEOUT = 2,
		WEBEVENT_LIST_SUBSCRIBERS_TIMEOUT = 5,
		WEBEVENT_LIST_SUBSCRIBERS_POLL_MS = 50,
		WEBEVENT_DROP_WARNING_INTERVAL = 5,
		WEBEVENT_QUEUE_WAIT_MS = 1000
	};

	using Event = Poco::BasicEvent<const NotificationEvent>;
	using EventPtr = Poco::SharedPtr<Event>;
	using EventMap = std::multimap<std::string, EventPtr>;
	using ThreadPtr = Poco::SharedPtr<Poco::Thread>;
	using ThreadVec = std::vector<ThreadPtr>;

	Poco::OSP::BundleContext::Ptr _pContext;
	int _maxSockets;
	int _maxPendingFrames;
	int _pingInterval;
	Poco::Clock _lastPingSweep;
	std::atomic<int> _socketCount;
	Poco::FastMutex _mapMutex;
		/// Guards _subjectMap and _eventMap where they are accessed from
		/// outside the main thread (subscriberCount, subjectNotified);
		/// never held while invoking delegates.
	SubjectMap _subjectMap;
	EventMap _eventMap;
	SocketMap  _socketMap;
	Poco::Net::Socket::SocketList _socketList;
	Poco::NotificationQueue _mainQueue;
	Poco::NotificationQueue _workerQueue;
	Poco::Thread _mainThread;
	ThreadVec _workerThreads;
	Poco::RunnableAdapter<WebEventServiceImpl> _mainRunnable;
	Poco::RunnableAdapter<WebEventServiceImpl> _workerRunnable;
	UserResolver _userResolver;
	mutable Poco::FastMutex _userResolverMutex;
	std::atomic<bool> _stopped;
	Poco::FastMutex _stopMutex;
	Poco::Logger& _logger;

	friend class WebEventRequestHandler;
	friend class AddSubscriberNotification;
	friend class RemoveSubscriberNotification;
	friend class SubscribeNotification;
	friend class UnsubscribeNotification;
	friend class NotifyNotification;
	friend class NotifyUserNotification;
	friend class SendNotification;
	friend class ReceiveNotification;
	friend class ShutdownNotification;
	friend class WatchSocketNotification;
	friend class ListSubscribersNotification;
	friend class StopNotification;

	Poco::Logger& logger() const;
};


} } } // namespace Poco::OSP::WebEvent


#endif // OSP_WebEvent_WebEventServiceImpl_INCLUDED
