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
		Rcon(boost::asio::io_service& io_service, std::shared_ptr<spdlog::logger> spdlog);
		~Rcon();

		#ifndef RCON_APP
			void extInit(AbstractExt *extension);
		#endif

		void start(std::string &address, unsigned int port, std::string &password);
		void disconnect();
		bool status();
		
		//void addCommand(std::string &command);
		void getMissions(std::string &command, unsigned int &unique_id);
		void getPlayers(std::string &command, unsigned int &unique_id);

	private:
		//boost::asio::deadline_timer     timer;

		std::shared_ptr<spdlog::logger> logger;

		// Inputs are strings + Outputs are strings.  Info is not kept for long, so no point converting to a different datatype just to convert back to string for armaserver
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

		typedef std::pair< int, std::unordered_map < int, std::string > > RconMultiPartMsg;
		struct RconSocket
		{
			std::atomic<bool> *rcon_run_flag;
			std::atomic<bool> *rcon_login_flag;

			std::unique_ptr<boost::asio::ip::udp::socket> socket;
			boost::array<char, 8192> recv_buffer;

			unsigned char sequence_num_counter;
			std::mutex mutex_sequence_num_counter;

			std::unique_ptr<Poco::ExpireCache<unsigned char, RconMultiPartMsg> > rcon_msg_cache;
		};

		RconSocket rcon_socket_1;
		RconSocket rcon_socket_2;

		char *rcon_password;
		
		//Requests
		std::vector<unsigned int> missions_requests;
		std::mutex mutex_missions_requests;

		std::vector<unsigned int> players_requests;
		std::mutex mutex_players_requests;

		// Functions
		void connect(RconSocket &rcon_socket);
		void startReceive(RconSocket &rcon_socket);

		void createKeepAlive(RconSocket &rcon_socket);
		void sendPacket(RconSocket &rcon_socket, RconPacket &rcon_packet);
		void extractData(RconSocket &rcon_socket, std::size_t &bytes_received, int pos, std::string &result);

		unsigned char getSequenceNum(RconSocket &rcon_socket);
		unsigned char resetSequenceNum(RconSocket &rcon_socket);

		void connectionHandler(RconSocket &rcon_socket, const boost::system::error_code& error);
		void handleReceive(RconSocket &rcon_socket, const boost::system::error_code& error, std::size_t bytes_received);
		void handleSent(const boost::system::error_code&, std::size_t bytes_transferred);

		void loginResponse(RconSocket &rcon_socket);
		void serverResponse(RconSocket &rcon_socket, std::size_t &bytes_received);
		void chatMessage(RconSocket &rcon_socket, std::size_t &bytes_received);

		#ifndef RCON_APP
			AbstractExt *extension_ptr;
		#endif
};
