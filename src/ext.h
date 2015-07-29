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
#include "backends/rcon.h"
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
		void delPlayerKey_mutexlock();

		void getPlayerKey_SteamID(std::string &player_steam_id, std::string &player_key);
		void getPlayerKey_BEGuid(std::string &player_beguid, std::string &player_key);
		std::string getPlayerRegex_BEGuid(std::string &player_beguid);

	protected:
		const unsigned int saveResult_mutexlock(const resultData &result_data);
		void saveResult_mutexlock(const unsigned int &unique_id, const resultData &result_data);
		void saveResult_mutexlock(std::vector<unsigned int> &unique_ids, const resultData &result_data);

		Poco::Thread steam_thread;

		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database);
		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr);

		void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup);
		void steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup);

	private:
		// Input
		std::string::size_type input_str_length;

		struct PlayerKeys
		{
			std::list<std::string> keys;
			std::string regex_rule;
		};
		std::unordered_map<std::string, PlayerKeys> player_unique_keys;
		std::list< std::pair<std::size_t, std::string> > del_players_keys;
		// std::mutex player_unique_keys_mutex;  defined in abstract_ext.h  used by BELogscanner

		// Rcon
		std::unique_ptr<Rcon> rcon;

		// BELogScanner
		BELogScanner belog_scanner;

		// Steam
		Steam steam;

		// Main ASIO Thread Queue
		std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
		boost::asio::io_service io_service;
		boost::thread_group threads;
		std::unique_ptr<boost::asio::deadline_timer> timer;

		// Rcon ASIO Thread Queue
		std::unique_ptr<boost::asio::io_service::work> rcon_io_work_ptr;
		boost::asio::io_service rcon_io_service;
		boost::thread_group rcon_threads;

		// Protocols
		std::unordered_map< std::string, std::unique_ptr<AbstractProtocol> > unordered_map_protocol;
		std::mutex mutex_unordered_map_protocol;

		// Unique Random String
		std::string random_chars;
		boost::random::random_device random_chars_rng;
		std::mutex mutex_RandomString;
		std::vector < std::string > uniqueRandomVarNames;

		// Unique ID
		std::string::size_type call_extension_input_str_length;
		unsigned int unique_id_counter = 100; // Can't be value 1

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

		// Protocols
		void addProtocol(char *output, const int &output_size, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data);
		void syncCallProtocol(char *output, const int &output_size, std::string &input_str);
		void onewayCallProtocol(std::string &input_str);
		void asyncCallProtocol(const int &output_size, const std::string &protocol, const std::string &data, const unsigned int unique_id);
};
