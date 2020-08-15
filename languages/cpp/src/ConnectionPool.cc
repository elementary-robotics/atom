////////////////////////////////////////////////////////////////////////////////
//
//  @file ConnectionPool.cc
//
//  @brief Connection Pool implementation
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#include "ConnectionPool.h"

#include<iostream>
#include <functional>


atom::ConnectionPool::ConnectionPool(boost::asio::io_context &iocon, 
                                     int max_connections, 
                                     int timeout,
                                     std::string redis_ip) : iocon(iocon),
                                                            logger(&std::cout, "Connection Pool"),
                                                            max_connections(max_connections),
                                                            timeout(timeout),
                                                            redis_ip(redis_ip),
                                                            open_tcp_connections(0),
                                                            open_unix_connections(0){ }

atom::ConnectionPool::ConnectionPool(boost::asio::io_context &iocon, 
                                     int max_connections, 
                                     int timeout,
                                     std::string redis_ip,
                                     std::ostream & logstream,
                                     std::string logger_name) : iocon(iocon),
                                                            logger(&logstream, logger_name),
                                                            max_connections(max_connections),
                                                            timeout(timeout),
                                                            redis_ip(redis_ip),
                                                            open_tcp_connections(0),
                                                            open_unix_connections(0){ }

void atom::ConnectionPool::init(int num_unix, int num_tcp){
    if(num_tcp + num_unix > max_connections){
        throw std::runtime_error("Number of maximum connections to Redis exceeded by the combined total of unix and tcp connections requested.");
    }
    //initialize tcp connections
    make_tcp_connections(num_tcp);
    //initialize unix connections
    make_unix_connections(num_unix);
}

int atom::ConnectionPool::number_open_unix(){
    return open_unix_connections;
}

int atom::ConnectionPool::number_open_tcp(){
    return open_tcp_connections;
}

int atom::ConnectionPool::number_available_unix(){
    return unix_connections.size();
}

int atom::ConnectionPool::number_available_tcp(){
    return tcp_connections.size();
}

std::shared_ptr<atom::ConnectionPool::UNIX_Redis> atom::ConnectionPool::get_unix_connection(){
    std::unique_lock<std::mutex> lock(mutex);
    if(unix_connections.empty()){
        if(open_unix_connections + open_tcp_connections < max_connections){
            resize_unix();
        } else{
            //wait for a connection to be released
            wait_for_unix_released(lock);
        }
    }
    //ensure the connection pool isn't empty
    assert(!unix_connections.empty());

    //pop off and return the connection from the beginning of the queue
    auto unix_connection = std::move(unix_connections.front());
    unix_connections.pop_front();
    lock.unlock();
    return unix_connection;
}

std::shared_ptr<atom::ConnectionPool::TCP_Redis> atom::ConnectionPool::get_tcp_connection(){
    std::unique_lock<std::mutex> lock(mutex);
    if(tcp_connections.empty()){
        if(open_unix_connections + open_tcp_connections < max_connections){
            resize_tcp();            
        } else{
            //wait for a connection to be released
            wait_for_tcp_released(lock);
        }
    }
    //ensure the connection pool isn't empty
    assert(!tcp_connections.empty());

    //pop off and return the connection from the beginning of the queue
    auto tcp_connection = std::move(tcp_connections.front());
    tcp_connections.pop_front();
    lock.unlock();
    return tcp_connection;
}


void atom::ConnectionPool::release_connection(std::shared_ptr<atom::ConnectionPool::UNIX_Redis> connection){
    std::lock_guard<std::mutex> lock(mutex);
    unix_connections.push_back(std::move(connection));
    cond_var.notify_one();
}

void atom::ConnectionPool::release_connection(std::shared_ptr<atom::ConnectionPool::TCP_Redis> connection){
    std::lock_guard<std::mutex> lock(mutex);
    tcp_connections.push_back(std::move(connection));
    cond_var.notify_one();
}

std::shared_ptr<atom::ConnectionPool::UNIX_Redis>  atom::ConnectionPool::make_unix(){
    return std::make_shared<atom::ConnectionPool::UNIX_Redis>(iocon, "/shared/redis.sock");
}

std::shared_ptr<atom::ConnectionPool::TCP_Redis>  atom::ConnectionPool::make_tcp(std::string redis_ip){
    return std::make_shared<atom::ConnectionPool::TCP_Redis>(iocon, redis_ip, 6379);
}

void atom::ConnectionPool::resize_unix(){
    //currently doubling the pool
    make_unix_connections(open_unix_connections);
}

void atom::ConnectionPool::resize_tcp(){
    //currently doubling the pool
    make_tcp_connections(open_tcp_connections);
}

void atom::ConnectionPool::make_unix_connections(int num_unix){
    atom::error unix_error;
    for(int i = 0; i < num_unix; i++){
        auto connection =  make_unix();
        connection->connect(unix_error);
        unix_connections.push_back(connection);
        if(unix_error){
            logger.alert("Unable to connect UNIX socket to Redis Server");
            unix_error.clear();
        } else{
            open_unix_connections++;
        }
    }
}

void atom::ConnectionPool::make_tcp_connections(int num_tcp){
    atom::error tcp_error;
    for(int i = 0; i < num_tcp; i++){
        auto connection = make_tcp(redis_ip);
        connection->connect(tcp_error);
        tcp_connections.push_back(connection);
        if(tcp_error){
            logger.alert("Unable to connect TCP socket to Redis Server");
            tcp_error.clear();
        } else{
            open_tcp_connections++;
        }
    }
}

void atom::ConnectionPool::wait_for_tcp_released(std::unique_lock<std::mutex> &lock){
    if(timeout > std::chrono::milliseconds(0)){
        if(!cond_var.wait_for(lock, timeout, [this] {return !(this->tcp_connections.empty()); })){
            std::string message = "No available TCP connections were released in " + std::to_string(timeout.count()) + " milliseconds";
            logger.emergency(message);
            throw std::runtime_error(message);
        }
    } else{
        cond_var.wait(lock, [this] {return !(this->tcp_connections.empty()); });
    }
}

void atom::ConnectionPool::wait_for_unix_released(std::unique_lock<std::mutex> &lock){
    if(timeout > std::chrono::milliseconds(0)){
        if(!cond_var.wait_for(lock, timeout, [this] {return !(this->unix_connections.empty()); })){
            std::string message = "No available UNIX connections were released in " + std::to_string(timeout.count()) + " milliseconds";
            logger.emergency(message);
            throw std::runtime_error(message);
        }
    } else{
        cond_var.wait(lock, [this] {return !(this->unix_connections.empty()); });
    }
}

template<> 
std::shared_ptr<atom::ConnectionPool::UNIX_Redis> atom::ConnectionPool::get_connection<atom::ConnectionPool::UNIX_Redis>() {
    return get_unix_connection();
}

template<> 
std::shared_ptr<atom::ConnectionPool::TCP_Redis> atom::ConnectionPool::get_connection<atom::ConnectionPool::TCP_Redis>() {
    return get_tcp_connection();
}