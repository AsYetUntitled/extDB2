#include <Poco/Timer.h>
#include <Poco/Data/DataException.h>

#include "redispool.h"
#include "redisclient.h"

#include "redisclientimpl.h"
#include "redisparser.h"
#include "redisvalue.h"


RedisPool::RedisPool(const std::string &address, const int &port, const std::string &password, const int &minSessions, const int &maxSessions, const int &idleTime):
	_port(port),
	_password(password),
	_minSessions(minSessions),
	_maxSessions(maxSessions),
	_janitorTimer(1000*idleTime, 1000*idleTime/4),
	_shutdown(false)
{
	boost::asio::ip::address _address = boost::asio::ip::address::from_string(address);
	Poco::TimerCallback<RedisPool> callback(*this, &RedisPool::onJanitorTimer);
	_janitorTimer.start(callback);

	// Setup ASIO Worker Pool
	io_work_ptr.reset(new boost::asio::io_service::work(io_service));
	for (int i = 0; i < 1; ++i)
	{
		threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
	}
}


RedisPool::~RedisPool()
{
	try
	{
		shutdown();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


RedisPool::Session RedisPool::get()
{
	Poco::Mutex::ScopedLock lock(_mutex);
    if (_shutdown) throw Poco::InvalidAccessException("Redis Session Pool has been shut down.");
	
	if (_idleSessions.empty())
	{
		boost::asio::io_service ioService;
		RedisPool::Session newSession = new RedisClient(ioService, this);

		if (!newSession->connect(_address, _port)) throw Poco::InvalidAccessException("Redis Session Connection Failed.");
		_idleSessions.push_front(newSession);
		++_nSessions;
	}

	_activeSessions.push_front(std::move(_idleSessions.front()));
	return _activeSessions.front();
}




void RedisPool::putBack(Session session)
{
	Poco::Mutex::ScopedLock lock(_mutex);
	if (_shutdown) return;

	for (SessionList::iterator it = _activeSessions.begin(); it != _activeSessions.end(); ++it)
	{
		if (*it == session)
		{
			_idleSessions.push_front(std::move(*it));
			/*
			if (*it->isConnected())
			{		
				session->access();
				_idleSessions.push_front(std::move(*it));
			}
			else
			{
				--_nSessions;
			};
			*/
			_activeSessions.erase(it);
			break;
		}
	}
}



void RedisPool::onJanitorTimer(Poco::Timer&)
{
	Poco::Mutex::ScopedLock lock(_mutex);
	if (_shutdown) return;

	SessionList::iterator it = _idleSessions.begin(); 
	while (_nSessions > _minSessions && it != _idleSessions.end())
	{
		/*
		if ((*it)->session->idle() > _idleTime || !(*it)->session->session()->isConnected())
		{	
			try	{ (*it)->session->session()->close(); }
			catch (...) { }
			it = _idleSessions.erase(it);
			--_nSessions;
		}
		else
		{
			++it;
		}
		*/
	}
}


void RedisPool::shutdown()
{
	Poco::Mutex::ScopedLock lock(_mutex);
	if (_shutdown) return;
	_shutdown = true;
	_janitorTimer.stop();
	closeAll(_idleSessions);
	closeAll(_activeSessions);
}


void RedisPool::closeAll(SessionList& sessionList)
{
	SessionList::iterator it = sessionList.begin(); 
	for (; it != sessionList.end();)
	{
		/*
		try	{ (*it)->session->session()->close(); }
		catch (...) { }
		it = sessionList.erase(it);
		if (_nSessions > 0) --_nSessions;
		*/
	}
}
