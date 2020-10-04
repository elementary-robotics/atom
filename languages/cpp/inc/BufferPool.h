////////////////////////////////////////////////////////////////////////////////
//
//  @file BufferPool.h
//
//  @brief implementation of the Buffer Pool class
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_BUFFERPOOL_H
#define __ATOM_CPP_BUFFERPOOL_H

#include <iostream>
#include <stdexcept>
#include <functional>
#include <memory>
#include <list>

#include <boost/asio.hpp>

#include "Logger.h"
#include "config.h"
#include "Messages.h"

namespace atom {




template<typename buffer>
class BufferPool {

public:

///Constructor for BufferPool, which holds a maximum of 20 buffers that can be used concurrently for separate read/write operations.
///This will throw if buffers_requested is greater than 20.
///@tparam buffer refers to the underlying buffer type, i.e. boost::asio::streambuf.
///@param buffers_requested the number of buffers to initialize BufferPool with
///@param timeout timeout in milliseconds to wait for a buffer to become available for use
BufferPool(int buffers_requested, int timeout) : buffers_requested(buffers_requested),
                                            num_available(0),
                                            timeout(std::chrono::milliseconds(timeout)),
                                            logger(&std::cout, "BufferPool"){
        if(buffers_requested > atom::params::BUFFER_CAP){
            std::string message = "Maximum number of buffers in pool is limited to " + std::to_string(atom::params::BUFFER_CAP);
            logger.emergency(message);
            throw std::runtime_error(message);
        }
}

///Constructor for BufferPool, which holds a maximum of 20 buffers that can be used concurrently for separate read/write operations.
///This will throw if buffers_requested is greater than 20.
///@tparam buffer refers to the underlying buffer type, i.e. boost::asio::streambuf.
///@param buffers_requested the number of buffers to initialize BufferPool with
///@param timeout timeout in milliseconds to wait for a buffer to become available for use
///@param logstream stream to which to publish log messages to
///@param logger_name name of log with which to identify messages that originate from BufferPool
BufferPool(int buffers_requested, int timeout, std::ostream & logstream, std::string logger_name): 
                                            buffers_requested(buffers_requested),
                                            num_available(0),
                                            timeout(std::chrono::milliseconds(timeout)),
                                            logger(logstream, logger_name){
        if(buffers_requested > atom::params::BUFFER_CAP){
            std::string message = "Maximum number of buffers in pool is limited to " + std::to_string(atom::params::BUFFER_CAP);
            logger.emergency(message);
            throw std::runtime_error(message);
        }
}

virtual ~BufferPool(){};

///Initializes the buffer pool,
///creates the buffers requested.
void init(){
    for(unsigned int i = 0; i < buffers_requested; i++){
        buffers.push_back(std::make_shared<atom::pooled_buffer<buffer>>());
        num_available++;
    }
}

///Get a buffer from the pool.
///@return a shared pointer to a pooled buffer
std::shared_ptr<atom::pooled_buffer<buffer>> get_buffer(){
    std::unique_lock<std::mutex> lock(mutex);
    logger.debug("get_buffer()");
    if(!check_available()){
        //if not available, and below max limit, make a new one
        if(count_buffers() < atom::params::BUFFER_CAP){
            auto buf = std::make_shared<atom::pooled_buffer<buffer>>();
            buf->add_ref();
            buffers.push_back(buf);
            lock.unlock();
            return buf;
        } else{
        //if none available, but cannot resize
            wait_for_buffer(lock);
        }
    }

    //ensure a buffer is available
    assert(check_available());

    //return a buffer, if available
    for(auto & buf : buffers){
        if(buf->get_refs() == 0){
            buf->add_ref();
            num_available--;
            lock.unlock();
            return buf;
        }
    }
}

///Decrement a reference to a buffer instance.
///After releasing a buffer, that buffer is not guaranteed to be available for use, and the user must call get_buffer() to get a buffer to work with.
///@param buf a shared pointer to the pooled buffer to decrement
void release_buffer(std::shared_ptr<atom::pooled_buffer<buffer>> buf, size_t size){
    std::unique_lock<std::mutex> lock(mutex);
    buf->consume(size);
    buf->remove_ref();
    num_available++;
    cond_var.notify_one();
}

///wait until one of the pooled buffers is available for use, determined by whether a buffer reaches zero references
///@param lock reference to a unique lock
void wait_for_buffer(std::unique_lock<std::mutex> &lock){
    logger.debug("wait_for_buffer(lock)");
    if(timeout > std::chrono::milliseconds(0)){
        if(!cond_var.wait_for(lock, timeout, [this] {return (this->check_available()); })){
            std::string message = "No available buffers were released in " + std::to_string(timeout.count()) + " milliseconds";
            logger.emergency(message);
            throw std::runtime_error(message);
        }
    } else{
        cond_var.wait(lock, [this] {return (this->check_available()); });
    }
}

///scan for a pooled buffer that is available (has zero references)
///@return boolean indicating whether there is at least one buffer in the pool available for use
bool check_available(){
    logger.debug("check_available()");
    for(auto & b : buffers){
        if(b->get_refs() == 0){
            logger.debug("buffer available");
            return true;
        }
    }
    logger.debug("buffer not available");
    return false;
}

///indicates number of buffers that are available for use
///@return integer
int buffers_available(){
    return num_available;
}

///indicates number of buffers that are in BufferPool
///@return integer
int count_buffers(){
    return buffers.size();
}

private:
    std::list<std::shared_ptr<atom::pooled_buffer<buffer>>> buffers;
    unsigned int buffers_requested;
    unsigned int num_available;
    std::chrono::milliseconds timeout;
    Logger logger;

    std::mutex mutex;
    std::condition_variable cond_var;

};

}
#endif //__ATOM_CPP_BUFFERPOOL_H