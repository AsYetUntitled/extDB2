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

#include <condition_variable>
#include <mutex>

#include "../abstract_ext.h"
#include "../redisclient/redisasyncclient.h"


class AbstractExt;

class Redis
{
    public:
		Redis(boost::asio::io_service &ioService, RedisAsyncClient &redisClient, AbstractExt *extension)
			: ioService(ioService), redisClient(redisClient)
        {
			extension_ptr = extension;
		}

        void onConnect(bool connected, const std::string &errorMessage, std::condition_variable &cnd, std::mutex &cnd_mutex, bool &cnd_bool);
		void command(std::vector<std::string> &args, const int unique_id);
		void processResult(const RedisValue &value, const int unique_id);
		
    private:
		AbstractExt *extension_ptr;

        boost::asio::io_service &ioService;
        RedisAsyncClient &redisClient;
};