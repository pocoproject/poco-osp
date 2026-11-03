//
// ServiceRef.h
//
// Library: OSP
// Package: Service
// Module:  ServiceRef
//
// Definition of the ServiceRef class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_ServiceRef_INCLUDED
#define OSP_ServiceRef_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/Properties.h"
#include "Poco/RefCountedObject.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"


namespace Poco::OSP {


class OSP_API ServiceRef: public Poco::RefCountedObject
	/// ServiceRef objects are used by the ServiceRegistry class
	/// to manage service registrations.
{
public:
	using Ptr = Poco::AutoPtr<ServiceRef>;
	using ConstPtr = const Ptr;

	ServiceRef(const std::string& name, const Properties& props, Service::Ptr pService);
		/// Creates the ServiceRef, using the given arguments.

	ServiceRef() = delete;
	ServiceRef(const ServiceRef&) = delete;
	ServiceRef& operator = (const ServiceRef&) = delete;

	[[nodiscard]] const std::string& name() const;
		/// Returns the name under which the service
		/// has been registered.

	[[nodiscard]] const Properties& properties() const;
		/// Returns the service properties.

	[[nodiscard]] Properties& properties();
		/// Returns the service properties.

	[[nodiscard]] Service::Ptr instance() const;
		/// Returns an instance of the Service.
		///
		/// If a ServiceFactory has been registered for the service,
		/// creates and returns a new instance of the Service class.
		/// Otherwise, returns the registered Service object.

	template <class Svc>
	[[nodiscard]] Poco::AutoPtr<Svc> castedInstance() const
		/// Returns an instance of the Service.
		///
		/// If a ServiceFactory has been registered for the service,
		/// creates and returns a new instance of the Service class.
		/// Otherwise, returns the registered Service object.
		///
		/// Throws a Poco::BadCastException if the service object
		/// cannot be casted to the desired type.
	{
		Service::Ptr pService = instance();
		if (pService->isA(typeid(Svc)))
		{
			// We'd like to use AutoPtr::unsafeCast<Svc>() here,
			// but older GCC versions don't like it.
			Svc* pCasted = static_cast<Svc*>(pService.get());
			return Poco::AutoPtr<Svc>(pCasted, true);
		}
		else
		{
			std::string msg("Cannot cast from ");
			msg += demangle(pService->type().name());
			msg += " to ";
			msg += demangle(typeid(Svc).name());
			throw Poco::BadCastException(msg);
		}
	}

	[[nodiscard]] static std::string demangle(const char* typeName);
		/// Demangles the given mangled type name, if
		/// demangling support is available in the compiler.
		///
		/// Note: On Windows/MSVC, the result of typeid().name
		/// already is a demangled type name so this method
		/// does nothing.

protected:
	~ServiceRef() override;
		/// Destroys the ServiceRef.

private:

	std::string  _name;
	Properties   _props;
	Service::Ptr _pService;
};


//
// inlines
//
inline const std::string& ServiceRef::name() const
{
	return _name;
}


inline const Properties& ServiceRef::properties() const
{
	return _props;
}


inline Properties& ServiceRef::properties()
{
	return _props;
}


} // namespace Poco::OSP


#endif // OSP_ServiceRef_INCLUDED
