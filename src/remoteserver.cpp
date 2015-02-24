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
	tcp_server = new Poco::Net::TCPServer(new RemoteConnectionFactory(extension_ptr), s, pParams);
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
	char incommingBuffer[1000];
	bool isOpen = true;

	// Send Password Request
	std::string input_str;
	std::string send_str = "Password: ";

	socket().sendBytes(send_str.c_str(), send_str.size());

	while (isOpen)
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
			isOpen = false;
		}
		else
		{
			nBytes = -1;

			try
			{
				nBytes = socket().receiveBytes(incommingBuffer, sizeof(incommingBuffer));
			}
			catch (Poco::Exception& e)
			{
				//Handle your network errors.
				#ifdef TESTING
					extension_ptr->console->warn("extDB2: Network Error: {0}", e.displayText());
				#endif
				extension_ptr->logger->warn("extDB2: Network Error: {0}", e.displayText());
				isOpen = false;
			}

			if (nBytes==0)
			{
				#ifdef TESTING
					extension_ptr->console->info("extDB2: Client closed connection");
				#endif
				extension_ptr->logger->info("extDB2: Client closed connection");
				isOpen = false;
			}
			else
			{
				if (extension_ptr->remote_access_info.password == std::string(incommingBuffer, nBytes - 2))
				{
					#ifdef TESTING
						extension_ptr->console->info("extDB2: Client Logged in");
					#endif
					extension_ptr->logger->info("extDB2: Client Logged in");
					send_str = "\nLogged in\n";
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
						socket().shutdown();
						isOpen = false;
						// Blacklist client
					}
				}
			}
		}
	}
	return false;
}


void RemoteConnection::run()
{
	#ifdef TESTING
		extension_ptr->console->info("New Connection from: {0}", socket().peerAddress().host().toString());
	#endif
	extension_ptr->logger->info("New Connection from: {0}", socket().peerAddress().host().toString());

	if (login())
	{
		//runMainloop()
	}

	#ifdef TESTING
		extension_ptr->console->info("Connection closed");
	#endif
}

