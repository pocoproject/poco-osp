//
// WebEventService.h
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventService
//
// Definition of the WebEventService interface.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_WebEvent_WebEventService_INCLUDED
#define OSP_WebEvent_WebEventService_INCLUDED


#include "Poco/OSP/WebEvent/WebEvent.h"
#include "Poco/OSP/Service.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/BasicEvent.h"
#include "Poco/AutoPtr.h"
#include <functional>
#include <utility>
#include <vector>


namespace Poco::OSP::WebEvent {


class OSPWebEvent_API WebEventService: public Poco::OSP::Service
	/// The WebEventService is a service that allows a web application
	/// server to notify clients running in a browser, using a RFC 6455
	/// WebSocket.
	///
	/// The way this service works is as follows:
	///
	/// JavaScript code running in the browser will open a WebSocket connection
	/// to the OSP web server (using path "/webevent") and send a single-frame
	/// text subscription message specifying the subjects to subscribe to. The format
	/// of the subscription message is:
	///
	///     SUBSCRIBE <subjectList> WebEvent/1.0
	///
	/// where <subjectList> is a comma-separated list of subject names not
	/// containing white space characters. A comma inside brackets ([], ()
	/// or {}) belongs to the subject name. An empty subject name makes the
	/// message invalid.
	///
	/// Subject names use a hierarchical, reverse DNS-style syntax, with the
	/// parts of the subject delimited by periods. Subject names must not
	/// contain spaces and their parts should be valid C++ identifiers.
	/// However, the latter restriction is not enforced.
	/// Subject names are hierarchical. A subscriber to
	/// a "parent" subject (e.g., "com.appinf.events") will also receive
	/// all "child" subjects (e.g., "com.appinf.events.someEvent" and
	/// "com.appinf.events.someOtherEvent").
	///
	/// A client can unsubscribe from specific events by either sending an
	/// unsubscribe request or by simply closing the WebSocket. The
	/// format of the UNSUBSCRIBE message is similar to the SUBSCRIBE message:
	///
	///     UNSUBSCRIBE <subjectList>|* WebEvent/1.0
	///
	/// where <subjectList> is a comma-separated list of subject names, as
	/// with the SUBSCRIBE message. Instead of a subject list, a single
	/// asterisk can be specified, which means unsubscribe from all
	/// subjects.
	///
	/// After the initial SUBSCRIBE message the client can send additional
	/// SUBSCRIBE or UNSUBSCRIBE messages, and thus dynamically change its
	/// subscriptions without having to re-establish the WebSocket.
	///
	/// Once a client has opened a WebSocket and sent the SUBSCRIBE
	/// message, the WebSocket connection will be "parked" on the server
	/// and only used when an event message needs to be sent to the
	/// client. This allows a very high number of simultaneous subscribers,
	/// since a single subscriber does not consume much server resources,
	/// except for the socket and some data overhead.
	/// Specifically, a specific thread for the WebSocket connection will only be
	/// used during the initial connection setup.
	///
	/// Notification messages sent to clients are single-frame text
	/// messages with the following format:
	///
	///     NOTIFY <subjectName> WebEvent/1.0\r\n
	///     <data>
	///
	/// where <subjectName> is the subjectName specified in the
	/// call to notify(), and <data> is the given data string, which
	/// typically should be a valid serialized JSON or XML document in UTF-8
	/// encoding, although any valid UTF-8 string can be sent.
	///
	/// If the osp.web.event.clientNotify configuration property is true, a
	/// client can also send notifications to other clients by sending a
	/// NOTIFY message to the server. Otherwise such messages are dropped.
	///
	/// A client can send a NOTIFY message using the special subject
	/// "system.ping" to the server, and the server will respond with a
	/// NOTIFY using the subject "system.pong", whatever the setting of
	/// osp.web.event.clientNotify. This can be used as a heartbeat
	/// mechanism to verify that the connection to the server is alive.
	/// Any data sent with the ping message will be sent back with the pong
	/// message.
	///
	/// The server also runs its own keepalive: connections that have not
	/// sent anything for a while receive a WebSocket PING control frame
	/// (answered automatically by browsers), and connections that stay
	/// silent past twice the ping interval are considered dead and removed.
	/// This reclaims connections whose peer disappeared without closing
	/// the socket (network drop, suspend, power loss).
	///
	/// The web server authenticates and authorizes the WebSocket upgrade
	/// request before the connection is established, as the bundle's web
	/// server extension declares it (the settings osp.web.event.permission
	/// and osp.web.event.session): by an Authorization header carrying HTTP
	/// Basic credentials or a bearer token, and, if osp.web.event.session
	/// names a web session, a browser by the cookie of that session once it
	/// has logged in to it. A request without valid
	/// credentials is answered with 401 Unauthorized, one of a user without
	/// the permission with 403 Forbidden. A connection that has been
	/// admitted may subscribe to every subject that the subject authorizer,
	/// if one is installed, allows (see setSubjectAuthorizer()).
	///
	/// An empty osp.web.event.permission turns the authentication off, for
	/// applications that authenticate their clients themselves: the web
	/// server admits every upgrade request, the connection has no user
	/// unless a user resolver gives it one, the Origin header is still
	/// checked, and a delegate of requestReceived decides. "**" is not a
	/// substitute: it refuses credentials that the web server cannot check.
{
public:
	using Ptr = Poco::AutoPtr<WebEventService>;
	using NotificationEvent = std::pair<std::string, std::string>;
	using UserResolver = std::function<std::string(const Poco::Net::HTTPServerRequest&)>;

	enum class SubjectAction
		/// What a client asks to do with a subject.
	{
		SUBSCRIBE, /// The client asks to be sent what is notified for the subject.
		NOTIFY     /// The client notifies the subject itself.
	};

	using SubjectAuthorizer = std::function<bool(const std::string& username, const std::string& subject, SubjectAction action)>;
		/// Says whether the user of a WebSocket connection (see
		/// setUserResolver(); empty for none) may subscribe to a subject, or
		/// notify it.

	struct SubscriberInfo
		/// Information about a connected WebSocket subscriber, captured
		/// at connection (upgrade) time.
	{
		std::string username;
			/// The user owning the connection, as determined at connection
			/// time (see setUserResolver()); empty for a connection that
			/// belongs to no user.

		std::string page;
			/// The URL (or path) of the document that opened the connection,
			/// taken from the upgrade request's "page" query parameter, or
			/// its Referer header as a fallback; may be empty. The value is
			/// supplied by the client and is not checked: escape it before
			/// showing it in a page.

		std::string clientAddress;
			/// The client's address (host:port) at connection time.
	};
	using SubscriberInfoVec = std::vector<SubscriberInfo>;

	struct InitialDataRequest
		/// Arguments of the initialDataRequested event.
	{
		std::string subject;
			/// The subject the socket has subscribed to.

		std::string data;
			/// The data to send to the subscribing socket, set by a delegate.

		bool hasData = false;
			/// Set by a delegate, together with data, to have data sent.
	};

	Poco::BasicEvent<const Poco::Net::HTTPServerRequest*> requestReceived;
		/// Fired when a HTTP WebSocket upgrade request has been received,
		/// after its Origin header has been checked. This can be used to
		/// restrict access further, or to reject the client's web socket
		/// connection for other reasons.
		/// If the delegate function sets the status code of the HTTP response
		/// (request.response()) to anything other than 200 (OK), the
		/// WebSocket connection will not be established.

	Poco::BasicEvent<const std::string> subscriptionRequestReceived;
		/// Fired when a subscription request for a new subject arrives.
		/// This event is fired once per subject subscription on a
		/// socket. To find out the number of clients subscribed to a subject,
		/// use the subscriberCount() call.
		///
		/// If a delegate throws, the exception is logged and the delegates
		/// registered after it are not called for this event.

	Poco::BasicEvent<InitialDataRequest> initialDataRequested;
		/// Fired right after subscriptionRequestReceived, for the same
		/// subject and socket. A delegate that sets hasData has data sent
		/// as a NOTIFY for the subject to the subscribing socket only, e.g.
		/// the current value of a subject that is otherwise only notified
		/// on change. Other subscribers of the subject receive nothing.
		/// The NOTIFY names the subscribed subject, whatever a delegate
		/// writes to the subject member.
		///
		/// A notification made afterwards reaches the socket after this
		/// data, and replaces it while it is still queued (see notify()).
		/// Server-side events (notificationSent, subjectNotified) are not
		/// fired.
		///
		/// If a delegate throws, the exception is logged, the delegates
		/// registered after it are not called, and the subscription stands.

	Poco::BasicEvent<const std::string> unsubscriptionRequestReceived;
		/// Fired when an unsubscription request for a subject arrives.
		/// This event is fired once per unsubscription on a socket.
		/// To find out the number of clients subscribed to a subject,
		/// use the subscriberCount() call.
		///
		/// If a delegate throws, the exception is logged and the delegates
		/// registered after it are not called for this event.

	Poco::BasicEvent<const NotificationEvent> notificationSent;
		/// Fired when a notification has been sent, either by calling
		/// notify(), or by a client sending a NOTIFY message to
		/// the server while client notifications are enabled.
		///
		/// This event is fired for all subjects. It's the
		/// responsibility of the delegate function to filter out
		/// ignore events not of interest.
		///
		/// If a delegate throws, the exception is logged and the delegates
		/// registered after it are not called for this notification. The
		/// notification is delivered to the subscribers all the same.

	virtual Poco::BasicEvent<const NotificationEvent>& subjectNotified(const std::string& subject) = 0;
		/// Fired when a notification has been sent for the given
		/// subject, either by calling notify(), or by a client sending a
		/// NOTIFY message to the server while client notifications are
		/// enabled.
		///
		/// If a delegate throws, the exception is logged and the delegates
		/// registered after it are not called for this notification. The
		/// notification is delivered to the subscribers all the same.

	virtual void notify(const std::string& subjectName, const std::string& data) = 0;
		/// Notify all registered subscribers to the given subject, using
		/// the given data, which is typically a serialized JSON or
		/// XML document in UTF-8 encoding.
		///
		/// Sending the notification is done asynchronously. If a notification cannot be
		/// delivered to a subscriber due to a network issue, the subscriber will be removed
		/// and its WebSocket closed.
		///
		/// Delivery to a subscriber that cannot keep up is lossy: while a frame for
		/// the given subject is still queued for a subscriber, a newer notification
		/// for the same subject replaces it (latest value wins), and a subscriber
		/// with overlapping subscriptions (e.g., to a subject and one of its
		/// ancestors) receives a single frame per notification. The number of
		/// queued frames per subscriber is bounded (see the
		/// osp.web.event.maxPendingFrames configuration property); when the limit
		/// is exceeded, the oldest queued frame is dropped. Server-side events
		/// (notificationSent, subjectNotified) are never affected and fire for
		/// every notification.

	virtual void notifyUser(const std::string& username, const std::string& subjectName, const std::string& data) = 0;
		/// Notify only the subscribers to the given subject whose WebSocket
		/// connection belongs to the given user, using the given data.
		///
		/// The user owning a WebSocket connection is determined at connection
		/// time (see setUserResolver()); connections that belong to no user
		/// are never notified by this method.
		///
		/// In contrast to notify(), server-side events (notificationSent,
		/// subjectNotified) are not fired.
		///
		/// Sending the notification is done asynchronously, as with notify(),
		/// with the same per-subscriber queueing and coalescing semantics.

	virtual void setUserResolver(UserResolver resolver) = 0;
		/// Installs the resolver used to determine the username owning a
		/// WebSocket connection from its upgrade request (e.g. by looking up
		/// the web session referenced by the request's cookies).
		///
		/// Without a resolver, a connection belongs to the user that the web
		/// server has authenticated for its upgrade request, with credentials
		/// or the cookie of a logged-in web session, and to no user if the
		/// request was not authenticated (an empty permission, or "**"
		/// without credentials) or was authenticated with a bearer token
		/// that carries a scope. A resolver can give such a connection a
		/// user; Poco::OSP::Auth::RequestIdentity::current() returns the
		/// authenticated user and scope while the resolver runs.
		///
		/// The resolver is invoked for every new WebSocket connection, before
		/// the connection is upgraded, and replaces the authenticated user. It
		/// should return an empty string for anonymous connections. Pass
		/// nullptr to remove the resolver (e.g. when the installing bundle
		/// stops).

	virtual void setSubjectAuthorizer(SubjectAuthorizer authorizer) = 0;
		/// Installs the authorizer that is asked whether a client may
		/// subscribe to a subject or notify it. Without one, every client
		/// that has a connection may do both for every subject.
		///
		/// The authorizer is asked for each subject a client names in a
		/// SUBSCRIBE message, and in a NOTIFY message while client
		/// notifications are enabled. A subject it refuses, or for which it
		/// throws, is left out of the subscription (or not notified), and the
		/// rest of the message is processed. Subscriptions made before the
		/// authorizer is installed are not checked again.
		///
		/// The authorizer runs on the worker threads that read the frames of
		/// the clients, possibly on several at once: it must be thread-safe,
		/// and it must not block, because a worker that waits holds up the
		/// frames of other clients.
		///
		/// Subjects have ancestors (see notify()). A notification of "a.b.c"
		/// reaches the subscribers of "a.b" and "a" as well, so for a NOTIFY
		/// the authorizer is asked about the subject and each of its
		/// ancestors, and the notification is dropped if it refuses any of
		/// them. For a SUBSCRIBE it is asked about the subject only, but a
		/// subscription to "a.b" is sent what is notified for "a.b.c" as
		/// well, so the authorizer must refuse an ancestor of what it
		/// protects to a user who may not see all that is below it.
		///
		/// What the server itself notifies (notify(), notifyUser()) is not
		/// asked about. Pass nullptr to remove the authorizer.

	[[nodiscard]] virtual int subscriberCount(const std::string& subject) = 0;
		/// Returns the number of subscribers to the subject.
		///
		/// When called from an unsubscriptionRequestReceived delegate, the
		/// unsubscribing socket is no longer counted.

	virtual SubscriberInfoVec listSubscribers() = 0;
		/// Returns a snapshot of all currently connected WebSocket
		/// subscribers, with the username, page (document URL) and client
		/// address captured at connection time.
		///
		/// The snapshot is taken on the service's event thread; the call
		/// blocks until it has been taken. If the service has been stopped,
		/// an empty list is returned. If the event thread does not answer
		/// in time, a Poco::TimeoutException is thrown rather than an empty
		/// list, which the caller could not tell from every subscriber
		/// having disconnected.
		/// Must not be called from a delegate of one of this service's
		/// events: the event thread would wait for itself until the
		/// timeout of five seconds and then throw the TimeoutException.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;

protected:
	WebEventService();
		/// Creates the WebEventService.

	~WebEventService() override;
		/// Destroys the WebEventService.
};


} // namespace Poco::OSP::WebEvent


#endif // OSP_WebEvent_WebEventService_INCLUDED
