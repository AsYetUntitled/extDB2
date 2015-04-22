/*
Copyright (C) 2015 Declan Ireland <http://github.com/torndeco/extDB2>

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

#include <memory> 
#include <mutex>
#include <list>

#include <Poco/Net/HTTPClientSession.h>


class HTTP
{
	public:

		HTTP(std::string host, int port, int maxSessions);
		~HTTP();
			
		std::unique_ptr<Poco::Net::HTTPClientSession> get();
		void putBack(std::unique_ptr<Poco::Net::HTTPClientSession> &session);


	private:

		int            _maxSessions;
		std::string	   _host;
		int			   _port;

		std::mutex     mutex;
		std::list< std::unique_ptr<Poco::Net::HTTPClientSession> >  _idleSessions;
};
