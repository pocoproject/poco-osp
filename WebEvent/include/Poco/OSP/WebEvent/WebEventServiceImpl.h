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
#include "Poco/Net/PollSet.h"
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
#include <chrono>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <unordered_map>
#include <memory>


namespace Poco::OSP::WebEvent {


class OSPWebEvent_API WebEventServiceImpl;


class OSPWebEvent_API WebEventTaskNotification: public Poco::Notification
	/// A task run on the main thread or a worker thread of a
	/// WebEventServiceImpl. Part of the interface for subclasses of
	/// WebEventServiceImpl (see there), not a stable interface.
{
public:
	using Ptr = Poco::AutoPtr<WebEventTaskNotification>;

	WebEventTaskNotification(WebEventServiceImpl& service):
		_service(service)
	{
	}

	~WebEventTaskNotification() override = default;

	virtual void execute() = 0;

protected:
	WebEventServiceImpl& _service;
};


class OSPWebEvent_API WebEventServiceImpl: public WebEventService
{
public:
	using Ptr = Poco::AutoPtr<WebEventServiceImpl>;

	static constexpr int DEFAULT_MAX_PENDING_FRAMES = 256;
	static constexpr int DEFAULT_MAX_SUBJECTS = 64;
	static constexpr int MAX_WORKER_COUNT = 64;
		/// The largest number of worker threads; a larger workerCount is
		/// reduced to it.

	[[nodiscard]] static int defaultWorkerCount();

	struct Config
		/// The settings of a WebEventServiceImpl.
	{
		int maxSockets = 0;
			/// The largest number of WebSocket connections; 0 means
			/// unlimited.

		int workerCount = 0;
			/// The number of worker threads, at most MAX_WORKER_COUNT; 0 or
			/// less means defaultWorkerCount().

		int maxPendingFrames = DEFAULT_MAX_PENDING_FRAMES;
			/// The largest number of outbound frames queued per subscriber;
			/// 0 means unlimited.

		std::chrono::seconds pingInterval{60};
			/// Every interval, subscribers that have not sent anything for at
			/// least half that long are sent a WebSocket PING frame (browsers
			/// reply with a PONG automatically); subscribers silent for twice
			/// the interval are considered dead and removed. 0 disables the
			/// keepalive sweep.

		bool clientNotify = false;
			/// If true, a NOTIFY message sent by a client is passed on like a
			/// call to notify(); otherwise such a message is dropped. A NOTIFY
			/// of the subject "system.ping" is answered either way.

		int maxSubjects = DEFAULT_MAX_SUBJECTS;
			/// The largest number of subjects one subscriber can subscribe
			/// to; a SUBSCRIBE beyond it closes the connection. 0 means
			/// unlimited.

		std::chrono::milliseconds pollTimeout{1000};
			/// The longest time the main thread waits for its sockets when
			/// nothing arrives, before it looks again at the keepalive sweep
			/// and the stalled sends and receives. Queued tasks wake it at
			/// once.
	};

	explicit WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext = nullptr);
		/// Creates the WebEventServiceImpl with the default Config.

	WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext, const Config& config);
		/// Creates the WebEventServiceImpl with the given Config.

	~WebEventServiceImpl() override;
		/// Destroys the WebEventServiceImpl, stopping it first.

	void stop();
		/// Stops the service: no further notifications or subscribers are
		/// accepted, every subscriber's socket is shut down, and the main
		/// and worker threads are joined before this returns. Idempotent.
		///
		/// Must not be called from a delegate of the service's events or
		/// from one of its own threads, which would join themselves.

	[[nodiscard]] bool stopped() const;
		/// Returns true once stop() has begun.

	// WebEventService
	Poco::BasicEvent<const NotificationEvent>& subjectNotified(const std::string& subject) override;
	void notify(const std::string& subjectName, const std::string& data) override;
	void notifyUser(const std::string& username, const std::string& subjectName, const std::string& data) override;
	void setUserResolver(UserResolver resolver) override;
	[[nodiscard]] int subscriberCount(const std::string& subject) override;
	SubscriberInfoVec listSubscribers() override;

	[[nodiscard]] std::string resolveUser(const Poco::Net::HTTPServerRequest& request) const;
		/// Invokes the installed user resolver for the given WebSocket
		/// upgrade request; returns an empty string if no resolver is
		/// installed or the resolver throws.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

