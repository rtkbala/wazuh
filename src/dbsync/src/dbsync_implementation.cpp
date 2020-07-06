/*
 * Wazuh DBSYNC
 * Copyright (C) 2015-2020, Wazuh Inc.
 * June 11, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "dbsync_implementation.h"
#include <iostream>
using namespace DbSync;
DBSYNC_HANDLE DBSyncImplementation::initialize(const HostType hostType,
                                               const DbEngineType dbType,
                                               const std::string& path,
                                               const std::string& sqlStatement)
{
    auto db{ FactoryDbEngine::create(dbType, path, sqlStatement) };
    const auto spDbEngineContext
    {
        std::make_shared<DbEngineContext>(db, hostType, dbType)
    };
    const DBSYNC_HANDLE handle{ spDbEngineContext.get() };
    std::lock_guard<std::mutex> lock{m_mutex};
    m_dbSyncContexts[handle] = spDbEngineContext;
    return handle;
}

void DBSyncImplementation::release()
{
    std::lock_guard<std::mutex> lock{m_mutex};
    m_dbSyncContexts.clear();
}

void DBSyncImplementation::insertBulkData(const DBSYNC_HANDLE handle,
                                          const char* jsonRaw)
{
    const auto ctx{ dbEngineContext(handle) };
    const auto json { nlohmann::json::parse(jsonRaw)};
    ctx->m_dbEngine->bulkInsert(json[0]["table"], json[0]["data"]);
}

void DBSyncImplementation::updateSnapshotData(const DBSYNC_HANDLE handle,
                                              const char* jsonSnapshot,
                                              std::string& result)
{
    const auto ctx{ dbEngineContext(handle) };
    const auto json { nlohmann::json::parse(jsonSnapshot)};
    nlohmann::json jsonResult;
    ctx->m_dbEngine->refreshTableData(json[0], std::make_tuple(std::ref(jsonResult), nullptr));
    result = std::move(jsonResult.dump());
}

void DBSyncImplementation::updateSnapshotData(const DBSYNC_HANDLE handle,
                                              const char* jsonSnapshot,
                                              void* callback)
{
    const auto ctx{ dbEngineContext(handle) };
    const auto json { nlohmann::json::parse(jsonSnapshot)};
    nlohmann::json fake;
    ctx->m_dbEngine->refreshTableData(json[0], std::make_tuple(std::ref(fake), callback));
}

std::shared_ptr<DBSyncImplementation::DbEngineContext> DBSyncImplementation::dbEngineContext(const DBSYNC_HANDLE handle)
{
    std::lock_guard<std::mutex> lock{m_mutex};
    const auto it{ m_dbSyncContexts.find(handle) };
    if (it == m_dbSyncContexts.end())
    {
        throw dbsync_error
        {
            2, "Invalid handle value."
        };
    }
    return it->second;
}
