//
// WebEventServiceImpl.cpp
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventServiceImpl
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/PollSet.h"
#include "Poco/Net/SocketDefs.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Buffer.h"
#include "Poco/Bugcheck.h"
#include "Poco/Debugger.h"
#include "Poco/Event.h"
#include "Poco/Environment.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Logger.h"
#include "Poco/Notification.h"
#include "Poco/Thread.h"
#include "Poco/Timespan.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>


namespace Poco::OSP::WebEvent {


const std::string WebEventServiceImpl::SUBSCRIBE("SUBSCRIBE");
const std::string WebEventServiceImpl::UNSUBSCRIBE("UNSUBSCRIBE");
const std::string WebEventServiceImpl::NOTIFY("NOTIFY");
const std::string WebEventServiceImpl::VERSION("WebEvent/1.0");
const std::string WebEventServiceImpl::SYSTEM_PING("system.ping");
const std::string WebEventServiceImpl::SYSTEM_PONG("system.pong");


using TaskNotification = WebEventTaskNotification;


namespace
{
	template <typename Fn>
	void forSubjectAndAncestors(const std::string& subjectName, const Fn& fn)
		/// Calls fn for subjectName and then for each of its ancestors, the
		/// prefixes that end before a '.', from the nearest to the root.
	{
		std::string subject(subjectName);
		while (true)
		{
			fn(subject);
			const std::string::size_type pos = subject.find_last_of('.');
			if (pos == std::string::npos) break;
			subject.resize(pos);
		}
	}


	template <typename TEvent, typename TArgs>
	void fireEvent(TEvent& event, const void* pSender, const TArgs& args, Poco::Logger& logger, const std::string& eventName, const std::string& consequence)
		/// Fires the event. A delegate's exception is logged rather than
		/// passed on, so that it neither ends the thread firing the event
		/// nor skips what that thread does after the event. The delegates
		/// registered after the failing one are not called (the event's
		/// strategy stops at the exception); consequence says what has
		/// happened all the same.
	{
		std::string reason;
		try
		{
			event.notify(pSender, args);
			return;
		}
		catch (Poco::Exception& exc)
		{
			reason = exc.displayText();
		}
		catch (std::exception& exc)
		{
			reason = exc.what();
		}
		catch (...)
		{
			reason = "unknown exception";
		}
		logger.error(Poco::format("A delegate of the WebEvent service event %s failed: %s. The delegates after it were not called; %s.",
			eventName, reason, consequence));
	}
}


class AddSubscriberNotification: public TaskNotification
{
public:
	AddSubscriberNotification(WebEventServiceImpl& service, Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress):
		TaskNotification(service),
		_pWS(pWS),
		_username(username),
		_page(page),
		_clientAddress(clientAddress)
	{
	}

	~AddSubscriberNotification() override
	{
		// Not executed because the service has stopped: the connection is
		// closed now rather than left open with nobody reading it.
		if (!_executed)
		{
			_service.releaseSocketSlot();
			try
			{
				_pWS->close();
			}
			catch (...)
			{
				poco_unexpected();
			}
		}
	}

	void execute() override
	{
		_executed = true;
		_service.addSubscriberImpl(_pWS, _username, _page, _clientAddress);
	}

private:
	Poco::SharedPtr<Poco::Net::WebSocket> _pWS;
	std::string _username;
	std::string _page;
	std::string _clientAddress;
	bool _executed = false;
};


class RemoveSubscriberNotification: public TaskNotification
{
public:
	RemoveSubscriberNotification(WebEventServiceImpl& service, Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown):
		TaskNotification(service),
		_pWS(pWS),
		_clientShutdown(clientShutdown)
	{
	}

	void execute() override
	{
		_service.removeSubscriberImpl(_pWS, _clientShutdown);
	}

private:
	Poco::SharedPtr<Poco::Net::WebSocket> _pWS;
	bool _clientShutdown;
};


class SubscribeNotification: public TaskNotification
{
public:
	SubscribeNotification(WebEventServiceImpl& service, Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames):
		TaskNotification(service),
		_pWS(pWS),
		_subjectNames(subjectNames)
	{
	}

	void execute() override
	{
		_service.subscribeImpl(_pWS, _subjectNames);
	}

private:
	Poco::SharedPtr<Poco::Net::WebSocket> _pWS;
	std::set<std::string> _subjectNames;
};


class UnsubscribeNotification: public TaskNotification
{
public:
	UnsubscribeNotification(WebEventServiceImpl& service, Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames):
		TaskNotification(service),
		_pWS(pWS),
		_subjectNames(subjectNames)
	{
	}

	void execute() override
	{
		_service.unsubscribeImpl(_pWS, _subjectNames);
	}

private:
	Poco::SharedPtr<Poco::Net::WebSocket> _pWS;
	std::set<std::string> _subjectNames;
};


class NotifyNotification: public TaskNotification
{
public:
	NotifyNotification(WebEventServiceImpl& service, const std::string& subjectName, const std::string& data):
		TaskNotification(service),
		_subjectName(subjectName),
		_data(data)
	{
	}

	void execute() override
	{
		_service.notifyImpl(_subjectName, _data);
	}

private:
	std::string _subjectName;
	std::string _data;
};


class NotifyUserNotification: public TaskNotification
{
public:
	NotifyUserNotification(WebEventServiceImpl& service, const std::string& username, const std::string& subjectName, const std::string& data):
		TaskNotification(service),
		_username(username),
		_subjectName(subjectName),
		_data(data)
	{
	}

	void execute() override
	{
		_service.notifyUserImpl(_username, _subjectName, _data);
	}

private:
	std::string _username;
	std::string _subjectName;
	std::string _data;
};


class SendNotification: public TaskNotification
{
public:
	SendNotification(WebEventServiceImpl& service, WebEventServiceImpl::Subscriber::Ptr pSubscriber):
		TaskNotification(service),
		_pSubscriber(pSubscriber)
	{
	}

	void execute() override
	{
		_service.sendPending(_pSubscriber);
	}

private:
	WebEventServiceImpl::Subscriber::Ptr _pSubscriber;
};


class ReceiveNotification: public TaskNotification
{
public:
	ReceiveNotification(WebEventServiceImpl& service, WebEventServiceImpl::Subscriber::Ptr pSubscriber):
		TaskNotification(service),
		_pSubscriber(pSubscriber)
	{
	}

	void execute() override
	{
		_service.receiveImpl(_pSubscriber);
	}

private:
	WebEventServiceImpl::Subscriber::Ptr _pSubscriber;
};


class ShutdownNotification: public TaskNotification
{
public:
	ShutdownNotification(WebEventServiceImpl& service, WebEventServiceImpl::Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive):
		TaskNotification(service),
		_pSubscriber(pSubscriber),
		_statusCode(statusCode),
		_statusMessage(statusMessage),
		_passive(passive)
	{
	}

	void execute() override
	{
		_service.shutdownImpl(_pSubscriber, _statusCode, _statusMessage, _passive);
	}

private:
	WebEventServiceImpl::Subscriber::Ptr _pSubscriber;
	Poco::UInt16 _statusCode;
	std::string _statusMessage;
	bool _passive;
};


class WatchSocketNotification: public TaskNotification
{
public:
	WatchSocketNotification(WebEventServiceImpl& service, const Poco::Net::Socket& socket, bool buffered):
		TaskNotification(service),
		_socket(socket),
		_buffered(buffered)
	{
	}