protected:
	// The protected members are the interface for subclasses that inspect or
	// drive the service's threads, such as test doubles. They expose the
	// threading model and change with it; they are not a stable interface.

	struct Subscriber: public Poco::RefCountedObject
		/// The state of one connected subscriber, shared between the main
		/// thread and the workers as described for each member.
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
		bool sending = false;
		Poco::Clock sendStarted;
			/// A worker is sending a frame to this subscriber, since
			/// sendStarted; guarded by queueMutex.
		bool receiving = false;
		Poco::Clock receiveStarted;
			/// A worker is receiving a frame from this subscriber, since
			/// receiveStarted; guarded by queueMutex.
		bool watched = false;
			/// Main-thread only: the socket is in the poll set. It is taken
			/// out while a worker reads a frame from it.

		Poco::FastMutex writeMutex;
			/// Serializes socket writes: drainer sendFrame vs. shutdownImpl close frame.
		Poco::FastMutex readMutex;
			/// Serializes socket reads and close(): receiveImpl vs. the
			/// confirmation read in shutdownImpl. Lock order is writeMutex
			/// before readMutex; receiveImpl takes readMutex only.
	};
	using SocketMap = std::map<Poco::Net::Socket, Subscriber::Ptr>;

	void shutdown(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive);
		/// Queues the shutdown of the subscriber's connection for a worker.
		/// With passive, the client's confirmation is not waited for.

	void enqueueMain(const WebEventTaskNotification::Ptr& pTask);
		/// Queues the task for the main thread and wakes the thread up.

	[[nodiscard]] SocketMap& socketMap();
		/// Returns the subscribers by socket. Main thread only.

	[[nodiscard]] Poco::NotificationQueue& mainQueue();
		/// Returns the queue of the main thread. Tasks are queued with
		/// enqueueMain(), which also wakes the thread up.

	[[nodiscard]] Poco::NotificationQueue& workerQueue();
		/// Returns the queue the worker threads take their tasks from.

