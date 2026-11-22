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
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_WebEvent_WebEventServiceImpl_INCLUDED
#define OSP_WebEvent_WebEventServiceImpl_INCLUDED


#include "Poco/OSP/WebEvent/WebEventService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/PollSet.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/RefCountedObject.h"
#include "Poco/Clock.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/AutoPtr.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"
#include "Poco/RWLock.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
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

	static void checkBundleProperties(const Poco::Util::AbstractConfiguration& bundleProperties,
		const Poco::Util::AbstractConfiguration& configuration, Poco::Logger& logger);
		/// Checks the properties of the WebEvent bundle, which include those
		/// of its extension bundles, for the bundle properties permission and
		/// websession.id, which the bundle does not read. Logs an error for
		/// each that names the setting to use instead: osp.web.event.permission
		/// or osp.web.event.session. Throws a Poco::InvalidArgumentException if
		/// permission is set and osp.web.event.permission is set neither in the
		/// configuration nor in the bundle properties, so that the bundle does
		/// not start without the restriction it is configured to apply.

	struct OSPWebEvent_API Config
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
			/// 0 means unlimited. Beyond it, the oldest queued notification
			/// is dropped. The keepalive PING and the PONG to the client's
			/// latest PING count against it but are never dropped.

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

		bool requireSubjectAuthorizer = false;
			/// If true, every subject of a client's SUBSCRIBE and NOTIFY is
			/// refused while no subject authorizer is installed, as a refusal
			/// of the authorizer; otherwise a client may then subscribe to
			/// and notify every subject.

		std::chrono::milliseconds pollTimeout{1000};
			/// The longest time the main thread waits for its sockets when
			/// nothing arrives, before it looks again at the keepalive sweep
			/// and the stalled sends and receives. Queued tasks wake it at
			/// once.

		[[nodiscard]] static Config fromConfiguration(const Poco::Util::AbstractConfiguration& configuration);
			/// Returns the Config given by the osp.web.event properties workers,
			/// maxWebSockets, maxPendingFrames, pingInterval (in seconds),
			/// clientNotify, maxSubjects and requireSubjectAuthorizer; a property
			/// not given keeps its default.
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
	void setSubjectAuthorizer(SubjectAuthorizer authorizer) override;
	[[nodiscard]] int subscriberCount(const std::string& subject) override;
	SubscriberInfoVec listSubscribers() override;

	struct ConnectionIdentity
		/// The user and the scope that a WebSocket connection records.
	{
		std::string username;
			/// The user of the connection, as in SubscriberInfo; empty for a
			/// connection that belongs to no user.

		std::string scope;
			/// The scope of the bearer token that authenticated the upgrade
			/// request, as in SubscriberInfo; empty for other credentials, a
			/// session login and an unauthenticated request.
	};

	[[nodiscard]] ConnectionIdentity resolveUser(const Poco::Net::HTTPServerRequest& request) const;
		/// Returns the user and the scope of the connection of the given
		/// WebSocket upgrade request. The user is the answer of the installed
		/// user resolver, or an empty string if the resolver throws a
		/// Poco::Exception or a std::exception. Any other exception is
		/// passed on. Without a resolver, the user is the one that the web
		/// server has authenticated for the request
		/// (Poco::OSP::Auth::RequestIdentity), or an empty string if it has
		/// authenticated none. The scope is that of the request identity,
		/// with or without a resolver.

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
				/// Published subject, used as coalescing key; empty for
				/// control frames and system.pong replies, which it does not coalesce.
			std::shared_ptr<const std::string> pFrame;
				/// Complete NOTIFY frame, shared across subscribers.
			int flags = Poco::Net::WebSocket::FRAME_TEXT;
				/// WebSocket frame flags; keepalive PINGs use a control opcode.
		};
		using PendingList = std::list<PendingFrame>;

		Poco::SharedPtr<Poco::Net::WebSocket> pWebSocket;
		std::set<std::string> subjectNames;
		std::string username;
		std::string scope;
		std::string page;
		std::string clientAddress;
		Poco::Clock lastActive;
			/// Main-thread only: refreshed whenever the socket is found
			/// readable, and when a worker has handled a frame from it
			/// (see watchSocketImpl()). Monotonic, so that a
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
			/// Serializes socket reads and close(): receiveImpl and
			/// receiveNext vs. the confirmation read in shutdownImpl. Lock
			/// order is writeMutex before readMutex; receiveImpl and
			/// receiveNext take readMutex only.
	};
	using SocketMap = std::map<Poco::Net::Socket, Subscriber::Ptr>;

	void shutdown(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive);
		/// Queues the shutdown of the subscriber's connection for a worker.
		/// With passive, the client's confirmation is not waited for. A
		/// connection not closed within WEBEVENT_SHUTDOWN_TIMEOUT is shut down.

	void enqueueMain(const WebEventTaskNotification::Ptr& pTask);
		/// Queues the task for the main thread and wakes the thread up.

	[[nodiscard]] SocketMap& socketMap();
		/// Returns the subscribers by socket. Main thread only.

	[[nodiscard]] Poco::NotificationQueue& mainQueue();
		/// Returns the queue of the main thread. Tasks are queued with
		/// enqueueMain(), which also wakes the thread up.

	[[nodiscard]] Poco::NotificationQueue& workerQueue();
		/// Returns the queue the worker threads take their tasks from.

	virtual void setterWaiting();
		/// Called by setUserResolver() and setSubjectAuthorizer() while they
		/// hold the lock that makes new calls of the function they replace
		/// wait, before they wait for the calls in progress. Does nothing.
		///
		/// It is a hook for test doubles, which override it to learn that a
		/// setter has reached this point; other overrides are not supported.
		/// An override must not block, because new calls of the function wait
		/// until the setter returns, and must not call into the service, which
		/// can deadlock.

