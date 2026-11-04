//
// LibraryReference.h
//
// Library: OSP/Web
// Package: Web
// Module:  LibraryReference
//
// Definition of the LibraryReference class template.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_LibraryReference_INCLUDED
#define OSP_Web_LibraryReference_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/ClassLoader.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include <memory>
#include <string>


namespace Poco::OSP::Web {


template <class Base>
class LibraryReference
	/// Holds one reference to a shared library loaded with a Poco::ClassLoader.
	///
	/// The constructor loads the library, or counts another reference if
	/// the library is already loaded; the destructor releases the reference.
	/// The ClassLoader unloads the library when its last reference is released,
	/// so a library used by several bundles stays loaded until none uses it.
{
public:
	using Loader = Poco::ClassLoader<Base>;

	LibraryReference() = delete;

	LibraryReference(std::shared_ptr<Loader> pLoader, const std::string& path, const std::string& manifest);
		/// Loads the library with the given path and manifest name.
		/// Throws a Poco::LibraryLoadException if the library cannot be loaded.

	~LibraryReference();
		/// Releases the reference to the library.

	[[nodiscard]] std::unique_ptr<Base> create(const std::string& className) const;
		/// Creates an object of the class with the given name from the manifest
		/// of this library. Classes of different libraries must have different
		/// fully qualified names (see WebServerExtensionPoint).
		///
		/// Throws a Poco::NotFoundException if the library does not export the class.

	LibraryReference(const LibraryReference&) = delete;
	LibraryReference& operator = (const LibraryReference&) = delete;

private:
	std::shared_ptr<Loader> _pLoader;
	std::string _path;
};


//
// implementation
//
template <class Base>
LibraryReference<Base>::LibraryReference(std::shared_ptr<Loader> pLoader, const std::string& path, const std::string& manifest):
	_pLoader(std::move(pLoader)),
	_path(path)
{
	_pLoader->loadLibrary(_path, manifest);
}


template <class Base>
LibraryReference<Base>::~LibraryReference()
{
	try
	{
		_pLoader->unloadLibrary(_path);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


template <class Base>
std::unique_ptr<Base> LibraryReference<Base>::create(const std::string& className) const
{
	const auto& manifest = _pLoader->manifestFor(_path);
	const auto it = manifest.find(className);
	if (it == manifest.end()) throw Poco::NotFoundException(className);
	return std::unique_ptr<Base>(it->create());
}


} // namespace Poco::OSP::Web


#endif // OSP_Web_LibraryReference_INCLUDED
