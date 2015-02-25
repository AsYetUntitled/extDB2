/*
Copyright (C) 2015 Declan Ireland <http://github.com/torndeco/extDB>

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

#include <boost/algorithm/string.hpp>

#include "remoteserver.h"


void RemoteServer::init(AbstractExt *extension)
{
	extension_ptr = extension;

	pParams = new Poco::Net::TCPServerParams();
	pParams->setMaxThreads(extension_ptr->pConf->getInt("RemoteAccess.MaxThreads", 4));
	pParams->setMaxQueued(extension_ptr->pConf->getInt("RemoteAccess.MaxQueued", 4));
	pParams->setThreadIdleTime(extension_ptr->pConf->getInt("RemoteAccess.IdleTime", 120));

	int port = extension_ptr->pConf->getInt("RemoteAccess.Port", 2301);

	extension_ptr->remote_access_info.password = extension_ptr->pConf->getString("RemoteAccess.Password", "");

	Poco::Net::ServerSocket s(port);
	tcp_server = new Poco::Net::TCPServer(new RemoteConnectionFactory(this), s, pParams);
	tcp_server->start();
}


void RemoteServer::stop()
{
	tcp_server->stop();
}


bool RemoteConnection::login()
{
	int nBytes = -1;
	int failed_attempt = 0;

	Poco::Timespan timeOut(30, 0);
	char incommingBuffer[1500];

	// Send Password Request
	std::string recv_str;
	std::string send_str = "Password: ";

	socket().sendBytes(send_str.c_str(), send_str.size());

	while (true)
	{
		if (socket().poll(timeOut, Poco::Net::Socket::SELECT_READ) == false)
		{
			#ifdef TESTING
				extension_ptr->console->info("extDB2: Client Timed Out");
			#endif
			extension_ptr->logger->info("extDB2: Client Timed Out");
			send_str = "Timed Out, Closing Connection";
			socket().sendBytes(send_str.c_str(), send_str.size());
			socket().shutdown();
			return false;
		}
		else
		{
			nBytes = -1;

			try
			{
				nBytes = socket().receiveBytes(incommingBuffer, sizeof(incommingBuffer));
				recv_str = std::string(incommingBuffer, nBytes);
				boost::algorithm::trim(recv_str);
			}
			catch (Poco::Exception& e)
			{
				//Handle your network errors.
				#ifdef TESTING
					extension_ptr->console->warn("extDB2: Network Error: {0}", e.displayText());
				#endif
				extension_ptr->logger->warn("extDB2: Network Error: {0}", e.displayText());
				return false;
			}

			if (nBytes==0)
			{
				#ifdef TESTING
					extension_ptr->console->info("extDB2: Client closed connection");
				#endif
				extension_ptr->logger->info("extDB2: Client closed connection");
				return false;
			}
			else
			{
				if (extension_ptr->remote_access_info.password == recv_str)
				{
					#ifdef TESTING
						extension_ptr->console->info("extDB2: Client Logged in");
					#endif
					extension_ptr->logger->info("extDB2: Client Logged in");
					send_str = "\n\r";
					send_str += "Logged in\n\r";
					send_str += "\n\r";
					send_str += "Type #HELP for commands\n\n\r";
					socket().sendBytes(send_str.c_str(), send_str.size());
					return true;
				}
				else
				{
					++failed_attempt;
					if (failed_attempt > 3)
					{
						#ifdef TESTING
							extension_ptr->console->info("extDB2: Client Failed Login 3 Times, blacklisting");
						#endif
						extension_ptr->logger->info("extDB2: Client Failed Login 3 Times, blacklisting");
						// TODO: Blacklist client
						socket().shutdown();
						return false;
					}
				}
			}
		}
	}
	return false;
}


void RemoteConnection::mainLoop()
{
	int nBytes = -1;

	Poco::Timespan timeOut(5, 0);
	char incommingBuffer[1500];
	bool isOpen = true;

	std::string recv_str;
	std::string send_str;
	
	bool store_receive = false;
	std::string store_str;

	int unique_id;
	{
		boost::lock_guard<boost::mutex> lock(remoteServer_ptr->id_mgr_mutex);
		unique_id = remoteServer_ptr->id_mgr.AllocateId();
	}
		
	while (isOpen)
	{
		if (socket().poll(timeOut, Poco::Net::Socket::SELECT_READ) == false)
		{
			boost::lock_guard<boost::mutex> lock(remoteServer_ptr->clients_data_mutex);
			if (!remoteServer_ptr->clients_data[unique_id].outputs.empty())
			{
				for (auto &output : remoteServer_ptr->clients_data[unique_id].outputs)
				{
					send_str = output + "\n\r";
					socket().sendBytes(send_str.c_str(), send_str.size());
				}
				remoteServer_ptr->clients_data[unique_id].outputs.clear();
			}
		}
		else
		{
			nBytes = -1;

			try
			{
				nBytes = socket().receiveBytes(incommingBuffer, sizeof(incommingBuffer));
				recv_str = std::string(incommingBuffer, nBytes);
				boost::algorithm::trim(recv_str);
			}
			catch (Poco::Exception& e)
			{
				isOpen = false;
				#ifdef TESTING
					extension_ptr->console->warn("extDB2: Network Error: {0}", e.displayText());
				#endif
				extension_ptr->logger->warn("extDB2: Network Error: {0}", e.displayText());
			}

			if (nBytes==0)
			{
				isOpen = false;
				#ifdef TESTING
					extension_ptr->console->info("extDB2: Client closed connection");
				#endif
				extension_ptr->logger->info("extDB2: Client closed connection");
			}
			else
			{
				if (recv_str.size() > 2)
				{
					if (recv_str[0] == '#')
					{
						// Command
						if (boost::iequals(recv_str, std::string("#START")) == 1)
						{
							store_receive = true;
							store_str.clear();
						}
						else if (boost::iequals(recv_str, std::string("#END")) == 1)
						{
							store_receive = false;
						}
						else if (boost::iequals(recv_str, std::string("#SEND")) == 1)
						{
							boost::lock_guard<boost::mutex> lock(remoteServer_ptr->inputs_mutex);
							std::string temp_str = Poco::NumberFormatter::format(unique_id) + ":" + send_str;
							remoteServer_ptr->inputs.push_back(temp_str);
							remoteServer_ptr->inputs_flag = true;
						}
						else if (boost::iequals(recv_str, std::string("#QUIT")) == 1)
						{
							isOpen = false;
							send_str = "Closing Connection\n";
							socket().sendBytes(send_str.c_str(), send_str.size());
						}
						else if (boost::iequals(recv_str, std::string("#HELP")) == 1)
						{
							send_str = "\n\r";
							send_str = "Example of Usage\n\r";
							send_str += "#START\n\r";
							send_str += ".... INSERT CODE HERE....\n\r";
							send_str += ".... INSERT CODE HERE....\n\r";
							send_str += ".... INSERT CODE HERE....\n\r";
							send_str += ".... INSERT CODE HERE....\n\r";
							send_str += "#END\n\r";
							send_str += "#SEND\n\r";
							send_str += "\n\r";
							send_str += "Note: You can also call #SEND Multiple Times if you like\n\r";
							send_str += "\n\r";
							send_str += "#QUIT -> Closes Connection\n\r";
							socket().sendBytes(send_str.c_str(), send_str.size());
						}
						else
						{
							send_str = "\nUnknown Command\n\r";
							send_str += "Type #HELP for commands\n\n\r";
							socket().sendBytes(send_str.c_str(), send_str.size());
						}
					}
				}
				else
				{
					if (store_receive)
					{
						store_str += recv_str;
					}
				}
			}
		}
	}
	{
		boost::lock_guard<boost::mutex> lock(remoteServer_ptr->clients_data_mutex);
		remoteServer_ptr->clients_data.erase(unique_id);
	}
	{
		boost::lock_guard<boost::mutex> lock(remoteServer_ptr->id_mgr_mutex);
		remoteServer_ptr->id_mgr.FreeId(std::move(unique_id));
	}
}


void RemoteConnection::run()
{
	#ifdef TESTING
		extension_ptr->console->info("New Connection from: {0}", socket().peerAddress().host().toString());
	#endif
	extension_ptr->logger->info("New Connection from: {0}", socket().peerAddress().host().toString());

	if (login())
	{
		mainLoop();
	}

	#ifdef TESTING
		extension_ptr->console->info("Connection closed");
	#endif
}

