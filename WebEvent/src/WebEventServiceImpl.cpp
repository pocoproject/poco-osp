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
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Buffer.h"
#include "Poco/Debugger.h"
#include "Poco/Event.h"
#include "Poco/Environment.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include <algorithm>
#include <iterator>


namespace Poco {
namespace OSP {
namespace WebEvent {


const std::string WebEventServiceImpl::SUBSCRIBE("SUBSCRIBE");
const std::string WebEventServiceImpl::UNSUBSCRIBE("UNSUBSCRIBE");
const std::string WebEventServiceImpl::NOTIFY("NOTIFY");
const std::string WebEventServiceImpl::VERSION("WebEvent/1.0");
const std::string WebEventServiceImpl::SYSTEM_PING("system.ping");
const std::string WebEventServiceImpl::SYSTEM_PONG("system.pong");


using TaskNotification = WebEventTaskNotification;


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

	void execute()
	{
		_service.addSubscriberImpl(_pWS, _username, _page, _clientAddress);
	}

private:
	Poco::SharedPtr<Poco::Net::WebSocket> _pWS;
	std::string _username;
	std::string _page;
	std::string _clientAddress;
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

	void execute()
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

	void execute()
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

	void execute()
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

	void execute()
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

	void execute()
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

	void execute()
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

	void execute()
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

	void execute()
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
	WatchSocketNotification(WebEventServiceImpl& service, const Poco::Net::Socket& socket):
		TaskNotification(service),
		_socket(socket)
	{
	}

	void execute()
	{
		_service.watchSocketImpl(_socket);
	}

private:
	Poco::Net::Socket _socket;
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

	void execute()
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

	void execute()
	{
	}
};


WebEventServiceImpl::WebEventServiceImpl(Poco::OSP::BundleContext::Ptr pContext, int maxSockets, int workerCount, int maxPendingFrames, int pingIntervalSeconds):
	_pContext(pContext),
	_maxSockets(maxSockets),
	_maxPendingFrames(maxPendingFrames),
	_pingInterval(pingIntervalSeconds),
	_socketCount(0),
	_mainRunnable(*this, &WebEventServiceImpl::runMain),
	_workerRunnable(*this, &WebEventServiceImpl::runWorker),
	_stopped(false),
	_logger(pContext ? pContext->logger() : Poco::Logger::get("Poco.OSP.WebEvent"))
{
	int actualWorkerCount = workerCount > 0 ? workerCount : defaultWorkerCount();
	for (int i = 0; i < actualWorkerCount; ++i)
	{
		ThreadPtr pThread = new Poco::Thread;
		pThread->start(_workerRunnable);
		_workerThreads.push_back(pThread);
	}
	_mainThread.start(_mainRunnable);
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

	// One wake-up per waiting thread, as a queued notification rather than
	// wakeUpAll(): its sticky flag is cleared by any later enqueue, which
	// would leave a thread waiting on an empty queue after the flag is set.
	_mainQueue.enqueueUrgentNotification(new StopNotification(*this));
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
	_mainQueue.enqueueNotification(new NotifyNotification(*this, subjectName, data));
}


void WebEventServiceImpl::notifyUser(const std::string& username, const std::string& subjectName, const std::string& data)
{
	if (_stopped) return;
	_mainQueue.enqueueNotification(new NotifyUserNotification(*this, username, subjectName, data));
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
			logger().error("User resolver failed: " + exc.displayText());
		}
		catch (std::exception& exc)
		{
			logger().error(std::string("User resolver failed: ") + exc.what());
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
			Poco::Net::Socket::SocketList readList(_socketList);
			Poco::Net::Socket::SocketList writeList;
			Poco::Net::Socket::SocketList exceptList(_socketList);

			Poco::Timespan timeout(5000);
			int n = Poco::Net::Socket::select(readList, writeList, exceptList, timeout);
			if (n > 0)
			{
				// A socket that is readable and in error at once goes to one
				// worker only: removing the subscriber takes it out of the
				// map first, so the read loop no longer finds it.
				for (Poco::Net::Socket::SocketList::iterator it = exceptList.begin(); it != exceptList.end(); ++it)
				{
					SocketMap::iterator its = _socketMap.find(*it);
					if (its != _socketMap.end())
					{
						removeSubscriberImpl(its->second->pWebSocket, false);
					}
				}
				for (Poco::Net::Socket::SocketList::iterator it = readList.begin(); it != readList.end(); ++it)
				{
					SocketMap::iterator its = _socketMap.find(*it);
					if (its != _socketMap.end())
					{
						// Readable means the peer has sent something, which is
						// what the keepalive asks about. Stamped here rather
						// than where the frame is finally read, because that
						// happens on a worker and is posted back to this queue:
						// a busy worker would let a subscriber that is in fact
						// answering look silent long enough to be reaped.
						its->second->lastActive.update();
						receive(its->second);
						unwatchSocketImpl(*it);
					}
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			logger().error("Exception in main thread: %s (%s)",
				exc.displayText(), poco_src_loc);
			removeBadSocketsImpl();
		}

		if (pingSweepDueImpl())
		{
			pingIdleSubscribersImpl();
			_lastPingSweep.update();
		}

		// A bounded wait, so the stop flag is re-checked at least once a
		// second whatever the queue does - and so a sweep that is due is
		// not held off by a queue that stays quiet.
		Poco::Notification::Ptr pNf = _socketList.empty() ? _mainQueue.waitDequeueNotification(WEBEVENT_QUEUE_WAIT_MS) : _mainQueue.dequeueNotification();
		while (pNf && !_stopped)
		{
			try
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf)
				{
					pTaskNf->execute();
				}
			}
			catch (Poco::Exception& exc)
			{
				logger().error("Exception in main thread: %s (%s)",
					exc.displayText(), poco_src_loc);
			}
			// Back to the outer loop between notifications when a sweep
			// falls due, so that a queue kept busy by its producers does
			// not leave dead subscribers unreclaimed. Nothing already
			// dequeued is dropped: the check sits after the notification
			// has been executed and before the next is taken.
			if (pingSweepDueImpl()) break;
			pNf = _socketList.empty() ?
				_mainQueue.waitDequeueNotification(WEBEVENT_QUEUE_WAIT_MS) :
				_mainQueue.dequeueNotification();
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
			Poco::Notification::Ptr pNf = _workerQueue.waitDequeueNotification(WEBEVENT_QUEUE_WAIT_MS);
			if (pNf && !_stopped)
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf)
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
	}
}