	void execute() override
	{
		_service.watchSocketImpl(_socket, _buffered);
	}

private:
	Poco::Net::Socket _socket;
	bool _buffered;
};


class ListSubscribersNotification: public TaskNotification
{
public:
	struct State: public Poco::RefCountedObject
		/// Shared between the requesting thread and the event thread, so
		/// that a snapshot completing after the requester timed out writes
		/// into still-valid storage nobody reads anymore.
	{
		using Ptr = Poco::AutoPtr<State>;

		WebEventService::SubscriberInfoVec subscribers;
		Poco::Event done;
	};

	ListSubscribersNotification(WebEventServiceImpl& service, State::Ptr pState):
		TaskNotification(service),
		_pState(pState)
	{
	}

	void execute() override
	{
		_service.listSubscribersImpl(_pState->subscribers);
		_pState->done.set();
	}

private:
	State::Ptr _pState;
};


class StopNotification: public TaskNotification
	/// Wakes a thread waiting on a queue so it re-checks the stop flag.
{
public:
	StopNotification(WebEventServiceImpl& service):
		TaskNotification(service)
	{
	}

	void execute() override
	{
	}
};


WebEventServiceImpl::Config WebEventServiceImpl::Config::fromConfiguration(
	const Poco::Util::AbstractConfiguration& configuration)
{
	Config config;
	config.workerCount = configuration.getInt("osp.web.event.workers", config.workerCount);
	config.maxSockets = configuration.getInt("osp.web.event.maxWebSockets", config.maxSockets);
	config.maxPendingFrames = configuration.getInt("osp.web.event.maxPendingFrames", config.maxPendingFrames);
	config.pingInterval = std::chrono::seconds(configuration.getInt("osp.web.event.pingInterval",
		static_cast<int>(config.pingInterval.count())));
	config.clientNotify = configuration.getBool("osp.web.event.clientNotify", config.clientNotify);
	config.maxSubjects = configuration.getInt("osp.web.event.maxSubjects", config.maxSubjects);
	return config;
}


WebEventServiceImpl::WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext):
	WebEventServiceImpl(std::move(pContext), Config())
{
}


WebEventServiceImpl::WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext, const Config& config):
	_pContext(pContext),
	_maxSockets(config.maxSockets),
	_maxPendingFrames(config.maxPendingFrames),
	_pingInterval(config.pingInterval),
	_pollTimeout(config.pollTimeout),
	_clientNotify(config.clientNotify),
	_maxSubjects(config.maxSubjects),
	_socketCount(0),
	_pPollSet(std::make_unique<Poco::Net::PollSet>()),
	_wakeUpPending(false),
	_mainRunnable(*this, &WebEventServiceImpl::runMain),
	_workerRunnable(*this, &WebEventServiceImpl::runWorker),
	_stopped(false),
	_logger(pContext != nullptr ? pContext->logger() : Poco::Logger::get("Poco.OSP.WebEvent"))
{
	_lastRejectionNotice.fill(Poco::Clock(0));
	const int actualWorkerCount = std::min(config.workerCount > 0 ? config.workerCount : defaultWorkerCount(), MAX_WORKER_COUNT);
	try
	{
		for (int i = 0; i < actualWorkerCount; ++i)
		{
			ThreadPtr pThread = new Poco::Thread;
			_workerThreads.push_back(pThread);
			pThread->start(_workerRunnable);
		}
		_mainThread.start(_mainRunnable);
	}
	catch (...)
	{
		// The threads already started run on this object, which is gone once
		// the exception has left the constructor: they are joined first.
		stop();
		throw;
	}
}


WebEventServiceImpl::~WebEventServiceImpl()
{
	try
	{
		stop();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void WebEventServiceImpl::stop()
{
	Poco::FastMutex::ScopedLock lock(_stopMutex);

	if (_stopped) return;
	_stopped = true;

	// One wake-up per waiting worker, as a queued notification rather than
	// wakeUpAll(): its sticky flag is cleared by any later enqueue, which
	// would leave a thread waiting on an empty queue after the flag is set.
	wakeUpMain();
	for (std::size_t i = 0; i < _workerThreads.size(); ++i)
	{
		_workerQueue.enqueueUrgentNotification(new StopNotification(*this));
	}
	_mainThread.join();
	for (auto& pThread: _workerThreads)
	{
		pThread->join();
	}
	_mainQueue.clear();
	_workerQueue.clear();
	{
		// The shutdowns still queued have gone with the queue, so no worker
		// closes their sockets; the last reference does.
		Poco::FastMutex::ScopedLock closingLock(_closingMutex);

		_closingSockets.clear();
	}
}


bool WebEventServiceImpl::stopped() const
{
	return _stopped;
}


Poco::BasicEvent<const WebEventService::NotificationEvent>& WebEventServiceImpl::subjectNotified(const std::string& subject)
{
	Poco::FastMutex::ScopedLock lock(_mapMutex);

	EventMap::iterator it = _eventMap.find(subject);
	if (it != _eventMap.end())
	{
		return *it->second;
	}
	else
	{
		EventPtr pEvent = new Event;
		_eventMap.insert(EventMap::value_type(subject, pEvent));
		return *pEvent;
	}
}


void WebEventServiceImpl::notify(const std::string& subjectName, const std::string& data)
{
	if (_stopped) return;
	enqueueMain(new NotifyNotification(*this, subjectName, data));
}


void WebEventServiceImpl::notifyUser(const std::string& username, const std::string& subjectName, const std::string& data)
{
	if (_stopped) return;
	enqueueMain(new NotifyUserNotification(*this, username, subjectName, data));
}


void WebEventServiceImpl::setUserResolver(UserResolver resolver)
{
	Poco::FastMutex::ScopedLock lock(_userResolverMutex);

	_userResolver = std::move(resolver);
}


std::string WebEventServiceImpl::resolveUser(const Poco::Net::HTTPServerRequest& request) const
{
	UserResolver resolver;
	{
		Poco::FastMutex::ScopedLock lock(_userResolverMutex);
		resolver = _userResolver;
	}
	if (resolver)
	{
		try
		{
			return resolver(request);
		}
		catch (Poco::Exception& exc)
		{
			logger().error(Poco::format("User resolver failed for the connection from %s; the connection belongs to no user: %s",
				request.clientAddress().toString(), exc.displayText()));
		}
		catch (std::exception& exc)
		{
			logger().error(Poco::format("User resolver failed for the connection from %s; the connection belongs to no user: %s",
				request.clientAddress().toString(), std::string(exc.what())));
		}
	}
	return std::string();
}


const std::type_info& WebEventServiceImpl::type() const
{
	return typeid(WebEventServiceImpl);
}


bool WebEventServiceImpl::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebEventServiceImpl).name());
	return name == otherType.name() || WebEventService::isA(otherType);
}


