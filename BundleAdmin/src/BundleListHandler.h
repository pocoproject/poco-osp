//
// BundleListHandler.h
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleAdmin_BundleListHandler_INCLUDED
#define BundleAdmin_BundleListHandler_INCLUDED


#include "RequestHandler.h"


class BundleListHandler: public RequestHandler
	/// The handler for the bundle list.
{
public:
	BundleListHandler(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the BundleListHandler.

	~BundleListHandler() override;
		/// Destroys the BundleListHandler.
		
	void handle() override;
};


#endif // BundleAdmin_BundleListHandler_INCLUDED