void WebEventServiceImpl::addSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress)
{
	if (_stopped) return;
	_mainQueue.enqueueNotification(new AddSubscriberNotification(*this, pWS, username, page, clientAddress));
}


void WebEventServiceImpl::removeSubscriber(Poco::SharedPtr<Poco::Net::WebSocket> pWS, bool clientShutdown)
{
	_mainQueue.enqueueNotification(new RemoveSubscriberNotification(*this, pWS, clientShutdown));
}


void WebEventServiceImpl::subscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	_mainQueue.enqueueNotification(new SubscribeNotification(*this, pWS, subjectNames));
}


void WebEventServiceImpl::unsubscribe(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	_mainQueue.enqueueNotification(new UnsubscribeNotification(*this, pWS, subjectNames));
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
	_mainQueue.enqueueNotification(new ListSubscribersNotification(*this, pState));

	Poco::Clock deadline;
	while (!pState->done.tryWait(WEBEVENT_LIST_SUBSCRIBERS_POLL_MS))
	{
		// A stop between the check above and the event thread leaving its
		// loop leaves this notification unexecuted. The documented answer
		// for a stopped service is an empty list, arrived at now rather
		// than at the end of a wait nothing can satisfy.
		if (_stopped) return SubscriberInfoVec();
		if (deadline.isElapsed(Poco::Clock::ClockDiff(WEBEVENT_LIST_SUBSCRIBERS_TIMEOUT)*Poco::Clock::resolution()))
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
		auto itBySubject = subject.empty() ? pSubscriber->bySubject.end() : pSubscriber->bySubject.find(subject);
		if (itBySubject != pSubscriber->bySubject.end())
		{
			// Latest value wins; move to the tail so frames still leave in
			// the order the notifications were made.
			itBySubject->second->pFrame = std::move(pFrame);
			itBySubject->second->flags = flags;
			pSubscriber->pending.splice(pSubscriber->pending.end(), pSubscriber->pending, itBySubject->second);
		}
		else
		{
			pSubscriber->pending.push_back(Subscriber::PendingFrame{subject, std::move(pFrame), flags});
			if (!subject.empty())
			{
				pSubscriber->bySubject[subject] = std::prev(pSubscriber->pending.end());
			}
			if (_maxPendingFrames > 0 && pSubscriber->pending.size() > static_cast<std::size_t>(_maxPendingFrames))
			{
				const Subscriber::PendingFrame& front = pSubscriber->pending.front();
				if (!front.subject.empty())
				{
					pSubscriber->bySubject.erase(front.subject);
				}
				pSubscriber->pending.pop_front();
				dropped = ++pSubscriber->droppedFrames;
				if (pSubscriber->lastDropWarning.isElapsed(Poco::Clock::ClockDiff(WEBEVENT_DROP_WARNING_INTERVAL)*Poco::Clock::resolution()))
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
		logger().warning(Poco::format("Subscriber cannot keep up; dropped %Lu frame(s) so far (limit %d per subscriber).", dropped, _maxPendingFrames));
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


void WebEventServiceImpl::shutdown(Subscriber::Ptr pSubscriber, Poco::UInt16 statusCode, const std::string& statusMessage, bool passive)
{
	_workerQueue.enqueueNotification(new ShutdownNotification(*this, pSubscriber, statusCode, statusMessage, passive));
}


void WebEventServiceImpl::notifyImpl(const std::string& subjectName, const std::string& data)
{
	NotificationEvent ev(subjectName, data);
	notificationSent(ev);

	std::string message(NOTIFY);
	message += ' ';
	message += subjectName;
	message += ' ';
	message += VERSION;
	message += "\r\n";
	message += data;
	auto pFrame = std::make_shared<const std::string>(std::move(message));

	std::vector<Subscriber::Ptr> targets = collectSubscribers(subjectName);

	std::string s(subjectName);
	std::string::size_type pos = std::string::npos;
	do
	{
		// collect under _mapMutex (subjectNotified() may insert concurrently),
		// fire outside it so delegates can call back into the service
		std::vector<EventPtr> events;
		{
			Poco::FastMutex::ScopedLock lock(_mapMutex);
			auto range = _eventMap.equal_range(s);
			for (auto itEv = range.first; itEv != range.second; ++itEv)
			{
				events.push_back(itEv->second);
			}
		}
		for (auto& pEvent: events)
		{
			(*pEvent)(ev);
		}
		pos = s.find_last_of('.');
		if (pos != std::string::npos)
		{
			s.resize(pos);
		}
	}
	while (pos != std::string::npos);
	for (auto& pSubscriber: targets)
	{
		enqueueSend(pSubscriber, subjectName, pFrame);
	}
}


void WebEventServiceImpl::notifyUserImpl(const std::string& username, const std::string& subjectName, const std::string& data)
{
	if (username.empty()) return;

	std::string message(NOTIFY);
	message += ' ';
	message += subjectName;
	message += ' ';
	message += VERSION;
	message += "\r\n";
	message += data;
	auto pFrame = std::make_shared<const std::string>(std::move(message));

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
	std::string s(subjectName);
	std::string::size_type pos = std::string::npos;
	do
	{
		auto subscribers = _subjectMap.equal_range(s);
		for (auto it = subscribers.first; it != subscribers.second; ++it)
		{
			if (!username.empty() && it->second->username != username) continue;
			if (seen.insert(it->second.get()).second)
			{
				targets.push_back(it->second);
			}
		}
		pos = s.find_last_of('.');
		if (pos != std::string::npos)
		{
			s.resize(pos);
		}
	}
	while (pos != std::string::npos);
	return targets;
}


void WebEventServiceImpl::addSubscriberImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::string& username, const std::string& page, const std::string& clientAddress)
{
	Subscriber::Ptr pSubscriber = new Subscriber;
	pSubscriber->pWebSocket = pWS;
	pSubscriber->username = username;
	pSubscriber->page = page;
	pSubscriber->clientAddress = clientAddress;
	_socketMap[*pWS] = pSubscriber;
	_socketCount = static_cast<int>(_socketMap.size());
	watchSocketImpl(*pWS);
	pWS->setSendTimeout(Poco::Timespan(WEBEVENT_SEND_TIMEOUT, 0));
	pWS->setReceiveTimeout(Poco::Timespan(WEBEVENT_RECEIVE_TIMEOUT, 0));
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
		_socketMap.erase(it);
		_socketCount = static_cast<int>(_socketMap.size());
		unwatchSocketImpl(*pWS);
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
			logger().error("Exception while removing subscriber: " + exc.displayText());
		}
	}
}


void WebEventServiceImpl::subscribeImpl(Poco::SharedPtr<Poco::Net::WebSocket> pWS, const std::set<std::string>& subjectNames)
{
	SocketMap::iterator it = _socketMap.find(*pWS);
	if (it != _socketMap.end())
	{
		for (std::set<std::string>::const_iterator itSub = subjectNames.begin(); itSub != subjectNames.end(); ++itSub)
		{
			if (it->second->subjectNames.find(*itSub) == it->second->subjectNames.end())
			{
				it->second->subjectNames.insert(*itSub);
				{
					Poco::FastMutex::ScopedLock lock(_mapMutex);
					_subjectMap.insert(SubjectMap::value_type(*itSub, it->second));
				}
				subscriptionRequestReceived.notify(this, *itSub);
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
		if (subjectNames.find("*") != subjectNames.end())
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
					unsubscriptionRequestReceived.notify(this, *itSub);
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
	}
	try
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->writeMutex);

		pSubscriber->pWebSocket->sendFrame(pFrame->data(), static_cast<int>(pFrame->size()), flags);
	}
	catch (Poco::Exception& exc)
	{
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->closed = true;
			pSubscriber->pending.clear();
			pSubscriber->bySubject.clear();
			pSubscriber->draining = false;
			pSubscriber->pingPending = false;
		}
		logger().error("Error while sending frame: " + exc.displayText());
		removeSubscriber(pSubscriber->pWebSocket, false);
		return;
	}
	// Re-enqueue one notification per frame so workers round-robin
	// across subscribers instead of one subscriber holding a worker.
	bool more = false;
	{
		Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
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
		Poco::Buffer<char> buffer(4096);
		int flags;
		int n;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->readMutex);
			n = pWS->receiveFrame(buffer.begin(), static_cast<int>(buffer.size()), flags);
		}
		if (logger().debug()) logger().debug(Poco::format("Frame received (length=%d, flags=0x%x).", n, unsigned(flags)));

		// Opcodes are a 4-bit field, not flags: CLOSE (0x8) shares a bit with
		// PONG (0xA), so testing bits instead of the masked value read every
		// client CLOSE as a stray PONG and left it unanswered. Firefox keeps
		// the TCP connection open for 20s waiting for the CLOSE reply, so a
		// closed tab stayed a live subscriber for that long.
		const int opcode = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
		if (opcode == Poco::Net::WebSocket::FRAME_OP_PONG)
		{
			logger().debug("Received unsolicited PONG frame - ignoring.");
			_mainQueue.enqueueNotification(new WatchSocketNotification(*this, *pWS));
			return;
		}

		if (opcode == Poco::Net::WebSocket::FRAME_OP_CLOSE || n == 0)
		{
			removeSubscriber(pWS, true);
			return;
		}

		const char* it  = buffer.begin();
		const char* end = buffer.begin() + n;
		std::string verb;
		while (it != end && *it != ' ') verb += *it++;
		if (verb == SUBSCRIBE || verb == UNSUBSCRIBE || verb == NOTIFY)
		{
			while (it != end && *it == ' ') ++it;
			std::set<std::string> subjectNames;
			while (it != end && *it != ' ')
			{
				std::string subject;
				int bracketDepth = 0;
				while (it != end && *it != ' ')
				{
					if (*it == '[' || *it == '(' || *it == '{') bracketDepth++;
					else if (*it == ']' || *it == ')' || *it == '}') bracketDepth--;
					else if (*it == ',' && bracketDepth == 0) break; // Only break on comma outside brackets
					subject += *it++;
				}
				if (it != end)
				{
					subjectNames.insert(subject);
					if (*it == ',') it++; // skip ','
				}
			}
			while (it != end && *it == ' ') ++it;
			std::string version;
			while (it != end && *it != '\r' && *it != '\n') version += *it++;
			if (it != end && *it == '\r') ++it;
			if (it != end && *it == '\n') ++it;
			if (version == VERSION)
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
					std::string data;
					while (it != end) data += *it++;
					for (std::set<std::string>::const_iterator it = subjectNames.begin(); it != subjectNames.end(); ++it)
					{
						if (*it == SYSTEM_PING)
						{
							std::string pong(NOTIFY);
							pong += ' ';
							pong += SYSTEM_PONG;
							pong += ' ';
							pong += VERSION;
							pong += "\r\n";
							pong += data;
							// empty subject: pong replies carry echo data and must not be coalesced
							enqueueSend(pSubscriber, std::string(), std::make_shared<const std::string>(std::move(pong)));
						}
						else
						{
							notify(*it, data);
						}
					}
				}
				_mainQueue.enqueueNotification(new WatchSocketNotification(*this, *pWS));
				return;
			}
		}
		logger().notice("Client protocol violation. Closing WebSocket.");
	}
	catch (Poco::Exception& exc)
	{
		logger().error("Error while receiving frame: " + exc.displayText());
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
		pWS->close();
		return;
	}

	logger().notice("Shutting down WebSocket.");
	try
	{
		pWS->shutdown(statusCode, statusMessage);
	}
	catch (Poco::Exception& exc)
	{
		logger().notice("Error while shutting down WebSocket: " + exc.displayText());
		passive = true; // don't wait for client confirmation
	}
	if (!passive)
	{
		try
		{
			if (pWS->poll(Poco::Timespan(WEBEVENT_SHUTDOWN_TIMEOUT, 0), Poco::Net::Socket::SELECT_READ))
			{
				Poco::Buffer<char> buffer(4096);
				int flags;
				pWS->receiveFrame(buffer.begin(), static_cast<int>(buffer.size()), flags);
				if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE)
				{
					pWS->close();
					return;
				}
			}
			logger().notice("Client failed to confirm shutdown.");
		}
		catch (Poco::Exception& exc)
		{
			logger().notice("Error while waiting for shutdown confirmation: " + exc.displayText());
		}
	}
	pWS->close();
}


