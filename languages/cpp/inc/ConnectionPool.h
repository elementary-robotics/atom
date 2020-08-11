////////////////////////////////////////////////////////////////////////////////
//
//  @file ConnectionPool.h
//
//  @brief Header-only implementation of the Connection Pool class
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

    using TCP_Redis = atom::Redis<boost::asio::ip::tcp::socket,
                                    boost::asio::ip::tcp::endpoint,
                                    boost::asio::streambuf,
                                    typename bredis::to_iterator<boost::asio::streambuf>::iterator_t,
                                    bredis::parsing_policy::keep_result>;
    using UNIX_Redis = atom::Redis<boost::asio::local::stream_protocol::socket,
                                    boost::asio::local::stream_protocol::endpoint,
                                boost::asio::streambuf,
                                typename bredis::to_iterator<boost::asio::streambuf>::iterator_t,
                                bredis::parsing_policy::keep_result>;

    ConnectionPool(boost::asio::io_context &iocon, int max_connections, int timeout, std::string redis_ip);

    void init(int num_unix, int num_tcp);

    virtual ~ConnectionPool(){};

    std::shared_ptr<UNIX_Redis> get_unix_connection();
    std::shared_ptr<TCP_Redis> get_tcp_connection();

    void release_connection(std::shared_ptr<UNIX_Redis>);
    void release_connection(std::shared_ptr<TCP_Redis>);

    int number_open_unix();
    int number_open_tcp();
    int number_available_unix();
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
    atom::logger logger;
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