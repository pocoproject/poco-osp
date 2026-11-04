//
// MailDeliveryServiceImpl.h
//
// Library: OSP/Mail
// Package: Mail
// Module:  MailDeliveryServiceImpl
//
// Definition of the MailDeliveryServiceImpl class.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Mail_MailDeliveryServiceImpl_INCLUDED
#define OSP_Mail_MailDeliveryServiceImpl_INCLUDED


#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"


namespace Poco::OSP::Mail {


class OSPMail_API MailDeliveryServiceImpl: public MailDeliveryService, public Poco::Runnable
	/// The default implementation of the MailDeliveryService.
	///
	/// This implementation uses a Poco::Net::SMTPClientSession to
	/// deliver the emails. Emails are managed through a queue and
	/// sending is done in batches.
{
public:
	using Ptr = Poco::AutoPtr<MailDeliveryServiceImpl>;
	using SMTPClientSessionPtr = Poco::SharedPtr<Poco::Net::SMTPClientSession>;

	struct Configuration
	{
		std::string host;
		Poco::UInt16 port = 0;
		std::string clientHost;
		std::string sender;
		bool generateMID = false;
		Poco::Timespan socketTimeout;
		Poco::Timespan sessionTimeout;
		Poco::Net::SMTPClientSession::LoginMethod loginMethod = Poco::Net::SMTPClientSession::AUTH_NONE;
		std::string username;
		std::string password;
		int maxQueued = 0;
		int maxFailedLogins = 0;
		Poco::Timespan loginRetryDelay{10, 0}; /// time to wait after a failed login
	};

	MailDeliveryServiceImpl(const Configuration& config, Poco::Logger& logger);
		/// Creates the MailDeliveryServiceImpl using the given configuration and logger.
		///
		/// Messages are delivered after start() has been called.
		///
		/// Throws a Poco::InvalidArgumentException if the session timeout is shorter
		/// than 1 second or longer than 2147483647 milliseconds, the socket timeout
		/// is shorter than 1 second, or the port is 0.

	~MailDeliveryServiceImpl() override;
		/// Destroys the MailDeliveryServiceImpl and calls stop().
		///
		/// The delivery thread calls createSMTPClientSession(), so a subclass that
		/// overrides it calls stop() in its own destructor.

	void start();
		/// Starts the thread that delivers the queued messages.

	void stop();
		/// Stops the delivery thread and waits until it has ended.
		///
		/// The messages that are still queued fail with a Poco::IllegalStateException,
		/// and sendMessage() throws one from then on.

	// MailDeliveryService
	MailDeliveryHandle::Ptr sendMessage(MailMessagePtr pMessage) override;
		/// Queues the message for delivery.
		///
		/// The message is not changed: a message without a sender is sent with the
		/// configured sender, and, if generateMID is set and clientHost is not empty,
		/// a message without a Message-ID header field is sent with a generated one.
		///
		/// Throws a Poco::RuntimeException if maxQueued messages are queued, and a
		/// Poco::IllegalStateException after stop().

	[[nodiscard]] static Configuration loadConfiguration(const Poco::Util::AbstractConfiguration& settings, const Poco::Util::AbstractConfiguration& defaults, Poco::Logger& logger);
		/// Reads the osp.mail.* settings from settings (the application configuration),
		/// and those that settings does not contain from defaults (the bundle properties).
		///
		/// The login method is case insensitive; an invalid one is logged as a warning
		/// and replaced by none.
		///
		/// Throws a Poco::InvalidArgumentException if osp.mail.port is not between
		/// 1 and 65535.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

protected:
	[[nodiscard]] virtual SMTPClientSessionPtr createSMTPClientSession(const Configuration& config);
	void closeSMTPClientSession();
	[[nodiscard]] std::string generateMID(const std::string& clientHost) const;
	void run() override;

private:
	void deliver(const Poco::Net::MailMessage& message);
		/// Sends the message over the SMTP session, with the sender and the
		/// Message-ID that sendMessage() describes.

	[[nodiscard]] bool resetSMTPClientSession();
		/// Sends RSET, which ends any transaction left in the SMTP session. Returns true if the
		/// server accepts it (250), false if it refuses it or the session fails.

	[[nodiscard]] bool failUnfinished(MailDeliveryHandle::Ptr pHandle, const Poco::Exception& exc);
		/// Fails the message of the handle with exc if its delivery has not finished.
		/// Returns true if the message has been failed.

	Configuration _config;
	Poco::NotificationQueue _queue;
	Poco::Thread _thread;
	SMTPClientSessionPtr _pSMTPClientSession;
	Poco::FastMutex _mutex;
	bool _stopped = false;
	Poco::Event _shutdown;
	Poco::Logger& _logger;
};


} // namespace Poco::OSP::Mail


#endif // OSP_Mail_MailDeliveryServiceImpl_INCLUDED