void WebEventServiceImpl::runMain()
{
	while (!_stopped)
	{
		try
		{
			// A descriptor closed while watched is never reported: epoll
			// drops it from its set, and poll() flags it as invalid, which
			// the poll set does not map to a mode. It is found here instead.
			if (std::any_of(_socketMap.begin(), _socketMap.end(), [](const SocketMap::value_type& entry)
				{
					return entry.second->watched && entry.first.impl()->sockfd() == POCO_INVALID_SOCKET;
				}))
			{
				removeBadSocketsImpl();
			}

			// Bounded, so that a keepalive sweep that falls due is not held
			// off by sockets and a queue that both stay quiet; enqueueMain()
			// ends the wait early.
			const Poco::Timespan timeout = _mainQueue.empty() ? _pollTimeout : Poco::Timespan(0);
			const Poco::Net::PollSet::SocketModeMap ready = _pPollSet->poll(timeout);
			_wakeUpPending.store(false);
			for (const auto& entry: ready)
			{
				SocketMap::iterator its = _socketMap.find(entry.first);
				if (its == _socketMap.end()) continue;
				// A socket that is readable and in error at once goes to one
				// worker only.
				if (entry.second & Poco::Net::PollSet::POLL_ERROR)
				{
					removeSubscriberImpl(its->second->pWebSocket, false);
				}
				else
				{
					// Readable means the peer has sent something, which is
					// what the keepalive asks about. Stamped here rather
					// than where the frame is finally read, because that
					// happens on a worker and is posted back to this queue:
					// a busy worker would let a subscriber that is in fact
					// answering look silent long enough to be reaped.
					its->second->lastActive.update();
					receive(its->second);
					unwatchSocketImpl(entry.first);
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			logger().error("Exception in main thread: %s (%s)",
				exc.displayText(), poco_src_loc);
			removeBadSocketsImpl();
		}
		catch (std::exception& exc)
		{
			logger().error("Exception in main thread: %s (%s)",
				std::string(exc.what()), poco_src_loc);
		}
		catch (...)
		{
			logger().error("Unknown exception in main thread (%s)", poco_src_loc);
		}

		if (pingSweepDueImpl())
		{
			pingIdleSubscribersImpl();
			_lastPingSweep.update();
		}
		if (stallCheckDueImpl())
		{
			removeStalledSubscribersImpl();
			_lastStallCheck.update();
		}

		Poco::Notification::Ptr pNf = _mainQueue.dequeueNotification();
		while (pNf != nullptr && !_stopped)
		{
			try
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf != nullptr)
				{
					pTaskNf->execute();
				}
			}
			catch (Poco::Exception& exc)
			{
				logger().error("Exception in main thread: %s (%s)",
					exc.displayText(), poco_src_loc);
			}
			catch (std::exception& exc)
			{
				logger().error("Exception in main thread: %s (%s)",
					std::string(exc.what()), poco_src_loc);
			}
			catch (...)
			{
				logger().error("Unknown exception in main thread (%s)", poco_src_loc);
			}
			// Back to the outer loop between notifications when a sweep
			// falls due, so that a queue kept busy by its producers does
			// not leave dead subscribers unreclaimed. Nothing already
			// dequeued is dropped: the check sits after the notification
			// has been executed and before the next is taken.
			if (pingSweepDueImpl() || stallCheckDueImpl()) break;
			pNf = _mainQueue.dequeueNotification();
		}
	}
	stopImpl();
}


void WebEventServiceImpl::runWorker()
{
	while (!_stopped)
	{
		try
		{
			Poco::Notification::Ptr pNf =
				_workerQueue.waitDequeueNotification(static_cast<long>(WEBEVENT_QUEUE_WAIT.count()));
			if (pNf != nullptr && !_stopped)
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf != nullptr)
				{
					pTaskNf->execute();
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			logger().error("Exception in worker thread: %s (%s)",
				exc.displayText(), poco_src_loc);
		}
		catch (std::exception& exc)
		{
			logger().error("Exception in worker thread: %s (%s)",
				std::string(exc.what()), poco_src_loc);
		}
		catch (...)
		{
			logger().error("Unknown exception in worker thread (%s)", poco_src_loc);
		}
	}
}


void WebEventServiceImpl::addSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress)
{
	enqueueMain(new AddSubscriberNotification(*this, std::move(pWS), username, page, clientAddress));
	if (_stopped)
	{
		// stop() may have cleared the queue before the enqueue above. Once it
		// has returned, nothing executes the notification, and clearing the
		// queue again destroys it, which closes the connection.
		Poco::FastMutex::ScopedLock lock(_stopMutex);
		_mainQueue.clear();
	}
}


void WebEventServiceImpl::removeSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown)
{
	enqueueMain(new RemoveSubscriberNotification(*this, std::move(pWS), clientShutdown));
}


void WebEventServiceImpl::subscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	enqueueMain(new SubscribeNotification(*this, std::move(pWS), subjectNames));
}


void WebEventServiceImpl::unsubscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	enqueueMain(new UnsubscribeNotification(*this, std::move(pWS), subjectNames));
}


int WebEventServiceImpl::subscriberCount(const std::string& subject)
{
	Poco::FastMutex::ScopedLock lock(_mapMutex);

	auto range = _subjectMap.equal_range(subject);
	return static_cast<int>(std::distance(range.first, range.second));
}


WebEventService::SubscriberInfoVec WebEventServiceImpl::listSubscribers()
{
	if (_stopped) return SubscriberInfoVec();

	ListSubscribersNotification::State::Ptr pState = new ListSubscribersNotification::State;
	enqueueMain(new ListSubscribersNotification(*this, pState));

	Poco::Clock deadline;
	while (!pState->done.tryWait(static_cast<long>(WEBEVENT_LIST_SUBSCRIBERS_POLL.count())))
	{
		// A stop between the check above and the event thread leaving its
		// loop leaves this notification unexecuted. The documented answer
		// for a stopped service is an empty list, arrived at now rather
		// than at the end of a wait nothing can satisfy.
		if (_stopped) return SubscriberInfoVec();
		if (deadline.isElapsed(Poco::Timespan(WEBEVENT_LIST_SUBSCRIBERS_TIMEOUT).totalMicroseconds()))
		{
			// Not an empty snapshot: the caller would read that as every
			// user having gone away, when in truth the event thread has
			// not answered.
			throw Poco::TimeoutException("Timeout waiting for subscriber snapshot");
		}
	}
	return std::move(pState->subscribers);
}


