# pragma once

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>

#include <Poco/AutoPtr.h>
#include <Poco/SharedPtr.h>
#include <Poco/Timer.h>

#include "redisclient.h"
#include "redisclientimpl.h"
#include "redisparser.h"
#include "redisvalue.h"

class RedisClient;

class RedisPool
{
public:
	typedef Poco::SharedPtr<RedisClient> Session;

	RedisPool(const std::string &address, const int &port, const std::string &password, const int &minSessions, const int &maxSessions, const int &idleTime);

	~RedisPool();
		
	Session get();
	
	void shutdown();
		/// Shuts down the session pool.
	
	void putBack(Session);

protected:
	typedef std::list<Session> SessionList;

	void onJanitorTimer(Poco::Timer&);

private:
	RedisPool(const RedisPool&);
	RedisPool& operator = (const RedisPool&);

	void closeAll(SessionList& sessionList);

	boost::asio::ip::address    _address;
	int				            _port;
	std::string   				_password;
	int            				_minSessions;
	int            				_maxSessions;
	int            				_idleTime;
	bool           				_shutdown;

	int							_nSessions;

	SessionList _idleSessions;
	SessionList _activeSessions;

	Poco::Timer _janitorTimer;
	
	Poco::Mutex _mutex;

	// ASIO Thread Queue
	std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
	boost::asio::io_service io_service;
	boost::mutex mutex_io_service;
	boost::thread_group threads;

	friend class RedisClient;
};