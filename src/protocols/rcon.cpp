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

#include <Poco/StringTokenizer.h>

#include "rcon.h"


bool RCON::init(AbstractExt *extension,  const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;

	if (!init_str.empty())
	{
		Poco::StringTokenizer tokens(init_str, "-");
		allowed_commands.insert(allowed_commands.begin(), tokens.begin(), tokens.end());
		#ifdef TESTING
			extension_ptr->console->warn("extDB2: RCON: Commands Allowed: {0}", init_str);
			extension_ptr->console->warn("extDB2: RCON Status: {0}", extension_ptr->extDB_connectors_info.rcon);
		#endif
		extension_ptr->logger->warn("extDB2: RCON: Commands Allowed: {0}", init_str);
		extension_ptr->logger->warn("extDB2: RCON Status: {0}", extension_ptr->extDB_connectors_info.rcon);
	}
	return extension_ptr->extDB_connectors_info.rcon;
}


bool RCON::callProtocol(std::string input_str, std::string &result, const int unique_id)
{
	#ifdef TESTING
		extension_ptr->console->info("extDB2: RCON: Trace: Input: {0}", input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB2: RCON: Trace: Input: {0}", input_str);
	#endif

	Poco::StringTokenizer tokens(input_str, ":");
	if (tokens.count() < 2)
	{
		result = "[0,\"RCon Syntax Error\"]";
		#ifdef TESTING
			extension_ptr->console->warn("extDB2: RCON: Syntax Error: Input: {0}", input_str);
		#endif
		extension_ptr->logger->warn("extDB2: RCON: Syntax Error: Input: {0}", input_str);
	}
	else
	{
		if ((std::find(allowed_commands.begin(), allowed_commands.end(), tokens[0]) == allowed_commands.end()) && (allowed_commands.size() > 0))
		{
			result ="[0,\"RCon Command Not Allowed\"]";
			#ifdef TESTING
				extension_ptr->console->warn("extDB2: RCON: Command Not Allowed: Input: {0}", input_str);
			#endif
			extension_ptr->logger->warn("extDB2: RCON: Command Not Allowed: Input: {0}", input_str);
		}
		else
		{
			result = "[1]"; 
			extension_ptr->rconCommand(input_str);
		}
	}
	return true;
}