void WebEventServiceImpl::enqueueSend(Subscriber::Ptr pSubscriber, const std::string& subject, std::shared_ptr<const std::string> pFrame, int flags)
{
	bool enqueueToken = false;
	bool warn = false;
	Poco::UInt64 dropped = 0;
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);

		if (pSubscriber->closed) return;
		++pSubscriber->enqueuedFrames;
		auto itQueued = pSubscriber->pending.end();
		if (!subject.empty())
		{
			const auto itBySubject = pSubscriber->bySubject.find(subject);
			if (itBySubject != pSubscriber->bySubject.end()) itQueued = itBySubject->second;
		}
		else if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_PONG)
		{
			// RFC 6455, section 5.5.3: answering the most recent PING is
			// enough, so a client that sends PINGs and does not read cannot
			// grow the queue, which never drops a control frame.
			itQueued = std::find_if(pSubscriber->pending.begin(), pSubscriber->pending.end(),
				[](const Subscriber::PendingFrame& frame)
				{
					return (frame.flags & Poco::Net::WebSocket::FRAME_OP_BITMASK)
						== Poco::Net::WebSocket::FRAME_OP_PONG;
				});
		}
		if (itQueued != pSubscriber->pending.end())
		{
			// Latest value wins; move to the tail so frames still leave in
			// the order the notifications were made.
			itQueued->pFrame = std::move(pFrame);
			itQueued->flags = flags;
			pSubscriber->pending.splice(pSubscriber->pending.end(), pSubscriber->pending, itQueued);
		}
		else
		{
			pSubscriber->pending.push_back(Subscriber::PendingFrame{subject, std::move(pFrame), flags});
			if (!subject.empty())
			{
				pSubscriber->bySubject[subject] = std::prev(pSubscriber->pending.end());
			}
			auto itDropped = pSubscriber->pending.end();
			if (_maxPendingFrames > 0 && pSubscriber->pending.size() > static_cast<std::size_t>(_maxPendingFrames))
			{
				// The oldest data frame goes; control opcodes start at CLOSE. A dropped
				// keepalive PING would stay pending for good, and the subscriber would be
				// neither pinged nor removed again. At most one PING and one PONG are queued.
				itDropped = std::find_if(pSubscriber->pending.begin(), pSubscriber->pending.end(),
					[](const Subscriber::PendingFrame& frame)
					{
						return (frame.flags & Poco::Net::WebSocket::FRAME_OP_BITMASK)
							< Poco::Net::WebSocket::FRAME_OP_CLOSE;
					});
			}
			if (itDropped != pSubscriber->pending.end())
			{
				if (!itDropped->subject.empty())
				{
					pSubscriber->bySubject.erase(itDropped->subject);
				}
				pSubscriber->pending.erase(itDropped);
				dropped = ++pSubscriber->droppedFrames;
				if (pSubscriber->lastDropWarning.isElapsed(
					Poco::Timespan(WEBEVENT_DROP_WARNING_INTERVAL).totalMicroseconds()))
				{
					warn = true;
					pSubscriber->lastDropWarning.update();
				}
			}
		}
		if (!pSubscriber->draining)
		{
			pSubscriber->draining = true;
			enqueueToken = true;
		}
	}
	if (warn)
	{
		logger().warning(Poco::format("WebSocket subscriber %s cannot keep up: %Lu frame(s) dropped so far (limit %d queued frames per subscriber).",
			describe(*pSubscriber), dropped, _maxPendingFrames));
	}
	if (enqueueToken)
	{
		_workerQueue.enqueueNotification(new SendNotification(*this, pSubscriber));
	}
}


void WebEventServiceImpl::receive(Subscriber::Ptr pSubscriber)
{
	_workerQueue.enqueueNotification(new ReceiveNotification(*this, pSubscriber));
}


void WebEventServiceImpl::receiveNext(Subscriber::Ptr pSubscriber)
{
	// Over TLS, the next frames can already be in the TLS buffer, read with the
	// record of the frame just handled; the socket does not become readable for them.
	bool buffered = false;
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->readMutex);

		buffered = pSubscriber->pWebSocket->available() > 0;
	}
	// Through the main thread either way, which stamps the keepalive.
	enqueueMain(new WatchSocketNotification(*this, *pSubscriber->pWebSocket, buffered));
}


void WebEventServiceImpl::shutdown(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive)
{
	// A read or send restarts its timeout with every byte, and the stall check no
	// longer covers a removed subscriber, so a client taking or sending a few bytes
	// at a time would otherwise hold the workers. The watchdog of the main thread
	// shuts the socket down at the deadline.
	{
		Poco::FastMutex::ScopedLock lock(_closingMutex);

		Poco::Clock deadline;
		deadline += Poco::Timespan(WEBEVENT_SHUTDOWN_TIMEOUT).totalMicroseconds();
		_closingSockets.push_back(ClosingSocket{*pSubscriber->pWebSocket, deadline});
	}
	_workerQueue.enqueueNotification(new ShutdownNotification(*this, pSubscriber, statusCode, statusMessage, passive));
}


void WebEventServiceImpl::notifyImpl(const std::string& subjectName, const std::string& data)
{
	// The frames are queued before any delegate runs, so that what a
	// delegate does cannot keep the notification from the subscribers.
	auto pFrame = makeNotifyFrame(subjectName, data);
	for (auto& pSubscriber: collectSubscribers(subjectName))
	{
		enqueueSend(pSubscriber, subjectName, pFrame);
	}

	NotificationEvent ev(subjectName, data);
	fireEvent(notificationSent, nullptr, ev, logger(), "notificationSent",
		"the notification had already been queued for the subscribers");
	forSubjectAndAncestors(subjectName, [&](const std::string& subject)
	{
		// collect under _mapMutex (subjectNotified() may insert concurrently),
		// fire outside it so delegates can call back into the service
		std::vector<EventPtr> events;
		{
			Poco::FastMutex::ScopedLock lock(_mapMutex);
			auto range = _eventMap.equal_range(subject);
			for (auto itEv = range.first; itEv != range.second; ++itEv)
			{
				events.push_back(itEv->second);
			}
		}
		for (auto& pEvent: events)
		{
			fireEvent(*pEvent, nullptr, ev, logger(), "subjectNotified(" + subject + ")",
				"the notification had already been queued for the subscribers");
		}
	});
}


void WebEventServiceImpl::notifyUserImpl(const std::string& username, const std::string& subjectName, const std::string& data)
{
	if (username.empty()) return;

	auto pFrame = makeNotifyFrame(subjectName, data);

	for (auto& pSubscriber: collectSubscribers(subjectName, username))
	{
		enqueueSend(pSubscriber, subjectName, pFrame);
	}
}


std::vector<WebEventServiceImpl::Subscriber::Ptr> WebEventServiceImpl::collectSubscribers(const std::string& subjectName, const std::string& username)
{
	// The walk visits a subscriber once per subscription it holds along the
	// subject's ancestry, and a client subscribed to both a subject and one
	// of its ancestors holds two. It must still be sent ONE frame, so the
	// matches are deduplicated here.
	//
	// The queue coalesces by subject and would usually hide a second
	// enqueue, but only while the first frame is still pending: the first
	// enqueue also wakes a worker, and when that worker sends before the
	// walk reaches the ancestor, the coalescing key is already gone and the
	// same frame goes out twice. That race is why this is not left to the
	// queue.
	std::vector<Subscriber::Ptr> targets;
	std::set<const Subscriber*> seen;
	forSubjectAndAncestors(subjectName, [&](const std::string& subject)
	{
		auto subscribers = _subjectMap.equal_range(subject);
		for (auto it = subscribers.first; it != subscribers.second; ++it)
		{
			if (!username.empty() && it->second->username != username) continue;
			if (seen.insert(it->second.get()).second)
			{
				targets.push_back(it->second);
			}
		}
	});
	return targets;
}


std::shared_ptr<const std::string> WebEventServiceImpl::makeNotifyFrame(const std::string& subjectName, const std::string& data)
{
	std::string message(NOTIFY);
	message += ' ';
	message += subjectName;
	message += ' ';
	message += VERSION;
	message += "\r\n";
	message += data;
	return std::make_shared<const std::string>(std::move(message));
}


void WebEventServiceImpl::addSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress)
{
	Subscriber::Ptr pSubscriber = new Subscriber;
	pSubscriber->pWebSocket = pWS;
	pSubscriber->username = username;
	pSubscriber->page = page;
	pSubscriber->clientAddress = clientAddress;
	_socketMap[*pWS] = pSubscriber;
	watchSocketImpl(*pWS, false);
	pWS->setMaxPayloadSize(WEBEVENT_MAX_FRAME_PAYLOAD);
	// Bounds each send call only: a peer that keeps taking a few bytes
	// restarts it. removeStalledSubscribersImpl() bounds the whole frame.
	pWS->setSendTimeout(Poco::Timespan(WEBEVENT_SEND_TIMEOUT));
	pWS->setReceiveTimeout(Poco::Timespan(WEBEVENT_RECEIVE_TIMEOUT));
}


