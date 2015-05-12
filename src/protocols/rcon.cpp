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


#include "rcon.h"

#include <boost/algorithm/string.hpp>

#include <Poco/StringTokenizer.h>



bool RCON::init(AbstractExt *extension,  const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;

	if (!init_str.empty())
	{
		Poco::StringTokenizer tokens(init_str, "-");
		allowed_commands.insert(allowed_commands.begin(), tokens.begin(), tokens.end());
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: RCON: Commands Allowed: {0}", init_str);
			extension_ptr->console->warn("extDB2: RCON Status: {0}", extension_ptr->extDB_connectors_info.rcon);
		#endif
		extension_ptr->logger->warn("extDB2: RCON: Commands Allowed: {0}", init_str);
		extension_ptr->logger->warn("extDB2: RCON Status: {0}", extension_ptr->extDB_connectors_info.rcon);
	}
	return extension_ptr->extDB_connectors_info.rcon;
}


void RCON::processCommand(std::string &command, std::string &input_str, const unsigned int unique_id, std::string &result)
{
	if (boost::iequals(command, std::string("players")) == 1)
	{
		extension_ptr->console->info("extDB2: RCON: DEBUG PLAYERS: {0}", result);
		extension_ptr->rconPlayers(input_str, unique_id);
	}
	else if (boost::iequals(command, std::string("missions")) == 1)
	{
		extension_ptr->console->info("extDB2: RCON: DEBUG MISSIONS: {0}", result);
		extension_ptr->rconMissions(input_str, unique_id);
	}
	else
	{
		extension_ptr->rconCommand(input_str);
		result = "[1]"; 
	}
}


bool RCON::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	#ifdef DEBUG_TESTING
		extension_ptr->console->info("extDB2: RCON: Trace: Input: {0}", input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB2: RCON: Trace: Input: {0}", input_str);
	#endif

	boost::trim(input_str);

	if (allowed_commands.size() > 0)
	{
		std::string command;
		const std::string::size_type found = input_str.find(" ");
		if (found==std::string::npos)
		{
			command = input_str;
		}
		else
		{
			command = input_str.substr(0, found-1);
		}

		if (std::find(allowed_commands.begin(), allowed_commands.end(), command) == allowed_commands.end())
		{
			result ="[0,\"RCon Command Not Allowed\"]";
			#ifdef DEBUG_TESTING
				extension_ptr->console->warn("extDB2: RCON: Command Not Allowed: Input: {0}", input_str);
			#endif
			extension_ptr->logger->warn("extDB2: RCON: Command Not Allowed: Input: {0}", input_str);
		}
		else
		{
			processCommand(command, input_str, unique_id, result);
		}
	}
	else
	{
		processCommand(input_str, input_str, unique_id, result);
	}

	return (!result.empty());  // If result is empty due to error, save error message
}