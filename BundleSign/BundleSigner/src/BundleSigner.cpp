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
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/OptionException.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/X509Certificate.h"
#include "Poco/FileStream.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include <iostream>
#include <memory>
#include <string>


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
		_digestType("SHA256"),
		_verify(false),
		_allowSHA1(false),
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
			Option("certificate", "c", "Specify the file containing the signer certificate (PEM format). "
				"With --verify, the certificate is trusted: the bundle must be signed with its key, "
				"or with a certificate issued by it.")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleCertificate)));

		options.addOption(
			Option("ca-file", "t", "Specify a file containing trusted certificates (PEM format) for --verify. "
				"The bundle must be signed with the key of one of them, or with a certificate issued by one of them.")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleCAFile)));

		options.addOption(
			Option("key", "k", "Specify the file containing the signer private key (PEM format).")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleKey)));

		options.addOption(
			Option("passphrase", "p", "Specify the passphrase for the private key. "
				"Other users of the system can see it in the process list; prefer --passphrase-file.")
				.required(false)
				.repeatable(false)
				.argument("<passphrase>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handlePassphrase)));

		options.addOption(
			Option("passphrase-file", "P", "Specify a file whose first line is the passphrase for the private key.")
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handlePassphraseFile)));

		options.addOption(
			Option("digest-type", "d", "Specify the digest type to use. Valid types are \"SHA1\", \"SHA224\", \"SHA256\", \"SHA384\" and \"SHA512\". Default is \"SHA256\".")
				.required(false)
				.repeatable(false)
				.argument("<type>")
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleDigestType)));

		options.addOption(
			Option("verify", "v", "Verify the integrity and the signer of a signed bundle. "
				"Requires --certificate or --ca-file. Exits with a non-zero status unless "
				"the bundle is signed by a trusted signer and has not been modified.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleVerify)));

		options.addOption(
			Option("allow-sha1", "", "With --verify, accept a signature that uses the SHA1 digest algorithm, "
				"which is refused by default.")
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleSignerApp>(this, &BundleSignerApp::handleAllowSHA1)));

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

	void handleCAFile(const std::string& name, const std::string& value)
	{
		_caFilePath = value;
	}

	void handleKey(const std::string& name, const std::string& value)
	{
		_keyPath = value;
	}

	void handlePassphrase(const std::string& name, const std::string& value)
	{
		_passphrase = value;
	}

	void handlePassphraseFile(const std::string& name, const std::string& value)
	{
		Poco::FileInputStream istr(value);
		std::getline(istr, _passphrase);
		if (!_passphrase.empty() && _passphrase.back() == '\r') _passphrase.pop_back();
	}

	void handleDigestType(const std::string& name, const std::string& value)
	{
		_digestType = value;
	}

	void handleVerify(const std::string& name, const std::string& value)
	{
		_verify = true;
	}

	void handleAllowSHA1(const std::string& name, const std::string& value)
	{
		_allowSHA1 = true;
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
			"This program adds a cryptographic signature "
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
		if (_pRSAKey == nullptr)
		{
			// The key is loaded once for all bundles, so the passphrase is not needed afterwards.
			try
			{
				_pRSAKey = std::make_unique<Poco::Crypto::RSAKey>("", _keyPath, _passphrase);
			}
			catch (...)
			{
				Poco::secureClear(_passphrase);
				throw;
			}
			Poco::secureClear(_passphrase);
		}
		const Poco::Crypto::RSAKey& rsaKey = *_pRSAKey;
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
		Poco::Crypto::X509Certificate::List trustedCertificates;
		if (!_certificatePath.empty())
		{
			trustedCertificates.push_back(Poco::Crypto::X509Certificate(_certificatePath));
		}
		if (!_caFilePath.empty())
		{
			Poco::Crypto::X509Certificate::List caCertificates = Poco::Crypto::X509Certificate::readPEM(_caFilePath);
			trustedCertificates.insert(trustedCertificates.end(), caCertificates.begin(), caCertificates.end());
		}
		if (trustedCertificates.empty())
		{
			std::cout << "Cannot verify bundle "
				<< path
				<< ": No trusted certificate specified (--certificate or --ca-file)."
				<< std::endl;
			_result = Application::EXIT_USAGE;
			return;
		}

		BundleVerifier::VerificationResult result = BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE;
		std::vector<std::string> messages;
		BundleVerifier verifier(path);
		verifier.setAllowSHA1(_allowSHA1);
		std::string alias(_alias);
		if (alias.empty())
		{
			try
			{
				alias = verifier.firstSigner();
			}
			catch (Poco::DataFormatException& exc)
			{
				std::cout << "Bundle verification failed for bundle "
					<< path << ":" << std::endl
					<< exc.message() << std::endl;
				_result = Application::EXIT_DATAERR;
				return;
			}
		}
		if (!alias.empty())
		{
			result = verifier.verify(alias, trustedCertificates, messages);
		}
		switch (result)
		{
		case BundleVerifier::BUNDLE_VERIFICATION_OK:
			{
				std::cout << "Bundle verification successful for bundle "
					<< path << " (using alias " << alias;
				BundleVerifier::CertificatePtr pCert = verifier.extractCertificate(alias);
				if (pCert != nullptr) std::cout << ", signed by " << pCert->subjectName();
				std::cout << ")." << std::endl;
			}
			break;
		case BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE:
			std::cout << "Bundle verification failed for bundle "
				<< path << " (not signed)."
				<< std::endl;
			_result = Application::EXIT_DATAERR;
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
	std::string  _caFilePath;
	std::string  _keyPath;
	std::string  _passphrase;
	std::unique_ptr<Poco::Crypto::RSAKey> _pRSAKey;
	std::string  _digestType;
	bool         _verify;
	bool         _allowSHA1;
	bool         _signers;
	int          _result;
};


POCO_APP_MAIN(BundleSignerApp)