void WebEventServiceImpl::listSubscribersImpl(SubscriberInfoVec& subscribers)
{
	subscribers.reserve(_socketMap.size());
	for (SocketMap::const_iterator it = _socketMap.begin(); it != _socketMap.end(); ++it)
	{
		SubscriberInfo info;
		info.username = it->second->username;
		info.page = it->second->page;
		info.clientAddress = it->second->clientAddress;
		subscribers.push_back(info);
	}
}


void WebEventServiceImpl::removeSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown)
{
	SocketMap::iterator it = _socketMap.find(*pWS);
	if (it != _socketMap.end())
	{
		Subscriber::Ptr pSubscriber = it->second;
		unsubscribeImpl(pWS, pSubscriber->subjectNames);
		unwatchSocketImpl(*pWS);
		_socketMap.erase(it);
		releaseSocketSlot();
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->closed = true;
			pSubscriber->pending.clear();
			pSubscriber->bySubject.clear();
		}
		try
		{
			shutdown(pSubscriber, Poco::Net::WebSocket::WS_NORMAL_CLOSE, "", clientShutdown);
		}
		catch (Poco::Exception& exc)
		{
			logger().error(Poco::format("WebSocket subscriber %s removed, but its connection could not be queued for shutdown: %s",
				describe(*pSubscriber), exc.displayText()));
		}
	}
}


void WebEventServiceImpl::subscribeImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	SocketMap::iterator it = _socketMap.find(*pWS);
	if (it != _socketMap.end())
	{
		const std::size_t newSubjects = static_cast<std::size_t>(std::count_if(subjectNames.begin(), subjectNames.end(),
			[&](const std::string& subject) { return !it->second->subjectNames.contains(subject); }));
		if (_maxSubjects > 0 && it->second->subjectNames.size() + newSubjects > static_cast<std::size_t>(_maxSubjects))
		{
			logger().information(Poco::format("Removing WebSocket subscriber %s: a SUBSCRIBE message would take it beyond the limit of %d subjects.",
				describe(*it->second), _maxSubjects));
			removeSubscriberImpl(pWS, false);
			return;
		}
		for (std::set<std::string>::const_iterator itSub = subjectNames.begin(); itSub != subjectNames.end(); ++itSub)
		{
			if (!it->second->subjectNames.contains(*itSub))
			{
				it->second->subjectNames.insert(*itSub);
				{
					Poco::FastMutex::ScopedLock lock(_mapMutex);
					_subjectMap.insert(SubjectMap::value_type(*itSub, it->second));
				}
				fireEvent(subscriptionRequestReceived, this, *itSub, logger(), "subscriptionRequestReceived(" + *itSub + ")",
					"the subscription has been made");
			}
		}
	}
}


void WebEventServiceImpl::unsubscribeImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	SocketMap::iterator it = _socketMap.find(*pWS);
	if (it != _socketMap.end())
	{
		std::set<std::string> allSubjectNames;
		if (subjectNames.contains("*"))
		{
			allSubjectNames = it->second->subjectNames;
		}
		else
		{
			allSubjectNames = subjectNames;
		}
		for (std::set<std::string>::const_iterator itSub = allSubjectNames.begin(); itSub != allSubjectNames.end(); ++itSub)
		{
			std::set<std::string>::iterator itFound = it->second->subjectNames.find(*itSub);
			if (itFound != it->second->subjectNames.end())
			{
				it->second->subjectNames.erase(itFound);
				bool erased = false;
				{
					Poco::FastMutex::ScopedLock lock(_mapMutex);
					auto range = _subjectMap.equal_range(*itSub);
					auto itSM = std::find_if(range.first, range.second,
						[&](const auto& entry) { return entry.second == it->second; });
					if (itSM != range.second)
					{
						_subjectMap.erase(itSM);
						erased = true;
					}
				}
				if (erased)
				{
					fireEvent(unsubscriptionRequestReceived, this, *itSub, logger(), "unsubscriptionRequestReceived(" + *itSub + ")",
						"the subscription has been removed");
				}
			}
		}
	}
}


void WebEventServiceImpl::sendPending(Subscriber::Ptr pSubscriber)
{
	std::shared_ptr<const std::string> pFrame;
	int flags = Poco::Net::WebSocket::FRAME_TEXT;
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);

		if (_stopped || pSubscriber->closed || pSubscriber->pending.empty())
		{
			pSubscriber->draining = false;
			return;
		}
		Subscriber::PendingFrame& front = pSubscriber->pending.front();
		if (!front.subject.empty())
		{
			pSubscriber->bySubject.erase(front.subject);
		}
		pFrame = std::move(front.pFrame);
		flags = front.flags;
		pSubscriber->pending.pop_front();
		pSubscriber->sending = true;
		pSubscriber->sendStarted.update();
	}
	try
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->writeMutex);

		pSubscriber->pWebSocket->sendFrame(pFrame->data(), static_cast<int>(pFrame->size()), flags);
	}
	catch (Poco::Exception& exc)
	{
		bool alreadyClosed = false;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			alreadyClosed = pSubscriber->closed;
			pSubscriber->closed = true;
			pSubscriber->pending.clear();
			pSubscriber->bySubject.clear();
			pSubscriber->draining = false;
			pSubscriber->pingPending = false;
			pSubscriber->sending = false;
		}
		// Already being removed, for instance because the send took too long.
		if (alreadyClosed) return;
		logger().information(Poco::format("Removing WebSocket subscriber %s: sending failed: %s",
			describe(*pSubscriber), exc.displayText()));
		removeSubscriber(pSubscriber->pWebSocket, false);
		return;
	}
	// Re-enqueue one notification per frame so workers round-robin
	// across subscribers instead of one subscriber holding a worker.
	bool more = false;
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
		pSubscriber->sending = false;
		if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_PING)
		{
			// The keepalive counts an unanswered ping from here, the point
			// at which the peer was actually asked.
			pSubscriber->lastPingSent.update();
			pSubscriber->pingPending = false;
		}
		more = !pSubscriber->closed && !pSubscriber->pending.empty();
		pSubscriber->draining = more;
	}
	if (more)
	{
		_workerQueue.enqueueNotification(new SendNotification(*this, pSubscriber));
	}
}


