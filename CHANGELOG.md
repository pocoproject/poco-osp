# Changelog

## 2.0.0 (unreleased)

The first release of POCO OSP as a repository of its own. It contains the Open Service Platform of macchina.io; the
entries below describe the changes that a user of the macchina.io versions must know. The macchina.io versions differ
from each other, so an entry can describe a difference to only some of them. The release also contains bug fixes and
other changes that are not listed here. The release notes (`doc/99100-OSPReleaseNotes.page`) have the same entries.

### Breaking changes

- Build: CMake is the only build system; there are no GNU make files.
- Build: POCO OSP requires a C++20 compiler; the configure stops without one.
- Build: POCO is not part of the source tree. The configure pulls the newest POCO 2.x release (`main` while none exists)
  or the ref that `POCO_OSP_POCO_VERSION` names, or uses the POCO of a superproject; an installed POCO is refused.
- Build: a superproject adds POCO OSP with `add_subdirectory()`; `PocoProOspSetup.cmake` is removed, and its option
  `ENABLE_REMOTING` is `ENABLE_OSP_REMOTING`.
- Build: the `ENABLE_OSP_*` options select the OSP parts, also the bundles of POCO libraries, and the configure switches
  on the POCO options they need; an OSP option that needs another OSP option that is OFF stops the configure.
- Build: Auth LDAP, Shell SSH, Data ODBC, MySQL and PostgreSQL, ActiveRecord, the Unit test runner and `ospsh` are OFF
  by default; only `ENABLE_OSP_AUTH_LDAP` enables LDAP, and it stops the configure on macOS.
- Build: `POCO_OSP_ENABLE_TESTS` and `POCO_OSP_ENABLE_SAMPLES` build the OSP tests and samples, not `ENABLE_TESTS` and
  `ENABLE_SAMPLES`; both are OFF by default and follow these when a superproject adds POCO.
- Build: the OSP shared libraries take their version from `libversion` (`200`), the CMake packages from `VERSION`
  (`2.0.0`).
- Build: bundles are installed into `<prefix>/bundles` (`POCO_INSTALL_BUNDLEDIR`), not `<prefix>/lib/bundles`; in a
  Debug build the tools have the suffix `d` (`bundled`, `signbundled`, `stripbundled`, `ccutild`, `ospshd`).
- Build: with `BUILD_SHARED_LIBS=OFF`, the configure switches off the bundles and the tests.
- Build: the bundle creator is the package component `OSPBundleCreator` in `<libdir>/cmake/Poco` (`cmake` on Windows),
  where `PocoOspBundle.cmake` is also installed; `BundleCreatorTargets.cmake` is removed.
- Build: the OSP libraries pass on only the libraries that their public headers use, for example no longer Zip, JSON,
  SSH or the LDAP library, and `Poco::OSPWebEvent` no longer `Poco::OSPWeb`; a target that uses them links them itself.
- Build: `poco_osp_bundle()` stops the configure for an unknown argument and for a `$<TARGET_FILE:...>` in `CODE` whose
  target is defined after the call.
- Build: `poco_remote_gen()` takes `GEN_DIR_NAME` relative to `CMAKE_CURRENT_BINARY_DIR`.
- Build: `poco_remote_gen()` runs RemoteGenNG in a directory of the build, so a configuration names its headers with
  `${CMAKE_CURRENT_SOURCE_DIR}` or `${POCO_OSP_BASE}`, not `${POCOPLUS_BASE}`.
- OSP: `Bundle::moduleProviders()`, `Bundle::resolvedDependencies()` and `ServiceRegistry::services()` return a copy
  instead of a reference.
- OSP: `ServiceImpl` does not add a `_1`, `_2` suffix to the default service name of further instances; instances
  registered at the same time need different names.
- Auth: `AuthAdminService` and `AbstractLDAPAuthAdminService` declare further pure virtual functions;
  `AuthAdminService::attributesForUser()` with a map returns `int`, and `escapeLDAP()` moved to `LDAPClient`.
- Auth: in the `ACCOUNT` mode with the `USER` bind mode, a login is refused when the LDAP entry found for the name does
  not have that name as its `sAMAccountName`, `DOMAIN\name` or `userPrincipalName`.