private:
	using SubjectMap = std::multimap<std::string, Subscriber::Ptr>;

	void preflightRequest(Poco::Net::HTTPServerRequest& request);
		/// Takes a connection slot for the upgrade request, or sets the
		/// response status to 503 if none is free. A slot taken is handed
		/// to addSubscriber() or given back with releaseSocketSlot().
	void releaseSocketSlot();
	void runMain();
	void runWorker();
	void addSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username = std::string(), const std::string& page = std::string(), const std::string& clientAddress = std::string());
	void removeSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown);
	void subscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void unsubscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void enqueueSend(Subscriber::Ptr pSubscriber, const std::string& subject, std::shared_ptr<const std::string> pFrame, int flags = Poco::Net::WebSocket::FRAME_TEXT);
	void receive(Subscriber::Ptr pSubscriber);

	void notifyImpl(const std::string& subjectName, const std::string& data);
	void notifyUserImpl(const std::string& username, const std::string& subjectName, const std::string& data);
	[[nodiscard]] std::vector<Subscriber::Ptr> collectSubscribers(const std::string& subjectName,
		const std::string& username = std::string());
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
	[[nodiscard]] bool stallCheckDueImpl() const;
	void removeStalledSubscribersImpl();
		/// Removes the subscribers to which a frame has not been sent
		/// within WEBEVENT_SEND_TIMEOUT, or from which a frame has not been
		/// received within WEBEVENT_FRAME_RECEIVE_TIMEOUT, ending the send
		/// or receive that holds a worker. Must run on the main thread.
	[[nodiscard]] bool pingSweepDueImpl() const;
		/// Returns true if a keepalive sweep is due. Read by the main
		/// thread between notifications, so that a queue which stays busy
		/// cannot hold off the sweep indefinitely.
	void receiveImpl(Subscriber::Ptr pSubscriber);
	void shutdownImpl(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive);
	void stopImpl();
		/// Runs on the main thread once its loop has ended: closes out
		/// every subscriber and clears the socket and subject maps.
	void shutdownSocket(const Poco::Net::Socket& socket);
		/// Shuts the socket down in both directions, which ends a send or
		/// receive another thread is blocked in. An error is logged at
		/// debug level only.
	void confirmShutdownImpl(Subscriber& subscriber);
		/// Waits, for at most WEBEVENT_SHUTDOWN_TIMEOUT, for the client's
		/// CLOSE frame. Runs on a worker, which holds both socket locks.
	void watchSocketImpl(const Poco::Net::Socket& socket);
	void unwatchSocketImpl(const Poco::Net::Socket& socket);
	void removeBadSocketsImpl();
	void wakeUpMain();

	static const std::string SUBSCRIBE;
	static const std::string UNSUBSCRIBE;
	static const std::string NOTIFY;
	static const std::string VERSION;
	static const std::string SYSTEM_PING;
	static const std::string SYSTEM_PONG;

	static constexpr Poco::Clock::ClockDiff WEBEVENT_LIST_SUBSCRIBERS_TIMEOUT = 5;
	static constexpr long WEBEVENT_LIST_SUBSCRIBERS_POLL_MS = 50;
	static constexpr Poco::Clock::ClockDiff WEBEVENT_DROP_WARNING_INTERVAL = 5;
	static constexpr long WEBEVENT_QUEUE_WAIT_MS = 1000;

	static constexpr int WEBEVENT_MAX_FRAME_PAYLOAD = 4096;
		/// Largest frame accepted from a client; a larger one is a
		/// protocol violation.

	static constexpr std::chrono::seconds WEBEVENT_RECEIVE_TIMEOUT{5};
		/// Timeout of each receive call on a subscriber's socket.

	static constexpr std::chrono::seconds WEBEVENT_SEND_TIMEOUT{10};
		/// Timeout of each send call on a subscriber's socket, and the
		/// longest time to send a whole frame.

	static constexpr std::chrono::seconds WEBEVENT_STALL_CHECK_INTERVAL{1};
		/// How often the main thread looks for sends and receives that have
		/// taken too long.

	static constexpr std::chrono::seconds WEBEVENT_FRAME_RECEIVE_TIMEOUT{10};
		/// Longest time to receive a whole frame. WEBEVENT_RECEIVE_TIMEOUT
		/// bounds each receive call only, which a client restarts with
		/// every byte it sends.

	static constexpr std::chrono::seconds WEBEVENT_SHUTDOWN_TIMEOUT{2};
		/// Longest time a worker waits for a client to confirm the close of
		/// its connection.

	struct ClosingSocket
		/// A socket whose worker waits for the client's CLOSE confirmation.
	{
		Poco::Net::Socket socket;
		Poco::Clock deadline;
		bool shutDown = false;
			/// The watchdog has shut the socket down.
	};
	using ClosingList = std::list<ClosingSocket>;

	using Event = Poco::BasicEvent<const NotificationEvent>;
	using EventPtr = Poco::SharedPtr<Event>;
	using EventMap = std::multimap<std::string, EventPtr>;
	using ThreadPtr = Poco::SharedPtr<Poco::Thread>;
	using ThreadVec = std::vector<ThreadPtr>;

	Poco::OSP::BundleContext::Ptr _pContext;
	int _maxSockets;
	int _maxPendingFrames;
	int _pingInterval;
	Poco::Timespan _pollTimeout;
	bool _clientNotify;
	int _maxSubjects;
	Poco::Clock _lastPingSweep;
	Poco::Clock _lastStallCheck;
	std::atomic<int> _socketCount;
		/// Connection slots taken: subscribers, and upgrades on their way.
	ClosingList _closingSockets;
	Poco::FastMutex _closingMutex;
		/// Guards _closingSockets. Held while the watchdog shuts one of
		/// them down, so that it never does so after the worker has closed
		/// the socket.
	Poco::FastMutex _mapMutex;
		/// Guards _subjectMap and _eventMap where they are accessed from
		/// outside the main thread (subscriberCount, subjectNotified);
		/// never held while invoking delegates.
	SubjectMap _subjectMap;
	EventMap _eventMap;
	SocketMap  _socketMap;
	std::unique_ptr<Poco::Net::PollSet> _pPollSet;
		/// Main-thread only, except for wakeUp(): the watched sockets.
	Poco::FastMutex _pollSetMutex;
		/// Guards the replacement of _pPollSet against wakeUpMain().
	std::atomic<bool> _wakeUpPending;
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

	[[nodiscard]] static std::shared_ptr<const std::string> makeNotifyFrame(const std::string& subjectName, const std::string& data);
		/// Returns the NOTIFY message for the given subject and data, to be
		/// shared by every subscriber it is sent to.

	[[nodiscard]] static std::string describe(const Subscriber& subscriber);
		/// Returns the client address of the subscriber, and its user if it
		/// has one, for log messages.

	[[nodiscard]] Poco::Logger& logger() const;
};


} // namespace Poco::OSP::WebEvent


#endif // OSP_WebEvent_WebEventServiceImpl_INCLUDED
