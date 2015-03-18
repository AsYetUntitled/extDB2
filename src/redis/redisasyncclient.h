/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <string>
#include <list>

#include "impl/redisclientimpl.h"
#include "redisvalue.h"


class RedisClientImpl;

class RedisAsyncClient : boost::noncopyable {
public:
    // Subscribe handle.
    struct Handle
    {
        size_t id;
        std::string channel;
    };

    inline RedisAsyncClient(boost::asio::io_service &ioService);
    inline ~RedisAsyncClient();

    // Connect to redis server
    inline void connect(
            const boost::asio::ip::address &address,
            unsigned short port,
            const boost::function<void(bool, const std::string &)> &handler);

    // Connect to redis server
    inline void connect(
            const boost::asio::ip::tcp::endpoint &endpoint,
            const boost::function<void(bool, const std::string &)> &handler);

    // backward compatibility
    inline void asyncConnect(
            const boost::asio::ip::address &address,
            unsigned short port,
            const boost::function<void(bool, const std::string &)> &handler)
    {
        connect(address, port, handler);
    }

    // backward compatibility
    inline void asyncConnect(
            const boost::asio::ip::tcp::endpoint &endpoint,
            const boost::function<void(bool, const std::string &)> &handler)
    {
        connect(endpoint, handler);
    }

    // Set custom error handler. 
    inline void installErrorHandler(
        const boost::function<void(const std::string &)> &handler);

    // Execute command on Redis server.
    inline void command(
            std::vector<std::string> &items, 
            const boost::function<void(const RedisValue &)> &handler);

    // Subscribe to channel. Handler msgHandler will be called
    // when someone publish message on channel. Call unsubscribe 
    // to stop the subscription.
    inline Handle subscribe(
            const std::string &channelName,
            const boost::function<void(const std::string &msg)> &msgHandler,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Unsubscribe
    inline void unsubscribe(const Handle &handle);

    // Subscribe to channel. Handler msgHandler will be called
    // when someone publish message on channel; it will be 
    // unsubscribed after call.
    inline void singleShotSubscribe(
            const std::string &channel,
            const boost::function<void(const std::string &msg)> &msgHandler,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Publish message on channel.
    inline void publish(
            const std::string &channel, const std::string &msg,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    inline static void dummyHandler(const RedisValue &) {}

    inline bool stateValid() const;

private:
    std::shared_ptr<RedisClientImpl> pimpl;
};

#include "impl/redisasyncclient.cpp"
