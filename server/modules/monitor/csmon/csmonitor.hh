/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-03-10
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include "csmon.hh"
#include <maxbase/http.hh>
#include <maxbase/semaphore.hh>
#include <maxscale/modulecmd.hh>
#include "csconfig.hh"
#include "csmonitorserver.hh"
#include "csrest.hh"

class CsMonitor : public maxscale::MonitorWorkerSimple
{
public:
    using Base = mxs::MonitorWorkerSimple;

    class Command;

    CsMonitor(const CsMonitor&) = delete;
    CsMonitor& operator=(const CsMonitor&) = delete;

    ~CsMonitor();
    static CsMonitor* create(const std::string& name, const std::string& module);

public:
    using ServerVector = std::vector<CsMonitorServer*>;

    ServerVector get_monitored_serverlist(const std::string& key, bool* error_out)
    {
        const auto& sl = Base::get_monitored_serverlist(key, error_out);

        return reinterpret_cast<const ServerVector&>(sl);
    }

    CsMonitorServer* get_monitored_server(SERVER* search_server)
    {
        return static_cast<CsMonitorServer*>(Base::get_monitored_server(search_server));
    }

    const ServerVector& servers() const
    {
        return reinterpret_cast<const ServerVector&>(Base::servers());
    }

    // Only to be called by the module call command mechanism.
    bool command_cluster_start(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_shutdown(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_ping(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_status(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_config_get(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_config_set(json_t** ppOutput, const char* zJson, CsMonitorServer* pServer);
    bool command_cluster_mode_set(json_t** ppOutput, const char* zEnum);

    bool command_cluster_add_node(json_t** ppOutput, CsMonitorServer* pServer);
    bool command_cluster_remove_node(json_t** ppOutput, CsMonitorServer* pServer);

private:
    enum Mode
    {
        READ_ONLY,
        READ_WRITE
    };

    static bool from_string(const char* zEnum, Mode* pMode);
    static const char* to_string(Mode mode);

    bool ready_to_run(json_t** ppOutput) const;
    static bool is_valid_json(json_t** ppOutput, const char* zJson, size_t len);

    bool command(json_t** ppOutput, mxb::Semaphore& sem, const char* zCmd, std::function<void()> cmd);

    void cluster_get(json_t** ppOutput,
                     mxb::Semaphore* pSem,
                     cs::rest::Action action,
                     CsMonitorServer* pServer);
    void cluster_put(json_t** ppOutput,
                     mxb::Semaphore* pSem,
                     cs::rest::Action action,
                     CsMonitorServer* pServer,
                     std::string&& body = std::string());

    void cluster_start(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_shutdown(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_ping(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_status(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_config_get(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_config_set(json_t** ppOutput, mxb::Semaphore* pSem,
                            std::string&& body, CsMonitorServer* pServer);
    void cluster_mode_set(json_t** ppOuput, mxb::Semaphore* pSem, Mode mode);
    void cluster_add_node(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);
    void cluster_remove_node(json_t** ppOutput, mxb::Semaphore* pSem, CsMonitorServer* pServer);

    bool has_sufficient_permissions();
    void update_server_status(mxs::MonitorServer* monitored_server);

    CsMonitorServer* create_server(SERVER* server, const mxs::MonitorServer::SharedSettings& shared) override;

private:
    CsMonitor(const std::string& name, const std::string& module);
    bool configure(const mxs::ConfigParameters* pParams) override;

    CsConfig                 m_config;
    mxb::http::Config        m_http_config;
    std::unique_ptr<Command> m_sCommand;
};