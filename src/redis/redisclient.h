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

#include "redisclientimpl.h"
#include "redisvalue.h"
#include "redispool.h"

class RedisPool;
class RedisClientImpl;

class RedisClient : boost::noncopyable {
public:
    // Subscribe handle.
    struct Handle {
        size_t id;
        std::string channel;
    };

    RedisClient(boost::asio::io_service &ioService, RedisPool *pool);
    ~RedisClient();

    // Connect to redis server, blocking call.
    bool connect(const boost::asio::ip::address &address,
                                   unsigned short port);

    // Connect to redis server, asynchronous call.
    void asyncConnect(
            const boost::asio::ip::address &address,
            unsigned short port,
            const boost::function<void(bool, const std::string &)> &handler);

    // Connect to redis server, asynchronous call.
    void asyncConnect(
            const boost::asio::ip::tcp::endpoint &endpoint,
            const boost::function<void(bool, const std::string &)> &handler);

    // Set custom error handler. 
    void installErrorHandler(
        const boost::function<void(const std::string &)> &handler);

    // Execute command on Redis server.
    void command(
            const std::string &cmd,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with one argument.
    void command(
            const std::string &cmd, const std::string &arg1,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with two arguments.
    void command(
            const std::string &cmd, const std::string &arg1, const std::string &arg2,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with three arguments.
    void command(
            const std::string &cmd, const std::string &arg1,
            const std::string &arg2, const std::string &arg3,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with four arguments.
    void command(
            const std::string &cmd, const std::string &arg1, const std::string &arg2,
            const std::string &arg3, const std::string &arg4,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with five arguments.
    void command(
            const std::string &cmd, const std::string &arg1,
            const std::string &arg2, const std::string &arg3,
            const std::string &arg4, const std::string &arg5,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with six arguments.
    void command(
            const std::string &cmd, const std::string &arg1,
            const std::string &arg2, const std::string &arg3,
            const std::string &arg4, const std::string &arg5,
            const std::string &arg6,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);


    // Execute command on Redis server with seven arguments.
    void command(
            const std::string &cmd, const std::string &arg1,
            const std::string &arg2, const std::string &arg3,
            const std::string &arg4, const std::string &arg5,
            const std::string &arg6, const std::string &arg7,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Execute command on Redis server with the list of arguments.
    void command(
            const std::string &cmd, const std::list<std::string> &args,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Subscribe to channel. Handler msgHandler will be called
    // when someone publish message on channel. Call unsubscribe 
    // to stop the subscription.
    Handle subscribe(
            const std::string &channelName,
            const boost::function<void(const std::string &msg)> &msgHandler,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Unsubscribe
    void unsubscribe(const Handle &handle);

    // Subscribe to channel. Handler msgHandler will be called
    // when someone publish message on channel; it will be 
    // unsubscribed after call.
    void singleShotSubscribe(
            const std::string &channel,
            const boost::function<void(const std::string &msg)> &msgHandler,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

    // Publish message on channel.
    void publish(
            const std::string &channel, const std::string &msg,
            const boost::function<void(const RedisValue &)> &handler = &dummyHandler);

	static void dummyHandler(const RedisValue &) {};

    RedisPool *_pool;

protected:
    bool stateValid() const;

private:
    boost::shared_ptr<RedisClientImpl> pimpl;

};
