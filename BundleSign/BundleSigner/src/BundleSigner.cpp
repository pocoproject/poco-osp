//
// BundleSigner.cpp
//
// An command line tool for signing OSP bundles.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/IntValidator.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/OptionException.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/X509Certificate.h"
#include <iostream>


using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::AbstractConfiguration;
using Poco::Util::OptionCallback;
using Poco::OSP::BundleSign::BundleSigner;
using Poco::OSP::BundleSign::BundleVerifier;


class BundleSignerApp: public Application
{
public:
	BundleSignerApp():
		_showHelp(false),
		_digestType("SHA1"),
		_verify(false),
		_signers(false),
		_result(Application::EXIT_OK)
	{
	}

	~BundleSignerApp()
	{
	}

protected:
	void defineOptions(OptionSet& options)
	{
		Application::defineOptions(options);

		options.addOption(
			Option("help", "h", "Display help information on command line arguments.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleHelp)));

		options.addOption(
			Option("alias", "a", "Specify the alias name of the signer. Default is the common name from the certificate.")
				.required(false)
				.repeatable(false)
				.argument("<name>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleAlias)));

		options.addOption(
			Option("certificate", "c", "Specify the file containing the signer certificate (PEM format).")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleCertificate)));

		options.addOption(
			Option("key", "k", "Specify the file containing the signer private key (PEM format).")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleKey)));

		options.addOption(
			Option("passphrase", "p", "Specify the passphrase for the private key.")
				.required(false)
				.repeatable(false)
				.argument("<passphrase>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handlePassphrase)));

		options.addOption(
			Option("digest-type", "d", "Specify the digest type to use. Valid types are \"SHA1\", \"SHA224\", \"SHA256\", \"SHA384\" and \"SHA512\". Default is \"SHA1\".")
				.required(false)
				.repeatable(false)
				.argument("<type>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleDigestType)));

		options.addOption(
			Option("verify", "v", "Verify the integrity of a signed bundle.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleVerify)));

		options.addOption(
			Option("signers", "s", "Print all signers (aliases).")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleSigners)));
	}

	void handleHelp(const std::string& name, const std::string& value)
	{
		_showHelp = true;
		stopOptionsProcessing();
	}

	void handleAlias(const std::string& name, const std::string& value)
	{
		_alias = value;
	}

	void handleCertificate(const std::string& name, const std::string& value)
	{
		_certificatePath = value;
	}

	void handleKey(const std::string& name, const std::string& value)
	{
		_keyPath = value;
	}

	void handlePassphrase(const std::string& name, const std::string& value)
	{
		_passphrase = value;
	}

	void handleDigestType(const std::string& name, const std::string& value)
	{
		_digestType = value;
	}

	void handleVerify(const std::string& name, const std::string& value)
	{
		_verify = true;
	}

	void handleSigners(const std::string& name, const std::string& value)
	{
		_signers = true;
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("[<option> ...] <file> ...");
		helpFormatter.setHeader(
			"\n"
			"The Applied Informatics OSP Bundle Signing Utility.\n"
			"Copyright (c) 2009-2026 by Applied Informatics Software Engineering GmbH.\n"
			"All rights reserved.\n\n"
			"This program builds adds a cryptographic signature "
			"to a bundle, which can be used to verify the integrity "
			"of the bundle.\n\n"
			"The following command line options are supported:"
		);
		helpFormatter.setFooter(
			"For more information, please see the Open Service Platform "
			"documentation at <https://www.appinf.com/docs>."
		);
		helpFormatter.setIndent(8);
		helpFormatter.format(std::cout);
	}

	void sign(const std::string& path)
	{
		if (_keyPath.empty()) throw Poco::Util::MissingOptionException("key", "A private key is required for signing a bundle.");

		std::string alias(_alias);
		Poco::Crypto::RSAKey rsaKey("", _keyPath, _passphrase);
		if (_certificatePath.empty())
		{
			if (alias.empty())
			{
				Poco::Path p(_keyPath);
				alias = p.getBaseName();
			}
			BundleSigner signer(alias, rsaKey, _digestType);
			signer.signBundle(path);
		}
		else
		{
			Poco::Crypto::X509Certificate cert(_certificatePath);
			if (alias.empty())
			{
				alias = cert.commonName();
			}
			BundleSigner signer(alias, rsaKey, _digestType);
			signer.signBundle(path, cert);
		}
	}

	void signers(const std::string& path)
	{
		BundleVerifier verifier(path);
		std::vector<std::string> signers;
		verifier.listSigners(signers);
		for (std::vector<std::string>::const_iterator it = signers.begin(); it != signers.end(); ++it)
		{
			std::cout << path << ": " << *it << std::endl;
		}
	}


	void verify(const std::string& path)
	{
		BundleVerifier::VerificationResult result = BundleVerifier::BUNDLE_VERIFICATION_OK;
		std::vector<std::string> messages;
		std::string signer;
		BundleVerifier verifier(path);
		std::string alias(_alias);
		if (alias.empty()) alias = verifier.firstSigner();
		if (!alias.empty())
		{
			if (_certificatePath.empty())
			{
				BundleVerifier::CertificatePtr pCert = verifier.extractCertificate(alias);
				if (pCert)
				{
					signer = pCert->subjectName();
					result = verifier.verify(alias, *pCert, messages);
				}
				else
				{
					std::cout << "Cannot verify bundle "
						<< path
						<< ": No certificate found in bundle and no certificate file specified."
						<< std::endl;
					_result = Application::EXIT_USAGE;
					return;
				}
			}
			else
			{
				Poco::Crypto::X509Certificate cert(_certificatePath);
				signer = cert.subjectName();
				result = verifier.verify(alias, cert, messages);
			}
		}
		else
		{
			result = BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE;
		}
		switch (result)
		{
		case BundleVerifier::BUNDLE_VERIFICATION_OK:
			std::cout << "Bundle verification successful for bundle "
				<< path << " (using alias " << alias << ", signed by " << signer << ")."
				<< std::endl;
			break;
		case BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE:
			std::cout << "Bundle verification skipped for bundle "
				<< path << " (not signed)."
				<< std::endl;
			break;
		case BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE:
			std::cout << "Bundle verification failed for bundle "
				<< path << " (invalid signature for alias " << alias << ")."
				<< std::endl;
			_result = Application::EXIT_DATAERR;
			break;
		case BundleVerifier::BUNDLE_VERIFICATION_FAILED:
			std::cout << "Bundle verification failed for bundle "
				<< path << ":" << std::endl;
			_result = Application::EXIT_DATAERR;
			break;
		}
		for (std::vector<std::string>::const_iterator it = messages.begin(); it != messages.end(); ++it)
		{
			std::cout << *it << std::endl;
		}
	}

	int main(const std::vector<std::string>& args)
	{
		if (_showHelp || args.empty())
		{
			displayHelp();
		}
		else
		{
			for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it)
			{
				if (_signers)
				{
					signers(*it);
				}
				if (_verify)
				{
					verify(*it);
				}
				if (!_verify && !_signers)
				{
					sign(*it);
				}
			}
		}
		return _result;
	}

private:
	bool         _showHelp;
	std::string  _alias;
	std::string  _certificatePath;
	std::string  _keyPath;
	std::string  _passphrase;
	std::string  _digestType;
	bool         _verify;
	bool         _signers;
	int          _result;
};


POCO_APP_MAIN(BundleSignerApp)
