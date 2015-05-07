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
#include <unordered_map>

#include <boost/asio.hpp>
#ifdef _WIN32
	#include <boost/filesystem.hpp>
#endif
#include <boost/thread/thread.hpp>

#include <Poco/Data/SessionPool.h>

#include "abstract_ext.h"
#include "backends/http.h"
#include "backends/rcon.h"
#include "backends/redis.h"
#include "backends/remoteserver.h"
#include "backends/steam.h"

#include "protocols/abstract_protocol.h"


class Ext: public AbstractExt
{
	public:
		Ext(std::string path, std::unordered_map<std::string, std::string> options, bool status);
		~Ext();
		void stop();	
		void callExtension(char *output, const int &output_size, const char *function);
		void rconCommand(std::string str);

	protected:
		const unsigned long saveResult_mutexlock(const resultData &result_data);
		void saveResult_mutexlock(const unsigned long &unique_id, const resultData &result_data);

		Poco::Thread rcon_thread;
		Poco::Thread steam_thread;

		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database);
		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr);

		void steamQuery(const unsigned long &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup);
		void steamQuery(const unsigned long &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup);

	private:
		// RCon
		Rcon rcon;

		/// Remote Server
		RemoteServer remote_server;

		// Steam
		Steam steam;

		// ASIO Thread Queue
		std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
		boost::asio::io_service io_service;
		boost::thread_group threads;

		// Protocols
		std::unordered_map< std::string, std::unique_ptr<AbstractProtocol> > unordered_map_protocol;
		std::mutex mutex_unordered_map_protocol;

		// Unique ID
		unsigned long unique_id_counter = 9816; // Can't be value 1

		// Results
		std::unordered_map<unsigned long, resultData> stored_results;
		std::mutex mutex_results;  // Using Same Lock for Unique ID aswell

		#ifdef _WIN32
			// Search for randomized config file
			void search(boost::filesystem::path &extDB_config_path, bool &conf_found, bool &conf_randomized);
		#endif

		// RCon
		void connectRcon(char *output, const int &output_size, const std::string &rcon_conf);

		// Remote
		void connectRemote(char *output, const int &output_size, const std::string &remote_conf);
		
		// Database
		void connectDatabase(char *output, const int &output_size, const std::string &database_conf, const std::string &database_id);

		// Results
		void getSinglePartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id);
		void getMultiPartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id);
		
		void getTCPRemote_mutexlock(char *output, const int &output_size);
		void sendTCPRemote_mutexlock(std::string &input_str);

		// Protocols
		void addProtocol(char *output, const int &output_size, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data);
		void syncCallProtocol(char *output, const int &output_size, std::string &input_str, std::string::size_type &input_str_length);
		void onewayCallProtocol(const int &output_size, std::string &input_str);
		void asyncCallProtocol(const int &output_size, const std::string &protocol, const std::string &data, const unsigned long &unique_id);
};