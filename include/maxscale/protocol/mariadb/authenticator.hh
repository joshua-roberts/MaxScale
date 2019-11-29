/*
 * Copyright (c) 2019 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include <maxscale/authenticator.hh>
#include <unordered_set>

class DCB;
class SERVICE;
class GWBUF;
class MYSQL_session;

namespace mariadb
{

class ClientAuthenticator;
class BackendAuthenticator;

using SClientAuth = std::unique_ptr<ClientAuthenticator>;
using SBackendAuth = std::unique_ptr<BackendAuthenticator>;

struct UserEntry
{
    std::string username;       /**< Username */
    std::string host_pattern;   /**< Hostname or IP, may have wildcards */
    std::string plugin;         /**< Auth plugin to use */
    std::string password;       /**< Auth data used by native auth plugin */
    std::string auth_string;    /**< Auth data used by other plugins */

    bool ssl {false};           /**< Should the user connect with ssl? */
    bool global_db_priv {false};/**< Does the user have access to all databases? */
    bool proxy_grant {false};   /**< Does the user have proxy grants? */

    bool        is_role {false};/**< Is the user a role? */
    std::string default_role;   /**< Default role if any */

    static bool host_pattern_is_more_specific(const UserEntry& lhs, const UserEntry& rhs);
};

/**
 * The base class of all authenticators for MariaDB-protocol. Contains the global data for
 * an authenticator module instance.
 */
class AuthenticatorModule : public mxs::AuthenticatorModule
{
public:
    AuthenticatorModule(const AuthenticatorModule&) = delete;
    AuthenticatorModule& operator=(const AuthenticatorModule&) = delete;

    enum Capabilities
    {
        CAP_REAUTHENTICATE  = (1 << 0), /**< Does the module support reauthentication? */
        CAP_BACKEND_AUTH    = (1 << 1), /**< Does the module support backend authentication? */
        CAP_CONC_LOAD_USERS = (1 << 2), /**< Does the module support concurrent user loading? */
        CAP_ANON_USER       = (1 << 3), /**< Does the module allow anonymous users? */
    };

    AuthenticatorModule() = default;
    virtual ~AuthenticatorModule() = default;

    /**
     * Create a client authenticator.
     *
     * @return Client authenticator
     */
    virtual SClientAuth create_client_authenticator() = 0;

    /**
     * Create a new backend authenticator. Should only be implemented by authenticator modules which
     * also support backend authentication.
     *
     * @return Backend authenticator
     */
    virtual SBackendAuth create_backend_authenticator() = 0;

    // Load or update authenticator user data
    virtual int load_users(SERVICE* service) = 0;

    /**
     * @brief Return diagnostic information about the authenticator
     *
     * The authenticator module should return information about its internal
     * state when this function is called.
     *
     * @return JSON representation of the authenticator
     */
    virtual json_t* diagnostics() = 0;

    /**
     * List the server authentication plugins this authenticator module supports.
     *
     * @return Supported authenticator plugins
     */
    virtual const std::unordered_set<std::string>& supported_plugins() const = 0;

    /**
     * Get module runtime capabilities. Returns 0 by default.
     *
     * @return Capabilities as a bitfield
     */
    virtual uint64_t capabilities() const;
};

using SAuthModule = std::unique_ptr<AuthenticatorModule>;

/**
 * The base class of authenticator client sessions. Contains session-specific data for an authenticator.
 */
class ClientAuthenticator
{
public:
    using ByteVec = std::vector<uint8_t>;

    // Return values for authenticate-functions.
    enum class AuthRes
    {
        SUCCESS,        /**< Authentication was successful */
        FAIL,           /**< Authentication failed */
        FAIL_DB,        /**< Authentication failed, database not found or no access */
        FAIL_WRONG_PW,  /**< Client provided wrong password */
        FAIL_SSL,       /**< SSL authentication failed */
        INCOMPLETE,     /**< Authentication is not yet complete */
        INCOMPLETE_SSL, /**< SSL connection is not yet complete */
        SSL_READY,      /**< SSL connection complete or not required */
        NO_SESSION,
        BAD_HANDSHAKE,      /**< Malformed client packet */
    };

    ClientAuthenticator(const ClientAuthenticator&) = delete;
    ClientAuthenticator& operator=(const ClientAuthenticator&) = delete;

    ClientAuthenticator() = default;
    virtual ~ClientAuthenticator() = default;

    /**
     * Extract client from a buffer and place it in a structure shared at the session level.
     * Typically, this is called just before the authenticate-entrypoint.
     *
     * @param buffer Packet from client
     * @return True on success
     */
    virtual bool extract(GWBUF* buffer, MYSQL_session* session) = 0;

    // Carry out the authentication.
    virtual AuthRes authenticate(DCB* client, const UserEntry* entry) = 0;

    /**
     * This entry point was added to avoid calling authenticator functions
     * directly when a COM_CHANGE_USER command is executed. Not implemented by most authenticators.
     *
     * @param dcb The connection
     * @param scramble Scramble sent by MaxScale to client
     * @param scramble_len Scramble length
     * @param auth_token Authentication token sent by client
     * @param output Hashed client password used by backend protocols
     * @return 0 on success
     */
    virtual AuthRes reauthenticate(const UserEntry* entry, DCB* client, uint8_t* scramble, size_t scramble_len,
                                   const ByteVec& auth_token, uint8_t* output);
};

// Helper template which stores the module reference.
template<class AuthModule>
class ClientAuthenticatorT : public ClientAuthenticator
{
public:
    /**
     * Constructor.
     *
     * @param module The global module data
     */
    ClientAuthenticatorT(AuthModule* module)
        : m_module(*module)
    {
    }

protected:
    AuthModule& m_module;
};

/**
 * The base class for all authenticator backend sessions. Created by the client session.
 */
class BackendAuthenticator
{
public:
    // Return values for authenticate-functions.
    enum class AuthRes
    {
        SUCCESS,    /**< Authentication was successful */
        FAIL,       /**< Authentication failed */
        INCOMPLETE, /**< Authentication is not yet complete */
    };

    BackendAuthenticator(const BackendAuthenticator&) = delete;
    BackendAuthenticator& operator=(const BackendAuthenticator&) = delete;

    BackendAuthenticator() = default;
    virtual ~BackendAuthenticator() = default;

    // Extract backend data from a buffer. Typically, this is called just before the authenticate-entrypoint.
    virtual bool extract(DCB* client, GWBUF* buffer) = 0;

    // Determine whether the connection can support SSL.
    virtual bool ssl_capable(DCB* client) = 0;

    // Carry out the authentication.
    virtual AuthRes authenticate(DCB* client) = 0;
};

}