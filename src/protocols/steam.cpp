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

Code to Convert SteamID -> BEGUID 
From Frank https://gist.github.com/Fank/11127158

*/


#include "steam.h"

#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Poco/StringTokenizer.h>

#include "../steamworker.h"


bool STEAM::init(AbstractExt *extension, const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;
	return true;
}


bool STEAM::isNumber(const std::string &input_str)
{
	bool status = true;
	for (unsigned int index=0; index < input_str.length(); index++)
	{
		if (!std::isdigit(input_str[index]))
		{
			status = false;
			break;
		}
	}
	return status;
}


bool STEAM::callProtocol(std::string input_str, std::string &result, const int unique_id)
{
	#ifdef DEBUG_TESTING
		extension_ptr->console->info("extDB2: STEAM: Trace: Input: {0}", input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB2: STEAM: Trace: Input: {0}", input_str);
	#endif

	bool status = true;
	if (unique_id == -1)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: STEAM: SYNC MODE NOT SUPPORTED");
		#endif
		extension_ptr->logger->warn("extDB2: STEAM: SYNC MODE NOT SUPPORTED");
		result = "[0, \"STEAM: SYNC MODE NOT SUPPORTED\"]";
		status = false;
	}
	else
	{
		Poco::StringTokenizer tokens(input_str, ":");
		if (tokens.count() > 1)
		{
			std::vector<std::string> steamIDs;
			for (auto &token : tokens)
			{
				if (isNumber(token))
				{
					steamIDs.push_back(token);
				}
				else
				{
					#ifdef DEBUG_TESTING
						extension_ptr->console->warn("extDB2: STEAM: Invalid SteamID: {0}", token);
					#endif
					extension_ptr->logger->warn("extDB2: STEAM: Invalid SteamID: {0}", token);
					result = "[0, \"STEAM: Invalid SteamID\"]";
					status = false;
					break;
				}
			}

			if (status)
			{
				if (boost::iequals(tokens[0], std::string("GetFriends")) == 1)
				{
					extension_ptr->steamQuery(unique_id, true, false, steamIDs, true);
				}
				else if (boost::iequals(tokens[0], std::string("STEAMBanned")) == 1)
				{
					extension_ptr->steamQuery(unique_id, false, true, steamIDs, true);
				}
				else
				{
					#ifdef DEBUG_TESTING
						extension_ptr->console->warn("extDB2: STEAM: Invalid Query Type: {0}", tokens[0]);
					#endif
					extension_ptr->logger->warn("extDB2: STEAM: Invalid Query Type: {0}", tokens[0]);
					result = "[0, \"STEAM: Invalid Query Type\"]";
					status = false;
				}
			}
		}
	}
	return status;
}