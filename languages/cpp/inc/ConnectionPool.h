////////////////////////////////////////////////////////////////////////////////
//
//  @file ConnectionPool.h
//
//  @brief implementation of the Connection Pool class
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_CONNECTIONPOOL_H
#define __ATOM_CPP_CONNECTIONPOOL_H

#include <boost/asio.hpp>

#include "Redis.h"

namespace atom{

class ConnectionPool {

public:

    ///TCP_Redis
    ///templated Redis type for TCP connections
    using TCP_Redis = atom::Redis<boost::asio::ip::tcp::socket,
                                    boost::asio::ip::tcp::endpoint,
                                    boost::asio::streambuf,
                                    typename bredis::to_iterator<boost::asio::streambuf>::iterator_t,
                                    bredis::parsing_policy::keep_result>;
    ///UNIX_Redis
    ///templated Redis type for UNIX connections
    using UNIX_Redis = atom::Redis<boost::asio::local::stream_protocol::socket,
                                    boost::asio::local::stream_protocol::endpoint,
                                boost::asio::streambuf,
                                typename bredis::to_iterator<boost::asio::streambuf>::iterator_t,
                                bredis::parsing_policy::keep_result>;

    ///Constructor for ConnectionPool
    ///@param iocon io context
    ///@param max_connections maximum number of connections to make to Redis Server
    ///@param timeout time in milliseconds to wait for a connection to be available for use
    ///@param redis_ip IP address for Redis Server (IP of nucleus docker container)
    ConnectionPool(boost::asio::io_context &iocon, int max_connections, int timeout, std::string redis_ip);

    ///Constructor for ConnectionPool
    ///@param iocon io context
    ///@param max_connections maximum number of connections to make to Redis Server
    ///@param timeout time in milliseconds to wait for a connection to be available for use
    ///@param redis_ip IP address for Redis Server (IP of nucleus docker container)
    ///@param logstream stream to which to publish log messages to
    ///@param logger_name name of log with which to identify messages that originate from ConnectionPool
    ConnectionPool(boost::asio::io_context &iocon, int max_connections, int timeout, std::string redis_ip, 
                    std::ostream & logstream, std::string logger_name);

    ///Initializes the connection pool, creates the number of unix and tcp connections requested. 
    //Throws if combined number of connections requested is more than max_connections
    ///@param num_unix number of unix connections to initialize
    ///@param num_tcp number of tcp connections to initialize
    void init(int num_unix, int num_tcp);

    virtual ~ConnectionPool(){};

    ///Get a unix connection from the connection pool
    ///@return shared pointer to instance of UNIX_Redis
    std::shared_ptr<UNIX_Redis> get_unix_connection();
    
    ///Get a tcp connection from the connection pool
    ///@return shared pointer to instance of TCP_Redis
    std::shared_ptr<TCP_Redis> get_tcp_connection();

    ///Get a unix or tcp connection from the connection pool. Will throw if unsupported connection type is requested.
    ///@tparam ConnectionType can be UNIX_Redis or TCP_Redis
    ///@return shared pointer to instance of UNIX_Redis or TCP_Redis
    template<typename ConnectionType> 
    std::shared_ptr<ConnectionType> get_connection() {
        logger.emergency("Unable to create desired conection type. Unsupported.");
            throw std::runtime_error("Unsupported connection type requested from Connection Pool.");
    };

    ///Release a unix connection back to the connection pool
    ///@param con shared pointer to the UNIX_Redis to release
    void release_connection(std::shared_ptr<UNIX_Redis> con);
    
    ///Release a tcp connection back to the connection pool
    ///@param con shared pointer to the TCP_Redis to release
    void release_connection(std::shared_ptr<TCP_Redis> con);

    ///Get the total number of unix connections in the connection pool
    ///@return integer
    int number_open_unix();

    ///Get the total number of tcp connections in the connection pool
    ///@return integer
    int number_open_tcp();

    ///Get the total number of unix connections in the connection pool that are available for use
    ///@return integer
    int number_available_unix();

    ///Get the total number of tcp connections in the connection pool that are available for use
    ///@return integer
    int number_available_tcp();
    
private:
    //cleanup 
    void cleanup();
    void resize_unix();
    void resize_tcp();

    //create one unix connection
    virtual std::shared_ptr<UNIX_Redis> make_unix();

    //queue many unix connections 
    void make_unix_connections(int num_unix);

    //create one tcp connection
    virtual std::shared_ptr<TCP_Redis> make_tcp(std::string redis_ip);

    //qeue many tcp connections
    void make_tcp_connections(int num_tcp);

    //wait for a connection to be released back into the pool for use
    void wait_for_tcp_released(std::unique_lock<std::mutex> &lock);
    void wait_for_unix_released(std::unique_lock<std::mutex> &lock);


    boost::asio::io_context& iocon;
    Logger logger;
    int max_connections;
    std::chrono::milliseconds timeout;
    std::string redis_ip;
    int open_tcp_connections;
    int open_unix_connections;
    std::deque<std::shared_ptr<UNIX_Redis>> unix_connections;
    std::deque<std::shared_ptr<TCP_Redis>> tcp_connections;
    std::mutex mutex;
    std::condition_variable cond_var;

};

}

#endif //__ATOM_CPP_CONNECTIONPOOL_H