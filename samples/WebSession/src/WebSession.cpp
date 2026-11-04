//
// WebSession.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/NetException.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DigestEngine.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Mutex.h"
#include "Poco/ClassLibrary.h"
#include <cstddef>
#include <map>


using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::HTMLForm;
using Poco::Net::htmlize;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::OSP::Web::WebSession;
using Poco::OSP::Web::WebSessionManager;
using Poco::OSP::ServiceRef;
using Poco::OSP::BundleContext;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;
using Poco::AnyCast;


namespace
{
	class SessionHandler: public HTTPRequestHandler
		/// Demonstrates the WebSessionManager. A GET request shows the entries of the session; a POST request with
		/// the CSRF token of the session sets or removes one. The entries carry a prefix, so that a client cannot
		/// change the attributes that the web server keeps in the session.
	{
	public:
		SessionHandler(WebSessionManager::Ptr pWebSessionManager, int timeout, BundleContext::Ptr pContext):
			_pWebSessionManager(pWebSessionManager),
			_timeout(timeout),
			_pContext(pContext)
		{
		}

		void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
		{
			WebSession::Ptr pSession = _pWebSessionManager->find("sample", request);
			bool haveSession = !pSession.isNull();
			if (!haveSession)
			{
				pSession = _pWebSessionManager->get("sample", request, _timeout, _pContext);
			}

			if (request.getMethod() == HTTPRequest::HTTP_POST)
			{
				HTMLForm form;
				try
				{
					form.load(request, request.stream());
				}
				catch (Poco::SyntaxException&)
				{
					sendError(response, HTTPResponse::HTTP_BAD_REQUEST,
						"The form contains a malformed percent escape.");
					return;
				}
				catch (Poco::Net::HTMLFormException&)
				{
					sendError(response, HTTPResponse::HTTP_BAD_REQUEST,
						"The form has too many fields, or a field that is too long.");
					return;
				}
				catch (Poco::Net::MessageException&)
				{
					sendError(response, HTTPResponse::HTTP_BAD_REQUEST, "The form is not a valid multipart message.");
					return;
				}
				catch (Poco::NotFoundException&)
				{
					// HTMLForm reports a multipart form without a boundary, or a part without a name, this way.
					sendError(response, HTTPResponse::HTTP_BAD_REQUEST, "The form is not a valid multipart message.");
					return;
				}
				if (!hasSessionToken(form, *pSession))
				{
					sendError(response, HTTPResponse::HTTP_FORBIDDEN,
						"The form does not contain the token of the session.");
					return;
				}
				if (!setEntry(*pSession, form.get("name", ""), form.get("value", "")))
				{
					sendError(response, HTTPResponse::HTTP_BAD_REQUEST,
						Poco::format("The entry is not stored: a name has 1 to %z characters, a value at most %z, "
							"and a session holds at most %z entries.", MAX_NAME_LENGTH, MAX_VALUE_LENGTH, MAX_ENTRIES));
					return;
				}
			}

			response.setContentType("text/html");
			response.setChunkedTransferEncoding(true);
			std::ostream& ostr = response.send();

			ostr <<
				"<HTML>\n"
				"<HEAD>\n"
				"<TITLE>WebSession Sample</TITLE>\n"
				"</HEAD>\n"
				"<BODY>\n"
				"<H1>WebSession Sample</H1>\n"
				"<FORM METHOD=\"POST\">\n"
				"<INPUT TYPE=\"hidden\" name=\"token\" value=\"" << htmlize(pSession->csrfToken()) << "\">\n"
				"<TABLE BORDER=\"0\" CELLSPACING=\"4\" CELLPADDING=\"4\">\n"
				"<TR>"
				"<TH>Name</TH><TH>Value (leave empty to remove)</TH>"
				"</TR>\n"
				"<TR>"
				"<TD><INPUT TYPE=\"text\" name=\"name\" size=\"31\"></TD>"
				"<TD><INPUT TYPE=\"text\" name=\"value\" size=\"31\"></TD>"
				"</TR>\n"
				"<TR>"
				"<TD></TD><TD><INPUT TYPE=\"submit\" VALUE=\"Set\"></TD>"
				"</TR>\n"
				"</TABLE>\n"
				"</FORM>\n";

			if (!haveSession)
			{
				ostr << "<P>A new session has been created.</P>";
			}
			ostr <<
				"<H2>Session:</H2>"
				"<UL>"
				"<LI>Created: "
				<< DateTimeFormatter::format(pSession->created(), DateTimeFormat::HTTP_FORMAT) << "</LI>"
				"<LI>Expires: "
				<< DateTimeFormatter::format(pSession->getExpiration(), DateTimeFormat::HTTP_FORMAT) << "</LI>";
			for (const auto& entry: entries(*pSession))
			{
				ostr << "<LI>" << htmlize(entry.first) << ": " << htmlize(entry.second) << "</LI>";
			}
			ostr << "</UL></BODY></HTML>";
		}

	protected:
		[[nodiscard]] static bool hasSessionToken(const HTMLForm& form, const WebSession& session)
			/// Returns true if the form contains the CSRF token of the session.
		{
			const std::string token = form.get("token", "");
			const std::string sessionToken = session.csrfToken();
			return !sessionToken.empty() && Poco::DigestEngine::constantTimeEquals(
				Poco::DigestEngine::Digest(token.begin(), token.end()),
				Poco::DigestEngine::Digest(sessionToken.begin(), sessionToken.end()));
		}

		[[nodiscard]] static bool setEntry(WebSession& session, const std::string& name, const std::string& value)
			/// Sets the entry, or removes it if value is empty.
			/// Returns false if the limits do not allow the entry.
		{
			if (name.empty() || name.size() > MAX_NAME_LENGTH || value.size() > MAX_VALUE_LENGTH)
			{
				return false;
			}

			const std::string key(ENTRY_PREFIX + name);
			Poco::FastMutex::ScopedLock lock(_entriesMutex);
			if (value.empty())
			{
				session.erase(key);
			}
			else
			{
				if (!session.has(key) && entries(session).size() >= MAX_ENTRIES)
				{
					return false;
				}
				session.set(key, value);
			}
			return true;
		}

		[[nodiscard]] static std::map<std::string, std::string> entries(const WebSession& session)
			/// Returns the entries of the session, without the prefix.
		{
			std::map<std::string, std::string> result;
			// A copy, as other requests of the session can change the attributes.
			for (const auto& attribute: session.attributes())
			{
				if (attribute.first.compare(0, ENTRY_PREFIX.size(), ENTRY_PREFIX) == 0)
				{
					result[attribute.first.substr(ENTRY_PREFIX.size())] = AnyCast<std::string>(attribute.second);
				}
			}
			return result;
		}

		static void sendError(HTTPServerResponse& response, HTTPResponse::HTTPStatus status, const std::string& message)
			/// Sends the message as a plain text response with the given status.
		{
			response.setStatusAndReason(status);
			response.setContentType("text/plain");
			response.sendBuffer(message.data(), message.size());
		}

		static const std::string ENTRY_PREFIX;
		static constexpr std::size_t MAX_ENTRIES = 32;
		static constexpr std::size_t MAX_NAME_LENGTH = 64;
		static constexpr std::size_t MAX_VALUE_LENGTH = 1024;

	private:
		static Poco::FastMutex _entriesMutex;
			/// Serializes setEntry(): another request of the session must not add an entry between the check
			/// of the limit and the set.

		WebSessionManager::Ptr _pWebSessionManager;
		int _timeout;
		BundleContext::Ptr _pContext;
	};


	const std::string SessionHandler::ENTRY_PREFIX("sample.");
	Poco::FastMutex SessionHandler::_entriesMutex;


	class SessionHandlerFactory: public WebRequestHandlerFactory
	{
	public:
		SessionHandlerFactory() = default;

		[[nodiscard]] HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& /*request*/) override
		{
			if (_pWebSessionManager == nullptr)
			{
				return nullptr;
			}
			return new SessionHandler(_pWebSessionManager, _timeout, context());
		}

	protected:
		void initImpl() override
		{
			_timeout = context()->thisBundle()->properties().getInt("timeout", 30);
			ServiceRef::Ptr pWebSessionManagerRef = context()->registry().findByName(WebSessionManager::SERVICE_NAME);
			if (pWebSessionManagerRef != nullptr)
			{
				 _pWebSessionManager = pWebSessionManagerRef->castedInstance<WebSessionManager>();
			}
			else
			{
				context()->logger().error(
					"The WebSessionManager service is not available. The WebSession sample page is not served.");
			}
		}

	private:
		WebSessionManager::Ptr _pWebSessionManager;
		int _timeout = 0;
	};
}


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(SessionHandlerFactory)
POCO_END_MANIFEST
