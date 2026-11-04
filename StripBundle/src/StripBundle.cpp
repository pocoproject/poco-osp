//
// StripBundle.cpp
//
// Copyright (c) 2018, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "ZipEntryAttributes.h"
#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipManipulator.h"
#include "Poco/Zip/ZipStream.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include <iostream>
#include <list>


using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::OptionCallback;


namespace Poco::OSP {


class StripBundle: public Application
{
public:
	StripBundle() = default;

protected:
	void initialize(Application& self) override
	{
		loadConfiguration(); // load default configuration files, if present
		Application::initialize(self);
	}

	void defineOptions(OptionSet& options) override
	{
		Application::defineOptions(options);

		options.addOption(
			Option("help", "h", "Display help information on command line arguments.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<StripBundle>(this, &StripBundle::handleHelp)));
	}

	void handleHelp(const std::string& /*name*/, const std::string& /*value*/)
	{
		_showHelp = true;
		stopOptionsProcessing();
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("[<option>] [<bundle>...]");
		helpFormatter.setHeader(
			"\n"
			"The Applied Informatics OSP Strip Bundle Utility.\n"
			"Copyright (c) 2018-2026 by Applied Informatics Software Engineering GmbH.\n"
			"All rights reserved.\n\n"
			"This program removes the bin directory, which contains the binaries "
			"(shared libraries), from a bundle. "
			"A typical use case is systems using a read-only file system, where "
			"the code cache is pre-populated with ccutil, and bundles do not "
			"need to contain any binaries to save space. "
			"The signature of a signed bundle is no longer valid afterwards.\n"
			"The following command line options are supported:"
		);
		helpFormatter.setFooter(
			"For more information, please see the Open Service Platform "
			"documentation at <https://www.appinf.com/docs>."
		);
		helpFormatter.setIndent(8);
		helpFormatter.format(std::cout);
	}

	void strip(const std::string& bundlePath)
	{
		Poco::File f(bundlePath);
		if (!f.exists())
			throw Poco::FileNotFoundException(bundlePath);
		if (f.isDirectory())
			stripDirectory(bundlePath);
		else
			stripFile(bundlePath);
	}

	void stripDirectory(const std::string& bundlePath)
	{
		Poco::Path p(bundlePath);
		p.makeDirectory();
		if (!Poco::File(Poco::Path(p, Poco::Path("META-INF/manifest.mf", Poco::Path::PATH_UNIX))).exists())
			throw Poco::DataFormatException("Not a bundle directory (no META-INF/manifest.mf)", bundlePath);
		p.pushDirectory("bin");
		Poco::File f(p.toString());
		if (f.exists() && f.isDirectory())
		{
			f.remove(true);
		}
	}

	void stripFile(const std::string& bundlePath)
	{
		Poco::Zip::ZipManipulator manip(bundlePath, false);
		const Poco::Zip::ZipArchive& archive = manip.originalArchive();
		if (archive.findHeader("META-INF/manifest.mf") == archive.headerEnd())
			throw Poco::DataFormatException("Not a bundle file (no META-INF/manifest.mf)", bundlePath);

		// ZipManipulator copies a kept entry raw, and Compress::addFileRaw() can write a local header of another length
		// for an entry with an extra field, so that its data no longer follows the header. Such a file is therefore
		// compressed again, and such a directory entry, which a bundle does not need, is removed.
		std::list<Poco::TemporaryFile> copies;
		Poco::FileInputStream bundleStream(bundlePath);
		for (auto it = archive.headerBegin(); it != archive.headerEnd(); ++it)
		{
			if (it->first.compare(0, 4, "bin/") == 0 || (it->second.isDirectory() && it->second.hasExtraField()))
			{
				manip.deleteFile(it->first);
			}
			else if (it->second.hasExtraField())
			{
				Poco::OSP::BundleSign::checkAttributesCanBeCopied(archive, it->first, bundlePath);
				Poco::Zip::ZipInputStream entryStream(bundleStream, it->second);
				Poco::FileOutputStream copyStream(copies.emplace_back().path());
				Poco::StreamCopier::copyStream(entryStream, copyStream);
				copyStream.close();
				if (copyStream.fail())
					throw Poco::WriteFileException(copies.back().path());
				// The stream reports a CRC mismatch and a decompression error only in its state.
				if (entryStream.bad() || !entryStream.crcValid())
					throw Poco::DataFormatException("Corrupt entry " + it->first + " in bundle file", bundlePath);
				// ZipManipulator::replaceFile() takes the modification time of the new entry from the file.
				Poco::File(copies.back().path()).setLastModified(it->second.lastModifiedAt().timestamp());
				manip.replaceFile(it->first, copies.back().path());
			}
		}
		bundleStream.close();
		manip.commit();
	}

	int main(const std::vector<std::string>& args) override
	{
		if (_showHelp || args.empty())
		{
			displayHelp();
			return Application::EXIT_OK;
		}

		try
		{
			for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it)
			{
				strip(*it);
			}
		}
		catch (Poco::FileNotFoundException& exc)
		{
			std::cerr << exc.displayText() << '\n';
			return Application::EXIT_NOINPUT;
		}
		catch (Poco::DataFormatException& exc)
		{
			std::cerr << exc.displayText() << '\n';
			return Application::EXIT_DATAERR;
		}
		catch (Poco::Exception& exc)
		{
			std::cerr << exc.displayText() << std::endl;
			return Application::EXIT_SOFTWARE;
		}

		return Application::EXIT_OK;
	}

private:
	bool _showHelp = false;
};


} // namespace Poco::OSP


POCO_APP_MAIN(Poco::OSP::StripBundle)
