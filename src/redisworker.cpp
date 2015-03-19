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


#include "redisworker.h"

#include <condition_variable>
#include <mutex>

#include "abstract_ext.h"


void RedisWorker::onConnect(bool connected, const std::string &errorMessage, std::condition_variable &cnd, std::mutex &cnd_mutex, bool &cnd_bool)
{
	if (connected)
	{
        #ifdef DEBUG_TESTING
            extension_ptr->console->info("extDB2: Redis Connected");
        #endif
        #ifdef DEBUG_LOGGING
            extension_ptr->logger->info("extDB2: Redis Connected");
        #endif
    }
	else
	{
		#ifdef DEBUG_TESTING
		  extension_ptr->console->info("extDB2: extDB2: Redis Connection Error: {0}", errorMessage);
		#endif
		extension_ptr->logger->info("extDB2: extDB2: Redis Connection Error: {0}", errorMessage);
	}

    std::unique_lock<std::mutex> cnd_lock(cnd_mutex);
    cnd_bool = true;
    cnd.notify_one();
    // TODO ADD Code for AUTH   
}


void RedisWorker::command(std::vector<std::string> &args, const int unique_id)
{
    redisClient.command(args, boost::bind(&RedisWorker::processResult, this, _1, unique_id));
}


void RedisWorker::processResult(const RedisValue &value, const int unique_id)
{
    #ifdef DEBUG_TESTING
        extension_ptr->console->info("processResult: {0}", value.toString());
    #endif
    #ifdef DEBUG_LOGGING
        extension_ptr->logger->info("processResult: {0}", value.toString());
    #endif
    if (unique_id > 0)
    {
        AbstractExt::resultData result_data;
        result_data.message = "[1," + value.toString() + "]";
        extension_ptr->saveResult_mutexlock(unique_id, result_data);
    }
}
