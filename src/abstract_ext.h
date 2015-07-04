/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB2>

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

#include "spdlog/spdlog.h"


#define EXTDB_VERSION "64"
#define EXTDB_CONF_VERSION 5


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

			// SQL Database Session Pool
			std::unique_ptr<Poco::Data::SessionPool> sql_pool;
			std::mutex mutex_sql_pool;
		};

		// extDB Connectors
		struct extConnectors
		{
			std::unordered_map<std::string, DBConnectionInfo> databases;

			bool mysql = false;
			bool sqlite = false;

			bool belog_scanner = false;
			bool rcon = false;
			bool remote = false;
			bool steam=false;
		};
		extConnectors ext_connectors_info;

		// ext Info
		struct extInfo
		{
			std::string var;
			std::string path;
			std::string be_path;
			std::string log_path;

			int max_threads;
			bool extDB_lock = false;
			bool logger_flush = true;

		};
		extInfo ext_info;

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

		std::mutex player_unique_keys_mutex;

		virtual void saveResult_mutexlock(const unsigned int &unique_id, const resultData &result_data)=0;
		virtual void saveResult_mutexlock(std::vector<unsigned int> &unique_ids, const resultData &result_data)=0;

		virtual Poco::Data::Session getDBSession_mutexlock(DBConnectionInfo &database)=0;
		virtual Poco::Data::Session getDBSession_mutexlock(DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr)=0;

		virtual void rconCommand(std::string input_str)=0;
		virtual void rconAddBan(std::string input_str) = 0;
		virtual void rconPlayers(unsigned int unique_id)=0;
		virtual void rconMissions(unsigned int unique_id)=0;

		virtual void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup)=0;
		virtual void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup)=0;

		virtual void getDateTime(const std::string &input_str, std::string &result)=0;
		virtual void getUniqueString(int &len_of_string, int &num_of_string, std::string &result)=0;

		virtual void createPlayerKey_mutexlock(std::string &player_beguid, int len_of_key)=0;
		virtual void delPlayerKey_delayed(std::string &player_beguid)=0;

		virtual void getPlayerKey_SteamID(std::string &player_steam_id, std::string &player_key)=0;
		virtual void getPlayerKey_BEGuid(std::string &player_beguid, std::string &player_key)=0;
		virtual std::string getPlayerRegex_BEGuid(std::string &player_beguid)=0;
};