void WebEventServiceImpl::receiveImpl(Subscriber::Ptr pSubscriber)
{
	if (_stopped) return;

	Poco::SharedPtr<Poco::Net::WebSocket> pWS = pSubscriber->pWebSocket;
	try
	{
		Poco::Buffer<char> buffer(WEBEVENT_MAX_FRAME_PAYLOAD);
		int flags;
		int n;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->receiving = true;
			pSubscriber->receiveStarted.update();
		}
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->readMutex);
			n = pWS->receiveFrame(buffer.begin(), static_cast<int>(buffer.size()), flags);
		}
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->receiving = false;
		}
		if (logger().debug()) logger().debug(Poco::format("Frame received (length=%d, flags=0x%x).", n,
			static_cast<unsigned>(flags)));

		// Opcodes are a 4-bit field, not flags: CLOSE (0x8) shares a bit with
		// PONG (0xA), so testing bits instead of the masked value read every
		// client CLOSE as a stray PONG and left it unanswered. Firefox keeps
		// the TCP connection open for 20s waiting for the CLOSE reply, so a
		// closed tab stayed a live subscriber for that long.
		const int opcode = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
		if (opcode == Poco::Net::WebSocket::FRAME_OP_PING)
		{
			// RFC 6455, section 5.5.2: answered with a PONG carrying the same
			// payload. Through the send queue, so it cannot interleave with a
			// data frame being sent.
			enqueueSend(pSubscriber, std::string(), std::make_shared<const std::string>(buffer.begin(), static_cast<std::size_t>(n)),
				static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG));
			receiveNext(pSubscriber);
			return;
		}

		if (opcode == Poco::Net::WebSocket::FRAME_OP_PONG)
		{
			logger().debug("Received unsolicited PONG frame - ignoring.");
			receiveNext(pSubscriber);
			return;
		}

		// No flags at all mark the end of the stream; an empty frame of any
		// kind has at least its opcode set.
		if (opcode == Poco::Net::WebSocket::FRAME_OP_CLOSE || (n == 0 && flags == 0))
		{
			removeSubscriber(pWS, true);
			return;
		}

		// A message is one text frame; continuation frames are not supported.
		const bool singleTextFrame = opcode == Poco::Net::WebSocket::FRAME_OP_TEXT && (flags & Poco::Net::WebSocket::FRAME_FLAG_FIN) != 0;
		const char* it  = buffer.begin();
		const char* end = buffer.begin() + n;
		const char* itVerbEnd = std::find(it, end, ' ');
		const std::string verb(it, itVerbEnd);
		it = itVerbEnd;
		if (singleTextFrame && (verb == SUBSCRIBE || verb == UNSUBSCRIBE || verb == NOTIFY))
		{
			while (it != end && *it == ' ') ++it;
			std::set<std::string> subjectNames;
			bool subjectsValid = true;
			while (it != end && *it != ' ')
			{
				std::string subject;
				int bracketDepth = 0;
				while (it != end && *it != ' ')
				{
					if (*it == '[' || *it == '(' || *it == '{') bracketDepth++;
					else if ((*it == ']' || *it == ')' || *it == '}') && bracketDepth > 0) bracketDepth--;
					else if (*it == ',' && bracketDepth == 0) break; // Only break on comma outside brackets
					subject += *it++;
				}
				if (it != end)
				{
					// An empty subject would be an ancestor of every subject
					// that starts with a period; a trailing comma stands for one.
					if (subject.empty()) subjectsValid = false;
					subjectNames.insert(subject);
					if (*it == ',')
					{
						it++; // skip ','
						if (it == end || *it == ' ') subjectsValid = false;
					}
				}
			}
			while (it != end && *it == ' ') ++it;
			const char* itVersionEnd = std::find_if(it, end, [](char c) { return c == '\r' || c == '\n'; });
			const std::string version(it, itVersionEnd);
			it = itVersionEnd;
			if (it != end && *it == '\r') ++it;
			if (it != end && *it == '\n') ++it;
			if (subjectsValid && version == VERSION)
			{
				if (verb == SUBSCRIBE)
				{
					subscribe(pWS, subjectNames);
				}
				else if (verb == UNSUBSCRIBE)
				{
					unsubscribe(pWS, subjectNames);
				}
				else if (verb == NOTIFY)
				{
					const std::string data(it, end);
					for (const auto& subjectName: subjectNames)
					{
						if (subjectName == SYSTEM_PING)
						{
							// empty subject: pong replies carry echo data and must not be coalesced
							enqueueSend(pSubscriber, std::string(), makeNotifyFrame(SYSTEM_PONG, data));
						}
						else if (_clientNotify)
						{
							notify(subjectName, data);
						}
						else if (logger().debug())
						{
							logger().debug(Poco::format("Dropping NOTIFY %s from %s: client notifications are disabled.",
								subjectName, describe(*pSubscriber)));
						}
					}
				}
				receiveNext(pSubscriber);
				return;
			}
		}
		logger().information(Poco::format("Removing WebSocket subscriber %s: the message received is not a valid WebEvent message.",
			describe(*pSubscriber)));
	}
	catch (Poco::Exception& exc)
	{
		bool alreadyClosed = false;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->receiving = false;
			alreadyClosed = pSubscriber->closed;
		}
		// Already being removed, for instance because the frame took too long.
		if (alreadyClosed) return;
		logger().information(Poco::format("Removing WebSocket subscriber %s: receiving failed: %s",
			describe(*pSubscriber), exc.displayText()));
	}
	removeSubscriber(pWS, false);
}


void WebEventServiceImpl::shutdownImpl(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive)
{
	// writeMutex keeps the close frame from interleaving with an in-flight
	// data frame sent by sendPending() on a worker thread; readMutex keeps
	// the confirmation read and close() from overlapping a receiveImpl()
	// still reading the same socket on another worker.
	Poco::FastMutex::ScopedLock writeLock(pSubscriber->writeMutex);
	Poco::FastMutex::ScopedLock readLock(pSubscriber->readMutex);
	Poco::SharedPtr<Poco::Net::WebSocket> pWS = pSubscriber->pWebSocket;

	if (_stopped)
	{
		// no handshake on the way out; the client sees the connection drop
		closeImpl(*pWS);
		return;
	}

	if (logger().debug()) logger().debug(Poco::format("Shutting down WebSocket of subscriber %s.", describe(*pSubscriber)));
	try
	{
		pWS->shutdown(statusCode, statusMessage);
	}
	catch (Poco::Exception& exc)
	{
		if (logger().debug()) logger().debug(Poco::format("Error while shutting down WebSocket of subscriber %s: %s",
			describe(*pSubscriber), exc.displayText()));
		passive = true; // don't wait for client confirmation
	}
	if (!passive)
	{
		confirmShutdownImpl(*pSubscriber);
	}
	closeImpl(*pWS);
}


void WebEventServiceImpl::closeImpl(Poco::Net::WebSocket& webSocket)
{
	{
		Poco::FastMutex::ScopedLock lock(_closingMutex);

		_closingSockets.remove_if([&](const ClosingSocket& closing) { return closing.socket == webSocket; });
	}
	webSocket.close();
}


void WebEventServiceImpl::shutdownSocket(const Poco::Net::Socket& socket)
{
	try
	{
		// Not a TLS shutdown, which waits for the alert of a peer that may never read.
		// The WebSocket shares the descriptor of its stream socket.
		socket.impl()->Poco::Net::SocketImpl::shutdown();
#if defined(POCO_OS_FAMILY_WINDOWS)
		// A shutdown does not end a blocking send on Windows; a cancelled send is repeated and then fails.
		::CancelIoEx(reinterpret_cast<HANDLE>(socket.impl()->sockfd()), nullptr);
#endif
	}
	catch (Poco::Exception& exc)
	{
		if (logger().debug()) logger().debug("Error while shutting down a WebSocket: " + exc.displayText());
	}
}


