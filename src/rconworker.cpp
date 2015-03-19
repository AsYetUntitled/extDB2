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


#include "rconworker.h"

#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef RCON_APP
	#include <boost/program_options.hpp>
	#include <fstream>
#else
	#include "abstract_ext.h"
#endif

#include <boost/crc.hpp>

#include <Poco/AbstractCache.h>
#include <Poco/ExpireCache.h>
#include <Poco/SharedPtr.h>

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/NetException.h>

#include <Poco/NumberFormatter.h>
#include <Poco/Timestamp.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Stopwatch.h>
#include <Poco/Thread.h>

#include <Poco/Exception.h>


void RconWorker::init(std::shared_ptr<spdlog::logger> ext_logger)
{
	rcon_run_flag = new std::atomic<bool>(false);
	rcon_login_flag = new std::atomic<bool>(false);
	logger.swap(ext_logger);
}


void RconWorker::createKeepAlive()
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(0x01);
	cmdStream.put(0x00); // Seq Number
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

	keepAlivePacket = cmdPacketStream.str();
}


void RconWorker::sendPacket()
{
	std::ostringstream cmdStream;
	cmdStream.put(0xFFu);
	cmdStream.put(rcon_packet.packetCode);
	
	if (rcon_packet.packetCode == 0x01)
	{
		cmdStream.put(0x00); // seq number
		cmdStream << rcon_packet.cmd;
	}
	else if (rcon_packet.packetCode == 0x02)
	{
		cmdStream.put(rcon_packet.cmd_char_workaround);
	}
	else if (rcon_packet.packetCode == 0x00)
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

	std::string packet = cmdPacketStream.str();
	dgs.sendBytes(packet.data(), packet.size());
}


void RconWorker::extractData(int pos, std::string &result)
{
	std::stringstream ss;
	for(size_t i = pos; i < buffer_size; ++i)
	{
		ss << buffer[i];
	}
	result = ss.str();
}


void RconWorker::updateLogin(std::string address, int port, std::string password)
{
	createKeepAlive();

	rcon_login.address = address;
	rcon_login.port = port;
	char *passwd = new char[password.size() + 1];
	std::strcpy(passwd, password.c_str());
	rcon_login.password = passwd;	
}


void RconWorker::connect()
{
	*rcon_login_flag = false;
	*rcon_run_flag = true;

	// Login Packet
	rcon_packet.cmd = rcon_login.password;
	rcon_packet.packetCode = 0x00;
	sendPacket();
	logger->info("Rcon: Sent Login Info");
	
	// Reset Timer
	rcon_timer.start();
}


bool RconWorker::status()
{
	return *rcon_login_flag;
}


void RconWorker::run()
{
	Poco::Net::SocketAddress sa(rcon_login.address, rcon_login.port);
	dgs.connect(sa);
	connect();
	mainLoop();
}


void RconWorker::addCommand(std::string command)
{
	if (*rcon_run_flag)
	{
		std::lock_guard<std::mutex> lock(mutex_rcon_commands);
		rcon_commands.push_back(std::move(command));
	}
}


void RconWorker::disconnect()
{
	*rcon_run_flag = false;	
}


