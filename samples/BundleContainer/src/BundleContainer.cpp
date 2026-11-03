//
// BundleContainer.cpp
//
// This sample demonstrates the OSP framework.
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/SharedLibrary.h"
#include <iostream>


using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::OptionCallback;
using Poco::OSP::OSPSubsystem;
using Poco::OSP::ServiceRegistry;


namespace
{
	class BundleContainerApplication: public Application
	{
	public:
		BundleContainerApplication():
			_pOSP(new OSPSubsystem)
		{
			addSubsystem(_pOSP);
		}

		[[nodiscard]] ServiceRegistry& serviceRegistry()
		{
			return _pOSP->serviceRegistry();
		}

	protected:
		void initialize(Application& self) override
		{
			// Windows: DLLs that bundles load also search the directory of the executable.
			Poco::SharedLibrary::setSearchPath(config().getString("application.dir"));

			loadConfiguration();
			Application::initialize(self);
		}

		void defineOptions(OptionSet& options) override
		{
			Application::defineOptions(options);

			options.addOption(
				Option("help", "h", "display help information on command line arguments")
					.required(false)
					.repeatable(false)
					.callback(OptionCallback<BundleContainerApplication>(this,
						&BundleContainerApplication::handleHelp)));

			options.addOption(
				Option("config-file", "f", "load configuration data from a file")
					.required(false)
					.repeatable(true)
					.argument("file")
					.callback(OptionCallback<BundleContainerApplication>(this,
						&BundleContainerApplication::handleConfig)));
		}

		void handleHelp(const std::string& /*name*/, const std::string& /*value*/)
		{
			displayHelp();
			stopOptionsProcessing();
			_pOSP->cancelInit();
		}

		void handleConfig(const std::string& /*name*/, const std::string& value)
		{
			// Above the default configuration file that initialize() loads.
			loadConfiguration(value, PRIO_APPLICATION);
		}

		void displayHelp()
		{
			HelpFormatter helpFormatter(options());
			helpFormatter.setCommand(commandName());
			helpFormatter.setUsage("OPTIONS");
			helpFormatter.setHeader("Starts the Open Service Platform bundles, stops them again and exits.");
			helpFormatter.format(std::cout);
		}

		int main(const std::vector<std::string>& /*args*/) override
		{
			return Application::EXIT_OK;
		}

	private:
		OSPSubsystem* _pOSP;
	};
}


POCO_APP_MAIN(BundleContainerApplication)
