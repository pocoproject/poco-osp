//
// Client.cpp
//
// The client application for the OSP Shell.
//
// Copyright (c) 2009-2018, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/IntValidator.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/FileStream.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/Ascii.h"
#include "Poco/NumberParser.h"
#include "Poco/Environment.h"
#include "Poco/Exception.h"
#include "Poco/TextIterator.h"
#include "Poco/Unicode.h"
#include "Poco/UTF8Encoding.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <cerrno>
#if defined(POCO_OS_FAMILY_WINDOWS)
#include <windows.h>
#elif defined(POCO_OS_FAMILY_UNIX)
#include <termios.h>
#endif
#if defined(OSPSH_HAVE_EDITLINE)
#include <histedit.h>
#include <cstdio>
#endif


using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::AbstractConfiguration;
using Poco::Util::OptionCallback;
using Poco::OSP::Shell::ShellConnection;
using namespace std::string_literals;


class ShellClientApp: public Application
{
public:
	ShellClientApp():
		_username(Poco::Environment::get("OSPSH_USERNAME"s, ""s)),
		_password(Poco::Environment::get("OSPSH_PASSWORD"s, ""s))
	{
	}

	~ShellClientApp() override
	{
		delete _pCommandStream;
	}

protected:
	static const Poco::UInt16 SERVER_PORT = 22023;
	static const std::string SERVER_HOST;
	static const std::string PROMPT;
	static constexpr int CONNECTION_CLOSED = 0;
		/// The status of a request that has no response, because the server has closed the connection.

	void defineOptions(OptionSet& options) override
	{
		Application::defineOptions(options);

		options.addOption(
			Option("help"s, "h"s, "Display help information on command line arguments."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleHelp)));

