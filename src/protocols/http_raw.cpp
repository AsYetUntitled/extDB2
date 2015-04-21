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


#include <Poco/Exception.h>


bool HTTP_RAW::init(AbstractExt *extension, const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;
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


	/*
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	session->sendRequest(request);

	#ifdef DEBUG_TESTING
		extension_ptr->console->info("{0}", path);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("{0}", path);
	#endif

	Poco::Net::HTTPResponse res;
	if (res.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
	{
		try
		{
			std::istream &is = session->receiveResponse(res);
			boost::property_tree::read_json(is, *pt);
			response = 1;
		}
		catch (boost::property_tree::json_parser::json_parser_error &e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.message(), path);
			#endif
			extension_ptr->logger->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.message(), path);
			response = -1;
		}
	}
	*/
	

		Poco::Data::Session session = extension_ptr->getDBSession_mutexlock(*database_ptr);
		Poco::Data::RecordSet rs(session, input_str);

		result = "[1,[";

// <-------------

		result += "]]";
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