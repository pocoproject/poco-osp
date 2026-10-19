//
// WebEventBundleActivator.cpp
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/WebEvent/WebEventRequestHandler.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/Environment.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::Properties;
using Poco::AutoPtr;


namespace Poco {
namespace OSP {
namespace WebEvent {


class WebEventRequestHandlerFactory: public WebRequestHandlerFactory
{
public:
	Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request)
	{
		return new WebEventRequestHandler(_pWebEventServiceImpl, context());
	}
	
protected:
	void initImpl()
	{
		ServiceRef::Ptr pWebEventServiceRef = context()->registry().findByName(WebEventService::SERVICE_NAME);
		if (pWebEventServiceRef)
		{
			 _pWebEventServiceImpl = pWebEventServiceRef->castedInstance<WebEventServiceImpl>();
		}
	}
	
private:
	WebEventServiceImpl::Ptr _pWebEventServiceImpl;
};


class WebEventBundleActivator: public BundleActivator
	/// The BundleActivator for the WebEvent Bundle.
	///
	/// Registers the WebEventService.
{
public:
	WebEventBundleActivator()
	{
	}
	
	~WebEventBundleActivator()
	{
	}
	
	void start(BundleContext::Ptr pContext)
	{
		Poco::OSP::PreferencesService::Ptr pPrefs = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);

		int workerCount = pPrefs->configuration()->getInt("osp.web.event.workers", 2*Poco::Environment::processorCount());
		int maxWebSockets = pPrefs->configuration()->getInt("osp.web.event.maxWebSockets", 0);
		int maxPendingFrames = pPrefs->configuration()->getInt("osp.web.event.maxPendingFrames", WebEventServiceImpl::DEFAULT_MAX_PENDING_FRAMES);
		int pingInterval = pPrefs->configuration()->getInt("osp.web.event.pingInterval", 60);

		_pWebEventService = new WebEventServiceImpl(pContext, maxWebSockets, workerCount, maxPendingFrames, pingInterval);
		_pWebEventServiceRef = pContext->registry().registerService(WebEventService::SERVICE_NAME, _pWebEventService, Properties());
	}
		
	void stop(BundleContext::Ptr pContext)
	{
		pContext->registry().unregisterService(_pWebEventServiceRef);
		_pWebEventServiceRef.reset();
		// stopped here, on the bundle's thread, rather than by whichever
		// thread drops the last reference later
		_pWebEventService->stop();
		_pWebEventService.reset();
	}
	
private:
	ServiceRef::Ptr _pWebEventServiceRef;
	WebEventServiceImpl::Ptr _pWebEventService;
};


} } } // namespace Poco::OSP::WebEvent


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::WebEvent::WebEventBundleActivator)
POCO_END_MANIFEST


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(Poco::OSP::WebEvent::WebEventRequestHandlerFactory)
POCO_END_MANIFEST
