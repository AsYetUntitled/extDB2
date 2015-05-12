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


Rcon::Rcon(boost::asio::io_service& io_service, std::shared_ptr<spdlog::logger> spdlog)
{
	active_socket = new std::atomic<int>(1);
	
	rcon_socket_1.id = 1;
	rcon_socket_2.id = 2;

	rcon_socket_1.rcon_run_flag = new std::atomic<bool>(false);
	rcon_socket_2.rcon_run_flag = new std::atomic<bool>(false);

	rcon_socket_1.rcon_login_flag = new std::atomic<bool>(false);
	rcon_socket_2.rcon_login_flag = new std::atomic<bool>(false);

	rcon_socket_1.socket.reset(new boost::asio::ip::udp::socket(io_service));
	rcon_socket_2.socket.reset(new boost::asio::ip::udp::socket(io_service));

	rcon_socket_1.sequence_num_counter = 0;
	rcon_socket_2.sequence_num_counter = 0;

	rcon_socket_1.rcon_msg_cache.reset(new Poco::ExpireCache<unsigned char, RconMultiPartMsg>(120000));
	rcon_socket_2.rcon_msg_cache.reset(new Poco::ExpireCache<unsigned char, RconMultiPartMsg>(120000));
}


Rcon::~Rcon(void)
{
}


#ifndef RCON_APP
	void Rcon::extInit(AbstractExt *extension)
	{
		extension_ptr = extension;
	}
#endif


unsigned char Rcon::getSequenceNum(RconSocket &rcon_socket)
{
	std::lock_guard<std::mutex> lock(rcon_socket.mutex_sequence_num_counter);

	if (rcon_socket.sequence_num_counter > 200)
	{
		if (rcon_socket.sequence_num_counter < 255)
		{
			++(rcon_socket.sequence_num_counter);
		}

		*(rcon_socket.rcon_run_flag) = false;
		// TODO Delayed Post to close socket
		if (rcon_socket.id == 1)
		{
			connect(rcon_socket_1);
		}
		else
		{
			connect(rcon_socket_2);
		}
	}
	else
	{
		++(rcon_socket.sequence_num_counter);
	}
	return rcon_socket.sequence_num_counter;
}


unsigned char Rcon::resetSequenceNum(RconSocket &rcon_socket)
{
	std::lock_guard<std::mutex> lock(rcon_socket.mutex_sequence_num_counter);
	rcon_socket.sequence_num_counter = 0;
	return rcon_socket.sequence_num_counter;
}


void Rcon::start(std::string &address, unsigned int port, std::string &password)
{
	boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(address), port);
	rcon_socket_1.socket->async_connect(endpoint, boost::bind(&Rcon::connectionHandler, this, std::ref(rcon_socket_1), boost::asio::placeholders::error));

	delete rcon_password;
	rcon_password = new char[password.size() + 1];
	startReceive(rcon_socket_1);
}


void Rcon::connectionHandler(RconSocket &rcon_socket, const boost::system::error_code& error)
{
	if (!error)
	{
		connect(rcon_socket);
	}
	else
	{
		logger->info("Rcon: UDP Socket Connection Error");
		disconnect();
	}
}


void Rcon::connect(RconSocket &rcon_socket)
{
	*(rcon_socket.rcon_login_flag) = false;
	*(rcon_socket.rcon_run_flag) = true;

	// Login Packet
	RconPacket rcon_packet;
	rcon_packet.cmd = rcon_password;
	rcon_packet.packetCode = 0x00;
	sendPacket(rcon_socket, rcon_packet);
	logger->info("Rcon: Sent Login Info");
}


void Rcon::disconnect()
{
	*(rcon_socket_1.rcon_run_flag) = false;
	*(rcon_socket_2.rcon_run_flag) = false;
	// TODO Delay Timer to close socket
}


bool Rcon::status()
{
	return (*(rcon_socket_1.rcon_run_flag) && (rcon_socket_1.rcon_login_flag)) || (*(rcon_socket_2.rcon_run_flag) && (rcon_socket_2.rcon_login_flag));
}



