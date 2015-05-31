/*
Copyright (C) 2012 Prithu "bladez" Parker <https://github.com/bladez-/bercon>
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
 * Changed Code to use Poco Net Library & Poco Checksums
*/


#include "rcon.h"

#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef RCON_APP
	#include <boost/program_options.hpp>
	#include <boost/thread/thread.hpp>
	#include <fstream>
#else
	#include "../abstract_ext.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>

#include <Poco/Data/MySQL/MySQLException.h>

#include <Poco/AbstractCache.h>
#include <Poco/Exception.h>
#include <Poco/ExpireCache.h>
#include <Poco/SharedPtr.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Stopwatch.h>



Rcon::Rcon(boost::asio::io_service &io_service, std::shared_ptr<spdlog::logger> spdlog)
{
	io_service_ptr = &io_service;
	logger = spdlog;

	rcon_socket.rcon_run_flag = new std::atomic<bool>(false);
	rcon_socket.rcon_login_flag = new std::atomic<bool>(false);
	rcon_socket.socket.reset(new boost::asio::ip::udp::socket(io_service));
	rcon_socket.rcon_msg_cache.reset(new Poco::ExpireCache<unsigned char, RconMultiPartMsg>(120000));
}


Rcon::~Rcon(void)
{
}


void Rcon::timerKeepAlive(const size_t delay)
{
	if (delay == 0)
	{
		rcon_socket.keepalive_timer->cancel();
	}
	else
	{
		rcon_socket.keepalive_timer->expires_from_now(boost::posix_time::seconds(delay));
		rcon_socket.keepalive_timer->async_wait(boost::bind(&Rcon::createKeepAlive, this, boost::asio::placeholders::error));
	}
}


void Rcon::timerReconnect(const size_t delay)
{
	if (delay == 0)
	{
		rcon_socket.keepalive_timer->cancel();
	}
	else
	{
		rcon_socket.keepalive_timer->expires_from_now(boost::posix_time::seconds(delay));
		rcon_socket.keepalive_timer->async_wait(boost::bind(&Rcon::Reconnect, this, boost::asio::placeholders::error));
	}
}


void Rcon::Reconnect(const boost::system::error_code& error)
{
	logger->info("Rcon: Attempting to Reconnect");
	boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(rcon_settings.address), rcon_settings.port);
	rcon_socket.socket->async_connect(endpoint, boost::bind(&Rcon::connectionHandler, this, boost::asio::placeholders::error));
}


#ifndef RCON_APP
	void Rcon::extInit(AbstractExt *extension)
	{
		extension_ptr = extension;
	}
#endif


void Rcon::start(RconSettings &rcon, BadPlayernameSettings &bad_playername, WhitelistSettings &whitelist, Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf)
{
	rcon_settings = std::move(rcon);
	bad_playername_settings = std::move(bad_playername);
	whitelist_settings = std::move(whitelist);

	rcon_password = new char[rcon_settings.password.size() + 1];
	std::strcpy(rcon_password, rcon_settings.password.c_str());

	if (whitelist_settings.enable)
	{
		if (whitelist_settings.database.empty())
		{
			logger->info("Rcon: No Database Backend");
		}
		else
		{
			connectDatabase(pConf);
		}
	}

	#ifndef RCON_APP
		// Disable Auto Reconnects for extension i.e if Rcon Settings are wrong
		auto_reconnect = false;
	#endif

	boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(rcon_settings.address), rcon_settings.port);

	boost::system::error_code ec;
	rcon_socket.socket->connect(endpoint, ec);
	if (!ec)
	{
		#ifndef RCON_APP
			auto_reconnect = true;  // Re-enable Auto Reconnects for extension i.e if Rcon Settings are good
		#endif
		startReceive();
		connect();
	}
	else
	{
		logger->info("Rcon: UDP Socket Connection Error");
		timerKeepAlive(0);
		rcon_socket.socket->close();
	}
}


void Rcon::connectionHandler(const boost::system::error_code& error)
{
	if (!error)
	{
		startReceive();
		connect();
	}
	else
	{

		logger->info("Rcon: UDP Socket Connection Error");
		timerKeepAlive(0);
		rcon_socket.socket->close();
		if (auto_reconnect)
		{
			timerReconnect(5);
		}
	}
}


void Rcon::connect()
{
	rcon_socket.keepalive_timer.reset(new boost::asio::deadline_timer(*io_service_ptr));

	*(rcon_socket.rcon_login_flag) = false;
	*(rcon_socket.rcon_run_flag) = true;

	// Login Packet
	RconPacket rcon_packet;
	rcon_packet.cmd = rcon_password;
	rcon_packet.packetCode = 0x00;
	sendPacket(rcon_packet);
	logger->info("Rcon: Sent Login Info");
}


void Rcon::disconnect()
{
	auto_reconnect = false;
	timerKeepAlive(0);
	rcon_socket.socket->close();
}


bool Rcon::status()
{
	return (*(rcon_socket.rcon_run_flag) && (rcon_socket.rcon_login_flag));
}