void WebEventServiceImpl::stopImpl()
{
	for (auto& entry: _socketMap)
	{
		Subscriber::Ptr pSubscriber = entry.second;
		{
			Poco::FastMutex::ScopedLock lock(pSubscriber->queueMutex);
			pSubscriber->closed = true;
			pSubscriber->pending.clear();
			pSubscriber->bySubject.clear();
		}
		try
		{
			// No locks and no close(): a worker may still be inside a read
			// on this socket. Shutting the socket down makes that read
			// return; the descriptor goes with the last reference.
			pSubscriber->pWebSocket->impl()->shutdown();
		}
		catch (Poco::Exception& exc)
		{
			if (logger().debug()) logger().debug("Error while shutting down socket on stop: " + exc.displayText());
		}
	}
	{
		Poco::FastMutex::ScopedLock lock(_mapMutex);
		_subjectMap.clear();
	}
	_socketMap.clear();
	_socketList.clear();
	_socketCount = 0;
}


void WebEventServiceImpl::watchSocketImpl(const Poco::Net::Socket& socket)
{
	// Only a socket still subscribed is watched, and only once: the
	// notification a completed receive posts can arrive after a failed
	// send has already handed the socket to shutdown.
	const SocketMap::iterator it = _socketMap.find(socket);
	if (it == _socketMap.end()) return;
	// It has just been heard from, whether or not it is watched again
	// below, so the keepalive counts from here.
	it->second->lastActive.update();
	if (std::find(_socketList.begin(), _socketList.end(), socket) != _socketList.end()) return;
	_socketList.push_back(socket);
}


