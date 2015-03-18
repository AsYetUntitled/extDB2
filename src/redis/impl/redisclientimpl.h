/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#pragma once

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <string>
#include <vector>
#include <queue>
#include <map>

#include "../redisparser.h"


class RedisClientImpl : public std::enable_shared_from_this<RedisClientImpl> {
public:
    inline RedisClientImpl(boost::asio::io_service &ioService);
    inline ~RedisClientImpl();

    inline void handleAsyncConnect(
            const boost::system::error_code &ec,
            const boost::function<void(bool, const std::string &)> &handler);

    inline void close();

    inline void doAsyncCommand(
            const std::vector<std::string> &command,
            const boost::function<void(const RedisValue &)> &handler);

    inline void sendNextCommand();
    inline void processMessage();
    inline void doProcessMessage(const RedisValue &v);
    inline void asyncWrite(const boost::system::error_code &ec, const size_t);
    inline void asyncRead(const boost::system::error_code &ec, const size_t);

    inline void onRedisError(const RedisValue &);
    inline void defaulErrorHandler(const std::string &s);
    inline static void ignoreErrorHandler(const std::string &s);

    inline static void append(std::vector<char> &vec, const std::string &s);
    inline static void append(std::vector<char> &vec, const char *s);
    inline static void append(std::vector<char> &vec, char c);
    template<size_t size>
    inline static void append(std::vector<char> &vec, const char (&s)[size]);

    template<typename Handler>
    inline void post(const Handler &handler);

    enum 
    {
        NotConnected,
        Connected,
        Subscribed,
        Closed 
    } state;

    boost::asio::strand strand;
    boost::asio::ip::tcp::socket socket;
    RedisParser redisParser;
    std::array<char, 4096> buf;
    size_t subscribeSeq;

    typedef std::pair<size_t, boost::function<void(const std::string &s)> > MsgHandlerType;
    typedef boost::function<void(const std::string &s)> SingleShotHandlerType;

    typedef std::multimap<std::string, MsgHandlerType> MsgHandlersMap;
    typedef std::multimap<std::string, SingleShotHandlerType> SingleShotHandlersMap;

    std::queue<boost::function<void(const RedisValue &v)> > handlers;
    MsgHandlersMap msgHandlers;
    SingleShotHandlersMap singleShotMsgHandlers;

    struct QueueItem 
    {
        boost::function<void(const RedisValue &)> handler;
        boost::shared_ptr<std::vector<char> > buff;
    };

    std::queue<QueueItem> queue;

    boost::function<void(const std::string &)> errorHandler;
};

#include "redisclientimpl.cpp"