- Auth/Data: for subclasses of `AuthAdminServiceImpl`, `addUserImpl()` takes the password hash, and `authenticateDB()`
  is replaced by `checkPassword()`.
- Auth/Data: `setUserAttribute()` and `removeUserAttribute()` refuse attributes whose name starts with `auth_`;
  `setAuthAttribute()` and `removeAuthAttribute()` change them.
- Auth/Data: `addUser()` and `changePassword()` refuse an empty password, and `auth add user` requires the password.
- Auth/Data: the SQL user database needs the tables `auth_role_attributes` and `auth_role_mappings` of `createdb.sql`;
  the default `auth.sql.effectivePermissionsForUser` reads `auth_role_mappings` and takes the user name three times.
- Auth/Data: the setting `auth.sql.verifyPassword` is not used; the service checks the password against the stored hash.
- Auth/Data: when the LDAP server cannot be used, the stored password of an LDAP user is accepted only with
  `auth.ldap.inaccessible.fallbackToLocal` and within `auth.ldap.cache.lifetimeDays` of the last LDAP login.
- Auth/Data: an LDAP user is stored under the `sAMAccountName` of its LDAP entry, in lower case, when the entry has one;
  `authenticate()` and `verifyCredentials()` return false for another name, such as the user principal name.
- Auth/TwoFactor: after `auth.twofactor.maxFailures` wrong codes (default 5; 0 disables the lock-out), the user is
  locked out for `auth.twofactor.lockOutPeriod` seconds (default 600), also a user of `auth.neverLockOutUsers`.
- Auth/TwoFactor: the secrets are stored with AES-256-GCM. A secret in the old format is stored again in the new format
  after the first accepted code; macchina.io versions cannot read the new format.
- Auth/TwoFactor: the key is derived from the passphrase and a key salt (`auth.twofactor.keySalt`, otherwise generated
  at the first start and kept in the bundle preferences); instances that share one SQL user database need the same salt.
- Auth/Remoting: `Authenticator` and `Authorizer` take the identity from `RequestIdentity::current()` and ignore the
  `X-OSP-Authorized-*` headers.
- Auth/AuthCommand, BundleAdmin, Shell: the `auth` command, and `passwd` for another user, check `authAdmin`;
  BundleAdmin and the bundle commands of the shell check `bundleAdmin`; `loglevel` checks `logAdmin`.
- Web: `WebServerDispatcher::addFilter()` takes the bundle of the filter factory, `WebSessionStore::saveTimeout()` is
  pure virtual, and some functions of `MediaTypeMapper` and `WebSession` return a copy instead of a reference.
- Web: a `permission` with several comma-separated permissions is granted to a user who has any one of them.
- Web: valid credentials, or the session of a logged-in user, without the permission are answered with 403 instead of
  401 where no redirect applies.
- Web: on a path with a request handler, a failed authorization is answered with 401 or 403 and a JSON body; only a GET
  request for a path without a request handler is redirected to `redirectOn401`.
- Web: on a path that redirects, a GET request in the session of a logged-in user without the permission is redirected
  to `/` instead of the login page; at `/` itself it is answered with 403.
- Web: the redirect to `redirectOn401` adds the query parameter `redirect` with only the path and query of the request,
  unless the request is for the login page or already has a `redirect` parameter.
- Web: a path without a `session` attribute that accepts bearer tokens but not HTTP Basic credentials answers a failed
  authentication with 401 and a Bearer challenge instead of a Basic challenge.
- Web: a path that requires a permission is answered with 503 while no authentication service is available (bundle
  message `message.unavailable`; `message.nohandler` is removed).
- Web: with CORS enabled, an empty `osp.web.cors.allowedOrigin` or `allowOrigin` allows no other origin instead of every
  origin with credentials, and `allowOrigin` `*` is sent without `Access-Control-Allow-Credentials`.
- Web: with CORS enabled, a request from an origin that is not allowed is answered with 403 instead of 405 for a
  preflight or a method other than GET and HEAD, and without CORS headers for GET and HEAD.
