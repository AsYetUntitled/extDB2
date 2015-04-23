/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

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


#include "http_raw.h"

#include <iostream>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>

#include <boost/algorithm/string.hpp>

#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/NumberFormatter.h>

#include "abstract_protocol.h"


bool HTTP_RAW::init(AbstractExt *extension, const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;
	int max_sessions = extension_ptr->pConf->getInt(database_id + ".MaxSessions", extension_ptr->extDB_info.max_threads);
	std::string host = extension_ptr->pConf->getString(database_id + ".Host", "127.0.0.1");

	int port = extension_ptr->pConf->getInt(database_id + ".Port", 80);
	http_pool = new HTTP(host, port, max_sessions);

	if (extension_ptr->pConf->has(database_id + ".Username"))
	{
		http_basic_credentials.setUsername(extension_ptr->pConf->getString(database_id + ".Username", ""));
		http_basic_credentials.setPassword(extension_ptr->pConf->getString(database_id + ".Password", ""));
		auth = true;
	}
	else
	{
		auth = false;
	}

 	if (boost::iequals(init_str, std::string("FULL_RETURN")))
	{
		http_return = 1;
	}
	else
	{
		http_return = 0;
	}

	return true;
}


bool HTTP_RAW::callProtocol(std::string input_str, std::string &result, const int unique_id)
{
	try
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: HTTP_RAW: Trace: Input: {0}", input_str);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: HTTP_RAW: Trace: Input: {0}", input_str);
		#endif

		if (input_str.size() >= 4)
		{
			std::unique_ptr<Poco::Net::HTTPClientSession> session = http_pool->get();
			Poco::Net::HTTPRequest request(Poco::Net::HTTPMessage::HTTP_1_1);
			
			if (input_str.substr(0,4) == "POST")
			{
				request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
				request.setURI(input_str.substr(5));
			}
			else if (input_str.substr(0,4) == "GET:")
			{
				request.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
				request.setURI(input_str.substr(4));
			}
			if (auth)
			{
				http_basic_credentials.authenticate(request);
			}
			session->sendRequest(request);

			Poco::Net::HTTPResponse res;
			std::istream &is = session->receiveResponse(res);

			char c;
			while (is.read(&c, sizeof(c)))
			{
				result.push_back(c);
			}

			std::string http_status_code = Poco::NumberFormatter::format(res.getStatus());
			switch (http_return)
			{
				case 0: //
				{
					if (res.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
					{
						result = "[1," + result + "]";
						http_pool->putBack(session);
					}
					else
					{
						result = "[0,\"Error: HTTP Status Code: " + http_status_code + "\"]"; // TODO HTTP Response STATUS
						#ifdef DEBUG_TESTING
							extension_ptr->console->error("extDB2: HTTP_RAW: Error: HTTP Status Code {0}", http_status_code);
							extension_ptr->console->error("extDB2: HTTP_RAW: Error: {0}", input_str);
						#endif
						extension_ptr->logger->error("extDB2: HTTP_RAW: Error: HTTP Status Code {0}", http_status_code);
						extension_ptr->logger->error("extDB2: HTTP_RAW: Error: {0}", input_str);
					}
					break;
				}
				case 1: // FULL_RETURN
				{
					if (res.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
					{
						result = "[1," + http_status_code + "," + result + "]";
						http_pool->putBack(session);
					}
					else
					{
						result = "[0," + http_status_code + "," + result + "]"; // TODO Log HTTP STATUS ERROR
						#ifdef DEBUG_TESTING
							extension_ptr->console->error("extDB2: HTTP_RAW: Error: HTTP Status Code {0}", http_status_code);
							extension_ptr->console->error("extDB2: HTTP_RAW: Error: {0}", input_str);
						#endif
						extension_ptr->logger->error("extDB2: HTTP_RAW: Error: HTTP Status Code {0}", http_status_code);
						extension_ptr->logger->error("extDB2: HTTP_RAW: Error: {0}", input_str);
					}
					break;
				}
			}
		}
		else
		{
			result = "[0,\"Error\"]"; // TODO Invalid Command / String
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: HTTP_RAW: Error: Input to Short {0}", input_str);
			#endif
			extension_ptr->logger->error("extDB2: HTTP_RAW: Error: Input to Short {0}", input_str);
		}
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: HTTP_RAW: Trace: Result: {0}", result);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: HTTP_RAW: Trace: Result: {0}", result);
		#endif
	}
	catch (Poco::Exception& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: HTTP_RAW: Error Exception: {0}", e.displayText());
			extension_ptr->console->error("extDB2: HTTP_RAW: Error Exception: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: HTTP_RAW: Error Exception: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: HTTP_RAW: Error Exception: SQL: {0}", input_str);
		result = "[0,\"Error Exception\"]";
	}
	return true;
}