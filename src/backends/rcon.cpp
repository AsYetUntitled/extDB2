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

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/crc.hpp>

#include <Poco/AbstractCache.h>
#include <Poco/ExpireCache.h>
#include <Poco/SharedPtr.h>
#include <Poco/StringTokenizer.h>

#include <Poco/Stopwatch.h>

#include <Poco/Exception.h>


Rcon::~Rcon(void)
{
}

void Rcon::init()
{
	rcon_run_flag = new std::atomic<bool>(false);
	rcon_login_flag = new std::atomic<bool>(false);
	rcon_msg_cache.reset(new Poco::ExpireCache<unsigned char, RconMultiPartMsg>(120000));
}


#ifndef RCON_APP
	void Rcon::extInit(AbstractExt *extension)
	{
		extension_ptr = extension;
	}
#endif


unsigned char Rcon::getSequenceNum()
{
	std::lock_guard<std::mutex> lock(mutex_sequence_num_counter);
	sequence_num_counter = sequence_num_counter + 1;
	// CHECK FOR > 200 Number  Reset RCon Connection
	return sequence_num_counter;
}


unsigned char Rcon::resetSequenceNum()
{
	std::lock_guard<std::mutex> lock(mutex_sequence_num_counter);
	sequence_num_counter = 0;
	// CHECK FOR > 200 Number  Reset RCon Connection
	return sequence_num_counter;
}


void Rcon::connectionHandler(const boost::system::error_code& error)
{
	if (!error)
	{
		//LOGIN
		//START
	}
	else
	{
		//ERROR
	}
}


void Rcon::updateLogin(std::string &address, unsigned int port, std::string &password)
{
	boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(address), port);
	socket_.async_connect(endpoint, boost::bind(&Rcon::connectionHandler, this, boost::asio::placeholders::error));

	delete rcon_password;
	rcon_password = new char[password.size() + 1]; 
}


