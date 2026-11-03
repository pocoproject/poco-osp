//
// MailDeliveryService.h
//
// Library: OSP/Mail
// Package: Mail
// Module:  MailDeliveryService
//
// Definition of the MailDeliveryService interface.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Mail_MailDeliveryService_INCLUDED
#define OSP_Mail_MailDeliveryService_INCLUDED


#include "Poco/OSP/Mail/Mail.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/RefCountedObject.h"
#include "Poco/SharedPtr.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/Notification.h"
#include "Poco/BasicEvent.h"
#include "Poco/Mutex.h"
#include <atomic>


namespace Poco::OSP::Mail {


using MailMessagePtr = Poco::SharedPtr<Poco::Net::MailMessage>;
using ExceptionPtr = Poco::SharedPtr<Poco::Exception>;


class OSPMail_API MailDeliveryHandle: public Poco::Notification
	/// This class contains status information about
	/// an email message delivery. It also provides
	/// events for asynchronous notification about
	/// delivery status.
{
public:
	using Ptr = Poco::AutoPtr<MailDeliveryHandle>;

	enum MailStatus
	{
		MAIL_QUEUED,      /// The mail message has been queued for delivery.
		MAIL_DELIVERING,  /// The mail message is being delivered to the SMTP server.
		MAIL_DELIVERED,   /// The mail message has been successfully delivered to the SMTP server.
		MAIL_FAILED       /// The mail message could not be sent.
	};

	Poco::BasicEvent<const Ptr> messageDelivering;
		/// Fired when the message is about to be delivered.

	Poco::BasicEvent<const Ptr> messageDelivered;
		/// Fired when the message was delivered successfully.

	Poco::BasicEvent<const Ptr> messageFailed;
		/// Fired when sending the message has failed.

	MailDeliveryHandle() = delete;

	explicit MailDeliveryHandle(MailMessagePtr pMessage);
		/// Creates the MailDeliveryHandle.

	~MailDeliveryHandle() override;
		/// Destroys the MailDeliveryHandle.

	[[nodiscard]] MailMessagePtr message() const;
		/// Returns the mail message.

	[[nodiscard]] MailStatus status() const;
		/// Returns the current status.

	[[nodiscard]] ExceptionPtr exception() const;
		/// Returns the exception if mail delivery failed, or a null pointer otherwise.

	void wait();
		/// Waits for the mail message to be delivered to the SMTP server.
		///
		/// After this method returns, call status() to check whether delivery
		/// was successful.

	void wait(long milliseconds);
		/// Waits for the mail message to be delivered to the SMTP server.
		///
		/// Throws a Poco::TimeoutException if the message could not be delivered
		/// within the given timeout.
		///
		/// After this method returns, call status() to check whether delivery
		/// was successful.

	[[nodiscard]] bool tryWait(long milliseconds);
		/// Waits for the mail message to be delivered to the SMTP server.
		///
		/// Returns true if the mail message was delivered (or delivery failed), false otherwise.
		///
		/// After this method returns, call status() to check whether delivery
		/// was successful.

protected:
	void updateStatus(MailStatus status);
		/// Updates the status and signals the event if the status is MAIL_DELIVERED
		/// or MAIL_FAILED.

	void updateStatus(const Poco::Exception& exc);
		/// Sets the status to MAIL_FAILED and stores a copy of the exception.

private:
	MailMessagePtr _pMessage;
	Poco::Event _event;
	std::atomic<MailStatus> _status;
	ExceptionPtr _pException;
	mutable Poco::FastMutex _mutex;

	friend class MailDeliveryServiceImpl;
};


class OSPMail_API MailDeliveryService: public Service
	/// The MailDeliveryService is used to send emails from OSP applications.
	///
	/// The default implementation of this service sets up a Poco::Net::SMTPClientSession,
	/// configures it from settings in the application configuration file,
	/// logs into the mail server and delivers a message.
{
public:
	using Ptr = Poco::AutoPtr<MailDeliveryService>;

	virtual MailDeliveryHandle::Ptr sendMessage(MailMessagePtr pMessage) = 0;
		/// Sends the given mail message to its recipient, using the configured SMTP server.
		///
		/// Note that depending on the implementation of the service, sending the email may
		/// be done asynchronously. The returned MailDeliveryHandle object can be used
		/// to monitor the progress of sending the mail message.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;
};


//
// inlines
//
inline MailMessagePtr MailDeliveryHandle::message() const
{
	return _pMessage;
}


inline MailDeliveryHandle::MailStatus MailDeliveryHandle::status() const
{
	return _status;
}


inline ExceptionPtr MailDeliveryHandle::exception() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _pException;
}


} // namespace Poco::OSP::Mail


#endif // OSP_Mail_MailDeliveryService_INCLUDED