bool WebEventServiceImpl::pingSweepDueImpl() const
{
	return _pingInterval > 0 && _lastPingSweep.isElapsed(Poco::Timespan(_pingInterval, 0).totalMicroseconds());
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
	const Poco::Clock::ClockDiff interval = Poco::Timespan(_pingInterval, 0).totalMicroseconds();
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
		logger().notice("Removing unresponsive WebSocket subscriber.");
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
	Poco::Net::Socket::SocketList::iterator itd = std::find(_socketList.begin(), _socketList.end(), socket);
	if (itd != _socketList.end())
	{
		_socketList.erase(itd);
	}
}


void WebEventServiceImpl::removeBadSocketsImpl()
{
	Poco::Net::Socket::SocketList badSockets;
	for (auto it = _socketList.begin(); it != _socketList.end(); ++it)
	{
		try
		{
			int socktype;
			it->impl()->getOption(SOL_SOCKET, SO_TYPE, socktype);
		}
		catch (...)
		{
			badSockets.push_back(*it);
		}
	}
	for (auto it = badSockets.begin(); it != badSockets.end(); ++it)
	{
		SocketMap::iterator its = _socketMap.find(*it);
		if (its != _socketMap.end())
		{
			logger().notice("Removing subscriber with bad socket descriptor.");
			removeSubscriberImpl(its->second->pWebSocket, false);
		}
		else
		{
			unwatchSocketImpl(*it);
		}
	}
}


Poco::Net::Socket::SocketList& WebEventServiceImpl::socketList()
{
	return _socketList;
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
	int processors = Poco::Environment::processorCount();
	if (processors <= 0) processors = 1;
	return 2 * processors;
}


Poco::Logger& WebEventServiceImpl::logger() const
{
	return _logger;
}


void WebEventServiceImpl::preflightRequest(Poco::Net::HTTPServerRequest& request)
{
	// runs on HTTP server threads; _socketMap itself is main-thread-only
	if (_maxSockets > 0 && _socketCount.load() >= _maxSockets)
	{
		request.response().setStatusAndReason(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	}
}


} } } // namespace Poco::OSP::WebEvent