private:
	using SubjectMap = std::multimap<std::string, Subscriber::Ptr>;

	void preflightRequest(Poco::Net::HTTPServerRequest& request);
		/// Takes a connection slot for the upgrade request, or sets the
		/// response status to 503 if none is free. A slot taken is handed
		/// to addSubscriber() or given back with releaseSocketSlot().
	void releaseSocketSlot();

	enum class Rejection
		/// Why an upgrade request, or a subject of a client's message, is
		/// rejected.
	{
		ORIGIN,
		LIMIT,
		DELEGATE,
		SUBSCRIBE,
		NOTIFY,
		AUTHORIZER
	};
	static constexpr std::size_t REJECTION_REASON_COUNT = 6;
		/// The number of Rejection values.
	static_assert(static_cast<std::size_t>(Rejection::AUTHORIZER) + 1 == REJECTION_REASON_COUNT);

	void logRejection(Rejection rejection, const std::string& message, Poco::Message::Priority priority = Poco::Message::PRIO_NOTICE);
		/// Logs the rejection of an upgrade request or of a subject: at the
		/// given priority at most once every WEBEVENT_REJECTION_NOTICE_INTERVAL
		/// for each reason, otherwise at debug level, so that a client that
		/// retries in a loop does not fill the log.

	void runMain();
	void runWorker();
	void addSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username,
		const std::string& scope, const std::string& page, const std::string& clientAddress);
	void removeSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown);
	[[nodiscard]] bool authorizedSubject(const Subscriber& subscriber, const std::string& subject, SubjectAction action);
		/// Returns whether the subject authorizer lets the user of the
		/// subscriber do that with the subject, and for NOTIFY with each of
		/// its ancestors as well; false if the authorizer throws. Without an
		/// authorizer, returns false if osp.web.event.requireSubjectAuthorizer
		/// is true, true otherwise. A refusal is logged with logRejection(), at
		/// warning level if the authorizer throws.
	void subscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void unsubscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames);
	void enqueueSend(Subscriber::Ptr pSubscriber, const std::string& subject, std::shared_ptr<const std::string> pFrame, int flags = Poco::Net::WebSocket::FRAME_TEXT);
	void receive(Subscriber::Ptr pSubscriber);
	void receiveNext(Subscriber::Ptr pSubscriber);
		/// Hands the subscriber's socket back to the main thread after a worker
		/// has handled a frame, telling whether the next frame is already waiting.

	void notifyImpl(const std::string& subjectName, const std::string& data);
	void notifyUserImpl(const std::string& username, const std::string& subjectName, const std::string& data);
	[[nodiscard]] std::vector<Subscriber::Ptr> collectSubscribers(const std::string& subjectName,
		const std::string& username = std::string());
		/// Returns, once each, the subscribers of subjectName and of every
		/// ancestor subject, restricted, when username is not empty, to the
		/// connections of that user without a scope. Must run on the main
		/// thread.
	void addSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username,
		const std::string& scope, const std::string& page, const std::string& clientAddress);
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
		/// Shuts the socket's TCP connection down in both directions, ending a
		/// send or receive another thread is blocked in; there is no TLS close.
		/// An error is logged at debug level only.
	void confirmShutdownImpl(Subscriber& subscriber);
		/// Waits, for at most WEBEVENT_SHUTDOWN_TIMEOUT, for the client's
		/// CLOSE frame. If a frame is rejected instead, shuts down the send
		/// direction and discards what the client sends, up to
		/// WEBEVENT_SHUTDOWN_DISCARD_LIMIT bytes, until the client closes its
		/// side or the same deadline passes. Runs on a worker, which holds
		/// both socket locks.
	void closeImpl(Poco::Net::WebSocket& webSocket);
		/// Takes the socket off the watchdog's list, then closes it. Runs on
		/// a worker, which holds both socket locks.
	void watchSocketImpl(const Poco::Net::Socket& socket, bool buffered);
		/// If the socket is still subscribed, stamps its subscriber as heard from
		/// and, unless the socket is already watched, hands it to a worker if
		/// buffered (its next frame is already waiting), or adds it to the poll set.
	void unwatchSocketImpl(const Poco::Net::Socket& socket);
	void removeBadSocketsImpl();
	void wakeUpMain();

	static const std::string SUBSCRIBE;
	static const std::string UNSUBSCRIBE;
	static const std::string NOTIFY;
	static const std::string VERSION;
	static const std::string SYSTEM_PING;
	static const std::string SYSTEM_PONG;

	static constexpr std::chrono::seconds WEBEVENT_LIST_SUBSCRIBERS_TIMEOUT{5};
	static constexpr std::chrono::milliseconds WEBEVENT_LIST_SUBSCRIBERS_POLL{50};
	static constexpr std::chrono::seconds WEBEVENT_DROP_WARNING_INTERVAL{5};
	static constexpr std::chrono::milliseconds WEBEVENT_QUEUE_WAIT{1000};
	static constexpr std::chrono::seconds WEBEVENT_REJECTION_NOTICE_INTERVAL{5};

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
		/// Longest time from the removal of a subscriber to the close of its
		/// connection: for a send or receive still in flight, the CLOSE
		/// frame and the client's confirmation.

	static constexpr int WEBEVENT_SHUTDOWN_DISCARD_LIMIT = 1024 * 1024;
		/// Most bytes discarded after a frame rejected during the shutdown,
		/// so that the rest of the frame does not reset the connection
		/// before the client has read the CLOSE frame.

	struct ClosingSocket
		/// The socket of a removed subscriber, until a worker closes it or
		/// the service stops.
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
	std::chrono::seconds _pingInterval;
	Poco::Timespan _pollTimeout;
	bool _clientNotify;
	int _maxSubjects;
	bool _requireSubjectAuthorizer;
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
	std::array<Poco::Clock, REJECTION_REASON_COUNT> _lastRejectionNotice;
		/// When a rejection was last logged at notice level, by Rejection;
		/// Clock(0) before the first one.
	Poco::FastMutex _rejectionLogMutex;
	UserResolver _userResolver;
	mutable Poco::RWLock _userResolverLock;
		/// Read-locked while the resolver runs, so that setUserResolver()
		/// waits for the calls in progress.
	mutable Poco::FastMutex _userResolverTurnstile;
		/// Held by setUserResolver() while it takes the write lock, and by a
		/// call while it takes the read lock, so that the calls which start
		/// while the setter waits wait behind it. A reader-preferring RWLock
		/// (glibc) would otherwise let them keep the setter waiting.
	SubjectAuthorizer _subjectAuthorizer;
	Poco::RWLock _subjectAuthorizerLock;
		/// Read-locked while the authorizer runs, so that
		/// setSubjectAuthorizer() waits for the calls in progress.
	Poco::FastMutex _subjectAuthorizerTurnstile;
		/// Held by setSubjectAuthorizer() while it takes the write lock, and by
		/// a call while it takes the read lock, as _userResolverTurnstile.
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