void Rcon::startReceive(RconSocket &rcon_socket)
{
	rcon_socket.socket->async_receive(
		boost::asio::buffer(rcon_socket.recv_buffer),
		boost::bind(&Rcon::handleReceive, this, std::ref(rcon_socket),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}


void Rcon::handleReceive(RconSocket &rcon_socket, const boost::system::error_code& error, std::size_t bytes_received)
{
	if (!error)
	{
		logger->info("Rcon: receiveBytes");
		rcon_socket.recv_buffer[bytes_received] = '\0';

		switch(rcon_socket.recv_buffer[7])
		{
			case 0x00:
				loginResponse(rcon_socket);
				break;
			case 0x01:
				serverResponse(rcon_socket, bytes_received);
				break;
			case 0x02:
				chatMessage(rcon_socket, bytes_received);
				break;
		};
		startReceive(rcon_socket);
	}
	else
	{
		logger->info("Rcon: UDP handleReceive Error: {0}", error);
		startReceive(rcon_socket);
	}
}


void Rcon::loginResponse(RconSocket &rcon_socket)
{
	if (rcon_socket.recv_buffer[8] == 0x01)
	{
		*(rcon_socket.rcon_login_flag) = true;
		resetSequenceNum(rcon_socket);

		logger->info("Rcon: Login Success");
		*active_socket = rcon_socket.id;
	}
	else
	{
		*(rcon_socket.rcon_login_flag) = false;
		disconnect();

		logger->info("Rcon: Login Failed");
	}
}


void Rcon::serverResponse(RconSocket &rcon_socket, std::size_t &bytes_received)
{
	// Rcon Server Ack Message Received
	unsigned char sequenceNum = rcon_socket.recv_buffer[8];
	logger->warn("Rcon: ACK: {0}", sequenceNum);	

	if (!((rcon_socket.recv_buffer[9] == 0x00) && (bytes_received > 9)))
	{
		// Server Received Command Msg
		std::string result;
		extractData(rcon_socket, bytes_received, 9, result);
		//processMessage(sequenceNum, result);
	}
	else
	{
		// Rcon Multi-Part Message Recieved
		int numPackets = rcon_socket.recv_buffer[10];
		int packetNum = rcon_socket.recv_buffer[11];
			
		std::string partial_msg;
		extractData(rcon_socket, bytes_received, 12, partial_msg);
				
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
				//processMessage(sequenceNum, result);
				rcon_socket.rcon_msg_cache->remove(sequenceNum);
			}
		}
	}
}


void Rcon::chatMessage(RconSocket &rcon_socket, std::size_t &bytes_received)
{
	// Received Chat Messages
	std::string result;
	extractData(rcon_socket, bytes_received, 9, result);
	logger->info("CHAT: {0}", result);
	
	// Respond to Server Msgs i.e chat messages, to prevent timeout
	RconPacket rcon_packet;
	rcon_packet.packetCode = 0x02;
	rcon_packet.cmd_char_workaround = rcon_socket.recv_buffer[8];
	sendPacket(rcon_socket, rcon_packet);
}


void Rcon::extractData(RconSocket &rcon_socket, std::size_t &bytes_received, int pos, std::string &result)
{
	std::stringstream ss;
	for (size_t i = pos; i < bytes_received; ++i)
	{
		ss << rcon_socket.recv_buffer[i];
	}
	result = ss.str();
}


void Rcon::createKeepAlive(RconSocket &rcon_socket)
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(0x01);
	cmdStream.put(getSequenceNum(rcon_socket)); // Seq Number    unsigned char + 1
	cmdStream.put('\0');

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
	cmdPacketStream.str();

	std::shared_ptr<std::string> packet;
	packet.reset(new std::string(cmdPacketStream.str()));

	rcon_socket.socket->async_send(boost::asio::buffer(*packet),
							boost::bind(&Rcon::handleSent, this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::sendPacket(RconSocket &rcon_socket, RconPacket &rcon_packet)
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);
	
	if (rcon_packet.packetCode == 0x01) //Everything else
	{
		cmdStream.put(getSequenceNum(rcon_socket));
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
							boost::bind(&Rcon::handleSent, this, 
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
}


void Rcon::handleSent(const boost::system::error_code&,	std::size_t bytes_transferred)
{
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

	}
}