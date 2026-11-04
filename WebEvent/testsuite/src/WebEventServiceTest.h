//
// WebEventServiceTest.h
//
// Copyright (c) 2025, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//

#ifndef WebEventServiceTest_INCLUDED
#define WebEventServiceTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/AutoPtr.h"
#include "Poco/Event.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/ThreadPool.h"
#include <atomic>
#include <memory>
#include <string>


class WebEventServiceTest: public CppUnit::TestCase
{
public:
	WebEventServiceTest(const std::string& name);
	~WebEventServiceTest() override;

	void testNotifyDispatchesToSubjectHierarchy();
	void testSubjectNotifiedReturnsSameEvent();
	void testNotifyNoDispatch();
	void testBadSocketDescriptorRecovery();
	void testOverlappingSubscriptionsEnqueueOneFrame();
	void testResetSocketIsDispatchedOnce();
	void testReceiveAndShutdownAreSerialized();
	void testStopReturnsWhenNotificationRacesStop();
	void testLocalServiceTearsDownSynchronously();
	void testListSubscribers();
	void testCloseFrameIsAnsweredWhileTcpStaysOpen();
	void testClientPingIsAnsweredWithPong();
	void testThrowingDelegateDoesNotStopDelivery();
	void testNotificationIsHandedOnWithoutPollDelay();
	void testCrossOriginUpgradeIsRejected();
	void testAllowedOriginsAreParsedTrimmedAndLowered();
	void testClientNotifyIsDroppedByDefault();
	void testClientNotifyReachesSubscribersWhenEnabled();
	void testConnectionLimitHoldsForConcurrentUpgrades();
	void testUpgradeAfterStopIsClosed();
	void testSubscribingBeyondSubjectLimitClosesConnection();
	void testWorkerCountIsDefaultedAndLimited();
	void testInvalidMessagesCloseTheConnection();
	void testSubjectListKeepsCommasInBrackets();
	void testUnsubscribeRemovesSubjects();
	void testUpgradeRequestHandlerChecks();
	void testOtherWebSocketErrorIsAnswered();
	void testNotifyUserWithoutUserSendsNothing();
	void testZeroPingIntervalDisablesKeepalive();
	void testListSubscribersThrowsOnTimeout();
	void testKeepaliveReapsSilentSubscribers();
	void testActivityIsStampedWhenReadable();
	void testClientThatKeepsSendingIsNotRemoved();
	void testKeepaliveSweepRunsUnderQueueLoad();
	void testUnsentPingDoesNotReap();
	void testSnapshotReturnsEmptyWhenStopped();
	void testSendQueueCoalescing();
	void testSendQueueBounded();
	void testKeepaliveSurvivesFullSendQueue();
	void testClientPingsQueueOnePong();
	void testPendingSendsAfterDisconnect();
	void testSlowConsumerDoesNotStallOthers();
	void testSlowReaderIsRemovedAfterSendTimeout();
	void testTricklingSenderIsRemovedAfterReceiveTimeout();
	void testTricklingCloseDoesNotHoldShutdown();
	void testRemovedSubscriberDoesNotHoldWorkers();
	void testStopClosesSocketsOfQueuedShutdowns();
#if defined(OSP_WEBEVENT_ENABLE_TLS)
	void testStopDoesNotWaitForTlsPeers();
	void testFramesInOneTlsRecordAreAllRead();
#endif
	void testNotifyUserDeliversOnlyToUser();
	void testConcurrentApiAccess();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	void onNotificationSent(const Poco::OSP::WebEvent::WebEventService::NotificationEvent& ev);
	void onChildNotified(const Poco::OSP::WebEvent::WebEventService::NotificationEvent& ev);
	void onParentNotified(const Poco::OSP::WebEvent::WebEventService::NotificationEvent& ev);
	void onSubscriptionReceived(const std::string& subject);
	void onSiblingNotified(const Poco::OSP::WebEvent::WebEventService::NotificationEvent& ev);
	void stopService(bool check = true);

	Poco::AutoPtr<Poco::OSP::WebEvent::WebEventServiceImpl> _pService;
	Poco::ThreadPool _pool;
	std::unique_ptr<Poco::Net::HTTPServer> _pServer;
	Poco::UInt16 _port = 0;
	Poco::Event _notificationEvent;
	Poco::Event _childEvent;
	Poco::Event _parentEvent;
	Poco::Event _siblingEvent;
	Poco::Event _childSubscriptionEvent;
	Poco::Event _parentSubscriptionEvent;
	Poco::Event _siblingSubscriptionEvent;
	Poco::Event _recoverySubscriptionEvent1;
	Poco::Event _recoverySubscriptionEvent2;
	int _recoverySubscriptionCount;
	std::atomic<int> _notificationCount;
	Poco::OSP::WebEvent::WebEventService::NotificationEvent _lastNotification;
	Poco::OSP::WebEvent::WebEventService::NotificationEvent _lastChildNotification;
	Poco::OSP::WebEvent::WebEventService::NotificationEvent _lastParentNotification;
	Poco::OSP::WebEvent::WebEventService::NotificationEvent _lastSiblingNotification;
	bool _notificationHandlerAdded = false;
	bool _childHandlerAdded = false;
	bool _parentHandlerAdded = false;
	bool _subscriptionHandlerAdded = false;
	bool _siblingHandlerAdded = false;
};


#endif // WebEventServiceTest_INCLUDED
