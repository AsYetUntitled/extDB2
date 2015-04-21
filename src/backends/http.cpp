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

#include <memory>
#include <thread>

#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPSessionInstantiator.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/URI.h>

//#include <Poco/Net/ConsoleCertificateHandler.h>
//#include <Poco/Net/PrivateKeyPassphraseHandler.h>
//#include <Poco/Net/KeyConsoleHandler.h>

#include <Poco/SharedPtr.h>


HTTP::HTTP(std::string uri, int maxSessions):
    _uri(uri),
	_maxSessions(maxSessions)
{
	Poco::Net::HTTPSessionFactory::defaultFactory().registerProtocol("http", new Poco::Net::HTTPSessionInstantiator);
	Poco::Net::HTTPSessionFactory::defaultFactory().registerProtocol("https", new Poco::Net::HTTPSessionInstantiator);
	ptrCert = new Poco::Net::AcceptCertificateHandler(false);

	//pConsoleHandler = new Poco::Net::KeyConsoleHandler(false);
	//pInvalidCertHandler = new Poco::Net::ConsoleCertificateHandler(false);
	context = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_RELAXED, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	Poco::Net::SSLManager::instance().initializeClient(0, ptrCert, context);
}


HTTP::~HTTP()
{
}


std::unique_ptr<Poco::Net::HTTPClientSession> HTTP::get()
{
	std::unique_ptr<Poco::Net::HTTPClientSession> session;
	if (_idleSessions.empty())
	{
		session.reset(Poco::Net::HTTPSessionFactory::defaultFactory().createClientSession(_uri));
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
