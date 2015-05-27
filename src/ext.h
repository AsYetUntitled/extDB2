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
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread/thread.hpp>

#include <Poco/Data/SessionPool.h>

#include "abstract_ext.h"
#include "backends/belogscanner.h"
#include "backends/http.h"
#include "backends/rcon.h"
#include "backends/remoteserver.h"
#include "backends/steam.h"

#include "protocols/abstract_protocol.h"


class Ext: public AbstractExt
{
	public:
		Ext(std::string shared_libary_path, std::unordered_map<std::string, std::string> &options);
		~Ext();
		void stop();
		void callExtension(char *output, const int &output_size, const char *function);
		void rconCommand(std::string input_str);

		void rconAddBan(std::string input_str);
		void rconMissions(unsigned int unique_id);
		void rconPlayers(unsigned int unique_id);

		void getDateTime(const std::string &input_str, std::string &result);
		void getUniqueString(int &len_of_string, int &num_of_string, std::string &result);

		void createPlayerKey_mutexlock(std::string &player_beguid, int len_of_key);

		void delPlayerKey_delayed(std::string &player_beguid);
		void delPlayerKey_mutexlock(std::string player_beguid);

		void getPlayerKey_SteamID(std::string &player_steam_id, std::string &player_key);
		void getPlayerKey_BEGuid(std::string &player_beguid, std::string &player_key);
		std::string getPlayerRegex_BEGuid(std::string &player_beguid);

	protected:
		const unsigned int saveResult_mutexlock(const resultData &result_data);
		void saveResult_mutexlock(const unsigned int &unique_id, const resultData &result_data);

		Poco::Thread steam_thread;

		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database);
		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr);

		void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup);
		void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup);

	private:
		struct PlayerKeys
		{
			std::list<std::string> keys;
			std::string regex_rule;
		};
		std::unordered_map<std::string, PlayerKeys> player_unique_keys;
		std::mutex player_unique_keys_mutex;

		// Rcon
		std::unique_ptr<Rcon> rcon;

		/// Remote Server
		RemoteServer remote_server;

		// BELogScanner
		BELogScanner belog_scanner;

		// Steam
		Steam steam;

		// ASIO Thread Queue
		std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
		boost::asio::io_service io_service;
		boost::thread_group threads;
		std::unique_ptr<boost::asio::deadline_timer> timer;

		// Protocols
		std::unordered_map< std::string, std::unique_ptr<AbstractProtocol> > unordered_map_protocol;
		std::mutex mutex_unordered_map_protocol;

		// Unique Random String
		std::string random_chars;
		boost::random::random_device random_chars_rng;
		std::mutex mutex_RandomString;
		std::vector < std::string > uniqueRandomVarNames;

		// Unique ID
		unsigned int unique_id_counter = 9816; // Can't be value 1

		// Results
		std::unordered_map<unsigned int, resultData> stored_results;
		std::mutex mutex_results;  // Using Same Lock for Unique ID aswell

		// Player Key
		Poco::MD5Engine md5;
		std::mutex mutex_md5;

		#ifdef _WIN32
			// Search for randomized config file
			void search(boost::filesystem::path &extDB_config_path, bool &conf_found, bool &conf_randomized);
		#endif

		// BELogScanner
		void startBELogscanner(char *output, const int &output_size, const std::string &conf);

		// Database
		void connectDatabase(char *output, const int &output_size, const std::string &database_conf, const std::string &database_id);
		void getSinglePartResult_mutexlock(char *output, const int &output_size, const unsigned int &unique_id);
		void getMultiPartResult_mutexlock(char *output, const int &output_size, const unsigned int &unique_id);

		// RCon
		void startRcon(char *output, const int &output_size, const std::string &conf, std::vector<std::string> &extra_rcon_options);

		// Remote
		void startRemote(char *output, const int &output_size, const std::string &conf);
		void getTCPRemote_mutexlock(char *output, const int &output_size);
		void sendTCPRemote_mutexlock(std::string &input_str);

		// Protocols
		void addProtocol(char *output, const int &output_size, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data);
		void syncCallProtocol(char *output, const int &output_size, std::string &input_str, std::string::size_type &input_str_length);
		void onewayCallProtocol(const int &output_size, std::string &input_str);
		void asyncCallProtocol(const int &output_size, const std::string &protocol, const std::string &data, const unsigned int unique_id);
};