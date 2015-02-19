/*
Copyright (C) 2012 Prithu "bladez" Parker <https://github.com/bladez-/RCONWORKER>
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

 * Change Log
 * Changed Code to use Poco Net Library 
*/


#pragma once

#include <boost/crc.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/NetException.h>

#include <Poco/Stopwatch.h>

#include <Poco/ExpireCache.h>

#include "spdlog/spdlog.h"
#include "protocols/abstract_ext.h"

#include <atomic>


class RCONWORKER: public Poco::Runnable
{
	public:
		void init(std::shared_ptr<spdlog::logger> console);
		void updateLogin(std::string address, int port, std::string password);
		
		void run();
		void disconnect();
		
		void addCommand(std::string command);


	private:
		typedef std::pair< int, std::unordered_map < int, std::string > > RconMultiPartMsg;

		struct RconPacket
		{
			char *cmd;
			char cmd_char_workaround;
			unsigned char packetCode;
		};
		
		struct RconLogin
		{
			char *password;
			bool auto_reconnect;

			int port;
			std::string address;
		};

		RconPacket rcon_packet;
		RconLogin rcon_login;

		Poco::Net::DatagramSocket dgs;

		Poco::Stopwatch rcon_timer;

		std::shared_ptr<spdlog::logger> logger;
		
		char buffer[4096];
		int buffer_size;

		std::string keepAlivePacket;

		boost::crc_32_type crc32;
		
		// Mutex Locks
		std::atomic<bool> *rcon_run_flag;
		bool logged_in = false;
		
		boost::recursive_mutex mutex_rcon_commands;
		std::vector< std::string > rcon_commands;

		// Functions
		void connect();
		void mainLoop();

		void createKeepAlive();
		void sendPacket();
		void extractData(int pos, std::string &result);
};