void RconWorker::mainLoop()
{
	*rcon_login_flag = false;

	int elapsed_seconds;
	int sequenceNum;

	// 2 Min Cache for UDP Multi-Part Messages
	Poco::ExpireCache<int, RconMultiPartMsg > rcon_msg_cache(120000);
	
	dgs.setReceiveTimeout(Poco::Timespan(5, 0));
	while (true)
	{
		try 
		{
			buffer_size = dgs.receiveBytes(buffer, sizeof(buffer)-1);
			buffer[buffer_size] = '\0';
			rcon_timer.restart();

			if (buffer[7] == 0x00)
			{
				if (buffer[8] == 0x01)
				{
					logger->warn("Rcon: Logged In");
					*rcon_login_flag = true;
				}
				else
				{
					// Login Failed
					logger->warn("Rcon: Failed Login... Disconnecting");
					*rcon_login_flag = false;
					disconnect();
					break;
				}
			}
			else if ((buffer[7] == 0x01) && *rcon_login_flag)
			{
				// Rcon Server Ack Message Received
				sequenceNum = buffer[8];
				
				if (!((buffer[9] == 0x00) && (buffer_size > 9)))
				{
					// Server Received Command Msg
					std::string result;
					extractData(9, result);
					#if defined(RCONWORKER_APP) || (TESTING)
						if (result.empty())
						{
							logger->info("Server Received Command Msg EMPTY");
						}
						else
						{
							logger->info("Server Received Command Msg: {0}", result);
						}
					#endif
				}
				else
				{
					// Rcon Multi-Part Message Recieved
					int numPackets = buffer[10];
					int packetNum = buffer[11];
					
					std::string partial_msg;
					extractData(12, partial_msg);
					
					if (!(rcon_msg_cache.has(sequenceNum)))
					{
						// Doesn't have sequenceNum in Buffer
						RconMultiPartMsg rcon_mp_msg;
						rcon_mp_msg.first = 1;
						rcon_msg_cache.add(sequenceNum, rcon_mp_msg);
						
						Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache.get(sequenceNum);
						ptrElem->second[packetNum] = partial_msg;
					}
					else
					{
						// Has sequenceNum in Buffer
						Poco::SharedPtr<RconMultiPartMsg> ptrElem = rcon_msg_cache.get(sequenceNum);
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
							#if defined(RCONWORKER_APP) || (TESTING)
								logger->info("Info: {0}", result);
							#endif
							rcon_msg_cache.remove(sequenceNum);
						}
					}
				}
			}
			else if (buffer[7] == 0x02)
			{
				if (!*rcon_login_flag)
				{
					// Already Logged In 
					*rcon_login_flag = true;
				}
				else
				{
					// Recieved Chat Messages
					std::string result;
					extractData(9, result);
					#if defined(RCONWORKER_APP) || (TESTING)
						logger->info("CHAT: {0}", result);
					#endif
					
					// Respond to Server Msgs i.e chat messages, to prevent timeout
					rcon_packet.packetCode = 0x02;
					rcon_packet.cmd_char_workaround = buffer[8];
					sendPacket();
				}
			}

			if (*rcon_login_flag)
			{
				// Checking for Commands to Send
				std::lock_guard<std::mutex> lock(mutex_rcon_commands);
				for (auto &rcon_command : rcon_commands)
				{
					char *cmd = new char[rcon_command.size()+1];
					std::strcpy(cmd, rcon_command.c_str());
					
					delete []rcon_packet.cmd;
					rcon_packet.cmd = cmd;
					
					rcon_packet.packetCode = 0x01;
					sendPacket();
				}
				// Clear Comands
				rcon_commands.clear();

				if (!*rcon_run_flag)
				{
					break;
				}
			}
			else
			{
				if (!*rcon_run_flag)
				{
					std::lock_guard<std::mutex> lock(mutex_rcon_commands);
					if (rcon_commands.empty())
					{
						break;
					}
				}
			}
		}
		catch (Poco::TimeoutException&)
		{
			if (!*rcon_run_flag)  // Checking Run Flag
			{
				break;
			}
			else
			{
				elapsed_seconds = rcon_timer.elapsedSeconds();
				if (elapsed_seconds >= 45)
				{
					logger->warn("Rcon: TIMED OUT...");
					rcon_timer.restart();
					connect();
				}
				else if (elapsed_seconds >= 30)
				{
					// Keep Alive
					#if defined(RCONWORKER_APP) || (TESTING)
						logger->info("Keep Alive Sending");
					#endif

					rcon_timer.restart();

					dgs.sendBytes(keepAlivePacket.data(), keepAlivePacket.size());

					#if defined(RCONWORKER_APP) || (TESTING)
						logger->info("Keep Alive Sent");
					#endif
				}
				else if (*rcon_login_flag)
				{
					// Checking for Commands to Send
					std::lock_guard<std::mutex> lock(mutex_rcon_commands);

					for (auto &rcon_command : rcon_commands)
					{
						char *cmd = new char[rcon_command.size()+1];
						std::strcpy(cmd, rcon_command.c_str());
						
						delete []rcon_packet.cmd;
						rcon_packet.cmd = cmd;
						
						rcon_packet.packetCode = 0x01;
						sendPacket();
					}
					// Clear Comands
					rcon_commands.clear();
				}
			}
		}
		catch (Poco::Net::ConnectionRefusedException& e)
		{
			logger->error("Rcon: Error Connect: {0}", e.displayText());
			disconnect();
		}
		catch (Poco::Exception& e)
		{
			logger->error("Rcon: Error Rcon: {0}", e.displayText());
			disconnect();
		}
	}
}



#ifdef RCON_APP

	int main(int nNumberofArgs, char* pszArgs[])
	{
		auto console = spdlog::stdout_logger_mt("extDB Console logger");

		boost::program_options::options_description desc("Options");
		desc.add_options()
			("help", "Print help messages")
			("ip", boost::program_options::value<std::string>()->required(), "IP Address for Server")
			("port", boost::program_options::value<int>()->required(), "Port for Server")
			("password", boost::program_options::value<std::string>()->required(), "Rcon Password for Server")
			("file", boost::program_options::value<std::string>(), "File to run i.e rcon restart warnings");

		boost::program_options::variables_map options;

		try 
		{
			boost::program_options::store(boost::program_options::parse_command_line(nNumberofArgs, pszArgs, desc), options);
			
			if (options.count("help") )
			{
				console->info("Rcon Command Line, based off bercon by Prithu \"bladez\" Parker");
				console->info("\t\t @ https://github.com/bladez-/bercon");
				console->info("");
				console->info("");
				console->info("Rewritten for extDB + crossplatform by Torndeco");
				console->info("\t\t @ https://github.com/Torndeco/extDB");
				console->info("");
				console->info("File Option is just for parsing rcon commands to be ran, i.e server restart warnings");
				console->info("\t\t For actually restarts use a cron program to run a script");
				console->info("");
				return 0;
			}
			
			boost::program_options::notify(options);
		}
		catch(boost::program_options::error& e)
		{
			std::cout << "ERROR: " << e.what() << std::endl;
			std::cout << desc << std::endl;
			return 1;
		}

		BERcon rcon;
		rcon.init(console);
		rcon.updateLogin(options["ip"].as<std::string>(), options["port"].as<int>(), options["password"].as<std::string>());
		Poco::Thread thread;
		thread.start(rcon);
		
		if (options.count("file"))
		{
			std::ifstream fin(options["file"].as<std::string>());
			//std::ifstream fin("test");
			if (fin.is_open() == false)
			{
				console->warn("ERROR: File is Open");
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
					boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
					console->info("Sleep", line);
				}
				else
				{
					rcon.addCommand(line);
				}
			}
			console->info("OK");
			rcon.disconnect();
			thread.join();
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
			for (;;) {
				std::getline(std::cin, input_str);
				if (std::string(input_str) == "quit")
				{
					console->info("Quitting Please Wait");
					rcon.disconnect();
					thread.join();
					break;
				}
				else
				{
					rcon.addCommand(input_str);
				}
			}
			console->info("Quitting");
			return 0;
		}
	}
#endif