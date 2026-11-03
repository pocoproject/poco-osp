//
// MailDeliveryService.cpp
//
// Library: OSP/Mail
// Package: Mail
// Module:  MailDeliveryService
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/Mutex.h"


namespace Poco::OSP::Mail {


//
// MailDeliveryHandle
//


MailDeliveryHandle::MailDeliveryHandle(MailMessagePtr pMessage):
	_pMessage(pMessage),
	_status(MAIL_QUEUED)
{
}

	
MailDeliveryHandle::~MailDeliveryHandle() = default;

	
void MailDeliveryHandle::wait()
{
	_event.wait();
}


void MailDeliveryHandle::wait(long milliseconds)
{
	_event.wait(milliseconds);
}

	
bool MailDeliveryHandle::tryWait(long milliseconds)
{
	return _event.tryWait(milliseconds);
}


void MailDeliveryHandle::updateStatus(MailStatus status)
{
	_status = status;
	switch (status)
	{
	case MAIL_DELIVERING:
		try
		{
			Ptr pThis(this, true);
			messageDelivering(pThis);
		}
		catch (...)
		{
		}
		break;
	case MAIL_DELIVERED:
		_event.set();
		try
		{
			Ptr pThis(this, true);
			messageDelivered(pThis);
		}
		catch (...)
		{
		}
		break;
	case MAIL_FAILED:
		_event.set();
		try
		{
			Ptr pThis(this, true);
			messageFailed(pThis);
		}
		catch (...)
		{
		}
		break;
	default:
		break;
	}
}


void MailDeliveryHandle::updateStatus(const Poco::Exception& exc)
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		_pException = exc.clone();
	}
	updateStatus(MAIL_FAILED);
}


//
// MailDeliveryService
//


const std::string MailDeliveryService::SERVICE_NAME("com.appinf.osp.mail");


const std::type_info& MailDeliveryService::type() const
{
	return typeid(MailDeliveryService);
}


bool MailDeliveryService::isA(const std::type_info& otherType) const
{
	std::string name(typeid(MailDeliveryService).name());
	return name == otherType.name() || Poco::OSP::Service::isA(otherType);
}


} // namespace Poco::OSP::Mail