void WebEventServiceImpl::confirmShutdownImpl(Subscriber& subscriber)
{
	// Once stopped, no watchdog ends the read at the deadline set by shutdown(),
	// and stopImpl() may already have shut the listed sockets down.
	if (!_stopped)
	{
		const Poco::Clock started;
		const Poco::Timespan timeout(WEBEVENT_SHUTDOWN_TIMEOUT);
		Poco::Buffer<char> buffer(WEBEVENT_MAX_FRAME_PAYLOAD);
		bool frameRejected = false;
		try
		{
			if (subscriber.pWebSocket->poll(timeout, Poco::Net::Socket::SELECT_READ))
			{
				int flags = 0;
				subscriber.pWebSocket->receiveFrame(buffer.begin(), static_cast<int>(buffer.size()), flags);
				if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE) return;
			}
			if (logger().debug()) logger().debug(Poco::format("WebSocket subscriber %s did not confirm the shutdown; the connection is closed.",
				describe(subscriber)));
		}
		catch (Poco::Net::WebSocketException& exc)
		{
			if (logger().debug())
			{
				logger().debug(Poco::format(
					"Frame from WebSocket subscriber %s rejected while waiting for the shutdown confirmation: %s",
					describe(subscriber), exc.displayText()));
			}
			frameRejected = true;
		}
		catch (Poco::Exception& exc)
		{
			if (logger().debug()) logger().debug(Poco::format("Error while waiting for WebSocket subscriber %s to confirm the shutdown: %s",
				describe(subscriber), exc.displayText()));
		}
		if (!frameRejected) return;

		// The rest of the frame may be unread, and closing a socket with unread data resets
		// the connection, which on Windows discards the CLOSE frame the client has not read.
		try
		{
			// Raw TCP: the WebSocket would reject the same header again, and a TLS
			// shutdown waits for the client.
			Poco::Net::SocketImpl* pImpl = subscriber.pWebSocket->impl();
			pImpl->Poco::Net::SocketImpl::shutdownSend();
			int discarded = 0;
			while (discarded < WEBEVENT_SHUTDOWN_DISCARD_LIMIT)
			{
				const Poco::Timespan remaining = timeout - started.elapsed();
				if (remaining <= 0 || !subscriber.pWebSocket->poll(remaining, Poco::Net::Socket::SELECT_READ)) break;
				const int n = pImpl->Poco::Net::SocketImpl::receiveBytes(buffer.begin(),
					std::min(static_cast<int>(buffer.size()), WEBEVENT_SHUTDOWN_DISCARD_LIMIT - discarded), 0);
				if (n <= 0) break;
				discarded += n;
			}
		}
		catch (Poco::Exception& exc)
		{
			if (logger().debug())
			{
				logger().debug(Poco::format("Error while discarding data from WebSocket subscriber %s: %s",
					describe(subscriber), exc.displayText()));
			}
		}
	}
}


void WebEventServiceImpl::stopImpl()
{
	{
		// Their workers may wait for a send, a receive or a confirmation
		// that the watchdog would no longer cut short.
		Poco::FastMutex::ScopedLock lock(_closingMutex);
		for (auto& closing: _closingSockets)
		{
			shutdownSocket(closing.socket);
		}
	}
	for (auto& entry: _socketMap)
	{
		Subscriber::Ptr pSubscriber = entry.second;
		unwatchSocketImpl(entry.first);
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->closed = true;
			pSubscriber->pending.clear();
			pSubscriber->bySubject.clear();
		}
		// No locks and no close(): a worker may still be inside a read
		// on this socket. Shutting the socket down makes that read
		// return; the descriptor goes with the last reference.
		shutdownSocket(*pSubscriber->pWebSocket);
	}
	{
		Poco::FastMutex::ScopedLock lock(_mapMutex);
		_subjectMap.clear();
	}
	_socketCount -= static_cast<int>(_socketMap.size());
	_socketMap.clear();
}


void WebEventServiceImpl::watchSocketImpl(const Poco::Net::Socket& socket, bool buffered)
{
	// Only a socket still subscribed is watched, and only once: the
	// notification a completed receive posts can arrive after a failed
	// send has already handed the socket to shutdown.
	const SocketMap::iterator it = _socketMap.find(socket);
	if (it == _socketMap.end()) return;
	// It has just been heard from, whether or not it is watched again
	// below, so the keepalive counts from here.
	it->second->lastActive.update();
	if (it->second->watched) return;
	if (buffered)
	{
		receive(it->second);
		return;
	}
	// Marked first: should the poll set fail to take a descriptor that has
	// been closed, the scan in runMain() still finds it.
	it->second->watched = true;
	_pPollSet->add(socket, Poco::Net::PollSet::POLL_READ | Poco::Net::PollSet::POLL_ERROR);
}


bool WebEventServiceImpl::stallCheckDueImpl() const
{
	return _lastStallCheck.isElapsed(Poco::Timespan(WEBEVENT_STALL_CHECK_INTERVAL).totalMicroseconds());
}


void WebEventServiceImpl::removeStalledSubscribersImpl()
{
	{
		Poco::FastMutex::ScopedLock lock(_closingMutex);

		const Poco::Clock now;
		for (auto& closing: _closingSockets)
		{
			if (!closing.shutDown && closing.deadline <= now)
			{
				closing.shutDown = true;
				shutdownSocket(closing.socket);
			}
		}
	}
	const Poco::Clock::ClockDiff sendTimeout = Poco::Timespan(WEBEVENT_SEND_TIMEOUT).totalMicroseconds();
	const Poco::Clock::ClockDiff receiveTimeout = Poco::Timespan(WEBEVENT_FRAME_RECEIVE_TIMEOUT).totalMicroseconds();
	std::vector<std::pair<Subscriber::Ptr, std::string>> stalled;
	for (auto& entry: _socketMap)
	{
		Poco::FastMutex::ScopedLock lock(entry.second->queueMutex);
		std::string reason;
		if (entry.second->sending && entry.second->sendStarted.isElapsed(sendTimeout))
		{
			reason = Poco::format("a frame has not been sent within %d seconds", static_cast<int>(WEBEVENT_SEND_TIMEOUT.count()));
		}
		else if (entry.second->receiving && entry.second->receiveStarted.isElapsed(receiveTimeout))
		{
			reason = Poco::format("a frame has not been received within %d seconds", static_cast<int>(WEBEVENT_FRAME_RECEIVE_TIMEOUT.count()));
		}
		if (!reason.empty())
		{
			// Closed before the socket is shut down, so that the failing
			// send or receive does not report the removal a second time.
			entry.second->closed = true;
			stalled.emplace_back(entry.second, reason);
		}
	}
	for (const auto& [pSubscriber, reason]: stalled)
	{
		logger().information(Poco::format("Disconnecting WebSocket subscriber %s: %s.", describe(*pSubscriber), reason));
		// Ends the send or receive that holds a worker; the worker then
		// finds the subscriber closed.
		shutdownSocket(*pSubscriber->pWebSocket);
		removeSubscriberImpl(pSubscriber->pWebSocket, true);
	}
}


bool WebEventServiceImpl::pingSweepDueImpl() const
{
	return _pingInterval > std::chrono::seconds::zero()
		&& _lastPingSweep.isElapsed(Poco::Timespan(_pingInterval).totalMicroseconds());
}


