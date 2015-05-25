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


#include "http.h"

#include <Poco/Net/HTTPClientSession.h>


HTTP::HTTP(std::string host, int port, int maxSessions):
	_maxSessions(maxSessions), _host(host), _port(port)
{
}


HTTP::~HTTP()
{
}


std::unique_ptr<Poco::Net::HTTPClientSession> HTTP::get()
{
	std::unique_ptr<Poco::Net::HTTPClientSession> session;
	if (_idleSessions.empty())
	{
		session.reset(new Poco::Net::HTTPClientSession(_host, _port));
	}
	else
	{
		std::lock_guard<std::mutex> lock(mutex);
		session = std::move(_idleSessions.front());
		_idleSessions.pop_front();
	}
	return session;
}


void HTTP::putBack(std::unique_ptr<Poco::Net::HTTPClientSession> &session)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (_idleSessions.size() > _maxSessions)
	{
		_idleSessions.push_front(std::move(session));
	}
}
