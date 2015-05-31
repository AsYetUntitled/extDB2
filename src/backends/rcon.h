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

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/crc.hpp>

#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/SQLite/Connector.h>

#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/SQLiteException.h>

#include <Poco/ExpireCache.h>
#include <Poco/Stopwatch.h>
#include <Poco/StringTokenizer.h>

#include "../abstract_ext.h"


class Rcon
{
	public:
		struct RconSettings
		{
			bool return_full_player_info = false;
			bool generate_unique_id = false;

			unsigned int port;

			std::string address;
			std::string password;
		};
		RconSettings rcon_settings;

		struct BadPlayernameSettings
		{
			bool enable = false;
			std::vector<std::string> bad_strings;
			std::vector<std::string> bad_regexs;
			std::string kick_message;
		};
		BadPlayernameSettings bad_playername_settings;

		struct WhitelistSettings
		{
			int open_slots = 0;
			bool enable = false;

			bool connected_database = false;
			bool kick_on_failed_sql_query = false;
			std::string database;

			std::vector<std::string> whitelisted_guids;

			std::unordered_map<std::string, std::string> players_whitelisted;
			std::unordered_map<std::string, std::string> players_non_whitelisted;

			std::string sql_statement;
			std::string kick_message;
		};
		WhitelistSettings whitelist_settings;

		std::unique_ptr<Poco::Data::Session> whitelist_session;
		std::unique_ptr<Poco::Data::Statement> whitelist_statement;

		std::mutex reserved_slots_mutex;

		// Player Name / BEGuid
		std::unordered_map<std::string, std::string> players_name_beguid;
		std::recursive_mutex players_name_beguid_mutex;

		Rcon(boost::asio::io_service &io_service, std::shared_ptr<spdlog::logger> spdlog);
		~Rcon();

		void timerReconnect(const size_t delay);
		void Reconnect(const boost::system::error_code& error);

		#ifndef RCON_APP
			void extInit(AbstractExt *extension);
		#endif

		void start(RconSettings &rcon, BadPlayernameSettings &bad_playername, WhitelistSettings &reserved_slots, Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf);

		void disconnect();
		bool status();

		void sendCommand(std::string command);

		void addBan(std::string command);
		void getMissions(unsigned int &unique_id);
		void getPlayers(unsigned int &unique_id);

	private:
		#ifdef RCON_APP
			struct DBConnectors
			{
				bool mysql=false;
				bool sqlite=false;
			};
			DBConnectors DB_connectors_info;
		#else
			AbstractExt *extension_ptr;
		#endif

		bool auto_reconnect = true;
		char *rcon_password;

		boost::asio::io_service *io_service_ptr;
		std::shared_ptr<spdlog::logger> logger;

		// Inputs are strings + Outputs are strings.
		// 		Info is not kept for long, so there no point converting to a different datatype just to convert back to a string for armaserver
		struct RconPlayerInfo
		{
			std::string number;
			std::string ip;
			std::string port;
			std::string ping;
			std::string guid;
			std::string verified;
			std::string player_name;
			std::string lobby;
		};

		struct RconPacket
		{
			char *cmd;
			char cmd_char_workaround;
			unsigned char packetCode;
		};

		struct RconRequest
		{
			unsigned int unique_id;
			int request_type;
		};


		typedef std::pair< int, std::unordered_map<int, std::string> > RconMultiPartMsg;
		struct RconSocket
		{
			std::atomic<bool> *rcon_run_flag;
			std::atomic<bool> *rcon_login_flag;

			std::unique_ptr<boost::asio::ip::udp::socket> socket;
			boost::array<char, 8192> recv_buffer;

			std::unique_ptr<boost::asio::deadline_timer> keepalive_timer;

			std::unique_ptr<Poco::ExpireCache<unsigned char, RconMultiPartMsg> > rcon_msg_cache;

			//Mission Requests
			std::vector<unsigned int> mission_requests;
			std::mutex mutex_mission_requests;

			//Player Requests
			std::vector<unsigned int> player_requests;
			std::mutex mutex_players_requests;

			boost::crc_32_type keep_alive_crc32;
		};
		RconSocket rcon_socket;


		void connect();
		void startReceive();

		void timerKeepAlive(const size_t delay);
		void createKeepAlive(const boost::system::error_code& error);

		void sendPacket(RconPacket &rcon_packet);
		void sendBanPacket(RconPacket &rcon_packet);
		void extractData(std::size_t &bytes_received, int pos, std::string &result);

		void connectionHandler(const boost::system::error_code& error);
		void handleReceive(const boost::system::error_code& error, std::size_t bytes_received);
		void handleSent(std::shared_ptr<std::string> packet, const boost::system::error_code &error, std::size_t bytes_transferred);
		void handleBanSent(std::shared_ptr<std::string> packet, const boost::system::error_code &error, std::size_t bytes_transferred);

		void loginResponse();
		void serverResponse(std::size_t &bytes_received);

		void processMessage(unsigned char &sequence_number, std::string &message);
		void processMessageMission(Poco::StringTokenizer &tokens);
		void processMessagePlayers(Poco::StringTokenizer &tokens);
		void chatMessage(std::size_t &bytes_received);

		void connectDatabase(Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf);
		void checkDatabase(bool &status, bool &error);

		void checkBadPlayerString(std::string &player_number, std::string &player_name, bool &kicked);
		void checkWhitelistedPlayer(std::string &player_number, std::string &player_name, std::string &player_guid, bool &kicked);
};