void Rcon::startReceive()
{
	socket_.async_receive(
		boost::asio::buffer(recv_buffer_),
		boost::bind(&Rcon::handleReceive, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}


void Rcon::loginResponse()
{
	if (recv_buffer_[8] == 0x01)
	{
		logger->warn("Rcon: Logged In");
		*rcon_login_flag = true;
	}
	else
	{
		// Login Failed
//		logger->warn("Rcon: ACK: {0}", sequenceNum);
		*rcon_login_flag = false;
		disconnect();
	}
}


void Rcon::serverResponse(std::size_t &bytes_received)
{
	// Rcon Server Ack Message Received
	unsigned char sequenceNum = recv_buffer_[8];
	logger->warn("Rcon: ACK: {0}", sequenceNum);	

	if (!((recv_buffer_[9] == 0x00) && (bytes_received > 9)))
	{
		// Server Received Command Msg
		std::string result;
		extractData(9, result, bytes_received);
		//processMessage(sequenceNum, result);
	}
	else
	{
		// Rcon Multi-Part Message Recieved
		int numPackets = recv_buffer_[10];
		int packetNum = recv_buffer_[11];
			
		std::string partial_msg;
		extractData(12, partial_msg, bytes_received);
				
		if (!(rcon_msg_cache->has(sequenceNum)))
		{
			// Doesn't have sequenceNum in Buffer
			RconMultiPartMsg rcon_mp_msg;
			rcon_mp_msg.first = 1;
			rcon_msg_cache->add(sequenceNum, rcon_mp_msg);
				
			Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache->get(sequenceNum);
			ptrElem->second[packetNum] = partial_msg;
		}
		else
		{
			// Has sequenceNum in Buffer
			Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache->get(sequenceNum);
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
				//processMessage(sequenceNum, result);
				rcon_msg_cache->remove(sequenceNum);
			}
		}
	}
}


void Rcon::chatMessage(std::size_t &bytes_received)
{
	// Received Chat Messages
	std::string result;
	extractData(9, result, bytes_received);
	logger->info("CHAT: {0}", result);
	
	// Respond to Server Msgs i.e chat messages, to prevent timeout
	RconPacket rcon_packet;
	rcon_packet.packetCode = 0x02;
	rcon_packet.cmd_char_workaround = recv_buffer_[8];
	sendPacket(rcon_packet);
}


void Rcon::handleReceive(const boost::system::error_code& error, std::size_t bytes_received)
{
	if (!error)
	{
		logger->info("Rcon: receiveBytes");
		recv_buffer_[bytes_received] = '\0';

		switch(recv_buffer_[7])
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
		startReceive();
	}
	else
	{
		// TODO Error
		// Reconnect
	}
}


void Rcon::handleSent(const boost::system::error_code&,	std::size_t bytes_transferred)
{
	// TODO
}


void Rcon::connect()
{
	*rcon_login_flag = false;
	*rcon_run_flag = true;

	// Login Packet
	RconPacket rcon_packet;
	rcon_packet.cmd = rcon_password;
	rcon_packet.packetCode = 0x00;
	sendPacket(rcon_packet);
	logger->info("Rcon: Sent Login Info");
}



void Rcon::createKeepAlive()
{
	boost::crc_32_type crc32;
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(0x01);
	cmdStream.put(getSequenceNum()); // Seq Number    unsigned char + 1
	cmdStream.put('\0');

	std::string cmd = cmdStream.str();
	crc32.reset();
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
	cmdPacketStream.str();

	std::shared_ptr<std::string> packet;
	packet.reset(new std::string(cmdPacketStream.str()));

	socket_.async_send(boost::asio::buffer(*packet),
							boost::bind(&Rcon::handleSent, this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::sendPacket(RconPacket &rcon_packet)
{
	boost::crc_32_type crc32;
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);
	
	if (rcon_packet.packetCode == 0x01) //Everything else
	{
		cmdStream.put(getSequenceNum());	
		cmdStream << rcon_packet.cmd;
	}
	else if (rcon_packet.packetCode == 0x02) //Respond to Chat Messages
	{
		cmdStream.put(rcon_packet.cmd_char_workaround);
	}
	else if (rcon_packet.packetCode == 0x00) //Login
	{
		cmdStream << rcon_packet.cmd;
	}

	std::string cmd = cmdStream.str();
	crc32.reset();
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

	socket_.async_send(boost::asio::buffer(*packet),
							boost::bind(&Rcon::handleSent, this, 
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::extractData(int pos, std::string &result, std::size_t &bytes_received)
{
	std::stringstream ss;
	for (size_t i = pos; i < bytes_received; ++i)
	{
		ss << recv_buffer_[i];
	}
	result = ss.str();
}


void Rcon::addCommand(std::string &command)
{
	std::lock_guard<std::mutex> lock(mutex_rcon_commands);
	rcon_commands.push_back(std::move(std::make_pair(0, command)));
}


void Rcon::getMissions(std::string &command, unsigned int &unique_id)
{
	logger->info("Rcon: getMissions called: {0}: unique_id: {1}", command, unique_id);
	#ifndef RCON_APP
		{
			std::lock_guard<std::mutex> lock(mutex_missions_requests);
			missions_requests.push_back(unique_id);
		}
	#endif
	{
		std::lock_guard<std::mutex> lock(mutex_rcon_commands);
		rcon_commands.push_back(std::move(std::make_pair(1, command)));
	}
}


void Rcon::getPlayers(std::string &command, unsigned int &unique_id)
{
	logger->info("Rcon: getPlayers called: {0}: unique_id: {1}", command, unique_id);
	#ifndef RCON_APP
		{
			std::lock_guard<std::mutex> lock(mutex_players_requests);
			players_requests.push_back(unique_id);
		}
	#endif
	{
		std::lock_guard<std::mutex> lock(mutex_rcon_commands);
		rcon_commands.push_back(std::move(std::make_pair(2, command)));
	}
}


void Rcon::disconnect()
{
	*rcon_run_flag = false;	
}


bool Rcon::status()
{
	return (*rcon_run_flag && *rcon_login_flag);
}