/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */


#pragma once

#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <string>
#include <vector>
#include <queue>
#include <map>

#include "redisclient.h"
#include "redisparser.h"


class RedisClientImpl : public boost::enable_shared_from_this<RedisClientImpl> {
public:
    RedisClientImpl(boost::asio::io_service &ioService);
    ~RedisClientImpl();

    void handleAsyncConnect(
            const boost::system::error_code &ec,
            const boost::function<void(bool, const std::string &)> &handler);

    void close();

    void doCommand(
            const std::vector<std::string> &command,
            const boost::function<void(const RedisValue &)> &handler);

    void sendNextCommand();
    void processMessage();
    void doProcessMessage(const RedisValue &v);
    void asyncWrite(const boost::system::error_code &ec, const size_t);
    void asyncRead(const boost::system::error_code &ec, const size_t);

    void onRedisError(const RedisValue &);
    void onError(const std::string &s);
    void defaulErrorHandler(const std::string &s);

    static void append(std::vector<char> &vec, const std::string &s);
    static void append(std::vector<char> &vec, const char *s);
    static void append(std::vector<char> &vec, char c);
    template<size_t size>
    static void append(std::vector<char> &vec, const char s[size]);

    template<typename Handler> void post(const Handler &handler)
    {
        strand.post(handler);
    };

    enum {
        NotConnected,
        Connected,
        Subscribed,
        Closed 
    } state;

    boost::asio::strand strand;
    boost::asio::ip::tcp::socket socket;
    RedisParser redisParser;
    boost::array<char, 4096> buf;
    size_t subscribeSeq;

    typedef std::pair<size_t, boost::function<void(const std::string &s)> > MsgHandlerType;
    typedef boost::function<void(const std::string &s)> SingleShotHandlerType;

    typedef std::multimap<std::string, MsgHandlerType> MsgHandlersMap;
    typedef std::multimap<std::string, SingleShotHandlerType> SingleShotHandlersMap;

    std::queue<boost::function<void(const RedisValue &v)> > handlers;
    MsgHandlersMap msgHandlers;
    SingleShotHandlersMap singleShotMsgHandlers;

    struct QueueItem {
        boost::function<void(const RedisValue &)> handler;
        boost::shared_ptr<std::vector<char> > buff;
    };

    std::queue<QueueItem> queue;

    boost::function<void(const std::string &)> errorHandler;
};