void WebEventServiceImpl::pingIdleSubscribersImpl()
{
	// The ping threshold must stay well below the sweep period and the dead
	// threshold well above it: sweeps run on the same cadence as the interval,
	// and a PONG lands milliseconds after the sweep that pinged, so a socket's
	// idle time naturally aligns to the sweep phase. With thresholds at exactly
	// 1x/2x the sweep period, a responsive socket's idle time peaks within
	// jitter of the dead threshold and healthy connections get reaped.
	// Half/double gives every live socket a full sweep period of margin.
	const Poco::Clock::ClockDiff interval = Poco::Timespan(_pingInterval).totalMicroseconds();
	const Poco::Clock::ClockDiff pingIdle = interval/2;
	const Poco::Clock::ClockDiff deadIdle = 2*interval;

	std::vector<Subscriber::Ptr> stale;
	for (SocketMap::const_iterator it = _socketMap.begin(); it != _socketMap.end(); ++it)
	{
		Subscriber::Ptr pSubscriber = it->second;
		Poco::Clock lastPingSent;
		bool pingQueued = false;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			lastPingSent = pSubscriber->lastPingSent;
			pingQueued = pSubscriber->pingPending;
		}

		// Only a ping that actually left counts against a subscriber. One
		// still queued behind data frames, or waiting on a busy worker, has
		// not been ignored - it has not been asked yet.
		if (pSubscriber->lastActive.isElapsed(deadIdle) && lastPingSent > pSubscriber->lastActive)
		{
			stale.push_back(pSubscriber);
		}
		else if (pSubscriber->lastActive.isElapsed(pingIdle) && !pingQueued)
		{
			{
				Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
				pSubscriber->pingPending = true;
			}
			// Protocol-level PING: browsers reply with a PONG automatically,
			// which lands in receiveImpl() and refreshes lastActive via
			// watchSocketImpl(). Invisible to client-side JavaScript.
			// Goes through the subscriber's send queue (empty subject: never
			// coalesced) so it cannot interleave with an in-flight data frame.
			enqueueSend(pSubscriber, std::string(), std::make_shared<const std::string>(),
				static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING));
		}
	}
	for (std::vector<Subscriber::Ptr>::const_iterator it = stale.begin(); it != stale.end(); ++it)
	{
		logger().information(Poco::format("Removing WebSocket subscriber %s: nothing received for at least %d seconds, although a keepalive PING was sent.",
			describe(**it), static_cast<int>(2*_pingInterval.count())));
		// Closed without waiting to be answered. This subscriber has
		// ignored pings for two intervals, so it will not confirm a close
		// either, and that wait holds a worker for the shutdown timeout -
		// long enough, where workers are few or slowed by a sanitizer, for
		// the sockets that ARE answering to go unread and be reaped in
		// their turn.
		removeSubscriberImpl((*it)->pWebSocket, true);
	}
}


void WebEventServiceImpl::unwatchSocketImpl(const Poco::Net::Socket& socket)
{
	const SocketMap::iterator it = _socketMap.find(socket);
	if (it == _socketMap.end() || !it->second->watched) return;
	it->second->watched = false;
	// The poll set finds a socket by its descriptor, so one closed behind
	// its back cannot be removed; removeBadSocketsImpl() replaces the set.
	if (socket.impl()->sockfd() != POCO_INVALID_SOCKET)
	{
		_pPollSet->remove(socket);
	}
}


void WebEventServiceImpl::removeBadSocketsImpl()
{
	std::vector<Subscriber::Ptr> badSubscribers;
	for (const auto& entry: _socketMap)
	{
		if (!entry.second->watched) continue;
		try
		{
			int socktype;
			entry.first.impl()->getOption(SOL_SOCKET, SO_TYPE, socktype);
		}
		catch (...)
		{
			badSubscribers.push_back(entry.second);
		}
	}
	if (badSubscribers.empty()) return;

	for (const auto& pSubscriber: badSubscribers)
	{
		logger().notice(Poco::format("Removing WebSocket subscriber %s: its socket descriptor is no longer valid.",
			describe(*pSubscriber)));
		removeSubscriberImpl(pSubscriber->pWebSocket, false);
	}

	// The descriptors are still in the old set, which would go on reporting
	// them (poll()), so the sockets still watched move to a new one.
	auto pPollSet = std::make_unique<Poco::Net::PollSet>();
	for (const auto& entry: _socketMap)
	{
		if (entry.second->watched)
		{
			pPollSet->add(entry.first, Poco::Net::PollSet::POLL_READ | Poco::Net::PollSet::POLL_ERROR);
		}
	}
	{
		Poco::FastMutex::ScopedLock lock(_pollSetMutex);
		std::swap(_pPollSet, pPollSet);
	}
	// A wake-up that went to the old set is lost; the queue is looked at
	// before the next wait, and later enqueues wake the new set.
	_wakeUpPending.store(false);
}


void WebEventServiceImpl::enqueueMain(const WebEventTaskNotification::Ptr& pTask)
{
	_mainQueue.enqueueNotification(pTask);
	wakeUpMain();
}


void WebEventServiceImpl::wakeUpMain()
{
	// Once until the main thread has woken up: more would only fill the
	// wake-up pipe of the poll() implementation.
	if (!_wakeUpPending.exchange(true))
	{
		Poco::FastMutex::ScopedLock lock(_pollSetMutex);
		_pPollSet->wakeUp();
	}
}


WebEventServiceImpl::SocketMap& WebEventServiceImpl::socketMap()
{
	return _socketMap;
}


Poco::NotificationQueue& WebEventServiceImpl::mainQueue()
{
	return _mainQueue;
}


Poco::NotificationQueue& WebEventServiceImpl::workerQueue()
{
	return _workerQueue;
}


int WebEventServiceImpl::defaultWorkerCount()
{
	int processors = static_cast<int>(Poco::Environment::processorCount());
	if (processors <= 0) processors = 1;
	return 2 * processors;
}


std::string WebEventServiceImpl::describe(const Subscriber& subscriber)
{
	if (subscriber.username.empty()) return subscriber.clientAddress;
	return Poco::format("%s (user %s)", subscriber.clientAddress, subscriber.username);
}


Poco::Logger& WebEventServiceImpl::logger() const
{
	return _logger;
}


void WebEventServiceImpl::preflightRequest(Poco::Net::HTTPServerRequest& request)
{
	// Runs on HTTP server threads. The slot is taken here rather than when
	// the main thread adds the subscriber, so that concurrent upgrades
	// cannot all pass the check before any of them has been added.
	int count = _socketCount.load();
	while (_maxSockets <= 0 || count < _maxSockets)
	{
		if (_socketCount.compare_exchange_weak(count, count + 1)) return;
	}
	request.response().setStatusAndReason(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	logRejectedUpgrade(Rejection::LIMIT,
		Poco::format("WebSocket connection from %s rejected: the limit of %d WebSocket connections "
			"(osp.web.event.maxWebSockets) is reached.",
			request.clientAddress().toString(), _maxSockets));
}


void WebEventServiceImpl::logRejectedUpgrade(Rejection rejection, const std::string& message)
{
	bool notice = false;
	{
		Poco::FastMutex::ScopedLock lock(_rejectionLogMutex);

		Poco::Clock& lastNotice = _lastRejectionNotice.at(static_cast<std::size_t>(rejection));
		if (lastNotice.isElapsed(Poco::Timespan(WEBEVENT_REJECTION_NOTICE_INTERVAL).totalMicroseconds()))
		{
			lastNotice.update();
			notice = true;
		}
	}
	if (notice)
	{
		logger().notice(message);
	}
	else if (logger().debug())
	{
		logger().debug(message);
	}
}


void WebEventServiceImpl::releaseSocketSlot()
{
	--_socketCount;
}


} // namespace Poco::OSP::WebEvent
