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

#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPSessionInstantiator.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/URI.h>

#include <Poco/Net/ConsoleCertificateHandler.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/KeyConsoleHandler.h>


class HTTP
{
	public:

		HTTP(std::string uri, int maxSessions);
		~HTTP();
			
		std::unique_ptr<Poco::Net::HTTPClientSession> get();
		void putBack(std::unique_ptr<Poco::Net::HTTPClientSession> &session);


	private:

		Poco::URI      _uri;
		int            _maxSessions;

		std::mutex     mutex;
		std::list< std::unique_ptr<Poco::Net::HTTPClientSession> >  _idleSessions;

		Poco::SharedPtr<Poco::Net::AcceptCertificateHandler> ptrCert;

		//Poco::SharedPtr<Poco::Net::PrivateKeyPassphraseHandler> pConsoleHandler;
		//Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pInvalidCertHandler;
		Poco::Net::Context::Ptr context;
};