- WebEvent: `WebEventService` declares further pure virtual functions; the constructors of `WebEventServiceImpl` and
  `WebEventRequestHandler` and the protected functions of `WebEventServiceImpl` are changed.
- WebEvent: a WebSocket upgrade whose `Origin` is neither the server's own nor listed in `osp.web.event.allowedOrigins`
  is answered with 403.
- WebEvent: a `NOTIFY` message from a client is passed on only with `osp.web.event.clientNotify` (default false).
- WebEvent: a subscriber is removed when it subscribes to more than `osp.web.event.maxSubjects` subjects (default 64) or
  sends nothing, not even a PONG, for twice `osp.web.event.pingInterval` (default 60 s).
- WebEvent: delivery to a subscriber that falls behind is lossy: beyond `osp.web.event.maxPendingFrames` queued frames,
  the oldest notification frame is dropped.
- Shell: `CommandProcessor::COMMAND_LOGIN`, `COMMAND_HELP` and `COMMAND_QUIT` are removed; the `ShellConnection` and
  `ShellConnectionFactory` constructors take a `ShellConnection::Config` and a `SessionTracker`.
- Shell: before a login, only `login`, `help` and `quit` run; every other command, also one registered without a
  permission, is denied.
- Shell: when the authentication service that `authServiceName` names (default `osp.auth`) is not registered, a password
  login is rejected; an empty `authServiceName` accepts every login.
- Shell: a session is closed when no command arrives within `sessionTimeout` seconds (default 600; 0 disables it) or
  when a command line is longer than 4096 characters.
- Shell: a rejected login is answered after a delay that grows per client address from 1 s to 16 s, three rejected
  logins close the connection, and `maxConnections` defaults to 4 instead of 1.
- Shell: the bundle `com.appinf.osp.shell` has the version 2.0.0, so a bundle that requires it with a range that ends at
  2.0.0, such as `[1.1.0,2.0.0)`, does not resolve.
- BundleAdmin: a request that changes a bundle must be a POST request whose `Origin` or `Referer` header names the
  server, and in a web session it must carry the CSRF token of the session (otherwise 403).
- BundleSign: `BundleSigner` and `signbundle` sign with SHA256 by default; `BundleVerifier` refuses a SHA1 signature
  unless `setAllowSHA1()` allows it (`signbundle --allow-sha1`).
- BundleSign: `BundleVerifier` accepts only a signer certificate that is valid at the time of the check and allows code
  signing.
- BundleSign: `signbundle --verify` needs `--certificate` or `--ca-file` and does not trust the certificate in the
  bundle.
- Unit: the test runner runs the tests that a bundle registers under its symbolic name, not under its name;
  `Poco/OSP/Unit/TestCaller.h` does not define the macro `error()`.
- Mail: the `MailDeliveryServiceImpl` constructor does not start the delivery thread; `start()` starts it, and `stop()`,
  which the destructor calls, stops it.
- SecureMail: the server certificate and, unless `osp.mail.tls.extendedCertificateVerification` is false, its host name
  are verified; TLS 1.2 or newer is required; only `osp.mail.tls.*` applies, not `openSSL.client`.
- SecureMail: a server that refuses STARTTLS fails the delivery; the login and the message are not sent unencrypted.
- JWT: a token without a subject (`sub`) is rejected.
- Prometheus: the request metrics are named `osp_web_requests_total`, `osp_web_requests_by_status_total` and
  `osp_web_request_duration_seconds`.
- Prometheus: the web server metrics have the label `server` instead of `host`, `port` and `protocol`.
- Prometheus: the `osp_web_buffer_pool_*` gauges and the jemalloc metrics are removed.
- RemotingNG: the TCP and REST bundles require the bundle `poco.remotingng`, which contains the POCO RemotingNG library.
- JS: the OSP JS library and the JavaScript bundles (`com.appinf.osp.js`, `com.appinf.osp.js.*`) are not part of POCO
  OSP.

### New components

- Auth/Remoting: a RemotingNG authenticator and authorizer that use the user authenticated by the OSP web server.
- Redis: the bundle `poco.redis` with the POCO Redis library.
- RemotingNG: the bundle `poco.remotingng` with the POCO RemotingNG library.
