//
// ServiceFactory.h
//
// Library: OSP
// Package: Service
// Module:  ServiceFactory
//
// Definition of the ServiceFactory class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_ServiceFactory_INCLUDED
#define OSP_ServiceFactory_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Service.h"


namespace Poco::OSP {


class OSP_API ServiceFactory: public Service
	/// A ServiceFactory is a special Service that, as its name
	/// implies, acts as a factory for Service objects.
	///
	/// If a subclass of ServiceFactory is registered as a
	/// service, the factory will be used to create a new
	/// instance of the service whenever the service is
	/// requested from the ServiceRegistry.
{
public:
	[[nodiscard]] virtual Service::Ptr createService() = 0;
		/// Creates and returns a new instance of a Service.
		
	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;
		
protected:
	ServiceFactory();
		/// Creates the ServiceFactory.

	~ServiceFactory() override;
		/// Destroys the ServiceFactory.
};


} // namespace Poco::OSP


#endif // OSP_ServiceFactory_INCLUDED
