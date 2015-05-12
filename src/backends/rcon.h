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

#include <atomic>
#include <memory>
#include <thread>

#include <boost/asio/ip/udp.hpp>
#include <boost/crc.hpp>

#include <Poco/ExpireCache.h>
#include <Poco/Stopwatch.h>

#include "../abstract_ext.h"


class Rcon
{
	public:
		Rcon(boost::asio::io_service& io_service, std::shared_ptr<spdlog::logger> spdlog) : socket_(io_service), logger(spdlog){};
		~Rcon();

		void init();
		#ifndef RCON_APP
			void extInit(AbstractExt *extension);
		#endif
		void updateLogin(std::string &address, unsigned int port, std::string &password);
		
		void run();
		void disconnect();
		bool status();
		
		void addCommand(std::string &command);
		void getMissions(std::string &command, unsigned int &unique_id);
		void getPlayers(std::string &command, unsigned int &unique_id);

	private:
		boost::asio::ip::udp::socket socket_;
		std::shared_ptr<spdlog::logger> logger;

		boost::array<char, 4096> recv_buffer_;
		
		typedef std::pair< int, std::unordered_map < int, std::string > > RconMultiPartMsg;

		// Inputs are strings + Outputs are strings.  Info is not kept for long, so no point converting to a different datatype
		struct RconPlayerInfo   
		{
			std::string number;
			std::string ip;
			std::string port;
			std::string ping;
			std::string guid;
			std::string verified;
			std::string player_name;
		};

		struct RconPacket
		{
			char *cmd;
			char cmd_char_workaround;
			unsigned char packetCode;
			unsigned char sequence_number;
		};

		char *rcon_password;
		unsigned char sequence_num_counter;


		std::unique_ptr<Poco::ExpireCache<unsigned char, RconMultiPartMsg> > rcon_msg_cache;
		
		// Mutex Locks
		std::atomic<bool> *rcon_run_flag;
		std::atomic<bool> *rcon_login_flag;
		
		std::vector<std::pair<int, std::string> > rcon_commands;
		std::mutex mutex_rcon_commands;

		//Requests
		std::vector<unsigned int> missions_requests;
		std::mutex mutex_missions_requests;

		std::vector<unsigned int> players_requests;
		std::mutex mutex_players_requests;

		// Functions
		void connect();
		void mainLoop();
		void startReceive();

		void createKeepAlive();
		void sendPacket(RconPacket &rcon_packet);
		void extractData(int pos, std::string &result, std::size_t &bytes_received);

		void processMessage(unsigned char &sequence_number, std::string &message);

		void connectionHandler(const boost::system::error_code& error);
		void handleReceive(const boost::system::error_code& error, std::size_t bytes_received);
		void handleSent(const boost::system::error_code&, std::size_t bytes_transferred);

		void loginResponse();
		void serverResponse(std::size_t &bytes_received);
		void chatMessage(std::size_t &bytes_received);

		#ifndef RCON_APP
			AbstractExt *extension_ptr;
		#endif
};