void Rcon::startReceive()
{
	rcon_socket.socket->async_receive(
		boost::asio::buffer(rcon_socket.recv_buffer),
		boost::bind(&Rcon::handleReceive, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}


void Rcon::handleReceive(const boost::system::error_code& error, std::size_t bytes_received)
{
	if (!error)
	{
		rcon_socket.recv_buffer[bytes_received] = '\0';

		switch(rcon_socket.recv_buffer[7])
		{
			case 0x00:
				loginResponse();
				break;
			case 0x01:
				serverResponse(bytes_received);
				break;
			case 0x02:
				chatMessage(bytes_received);
				break;
		};
	}
	else
	{
		logger->info("Rcon: UDP handleReceive Error: {0}", error.message());

		timerKeepAlive(0);
		rcon_socket.socket->close();
		if (auto_reconnect)
		{
			timerReconnect(5);
		}
	}
}


void Rcon::loginResponse()
{
	if (rcon_socket.recv_buffer[8] == 0x01)
	{
		*(rcon_socket.rcon_login_flag) = true;

		logger->info("Rcon: Login Success");
		timerKeepAlive(30);
		unsigned int unique_id = 1;
		getPlayers(unique_id);
		startReceive();
	}
	else
	{
		*(rcon_socket.rcon_login_flag) = false;
		logger->info("Rcon: Login Failed");
		disconnect();
	}
}


void Rcon::serverResponse(std::size_t &bytes_received)
{
	// Rcon Server Ack Message Received
	unsigned char sequenceNum = rcon_socket.recv_buffer[8];

	if (!((rcon_socket.recv_buffer[9] == 0x00) && (bytes_received > 9)))
	{
		// Server Received Command Message
		std::string result;
		extractData(bytes_received, 9, result);
		processMessage(sequenceNum, result);
	}
	else
	{
		// Rcon Multi-Part Message Recieved
		int numPackets = rcon_socket.recv_buffer[10];
		int packetNum = rcon_socket.recv_buffer[11];

		std::string partial_msg;
		extractData(bytes_received, 12, partial_msg);

		if (!(rcon_socket.rcon_msg_cache->has(sequenceNum)))
		{
			// Doesn't have sequenceNum in Buffer
			RconMultiPartMsg rcon_mp_msg;
			rcon_mp_msg.first = 1;
			rcon_socket.rcon_msg_cache->add(sequenceNum, rcon_mp_msg);

			Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_socket.rcon_msg_cache->get(sequenceNum);
			ptrElem->second[packetNum] = partial_msg;
		}
		else
		{
			// Has sequenceNum in Buffer
			Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_socket.rcon_msg_cache->get(sequenceNum);
			ptrElem->first = ptrElem->first + 1;
			ptrElem->second[packetNum] = partial_msg;

			if (ptrElem->first == numPackets)
			{
				// All packets Received, re-construct message
				std::string result;
				for (int i = 0; i < numPackets; ++i)
				{
					result = result + ptrElem->second[i];
				}
				processMessage(sequenceNum, result);
				rcon_socket.rcon_msg_cache->remove(sequenceNum);
			}
		}
	}
	startReceive();
}


void Rcon::processMessage(unsigned char &sequence_number, std::string &message)
{
	#if defined(RCON_APP) || (DEBUG_TESTING)
		logger->info("RCon: {0}", message);
	#endif

	Poco::StringTokenizer tokens(message, "\n");
	if (tokens.count() > 0)
	{
		if (tokens[0] == "Missions on server:")
		{
			processMessageMission(tokens);
		}
		else if (tokens[0] == "Players on server:")
		{
			processMessagePlayers(tokens);
		}
		else
		{
			logger->warn("RCon: Unknown Message {0}", message);
		}
	}
}


void Rcon::processMessageMission(Poco::StringTokenizer &tokens)
{
	std::vector<std::string> info_vector;
	for (int i = 1; i < (tokens.count()); ++i)
	{
		if (boost::algorithm::ends_with(tokens[i], ".pbo"))
		{
			info_vector.push_back(tokens[i].substr(0, tokens[i].size() - 4));
		}
		else
		{
			info_vector.push_back(tokens[i]);
		}
	}

	AbstractExt::resultData result_data;
	if (info_vector.empty())
	{
		result_data.message  = "[1,[]]";
	}
	else
	{
		result_data.message = "[1,[";
		for(auto &info : info_vector)
		{
			result_data.message += info;
			result_data.message += ",";
			logger->info("Server Mission: {0}", info);
		}
		result_data.message.pop_back();
		result_data.message += "]]";
	}

	#ifdef RCON_APP
		logger->info("RCON: Mission: {0}", result_data.message);
	#else
		std::vector<unsigned int> unique_id_saves;
		{
			std::lock_guard<std::mutex> lock(rcon_socket.mutex_mission_requests);
				for (unsigned int unique_id : rcon_socket.mission_requests)
				{
					unique_id_saves.push_back(unique_id);
				}
				rcon_socket.mission_requests.clear();
		}
		for (unsigned int unique_id: unique_id_saves)
		{
			if (unique_id != 1)
			{
				extension_ptr->saveResult_mutexlock(unique_id, result_data);
			}
		}
	#endif
}


void Rcon::processMessagePlayers(Poco::StringTokenizer &tokens)
{
	{
		std::lock_guard<std::mutex> lock(reserved_slots_mutex);
		whitelist_settings.players_whitelisted.clear();
		whitelist_settings.players_non_whitelisted.clear();
	}

	std::vector<RconPlayerInfo> info_vector;
	std::string player_str;
	for (int i = 3; i < (tokens.count() - 1); ++i)
	{
		player_str = tokens[i];
		player_str.erase(std::unique(player_str.begin(), player_str.end(), [](char a, char b) { return a == ' ' && b == ' '; } ), player_str.end() );

		Poco::StringTokenizer player_tokens(player_str, " ");
		if (player_tokens.count() >= 5)
		{
			RconPlayerInfo player_data;

			player_data.number = player_tokens[0];
			auto found = player_tokens[1].find(":");
			player_data.ip = player_tokens[1].substr(0, found - 1);
			player_data.port = player_tokens[1].substr(found + 1);
			player_data.ping = player_tokens[2];

			if (boost::algorithm::ends_with(player_tokens[3], "(OK)"))
			{
				player_data.verified = "true";
				player_data.guid = player_tokens[3].substr(0, (player_tokens[3].size() - 4));
			}
			else
			{
				player_data.verified = "false";
				player_data.guid = player_tokens[3].substr(0, (player_tokens[3].size() - 12));
			}
			found = tokens[i].find(")");
			player_data.player_name = tokens[i].substr(found + 2);
			boost::replace_all(player_data.player_name, "\"", "\"\"");
			boost::replace_all(player_data.player_name, "'", "''");

			if (boost::algorithm::ends_with(player_data.player_name, " (Lobby)"))
			{
				player_data.player_name = player_data.player_name.substr(0, player_data.player_name.size() - 8);
				player_data.lobby = "true";
			}
			else
			{
				player_data.lobby = "false";
			}

			#if defined(RCON_APP) || (DEBUG_TESTING)
				logger->info("DEBUG players Player Number: {0}.", player_data.number);
				logger->info("DEBUG players Player Name: {0}.", player_data.player_name);
				logger->info("DEBUG players Player GUID: {0}.", player_data.guid);
			#endif

			{
				std::lock_guard<std::recursive_mutex> lock(players_name_beguid_mutex);
				players_name_beguid[player_data.player_name] = player_data.guid;
			}

			bool kicked = false;
			if (bad_playername_settings.enable)
			{
				checkBadPlayerString(player_data.number, player_data.player_name, kicked);
			}

			if (whitelist_settings.enable && (whitelist_settings.open_slots == 0))
			{
				if (!kicked)
				{
					checkWhitelistedPlayer(player_data.number, player_data.player_name, player_data.guid, kicked);
				}
			}

			#ifndef RCON_APP
				if ((!kicked) && rcon_settings.generate_unique_id)
				{
					// We only bother to generate a key if player has not been kicked
					extension_ptr->createPlayerKey_mutexlock(player_data.guid, 10);
				}
			#endif

			info_vector.push_back(std::move(player_data));
		}
		else
		{
			logger->info("Rcon: Error: Wrong RconPlayerInfo count: {0}.", player_tokens.count());
		}
	}

	AbstractExt::resultData result_data;
	if (info_vector.empty())
	{
		result_data.message  = "[1,[]]";
	}
	else
	{
		result_data.message = "[1,[";
		if (rcon_settings.return_full_player_info)
		{
			for(auto &info : info_vector)
			{
				result_data.message += "[\"" + info.number + "\",";
				result_data.message += "\"" + info.ip + "\",";
				result_data.message += info.port + ",";
				result_data.message += info.ping + ",";
				result_data.message += "\"" + info.guid + "\",";
				result_data.message += info.verified + ",";
				result_data.message += "\"" + info.player_name + "\",";
				result_data.message += info.lobby + "],";
			}
		}
		else
		{
			for(auto &info : info_vector)
			{
				result_data.message += "[\"" + info.number + "\",";
				result_data.message += "\"" + info.guid + "\",";
				result_data.message += info.verified + ",";
				result_data.message += "\"" + info.player_name + "\",";
				result_data.message += info.lobby + "],";
			}
		}

		result_data.message.pop_back();
		result_data.message += "]]";
	}

	#ifndef RCON_APP
		std::vector<unsigned int> unique_id_saves;
	#endif
	{
		std::lock_guard<std::mutex> lock(rcon_socket.mutex_players_requests);
		#ifdef RCON_APP
			logger->info("RCON: Player: {0}", result_data.message);
		#else
			for (unsigned int unique_id : rcon_socket.player_requests)
			{
				unique_id_saves.push_back(unique_id);
			}
			rcon_socket.player_requests.clear();
		#endif
	}
	#ifndef RCON_APP
		for (unsigned int unique_id: unique_id_saves)
		{
			if (unique_id != 1)
			{
				extension_ptr->saveResult_mutexlock(unique_id, result_data);
			}
		}
	#endif
}


void Rcon::checkBadPlayerString(std::string &player_number, std::string &player_name, bool &kicked)
{
	for (auto &bad_string : bad_playername_settings.bad_strings)
	{
		if (!(boost::algorithm::ifind_first(player_name, bad_string).empty()))
		{
			kicked = true;
			sendCommand("kick " + player_number + " " + bad_playername_settings.kick_message);
			logger->info("RCon: Kicked Playername: {0} String: {1}", player_name, bad_string);
			break;
		}
	}
	if (!kicked)
	{
		for (auto &bad_regex : bad_playername_settings.bad_regexs)
		{
			std::regex expression(bad_regex);
			if(std::regex_search(player_name, expression))
			{
				kicked = true;
				sendCommand("kick " + player_number + " " + bad_playername_settings.kick_message);
				logger->info("RCon: Kicked Playername: {0} Regrex: {1}", player_name, bad_regex);
				break;
			}
		}
	}
}


void Rcon::checkWhitelistedPlayer(std::string &player_number, std::string &player_name, std::string &player_guid, bool &kicked)
{
	bool whitelisted_player = false;
	{
		std::lock_guard<std::mutex> lock(reserved_slots_mutex);

		// Checking vector for Whitelisted GUID
		if (std::find(whitelist_settings.whitelisted_guids.begin(), whitelist_settings.whitelisted_guids.end(), player_guid) != whitelist_settings.whitelisted_guids.end())
		{
			// Whitelisted Player - Unordered_map
			whitelisted_player = true;
			whitelist_settings.players_whitelisted[std::move(player_guid)] = std::move(player_name);
		}
		// Checking database for Whitelisted GUID
		else if (!whitelisted_player && whitelist_settings.connected_database)
		{
			bool status = false;
			bool error = false;

			whitelist_statement->bindClear();
			*whitelist_statement.get(), Poco::Data::Keywords::use(player_guid);
			whitelist_statement->bindFixup();
			checkDatabase(status, error);

			if (error)
			{
				// Whitelisted Player - DB, error occured during Database Check
				if (whitelist_settings.kick_on_failed_sql_query)
				{
					logger->info("RCon: Database Player Check error occurred, kicking player");
					sendCommand("kick " + player_number + " " + whitelist_settings.kick_message);
					kicked = true;
				}
				else
				{
					logger->info("RCon: Database Player Check error occurred, will assume player is whitelisted");
					whitelisted_player = true;
					whitelist_settings.players_whitelisted[std::move(player_guid)] = std::move(player_name);
				}
			}
			else if (status)
			{
				// Whitelisted Player - DB
				whitelisted_player = true;
				whitelist_settings.players_whitelisted[std::move(player_guid)] = std::move(player_name);
			}
		}

		if (!whitelisted_player)
		{
			// NON-WHITELISTED PLAYER
			if ((whitelist_settings.players_non_whitelisted.size() + whitelist_settings.players_whitelisted.size()) <= whitelist_settings.open_slots)
			{
				whitelist_settings.players_non_whitelisted[std::move(player_guid)] = std::move(player_name);
			}
			else
			{
				logger->info("RCon: Kicked Playername: {0} GUID: {1}  Not Whitelisted", player_name, player_guid);
				sendCommand("kick " + player_number + " " + whitelist_settings.kick_message);
				kicked = true;
			}
		}
	}
}



void Rcon::chatMessage(std::size_t &bytes_received)
{
	// Received Chat Messages
	std::string result;
	extractData(bytes_received, 9, result);
	logger->info("CHAT: {0}", result);

	// Respond to Server Msgs i.e chat messages, to prevent timeout
	RconPacket rcon_packet;
	rcon_packet.packetCode = 0x02;
	rcon_packet.cmd_char_workaround = rcon_socket.recv_buffer[8];
	sendPacket(rcon_packet);

	//boost::algorithm::trim(result);
	if (bad_playername_settings.enable || whitelist_settings.enable)
	{
		if (boost::algorithm::starts_with(result, "Player #"))
		{
			if (boost::algorithm::ends_with(result, " connected")) //Whitespace so doesn't pickup on disconnect
			{
				if (bad_playername_settings.enable)
				{
					result = result.substr(8);
					const std::string::size_type found = result.find(" ");
					std::string player_number = result.substr(0, found);
					const std::string::size_type found2 = result.find_last_of("(");
					std::string player_name = result.substr(found+1, found2-(found+1));

					#if defined(RCON_APP) || (DEBUG_TESTING)
						logger->info("DEBUG Connected Player Number: {0}.", player_number);
						logger->info("DEBUG Connected Player Name: {0}.", player_name);
					#endif

					bool kicked = false;
					checkBadPlayerString(player_number, player_name, kicked);
				}
			}
			else if (boost::algorithm::ends_with(result, "disconnected"))
			{
				auto pos = result.find(" ", result.find("#"));
				std::string player_name = result.substr(pos + 1, result.size() - (pos + 14));

				if (whitelist_settings.enable)
				{
					std::lock_guard<std::mutex> lock(reserved_slots_mutex);
					whitelist_settings.players_whitelisted.erase(players_name_beguid[player_name]);
					whitelist_settings.players_non_whitelisted.erase(players_name_beguid[player_name]);
				}

				#if defined(RCON_APP) || (DEBUG_TESTING)
					logger->info("DEBUG Disconnected Player Name: {0}.", player_name);
				#endif

				#ifndef RCON_APP
					if (rcon_settings.generate_unique_id)
					{
						// We only bother to generate a key if player has not been kicked
						extension_ptr->delPlayerKey_delayed(players_name_beguid[player_name]);
					}
				#endif

				// REMOVE PLAYER BEGUID
				{
					std::lock_guard<std::recursive_mutex> lock(players_name_beguid_mutex);
					players_name_beguid.erase(player_name);
				}
			}
		}
		else if (boost::algorithm::starts_with(result, "Verified GUID"))
		{
			auto pos_1 = result.find("(");
			auto pos_2 = result.find(")", pos_1);

			std::string player_guid = result.substr((pos_1 + 1), (pos_2 - (pos_1 + 1)));

			pos_1 = result.find("#");
			pos_2 = result.find(" ", pos_1);
			std::string player_number = result.substr((pos_1 + 1), (pos_2 - (pos_1 + 1)));
			std::string player_name = result.substr(pos_2);

			#if defined(RCON_APP) || (DEBUG_TESTING)
				logger->info("DEBUG Verified Player Number: {0}.", player_number);
				logger->info("DEBUG Verified Player Name: {0}.", player_name);
				logger->info("DEBUG Verified Player GUID: {0}.", player_guid);
			#endif

			// ADD PLAYER BEGUID
			{
				std::lock_guard<std::recursive_mutex> lock(players_name_beguid_mutex);
				players_name_beguid[player_name] = player_guid;
			}

			bool kicked = false;
			if (whitelist_settings.enable)
			{
				checkWhitelistedPlayer(player_number, player_name, player_guid, kicked);
			}

			#ifndef RCON_APP
				if ((!kicked) && rcon_settings.generate_unique_id)
				{
					// We only bother to generate a key if player has not been kicked
					extension_ptr->createPlayerKey_mutexlock(player_guid, 10); // TODO Make this configureable
				}
			#endif
		}
	}
	startReceive();
}


void Rcon::extractData(std::size_t &bytes_received, int pos, std::string &result)
{
	std::stringstream ss;
	for (size_t i = pos; i < bytes_received; ++i)
	{
		ss << rcon_socket.recv_buffer[i];
	}
	result = ss.str();
}


void Rcon::createKeepAlive(const boost::system::error_code& error)
{
	if (!error)
	{
		std::ostringstream cmdStream;
		cmdStream.put(0xFFu);
		cmdStream.put(0x01);
		//cmdStream.put(getSequenceNum(rcon_socket));
		cmdStream.put(0x00);
		cmdStream.put('\0');

		std::string cmd = cmdStream.str();
		rcon_socket.keep_alive_crc32.reset();
		rcon_socket.keep_alive_crc32.process_bytes(cmd.data(), cmd.length());
		long int crcVal = rcon_socket.keep_alive_crc32.checksum();

		std::stringstream hexStream;
		hexStream << std::setfill('0') << std::setw(sizeof(int)*2);
		hexStream << std::hex << crcVal;
		std::string crcAsHex = hexStream.str();

		unsigned char reversedCrc[4];
		unsigned int x;

		std::stringstream converterStream;
		for (int i = 0; i < 4; i++)
		{
			converterStream << std::hex << crcAsHex.substr(6-(2*i),2).c_str();
			converterStream >> x;
			converterStream.clear();
			reversedCrc[i] = x;
		}

		// Create Packet
		std::stringstream cmdPacketStream;
		cmdPacketStream.put(0x42); // B
		cmdPacketStream.put(0x45); // E
		cmdPacketStream.put(reversedCrc[0]); // 4-byte Checksum
		cmdPacketStream.put(reversedCrc[1]);
		cmdPacketStream.put(reversedCrc[2]);
		cmdPacketStream.put(reversedCrc[3]);
		cmdPacketStream << cmd;
		cmdPacketStream.str();

		std::shared_ptr<std::string> packet;
		packet.reset(new std::string(cmdPacketStream.str()));

		rcon_socket.socket->async_send(boost::asio::buffer(*packet),
								boost::bind(&Rcon::handleSent, this, packet,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred));
		timerKeepAlive(30);
	}
	else
	{
		logger->warn("RCon: Keepalive Error: {0}", error.message());
	}
}


void Rcon::sendPacket(RconPacket &rcon_packet)
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);

	if (rcon_packet.packetCode == 0x01) //Everything else
	{
		//cmdStream.put(getSequenceNum(rcon_socket));
		cmdStream.put(0x00);
		cmdStream << rcon_packet.cmd;
	}
	else if (rcon_packet.packetCode == 0x02) //Respond to Chat Messages
	{
		cmdStream.put(rcon_packet.cmd_char_workaround);
	}
	else if (rcon_packet.packetCode == 0x00) //Login
	{
		logger->info("Rcon: Sending Login Packet");
		cmdStream << rcon_packet.cmd;
	}

	std::string cmd = cmdStream.str();
	boost::crc_32_type crc32;
	crc32.process_bytes(cmd.data(), cmd.length());
	long int crcVal = crc32.checksum();

	std::stringstream hexStream;
	hexStream << std::setfill('0') << std::setw(sizeof(int)*2);
	hexStream << std::hex << crcVal;
	std::string crcAsHex = hexStream.str();

	unsigned char reversedCrc[4];
	unsigned int x;

	std::stringstream converterStream;
	for (int i = 0; i < 4; i++)
	{
		converterStream << std::hex << crcAsHex.substr(6-(2*i),2).c_str();
		converterStream >> x;
		converterStream.clear();
		reversedCrc[i] = x;
	}

	// Create Packet
	std::stringstream cmdPacketStream;
	cmdPacketStream.put(0x42); // B
	cmdPacketStream.put(0x45); // E
	cmdPacketStream.put(reversedCrc[0]); // 4-byte Checksum
	cmdPacketStream.put(reversedCrc[1]);
	cmdPacketStream.put(reversedCrc[2]);
	cmdPacketStream.put(reversedCrc[3]);
	cmdPacketStream << cmd;

	std::shared_ptr<std::string> packet;
	packet.reset( new std::string(cmdPacketStream.str()) );

	rcon_socket.socket->async_send(boost::asio::buffer(*packet),
							boost::bind(&Rcon::handleSent, this, packet,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::sendBanPacket(RconPacket &rcon_packet)
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);

	if (rcon_packet.packetCode == 0x01) //Everything else
	{
		//cmdStream.put(getSequenceNum(rcon_socket));
		cmdStream.put(0x00);
		cmdStream << rcon_packet.cmd;
	}
	else if (rcon_packet.packetCode == 0x02) //Respond to Chat Messages
	{
		cmdStream.put(rcon_packet.cmd_char_workaround);
	}
	else if (rcon_packet.packetCode == 0x00) //Login
	{
		logger->info("Rcon: Sending Ban Packet");
		cmdStream << rcon_packet.cmd;
	}

	std::string cmd = cmdStream.str();
	boost::crc_32_type crc32;
	crc32.process_bytes(cmd.data(), cmd.length());
	long int crcVal = crc32.checksum();

	std::stringstream hexStream;
	hexStream << std::setfill('0') << std::setw(sizeof(int)*2);
	hexStream << std::hex << crcVal;
	std::string crcAsHex = hexStream.str();

	unsigned char reversedCrc[4];
	unsigned int x;

	std::stringstream converterStream;
	for (int i = 0; i < 4; i++)
	{
		converterStream << std::hex << crcAsHex.substr(6-(2*i),2).c_str();
		converterStream >> x;
		converterStream.clear();
		reversedCrc[i] = x;
	}

	// Create Packet
	std::stringstream cmdPacketStream;
	cmdPacketStream.put(0x42); // B
	cmdPacketStream.put(0x45); // E
	cmdPacketStream.put(reversedCrc[0]); // 4-byte Checksum
	cmdPacketStream.put(reversedCrc[1]);
	cmdPacketStream.put(reversedCrc[2]);
	cmdPacketStream.put(reversedCrc[3]);
	cmdPacketStream << cmd;

	std::shared_ptr<std::string> packet;
	packet.reset( new std::string(cmdPacketStream.str()) );

	rcon_socket.socket->async_send(boost::asio::buffer(*packet),
							boost::bind(&Rcon::handleBanSent, this, packet,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::handleBanSent(std::shared_ptr<std::string> packet, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (error)
	{
		logger->warn("Rcon: Error handleBanSent: {0}", error.message());
	}
	else
	{
		sendCommand("writeBans");
		sendCommand("loadBans");
	}
}


void Rcon::handleSent(std::shared_ptr<std::string> packet, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (error)
	{
		logger->warn("Rcon: Error handleSent: {0}", error.message());
	}
}


void Rcon::sendCommand(std::string command)
{
	logger->info("Rcon: sendCommand: {0}", command);

	RconPacket rcon_packet;
	char *cmd = new char[command.size() + 1];
	std::strcpy(cmd, command.c_str());
	rcon_packet.cmd = cmd;
	rcon_packet.packetCode = 0x01;

	sendPacket(rcon_packet);
	delete []rcon_packet.cmd;
}


void Rcon::getMissions(unsigned int &unique_id)
{
	std::string command = "missions";
	logger->info("Rcon: getMissions");

	RconPacket rcon_packet;
	char *cmd = new char[command.size() + 1];
	std::strcpy(cmd, command.c_str());
	rcon_packet.cmd = cmd;
	rcon_packet.packetCode = 0x01;

	sendPacket(rcon_packet);
	{
		std::lock_guard<std::mutex> lock(rcon_socket.mutex_mission_requests);
		rcon_socket.mission_requests.push_back(unique_id);
	}
	delete []rcon_packet.cmd;
}


void Rcon::addBan(std::string command)
{
	logger->info("Rcon: addBan: {0}", command);

	RconPacket rcon_packet;
	char *cmd = new char[command.size() + 1];
	std::strcpy(cmd, command.c_str());
	rcon_packet.cmd = cmd;
	rcon_packet.packetCode = 0x01;

	sendBanPacket(rcon_packet);
	delete []rcon_packet.cmd;
}


void Rcon::getPlayers(unsigned int &unique_id)
{
	std::string command = "players";
	logger->info("Rcon: getPlayers");

	RconPacket rcon_packet;
	char *cmd = new char[command.size() + 1];
	std::strcpy(cmd, command.c_str());
	rcon_packet.cmd = cmd;
	rcon_packet.packetCode = 0x01;

	sendPacket(rcon_packet);
	{
		std::lock_guard<std::mutex> lock(rcon_socket.mutex_players_requests);
		rcon_socket.player_requests.push_back(unique_id);
	}
	delete []rcon_packet.cmd;
}


void Rcon::connectDatabase(Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf)
// Connection to Database, database_id used when connecting to multiple different database.
{
	if (pConf->hasOption(whitelist_settings.database + ".Type"))
	{
		std::string db_type = pConf->getString(whitelist_settings.database + ".Type");
		logger->info("Rcon: Database Type: {0}", db_type);
		if ((boost::algorithm::iequals(db_type, std::string("MySQL")) == 1) || (boost::algorithm::iequals(db_type, "SQLite") == 1))
		{
			try
			{
				// Database Type
				std::string connection_str;
				if (boost::algorithm::iequals(db_type, std::string("MySQL")) == 1)
				{
					db_type = "MySQL";
					#ifdef RCON_APP
						if (!(DB_connectors_info.mysql))
						{
							Poco::Data::MySQL::Connector::registerConnector();
							DB_connectors_info.mysql = true;
						}
					#else
						if (!(extension_ptr->ext_connectors_info.mysql))
						{
							Poco::Data::MySQL::Connector::registerConnector();
							extension_ptr->ext_connectors_info.mysql = true;
						}
					#endif
					connection_str += "host=" + pConf->getString(whitelist_settings.database + ".IP") + ";";
					connection_str += "port=" + pConf->getString(whitelist_settings.database + ".Port") + ";";
					connection_str += "user=" + pConf->getString(whitelist_settings.database + ".Username") + ";";
					connection_str += "password=" + pConf->getString(whitelist_settings.database + ".Password") + ";";
					connection_str += "db=" + pConf->getString(whitelist_settings.database + ".Name") + ";";
					connection_str += "auto-reconnect=true";

					if (pConf->getBool(whitelist_settings.database + ".Compress", false))
					{
						connection_str += ";compress=true";
					}
					if (pConf->getBool(whitelist_settings.database + ".Secure Auth", false))
					{
						connection_str += ";secure-auth=true";
					}
				}
				else if (boost::algorithm::iequals(db_type, "SQLite") == 1)
				{
					db_type = "SQLite";
					#ifdef RCON_APP
						if (!(DB_connectors_info.sqlite))
						{
							Poco::Data::SQLite::Connector::registerConnector();
							DB_connectors_info.sqlite = true;
						}
						boost::filesystem::path sqlite_path(boost::filesystem::current_path());
					#else
						if (!(extension_ptr->ext_connectors_info.sqlite))
						{
							Poco::Data::SQLite::Connector::registerConnector();
							extension_ptr->ext_connectors_info.sqlite = true;
						}
						boost::filesystem::path sqlite_path(extension_ptr->ext_info.path);
					#endif
					sqlite_path /= "extDB";
					sqlite_path /= "sqlite";
					sqlite_path /= pConf->getString(whitelist_settings.database + ".Name");
					connection_str = sqlite_path.make_preferred().string();
				}

				// Session
				whitelist_session.reset(new Poco::Data::Session(db_type, connection_str));
				if (whitelist_session->isConnected())
				{
					logger->info("Rcon: Database Session Started");
					whitelist_statement.reset(new Poco::Data::Statement(*whitelist_session.get()));
					*whitelist_statement.get() << whitelist_settings.sql_statement;
					whitelist_settings.connected_database = true;
				}
				else
				{
					logger->warn("Rcon: Database Session Failed");
					whitelist_settings.connected_database = false;
				}
			}
			catch (Poco::Data::NotConnectedException& e)
			{
				logger->error("Rcon: Database NotConnectedException Error: {0}", e.displayText());
				whitelist_settings.connected_database = false;
			}
			catch (Poco::Data::MySQL::ConnectionException& e)
			{
				logger->error("Rcon: Database ConnectionException Error: {0}", e.displayText());
				whitelist_settings.connected_database = false;
			}
			catch (Poco::Exception& e)
			{
				logger->error("Rcon: Database Exception Error: {0}", e.displayText());
				whitelist_settings.connected_database = false;
			}
		}
		else
		{
			logger->warn("Rcon: No Database Engine Found for {0}", db_type);
			whitelist_settings.connected_database = false;
		}
	}
	else
	{
		logger->warn("Rcon: No Database Config Option Found: {0}", whitelist_settings.database);
		whitelist_settings.connected_database = false;
	}

	if (!(whitelist_settings.connected_database))
	{
		whitelist_statement.release();
		whitelist_session.release();
	}
}


void Rcon::checkDatabase(bool &status, bool &error)
{
	try
	{
		logger->info("Rcon: checking Database");
		whitelist_statement->execute();
		Poco::Data::RecordSet rs(*whitelist_statement.get());
		if (rs.columnCount() == 1)
		{
			rs.moveFirst();
			if (rs[0].isInteger())
			{
				if (rs[0].convert<int>() > 0)
				{
					logger->info("Rcon: checking Database: True");
					status = true;
				}
				else
				{
					logger->info("Rcon: checking Database: False");
					status = false;
				}
			}
		}
	}
	catch (Poco::InvalidAccessException& e)
	{
		error = false;
		logger->error("Rcon: Error NotConnectedException: {0}", e.displayText());
	}
	catch (Poco::Data::NotConnectedException& e)
	{
		error = false;
		logger->error("Rcon: Error NotConnectedException: {0}", e.displayText());
	}
	catch (Poco::NotImplementedException& e)
	{
		error = false;
		logger->error("Rcon: Error NotImplementedException: {0}", e.displayText());
	}
	catch (Poco::Data::SQLite::DBLockedException& e)
	{
		error = false;
		logger->error("Rcon: Error DBLockedException: {0}", e.displayText());
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		error = false;
		logger->error("Rcon: Error ConnectionException: {0}", e.displayText());
	}
	catch(Poco::Data::MySQL::StatementException& e)
	{
		error = false;
		logger->error("Rcon: Error StatementException: {0}", e.displayText());
	}
	catch (Poco::Data::ConnectionFailedException& e)
	{
		error = false;
		logger->error("Rcon: Error ConnectionFailedException: {0}", e.displayText());
	}
	catch (Poco::Data::DataException& e)
	{
		error = false;
		logger->error("Rcon: Error DataException: {0}", e.displayText());
	}
	catch (Poco::Exception& e)
	{
		error = false;
		logger->error("Rcon: Error Exception: {0}", e.displayText());
	}
}


#ifdef RCON_APP

	int main(int nNumberofArgs, char* pszArgs[])
	{
		auto console = spdlog::stdout_logger_mt("extDB Console logger");

		boost::program_options::options_description desc("Options");
		desc.add_options()
			("help", "Print help messages")
			("config_file", boost::program_options::value<std::string>()->required(), "Rcon Config File")
			("config_section", boost::program_options::value<std::string>()->required(), "Rcon Config Section to Use")
			("file", boost::program_options::value<std::string>(), "File to run i.e rcon restart warnings");
		boost::program_options::variables_map options;

		try
		{
			boost::program_options::store(boost::program_options::parse_command_line(nNumberofArgs, pszArgs, desc), options);
			if (options.count("help") )
			{
				console->info("Rcon Command Line, originally based off bercon by Prithu \"bladez\" Parker");
				console->info("\t\t @ https://github.com/bladez-/bercon");
				console->info("");
				console->info("");
				console->info("Almost Completely Rewritten for extDB2 + crossplatform by Torndeco");
				console->info("\t\t @ https://github.com/Torndeco/extDB2");
				console->info("");
				console->info("Run File Option is just for parsing rcon commands to be ran, i.e server restart warnings");
				console->info("\t\t The file is just a text file with rcon commands, empty line = wait 1 second");
				console->info("\t\t Useful option for restart warnings i.e run the program using a cron job / script");
				console->info("");
				return 0;
			}
			boost::program_options::notify(options);
		}
		catch(boost::program_options::error& e)
		{
			console->error("Error: {0}", e.what());
			console->error("Error: Description {0}", desc);
			return 1;
		}

		boost::filesystem::path config_file_path(options["config_file"].as<std::string>());
		if (boost::filesystem::is_regular_file(config_file_path))
		{
			boost::asio::io_service io_service;
			boost::thread_group threads;
			boost::asio::io_service::work io_work(io_service);
			for (int i = 0; i < 2; ++i)
			{
				threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
			}
			Rcon rcon(io_service, console);

			std::string conf_section = options["config_section"].as<std::string>();
			Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf(new Poco::Util::IniFileConfiguration(config_file_path.make_preferred().string()));
			if (pConf->hasOption(conf_section + ".Password"))
			{
				boost::filesystem::path config_file_path(options["config_file"].as<std::string>());
				config_file_path /= "rcon-conf.ini";

				Rcon::RconSettings rcon_settings;
				rcon_settings.address = pConf->getString(conf_section + ".ip", "127.0.0.1");;
				rcon_settings.port = pConf->getInt(conf_section + ".port", 2302);;
				rcon_settings.password = pConf->getString(conf_section + ".password", "password");;

				Rcon::BadPlayernameSettings bad_playername_settings;
				Rcon::WhitelistSettings whitelist_settings;

				if (options.count("file") == 0)
				{
					bad_playername_settings.enable = pConf->getBool((conf_section + ".Bad Playername Enable"), false);
					if (bad_playername_settings.enable)
					{
						console->info("extDB2: RCon Bad Playername Enabled");

						bad_playername_settings.kick_message = pConf->getString(((conf_section) + ".Bad Playername Kick Message"), "");

						bad_playername_settings.bad_strings.push_back(":");
						Poco::StringTokenizer tokens(pConf->getString(((conf_section) + ".Bad Playername Strings"), ""), ":");
						for (auto &token : tokens)
						{
							bad_playername_settings.bad_strings.push_back(token);
						}

						int regrex_rule_num = 0;
						std::string regex_rule_num_str;
						while (true)
						{
							++regrex_rule_num;
							regex_rule_num_str = ".Bad Playername Regrex_" + Poco::NumberFormatter::format(regrex_rule_num);
							if (!(pConf->has(conf_section + regex_rule_num_str)))
							{
								break;
							}
							else
							{
								bad_playername_settings.bad_regexs.push_back(pConf->getString(conf_section + regex_rule_num_str));
							}
						}
					}

					// Reserved Slots
					whitelist_settings.enable = pConf->getBool((conf_section + ".Whitelist Enable"), false);
					if (whitelist_settings.enable)
					{
						whitelist_settings.open_slots = pConf->getInt((conf_section + ".Whitelist Public Slots"), 0);
						whitelist_settings.database = pConf->getString((conf_section + ".Whitelist Database"), "");
						whitelist_settings.kick_message = pConf->getString(((conf_section) + ".Whitelist Kick Message"), "");

						whitelist_settings.sql_statement = pConf->getString((conf_section + ".Whitelist SQL Prepared Statement"), "");
						whitelist_settings.kick_on_failed_sql_query = pConf->getBool((conf_section + ".Whitelist Kick on SQL Query Failed"), false);

						Poco::StringTokenizer tokens(pConf->getString((conf_section + ".Whitelist BEGuids"), ""), ":", Poco::StringTokenizer::TOK_TRIM);
						for (auto &token : tokens)
						{
							whitelist_settings.whitelisted_guids.push_back(token);
						}
					}
				}
				rcon.start(rcon_settings, bad_playername_settings, whitelist_settings, pConf);

				if (options.count("file"))
				{
					std::ifstream fin(options["file"].as<std::string>());
					if (fin.is_open() == false)
					{
						console->warn("Error: File is already Opened");
						return 1;
					}
					else
					{
						console->info("File is OK");
					}

					std::string line;
					while (std::getline(fin, line))
					{
						console->info("{0}", line);
						if (line.empty())
						{
							boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
						}
						else
						{
							rcon.sendCommand(line);
						}
					}
					console->info("OK");
					rcon.disconnect();
					return 0;
				}
				else
				{
					console->info("**********************************");
					console->info("**********************************");
					console->info("To talk type ");
					console->info("SAY -1 Server Restart in 10 mins");
					console->info();
					console->info("To see all players type");
					console->info("players");
					console->info("**********************************");
					console->info("**********************************");
					console->info();

					std::string input_str;
					unsigned int unique_id = 1;
					for (;;) {
						std::getline(std::cin, input_str);
						if (boost::algorithm::iequals(input_str,"quit") == 1)
						{
							console->info("Quitting Please Wait");
							rcon.disconnect();
							break;
						}
						else if (boost::algorithm::istarts_with(input_str,"players"))
						{
							rcon.getPlayers(unique_id);
						}
						else if (boost::algorithm::istarts_with(input_str,"missions"))
						{
							rcon.getMissions(unique_id);
						}
						else if (boost::algorithm::istarts_with(input_str,"addban"))
						{
							rcon.addBan(input_str);
						}
						else if (boost::algorithm::istarts_with(input_str,"ban"))
						{
							rcon.addBan(input_str);
						}
						else
						{
							rcon.sendCommand(input_str);
						}
					}
					console->info("Quitting");
					return 0;
				}
			}
		}
	}
#endif