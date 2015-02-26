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


#pragma once

#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

#include <Poco/Net/StreamSocket.h>

#include "protocols/abstract_ext.h"
#include "uniqueid.h"

class RemoteConnection;

class RemoteServer
{
public:
	void init(AbstractExt *extension);
	void setup(const std::string &remote_conf);
	void stop();

	AbstractExt *extension_ptr;

	IdManager id_mgr;
	boost::mutex id_mgr_mutex;

	struct clients
	{
		std::vector<std::string> outputs;
		std::shared_ptr<RemoteConnection> connection;
	};
	std::unordered_map<int, clients> clients_data;
	boost::mutex clients_data_mutex;

	std::vector<std::string> inputs;
	boost::mutex inputs_mutex;
	
	std::atomic<bool> *inputs_flag;

private:
	Poco::Net::TCPServerParams* pParams;
	Poco::Net::TCPServer *tcp_server;
};


class RemoteConnection: public Poco::Net::TCPServerConnection
	/// This class handles all client connections.
	///
	/// A string with the current date and time is sent back to the client.
{
public:
	RemoteConnection(const Poco::Net::StreamSocket& s, RemoteServer *remoteServer) :
		Poco::Net::TCPServerConnection(s),
		remoteServer_ptr(remoteServer),
		extension_ptr(remoteServer->extension_ptr)
	{
	}

	void run();
	bool login();
	void mainLoop();
	
private:
	AbstractExt *extension_ptr;
	RemoteServer *remoteServer_ptr;
};



class RemoteConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	/// A factory for TimeServerConnection.
{
public:
	RemoteConnectionFactory(RemoteServer *remoteServer) :
		remoteServer_ptr(remoteServer)
	{
	}
	
	Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket)
	{
		return new RemoteConnection(socket, remoteServer_ptr);
	}

private:
	RemoteServer *remoteServer_ptr;
};
