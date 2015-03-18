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


#include "redis_raw.h"

#include <Poco/StringTokenizer.h>


bool REDIS_RAW::init(AbstractExt *extension, const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;
	if (extension_ptr->extDB_connectors_info.databases.count(database_id) == 0)
	{
		#ifdef TESTING
			extension_ptr->console->warn("extDB2: REDIS_RAW: No Database Connection ID: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB2: REDIS_RAW: No Database Connection ID: {0}", database_id);
		return false;
	}

	database_ptr = &extension_ptr->extDB_connectors_info.databases[database_id];

	bool status;
	if (database_ptr->type == std::string("Redis"))
	{
		status = true;
	}
	else
	{
		// DATABASE NOT SETUP YET
		#ifdef TESTING
			extension_ptr->console->warn("extDB2: REDIS_RAW: No Database Connection");
		#endif
		extension_ptr->logger->warn("extDB2: REDIS_RAW: No Database Connection");
		status = false;
	}

	if (status)
	{
		if (init_str.empty())
		{
			#ifdef TESTING
				extension_ptr->console->info("extDB2: REDIS_RAW: Initialized");
			#endif
			extension_ptr->logger->info("extDB2: REDIS_RAW: Initialized");
		}
		else
		{
			Poco::StringTokenizer tokens(init_str, "-");
			allowed_commands.insert(allowed_commands.begin(), tokens.begin(), tokens.end());
			#ifdef TESTING
				extension_ptr->console->warn("extDB2: REDIS_RAW: Commands Allowed: {0}", init_str);
			#endif
			extension_ptr->logger->warn("extDB2: REDIS_RAW: Commands Allowed: {0}", init_str);
		}
	}
	return status;
}


bool REDIS_RAW::callProtocol(std::string input_str, std::string &result, const int unique_id)
{
	#ifdef TESTING
		extension_ptr->console->info("extDB2: REDIS_RAW: Trace: Input: {0}", input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB2: REDIS_RAW: Trace: Input: {0}", input_str);
	#endif

	if (input_str.empty())
	{
		// TODO
	}
	else
	{
		Poco::StringTokenizer tokens(input_str, ":");
		std::vector<std::string> args;
		args.insert(args.begin(), tokens.begin(), tokens.end());

		if (allowed_commands.size() > 0)
		{
			if (std::find(allowed_commands.begin(), allowed_commands.end(), args[0]) == allowed_commands.end())
			{
				result ="[0,\"Redis Command Not Allowed\"]";
				#ifdef TESTING
					extension_ptr->console->warn("extDB2: REDIS_RAW: Command Not Allowed: Input: {0}", input_str);
				#endif
				extension_ptr->logger->warn("extDB2: REDIS_RAW: Command Not Allowed: Input: {0}", input_str);
			}
			else
			{
				result = "[1]";
				database_ptr->redis_worker->command(args);
			}
		}
		else
		{
			result = "[1]"; 
			// TODO extension_ptr->rconCommand(input_str);
		}
	}
	return true;
}