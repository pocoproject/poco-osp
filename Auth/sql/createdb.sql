--
-- createdb.sql
--
-- Create SQL tables for OSP Auth bundle.
-- Will also create user "admin" (password "admin")
-- with administrator permissions ("bundleAdmin" and "authAdmin").
--

CREATE TABLE auth_user
(
	username VARCHAR(256) PRIMARY KEY,
	password VARCHAR(256) NOT NULL,
	auth_failures INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE auth_role
(
	rolename VARCHAR(256) PRIMARY KEY
);

CREATE TABLE auth_user_roles
(
	username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,
	rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE
);

CREATE TABLE auth_user_permissions
(
	username VARCHAR(256) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,
	permission VARCHAR(32) NOT NULL 
);

CREATE TABLE auth_role_permissions
(
	rolename VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE,
	permission VARCHAR(32) NOT NULL 
);

CREATE TABLE auth_user_attributes
(
	username VARCHAR(64) NOT NULL REFERENCES auth_user(username) ON DELETE CASCADE,
	attribute VARCHAR(64) NOT NULL,
	value VARCHAR(256) NOT NULL
);

CREATE TABLE auth_role_attributes
(
	rolename VARCHAR(64) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE,
	attribute VARCHAR(64) NOT NULL,
	value VARCHAR(256) NOT NULL
);

CREATE TABLE auth_role_mappings
(
	external_role VARCHAR(256) PRIMARY KEY REFERENCES auth_role(rolename) ON DELETE CASCADE,
	internal_role VARCHAR(256) NOT NULL REFERENCES auth_role(rolename) ON DELETE CASCADE
);

INSERT INTO auth_user (username, password) VALUES('admin', '3d29e163b9107fcb31077b45b47026e2');

INSERT INTO auth_user_attributes VALUES('admin', 'auth_ldap', 'false');

INSERT INTO auth_role VALUES('admin');

INSERT INTO auth_role_attributes VALUES('admin', 'auth_ldap_role', 'false');

INSERT INTO auth_role_permissions VALUES('admin', 'bundleAdmin');
INSERT INTO auth_role_permissions VALUES('admin', 'authAdmin');

INSERT INTO auth_user_roles VALUES('admin', 'admin');
