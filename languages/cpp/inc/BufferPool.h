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

namespace atom {

template<typename buffer>
class pooled_buffer : public std::enable_shared_from_this<pooled_buffer<buffer>> {
public:
    pooled_buffer() : ref_counter(0){};
    pooled_buffer(const pooled_buffer&) = delete;

    virtual ~pooled_buffer(){};
    void add_ref(){
        ref_counter++;
    }

    void remove_ref(){
        if(ref_counter > 0){
            ref_counter--;
        }
        else{
            cleanup();
        }
    }

    int get_refs(){
        return ref_counter;
    }

private:

    void cleanup(){
        //TODO
    }

    buffer io_buff;
    int ref_counter;
};


enum params {
    BUFFER_CAP=20
};


template<typename buffer>
class BufferPool {

public:


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

    //initialize buffer pool
    void init(){
        for(int i = 0; i < buffers_requested; i++){
            buffers.push_back(std::make_shared<atom::pooled_buffer<buffer>>());
            num_available++;
        }
    }

    //get a buffer from the pool
    std::shared_ptr<atom::pooled_buffer<buffer>> get_buffer(){
        std::unique_lock<std::mutex> lock(mutex);
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

    //decrement a reference to a buffer instance
    void release_buffer(std::shared_ptr<atom::pooled_buffer<buffer>> buf){
        std::unique_lock<std::mutex> lock(mutex);
        buf->remove_ref();
        num_available++;
        cond_var.notify_one();
    }

    //wait until one of the buffers in the pool have zero references
    void wait_for_buffer(std::unique_lock<std::mutex> &lock){
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

    //scan for a buffer that has zero references
    bool check_available(){
        for(auto & b : buffers){
            if(b->get_refs() == 0){
                return true;
            }
        }
        return false;
    }

    int buffers_available(){
        return num_available;
    }

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