		options.addOption(
			Option("username"s, "u"s, "Specify the user name for logging in to the OSP Shell server."s)
				.required(false)
				.repeatable(false)
				.argument("<username>"s)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleUserName)));

		options.addOption(
			Option("password"s, "p"s, "Specify the password for logging in to the OSP Shell server."s)
				.required(false)
				.repeatable(false)
				.argument("<password>"s)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handlePassword)));

		options.addOption(
			Option("host"s, "H"s, "Specify the OSP Shell server hostname or IP address."s)
				.required(false)
				.repeatable(false)
				.argument("<host>"s)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleHost)));

		options.addOption(
			Option("port"s, "P"s, "Specify the OSP Shell server port number."s)
				.required(false)
				.repeatable(false)
				.argument("<port>"s)
				.validator(new Poco::Util::IntValidator(1, 65535))
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handlePort)));

		options.addOption(
			Option("file"s, "f"s, "Read commands from the given file."s)
				.required(false)
				.repeatable(false)
				.argument("<path>")
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleFile)));

		options.addOption(
			Option("noprompt"s, "n"s, "Do not display prompt and welcome message at startup."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleNoPrompt)));

		options.addOption(
			Option("noauth"s, "N"s, "Do not authenticate against the server."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleNoAuth)));

		options.addOption(
			Option("config-file"s, "C"s, "Read configuration properties from the given file."s)
				.required(false)
				.repeatable(true)
				.argument("file")
				.callback(OptionCallback<ShellClientApp>(this, &ShellClientApp::handleConfig)));
	}

	void handleHelp(const std::string& /*name*/, const std::string& /*value*/)
	{
		_showHelp = true;
		displayHelp();
		stopOptionsProcessing();
	}

	void handleUserName(const std::string& /*name*/, const std::string& value)
	{
		_username = value;
	}

	void handlePassword(const std::string& /*name*/, const std::string& value)
	{
		_password = value;
	}

	void handleHost(const std::string& /*name*/, const std::string& value)
	{
		_host = value;
	}

	void handlePort(const std::string& /*name*/, const std::string& value)
	{
		_port = static_cast<Poco::UInt16>(Poco::NumberParser::parse(value));
	}

	void handleFile(const std::string& /*name*/, const std::string& value)
	{
		_pCommandStream = new Poco::FileInputStream(value);
		_prompt = false;
	}

	void handleNoPrompt(const std::string& /*name*/, const std::string& /*value*/)
	{
		_prompt = false;
	}

	void handleNoAuth(const std::string& /*name*/, const std::string& /*value*/)
	{
		_auth = false;
	}

	void handleConfig(const std::string& /*name*/, const std::string& value)
	{
		loadConfiguration(value);
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		// Only the Unix option syntax has "--", which ends the options.
		helpFormatter.setUsage(
			helpFormatter.isUnixStyle() ? "[<option> ...] [-- <command>]"s : "[<option> ...] [<command>]"s);
		helpFormatter.setHeader(
			"\n"
			"The Open Service Platform Shell Client.\n"
			"Copyright (c) 2009-2026 by Applied Informatics Software Engineering GmbH.\n"
			"All rights reserved.\n\n"
			"This program allows you to interact with the OSP Shell service "
			"running within an OSP server application.\n\n"
			"The following command line options are supported:"s
		);
		helpFormatter.setFooter(
			"For more information, please see the Open Service Platform "
			"documentation at <https://www.appinf.com/docs>."s
		);
		helpFormatter.setIndent(8);
		helpFormatter.format(std::cout);
	}

	void writeResponse(const std::string& response, std::ostream& ostr)
	{
		std::string::const_iterator it(response.begin());
		std::string::const_iterator end(response.end());
		while (it != end)
		{
			while (it != end && std::isdigit(*it)) ++it;
			if (it != end && (*it == ' ' || *it == '-')) ++it;
			std::string::const_iterator lineBegin(it);
			while (it != end && *it != '\n') ++it;
			writeText(std::string(lineBegin, it), ostr);
			while (it != end && std::isspace(*it)) ++it;
			ostr << '\n';
		}
	}

	static void writeText(const std::string& text, std::ostream& ostr)
		/// Writes text from the server with each control character other than a tab,
		/// and each byte that is not valid UTF-8, replaced by '?', so that data from
		/// bundles and services cannot send escape sequences to the terminal.
	{
		constexpr int MAX_SEQUENCE_LENGTH = 4;
		Poco::UTF8Encoding utf8;
		Poco::TextIterator end(text);
		for (Poco::TextIterator it(text, utf8); it != end; ++it)
		{
			const int ch = *it;
			Poco::Unicode::CharacterProperties properties{};
			std::array<unsigned char, MAX_SEQUENCE_LENGTH> bytes{};
			int length = 0;
			if (ch >= 0)
			{
				Poco::Unicode::properties(ch, properties);
				length = utf8.convert(ch, bytes.data(), MAX_SEQUENCE_LENGTH);
			}
			if (length <= 0 || (properties.type == Poco::Unicode::UCP_CONTROL && ch != '\t'))
				ostr << '?';
			else
				std::copy_n(bytes.begin(), length, std::ostreambuf_iterator<char>(ostr));
		}
	}

	void writeResponse(int status, const std::string& response, std::ostream& ostr)
	{
		switch (status)
		{
		case ShellConnection::STATUS_CLIENT_ERROR:
		case ShellConnection::STATUS_CLIENT_USAGE:
			break;
		case ShellConnection::STATUS_CLIENT_DENIED:
			ostr << "Not authorized." << '\n';
			break;
		default:
			writeResponse(response, ostr);
			break;
		}
	}

	[[nodiscard]] int processCommand(const std::string& command, std::ostream& out, std::ostream& err)
	{
		std::vector<std::string> items = splitCommandLine(command);
		if (!items.empty() && items[0] == "login")
		{
			return login(items, out, err);
		}
		else if (!items.empty() && items[0] == "passwd")
		{
			return passwd(items, out, err);
		}
		else return sendRequest(command, out, err);
	}

	[[nodiscard]] int sendRequest(const std::string& request, std::ostream& out, std::ostream& err)
	{
		int status = CONNECTION_CLOSED;
		std::string response;
		try
		{
			_socket.sendMessage(request);
			status = receiveReply(response);
			while (status == ShellConnection::STATUS_INTERMEDIATE)
			{
				writeResponse(response, out);
				status = receiveReply(response);
			}
		}
		catch (Poco::Net::ConnectionResetException&)
		{
			// A request that arrives after the server has closed the connection is answered with a reset.
			status = CONNECTION_CLOSED;
		}
		catch (Poco::Net::ConnectionAbortedException&)
		{
			status = CONNECTION_CLOSED;
		}
		catch (const Poco::IOException& exc)
		{
			// A request sent after the reset fails with a broken pipe.
			if (exc.code() != EPIPE) throw;
			status = CONNECTION_CLOSED;
		}
		if (status == CONNECTION_CLOSED)
		{
			err << "The server has closed the connection.\n";
		}
		else if (status >= ShellConnection::STATUS_CLIENT_ERROR)
		{
			writeResponse(status, response, err);
		}
		return status;
	}

	[[nodiscard]] int receiveReply(std::string& reply)
		/// Receives a reply, stores its lines, separated by newline characters, in reply and returns
		/// its status code, or CONNECTION_CLOSED if the connection ends before the last line.
		/// Not DialogSocket::receiveStatusMessage(), which does not return when the connection ends after a '-' line.
	{
		reply.clear();
		std::string line;
		while (_socket.receiveMessage(line))
		{
			int status = 0;
			if (line.size() >= 3 && std::all_of(line.begin(), line.begin() + 3, Poco::Ascii::isDigit))
			{
				status = Poco::NumberParser::parse(line.substr(0, 3));
			}
			if (reply.empty() && status == 0)
			{
				throw Poco::ProtocolException("The server has sent a reply without a status code");
			}
			if (!reply.empty()) reply += '\n';
			reply += line;
			if (status != 0 && (line.size() == 3 || line[3] != '-')) return status;
		}
		return CONNECTION_CLOSED;
	}

	[[nodiscard]] int login(const std::vector<std::string>& items, std::ostream& out, std::ostream& err)
	{
		const std::optional<std::size_t> operand = firstOperand(items);
		if (!operand.has_value())
		{
			return sendRequest("login --help", out, err);
		}
		const std::size_t first = *operand;

		std::string username;
		if (items.size() > first)
		{
			username = items[first];
		}
		else
		{
			std::cout << "Username: " << std::flush;
			std::getline(std::cin, username);
		}

		std::string password;
		if (items.size() > first + 1)
		{
			password = items[first + 1];
		}
		else
		{
			std::cout << "Password: " << std::flush;
			echo(false);
			std::getline(std::cin, password);
			echo(true);
			std::cout << '\n';
		}

		int status = sendRequest(loginRequest(username, password), out, err);
		if (status == ShellConnection::STATUS_OK)
		{
			_username = username;
			_password = password;
		}
		return status;
	}

	[[nodiscard]] int passwd(const std::vector<std::string>& items, std::ostream& out, std::ostream& err)
	{
		const std::optional<std::size_t> operand = firstOperand(items);
		if (!operand.has_value())
		{
			return sendRequest("passwd --help", out, err);
		}
		const std::size_t first = *operand;

		std::string username;
		if (items.size() > first)
		{
			username = items[first];
		}
		else
		{
			username = _username;
		}

		std::string password;
		if (items.size() > first + 1)
		{
			password = items[first + 1];
		}
		else
		{
			std::string confirm;
			out << "New Password: " << std::flush;
			echo(false);
			std::getline(std::cin, password);
			out << '\n';

			if (password.empty())
			{
				err << "Password not changed." << '\n';
				return ShellConnection::STATUS_OK;
			}

			out << "Retype New Password: " << std::flush;
			std::getline(std::cin, confirm);
			echo(true);
			out << '\n';

			if (password != confirm)
			{
				err << "Passwords do not match." << '\n';
				return ShellConnection::STATUS_CLIENT_ERROR;
			}
		}

		// "--" ends the options, as in loginRequest().
		std::string command = "passwd -- \"";
		command += escape(username);
		command += "\" \"";
		command += escape(password);
		command += "\"";

		return sendRequest(command, out, err);
	}

	void promptLogin()
	{
		if (_username.empty())
		{
			std::cout << "Username: " << std::flush;
			std::getline(std::cin, _username);
		}
		if (_password.empty())
		{
			std::cout << "Password: " << std::flush;
			echo(false);
			std::getline(std::cin, _password);
			echo(true);
			std::cout << '\n';
		}
	}

	void echo(bool status)
	{
#if defined(POCO_OS_FAMILY_WINDOWS)
		HANDLE stdIn = GetStdHandle(STD_INPUT_HANDLE);
		DWORD mode;
		GetConsoleMode(stdIn, &mode);
		mode = status ? mode | ENABLE_ECHO_INPUT : mode & ~ENABLE_ECHO_INPUT;
		SetConsoleMode(stdIn, mode);
#elif defined(POCO_OS_FAMILY_UNIX)
		struct termios tio;
		tcgetattr(0, &tio);
		tio.c_lflag = status ? tio.c_lflag | ECHO : tio.c_lflag & ~(ECHO);
		tcsetattr(0, TCSANOW, &tio);
#endif
	}

