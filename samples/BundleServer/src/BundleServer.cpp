//
// BundleServer.cpp
//
// This sample demonstrates the OSP framework.
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/SharedLibrary.h"
#include "Poco/ErrorHandler.h"
#include "Poco/ThreadPool.h"
#include <iostream>


using Poco::Util::ServerApplication;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::OptionCallback;
using Poco::OSP::OSPSubsystem;
using Poco::OSP::ServiceRegistry;


namespace
{
	class BundleServerApplication: public ServerApplication
	{
	public:
		BundleServerApplication():
			_errorHandler(*this),
			_pPreviousErrorHandler(Poco::ErrorHandler::set(&_errorHandler)),
			_pOSP(new OSPSubsystem)
		{
			addSubsystem(_pOSP);
		}

		~BundleServerApplication() override
		{
			// Pool threads can still run code of the application.
			Poco::ThreadPool::defaultPool().joinAll();
			// _errorHandler is destroyed with this object.
			Poco::ErrorHandler::set(_pPreviousErrorHandler);
		}

		[[nodiscard]] ServiceRegistry& serviceRegistry()
		{
			return _pOSP->serviceRegistry();
		}

	protected:
		class ErrorHandler: public Poco::ErrorHandler
		{
		public:
			ErrorHandler(BundleServerApplication& app):
				_app(app)
			{
			}

			void exception(const Poco::Exception& exc) override
			{
				log(exc.displayText());
			}

			void exception(const std::exception& exc) override
			{
				log(exc.what());
			}

			void exception() override
			{
				log("unknown exception");
			}

			void log(const std::string& message)
			{
				_app.logger().error("A thread was terminated by an unhandled exception: " + message);
			}

		private:
			BundleServerApplication& _app;
		};

		void initialize(Application& self) override
		{
			// Windows: DLLs that bundles load also search the directory of the executable.
			Poco::SharedLibrary::setSearchPath(config().getString("application.dir"));

			loadConfiguration();
			Application::initialize(self);
		}

		void defineOptions(OptionSet& options) override
		{
			ServerApplication::defineOptions(options);

			options.addOption(
				Option("help", "h", "display help information on command line arguments")
					.required(false)
					.repeatable(false)
					.callback(OptionCallback<BundleServerApplication>(this, &BundleServerApplication::handleHelp)));

			options.addOption(
				Option("config-file", "c", "load configuration data from a file")
					.required(false)
					.repeatable(true)
					.argument("file")
					.callback(OptionCallback<BundleServerApplication>(this, &BundleServerApplication::handleConfig)));
		}

		void handleHelp(const std::string& /*name*/, const std::string& /*value*/)
		{
			_showHelp = true;
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
			helpFormatter.setHeader("A container for Open Service Platform bundles.");
			helpFormatter.format(std::cout);
		}

		int main(const std::vector<std::string>& /*args*/) override
		{
			if (!_showHelp)
			{
				waitForTerminationRequest();
			}
			return Application::EXIT_OK;
		}

	private:
		ErrorHandler _errorHandler;
		Poco::ErrorHandler* _pPreviousErrorHandler;
		OSPSubsystem* _pOSP;
		bool _showHelp = false;
	};
}


POCO_SERVER_MAIN(BundleServerApplication)
