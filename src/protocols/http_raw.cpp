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

	std::string host = extension_ptr->pConf->getString(database_id + ".Host", "127.0.0.1");
	int port = extension_ptr->pConf->getInt(database_id + ".Port", 80);
	int max_sessions = extension_ptr->pConf->getInt(database_id + ".MaxSessions", extension_ptr->extDB_info.max_threads);
	http_pool = new HTTP(host, port, max_sessions);

	bool status;
	if (extension_ptr->pConf->hasOption(database_id + ".Type"))
	{
		std::string database_type = extension_ptr->pConf->getString(database_id) + ".Type";
		if (boost::iequals(database_type, std::string("HTTP")))
		{
			if (extension_ptr->pConf->has(database_id + ".Username"))
			{
				http_basic_credentials.setUsername(extension_ptr->pConf->getString(database_id + ".Username", ""));
				http_basic_credentials.setPassword(extension_ptr->pConf->getString(database_id + ".Password", ""));
				auth = true;
			}			
		 	if (boost::iequals(init_str, std::string("FULL_RETURN")))
			{
				http_return = 1;
				#ifdef DEBUG_TESTING
					extension_ptr->console->info("extDB2: HTTP_RAW: Initialized");
				#endif
				extension_ptr->logger->info("extDB2: HTTP_RAW: Initialized");
			}
			else
			{
				http_return = 0;
				#ifdef DEBUG_TESTING
					extension_ptr->console->info("extDB2: HTTP_RAW: Initialized: Full Return");
				#endif
				extension_ptr->logger->info("extDB2: HTTP_RAW: Initialized: Full Return");
			}
			status = true;
		}
		else
		{
			#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: HTTP_RAW: Wrong Database Type in config file: {0}", extension_ptr->pConf->getString(database_id) + ".Type");
				extension_ptr->console->warn("extDB2: HTTP_RAW: Database: {0}", database_id);
			#endif
				extension_ptr->logger->warn("extDB2: HTTP_RAW: Wrong Database Type in config file: {0}", extension_ptr->pConf->getString(database_id) + ".Type");
			extension_ptr->logger->warn("extDB2: HTTP_RAW: Database: {0}", database_id);
			status = false;
		}
	}
	else
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: HTTP_RAW: Missing HTTP Backend Info: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB2: HTTP_RAW: Missing HTTP Backend Info: {0}", database_id);
		status = false;
	}

	return status;
}


bool HTTP_RAW::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
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
						result = "[0,\"Error: HTTP Status Code: " + http_status_code + "\"]";
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
						result = "[0," + http_status_code + "," + result + "]";
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
			result = "[0,\"Error\"]";
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