#if defined(OSPSH_HAVE_EDITLINE)
	[[nodiscard]] static const char* prompt(EditLine*)
	{
		return PROMPT.c_str();
	}
#endif

	[[nodiscard]] static std::string escape(const std::string& str)
	{
		std::string result;
		for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
		{
			switch (*it)
			{
			case '"':
				result += "\\\"";
				break;
			case '\'':
				result += "\\'";
				break;
			case '\\':
				result += "\\\\";
				break;
			default:
				result += *it;
				break;
			}
		}
		return result;
	}

	[[nodiscard]] static std::optional<std::size_t> firstOperand(const std::vector<std::string>& items)
		/// Returns the index of the first operand of a login or passwd command, after a leading "--",
		/// or no value for a single -h or --help option, the only arguments for which the server shows the help.
	{
		if (items.size() == 2 && (items[1] == "-h" || items[1] == "--help")) return std::nullopt;
		return (items.size() > 1 && items[1] == "--") ? std::size_t{2} : std::size_t{1};
	}

	[[nodiscard]] static std::string loginRequest(const std::string& username, const std::string& password)
		/// Returns the login command for the given user name and password, after a "--",
		/// so that credentials that start with '-' are not taken as options.
	{
		return "login -- \"" + escape(username) + "\" \"" + escape(password) + "\"";
	}

	[[nodiscard]] static int exitCode(int status)
		/// Returns the exit code of ospsh for the status of the last request.
	{
		if (status == CONNECTION_CLOSED) return Application::EXIT_UNAVAILABLE;
		return status >= ShellConnection::STATUS_CLIENT_ERROR ? Application::EXIT_SOFTWARE : Application::EXIT_OK;
	}

	[[nodiscard]] static std::vector<std::string> splitCommandLine(const std::string& commandLine)
	{
		std::vector<std::string> items;

		std::string::const_iterator it  = commandLine.begin();
		std::string::const_iterator end = commandLine.end();

		std::string item;
		while (it != end)
		{
			switch (*it)
			{
			case '"':
				++it;
				while (it != end && *it != '"')
				{
					if (*it == '\\')
					{
						++it;
						if (it == end) break;
					}
					item += *it++;
				}
				if (it != end) ++it;
				break;
			case '\'':
				++it;
				while (it != end && *it != '\'')
				{
					if (*it == '\\')
					{
						++it;
						if (it == end) break;
					}
					item += *it++;
				}
				if (it != end) ++it;
				break;
			case '\\':
				++it;
				if (it != end) item += *it++;
				break;
			case ' ':
			case '\t':
				if (!item.empty())
				{
					items.push_back(item);
					item.clear();
				}
				++it;
				break;
			default:
				item += *it++;
				break;
			}
		}
		if (!item.empty())
			items.push_back(item);

		return items;
	}

	void loadDefaultConfig()
	{
		Poco::Path configPath(Poco::Path::home());
		configPath.setBaseName(commandName());
		configPath.setExtension("properties"s);
		Poco::File configFile(configPath.toString());
		if (configFile.exists())
		{
			loadConfiguration(configFile.path());
		}
	}

	int main(const std::vector<std::string>& args) override
	{
		if (!_showHelp)
		{
			loadDefaultConfig();

			if (_username.empty()) _username = config().getString("username"s, ""s);
			if (_password.empty()) _password = config().getString("password"s, ""s);
			if (_host.empty()) _host = config().getString("host"s, SERVER_HOST);
			if (!_port)
			{
				const int port = config().getInt("port"s, SERVER_PORT);
				if (port < 1 || port > std::numeric_limits<Poco::UInt16>::max())
				{
					std::cerr << "The port " << port << " is not between 1 and 65535.\n";
					return Application::EXIT_CONFIG;
				}
				_port = static_cast<Poco::UInt16>(port);
			}

			if (_prompt && _auth) promptLogin();

			if (!args.empty())
			{
				if (_pCommandStream == nullptr)
				{
					std::string command = args[0];
					for (std::size_t i = 1; i < args.size(); i++)
					{
						command += " \"";
						command += escape(args[i]);
						command += "\"";
					}
					command += "\n";
					_pCommandStream = new std::istringstream(command);
					_prompt = false;
				}
			}

			Poco::Net::SocketAddress sa(_host, _port);
			_socket.connect(sa);

			std::string greeting;
			if (receiveReply(greeting) == CONNECTION_CLOSED)
			{
				std::cerr << "The server has closed the connection.\n";
				return Application::EXIT_UNAVAILABLE;
			}
			if (_prompt)
			{
				std::cout << "\n";
				writeResponse(greeting, std::cout);
				std::cout << '\n';
			}

			int status(ShellConnection::STATUS_OK);
			if (_auth)
			{
				status = sendRequest(loginRequest(_username, _password), std::cout, std::cerr);
			}
			if (status == ShellConnection::STATUS_OK)
			{
				std::istream* pStream = _pCommandStream;
				if (pStream == nullptr) pStream = &std::cin;

#if defined(OSPSH_HAVE_EDITLINE)
				bool useEditLine = false;

				EditLine* editLine = nullptr;
				History* elHist = nullptr;
				HistEvent elHistEvent;
				if (_pCommandStream == nullptr)
				{
					editLine = el_init("ospsh", stdin, stdout, stderr);
					if (editLine == nullptr) throw Poco::IOException("Failed to initialize editline");
					el_set(editLine, EL_PROMPT, &ShellClientApp::prompt);
					el_set(editLine, EL_EDITOR, "emacs");
					el_set(editLine, EL_SIGNAL, 1);
					elHist = history_init();
					if (elHist != nullptr)
					{
						history(elHist, &elHistEvent, H_SETSIZE, 64);
						el_set(editLine, EL_HIST, history, elHist);
					}
					useEditLine = true;
				}
#else
				if (_prompt) std::cout << PROMPT << std::flush;
#endif

#if defined(OSPSH_HAVE_EDITLINE)
				if (useEditLine)
				{
					int count;
					const char* line;
					while ((line = el_gets(editLine, &count)) != nullptr)
					{
						std::string request(line);
						Poco::trimRightInPlace(request);
						// An empty line is not sent, so that the exit code is that of the last command.
						if (request.empty()) continue;
						status = processCommand(request, std::cout, std::cerr);
						if (status == ShellConnection::STATUS_GOODBYE || status == CONNECTION_CLOSED) break;
						if (elHist != nullptr) history(elHist, &elHistEvent, H_ENTER, line);
					}
					if (elHist != nullptr) history_end(elHist);
					el_end(editLine);
					return exitCode(status);
				}
#endif
				std::string request;
				while (std::getline(*pStream, request))
				{
					Poco::trimRightInPlace(request);
					if (!request.empty())
					{
						status = processCommand(request, std::cout, std::cerr);
						if (status == ShellConnection::STATUS_GOODBYE || status == CONNECTION_CLOSED) break;
					}
					if (_prompt) std::cout << PROMPT << std::flush;
				}
				return exitCode(status);
			}
			return status == CONNECTION_CLOSED ? Application::EXIT_UNAVAILABLE : Application::EXIT_NOPERM;
		}
		return Application::EXIT_OK;
	}

private:
	bool         _showHelp = false;
	std::string  _username;
	std::string  _password;
	std::string  _host;
	Poco::UInt16 _port = 0;
	bool         _prompt = true;
	bool         _auth = true;
	std::istream* _pCommandStream = nullptr;
	Poco::Net::DialogSocket _socket;
};


const std::string ShellClientApp::SERVER_HOST("localhost");
const std::string ShellClientApp::PROMPT("osp> ");


POCO_APP_MAIN(ShellClientApp)
