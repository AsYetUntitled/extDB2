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

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>

#include <Poco/Data/SessionPool.h>

#include <unordered_map>

#include "rconworker.h"
#include "remoteserver.h"
#include "steamworker.h"
#include "uniqueid.h"

#include "protocols/abstract_ext.h"
#include "protocols/abstract_protocol.h"



class Ext: public AbstractExt
{
	public:
		Ext(std::string path);
		~Ext();
		void stop();	
		void callExtenion(char *output, const int &output_size, const char *function);
		void rconCommand(std::string str);

	protected:
		const int saveResult_mutexlock(const resultData &result_data);
		void saveResult_mutexlock(const int &unique_id, const resultData &result_data);

		Poco::Thread rcon_worker_thread;
		Poco::Thread steam_worker_thread;

		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database);
		Poco::Data::Session getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr);

		void steamQuery(const int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup);

	private:
		// RCon
		RCONWORKER rcon_worker;

		/// Remote Server
		RemoteServer remote_server;

		// Steam
		STEAMWORKER steam_worker;

		// Unique ID for key for ^^
		IdManager mgr;

		// ASIO Thread Queue
		std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
		boost::asio::io_service io_service;
		boost::thread_group threads;

		// Protocols
		std::unordered_map< std::string, std::unique_ptr<AbstractProtocol> > unordered_map_protocol;
		boost::mutex mutex_unordered_map_protocol;

		std::unordered_map<int, resultData> stored_results;
		boost::mutex mutex_results;  // Using Same Lock for Wait / Results / Plugins


		// RCon
		void connectRCon(char *output, const int &output_size, const std::string &rcon_conf);
		
		// Database
		void connectDatabase(char *output, const int &output_size, const std::string &database_conf, const std::string &database_id);

		// Results
		void getSinglePartResult_mutexlock(const int &unique_id, char *output, const int &output_size);
		void getMultiPartResult_mutexlock(const int &unique_id, char *output, const int &output_size);
		void sendResult_mutexlock(const std::string &result, char *output, const int &output_size);
		
		void getTCPRemote_mutexlock(char *output, const int &output_size);
		void sendTCPRemote_mutexlock(std::string &input_str);

		// Protocols
		void addProtocol(char *output, const int &output_size, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data);
		void syncCallProtocol(char *output, const int &output_size, const std::string &protocol, const std::string &data);
		void onewayCallProtocol(const int &output_size, const std::string protocol, const std::string data);
		void asyncCallProtocol(const int &output_size, const std::string protocol, const std::string data, const int unique_id);
};