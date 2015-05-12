/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once

#include <thread>

#include <Poco/AutoPtr.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "backends/redis.h"
#include "redisclient/redisasyncclient.h"

#include "spdlog/spdlog.h"


#define EXTDB_VERSION "53"
#define EXTDB_CONF_VERSION 3

class Redis;

class AbstractExt
{
	public:
		struct resultData
		{
			std::string message;
			bool wait = true;
		};

		// Database Connection Info
		struct DBConnectionInfo
		{
			std::string type;

			// Redis
			std::unique_ptr<RedisAsyncClient> redis_async_client;
			std::unique_ptr<Redis> redis;

			// SQL Database Session Pool
			std::unique_ptr<Poco::Data::SessionPool> sql_pool;
			std::mutex mutex_sql_pool;
		};

		// extDB Connectors
		struct extDBConnectors
		{
			std::unordered_map<std::string, DBConnectionInfo> databases;

			bool mysql=false;
			bool sqlite=false;
			bool redis=false;

			bool steam=false;
			bool rcon=false;
			bool remote=false;
		};
		extDBConnectors extDB_connectors_info;

		// extDB Info
		struct extDBInfo
		{
			std::string var;
			std::string path;
			std::string log_path;
			bool logger_flush=true;
			
			int max_threads;
			bool extDB_lock=false;
		};
		extDBInfo extDB_info;

		// RemoteAccess Info
		struct remoteAccessInfo
		{
			std::string password;
		};
		remoteAccessInfo remote_access_info;

		Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf;

		#ifdef DEBUG_TESTING
			std::shared_ptr<spdlog::logger> console;
		#endif
		std::shared_ptr<spdlog::logger> logger;
		std::shared_ptr<spdlog::logger> vacBans_logger;
	
		virtual void saveResult_mutexlock(const unsigned int &unique_id, const resultData &result_data)=0;

		virtual Poco::Data::Session getDBSession_mutexlock(DBConnectionInfo &database)=0;
		virtual Poco::Data::Session getDBSession_mutexlock(DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr)=0;
		
		virtual void rconCommand(std::string input_str)=0;
		virtual void rconPlayers(std::string input_str, unsigned int unique_id)=0;
		virtual void rconMissions(std::string input_str, unsigned int unique_id)=0;

		virtual void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup)=0;
		virtual void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup)=0;
};