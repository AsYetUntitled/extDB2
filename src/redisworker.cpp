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


void RedisWorker::dummy(const RedisValue &value)
{
}

void RedisWorker::onConnect(bool connected, const std::string &errorMessage, std::condition_variable &cnd, std::mutex &cnd_mutex, bool &cnd_bool)
{
	if (connected)
	{
		#ifdef TESTING
			extension_ptr->console->info("extDB2: Redis Connected");
		#endif
		extension_ptr->logger->info("extDB2: Redis Connected");
	}
	else
	{
		#ifdef TESTING
		  extension_ptr->console->info("extDB2: extDB2: Redis Connection Error: {0}", errorMessage);
		#endif
		extension_ptr->logger->info("extDB2: extDB2: Redis Connection Error: {0}", errorMessage);
	}

    std::unique_lock<std::mutex> cnd_lock(cnd_mutex);
    cnd_bool = true;
    cnd.notify_one();
    // TODO ADD Code for AUTH   
}

void RedisWorker::command(std::vector<std::string> &args)
{
    if (args[0] == "GET")
    {
        redisClient.command(args, boost::bind(&RedisWorker::onGet, this, _1));
    }
    else if (args[0] == "SET")
    {
        redisClient.command(args, boost::bind(&RedisWorker::onSet, this, _1));
    }
    else
    {
        redisClient.command(args, boost::bind(&RedisWorker::dummy, this, _1));    
    }
}

void RedisWorker::onSet(const RedisValue &value)
{
    extension_ptr->console->info(value.toString());
    /*
    std::cerr << "SET: " << value.toString() << std::endl;
    if (value.toString() == "OK")
    {
        redisClient.command("GET",  redisKey,
                                boost::bind(&RedisWorker::onGet, this, _1));
    }
    else
    {
        std::cerr << "Invalid value from redis: " << value.toString() << std::endl;
    }
    */
}

void RedisWorker::onGet(const RedisValue &value)
{
    extension_ptr->console->info(value.toString());
    /*
    std::cerr << "GET " << value.toString() << std::endl;
    if (value.toString() != redisValue)
    {
        std::cerr << "Invalid value from redis: " << value.toString() << std::endl;
    }

    redisClient.command("DEL", redisKey,
                            boost::bind(&boost::asio::io_service::stop, boost::ref(ioService)));
    */
}