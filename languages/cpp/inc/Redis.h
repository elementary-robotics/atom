////////////////////////////////////////////////////////////////////////////////
//
//  @file Redis.h
//
//  @brief Header-only implementation of the Redis class - wraps cpp-bredis
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_REDIS_H
#define __ATOM_CPP_REDIS_H

#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>

#include <bredis.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>

#include "Error.h"
#include "Logger.h"

using bytes_t = char;

namespace atom {

//to store redis replies read into the buffer
struct redis_reply {
    const size_t size;
    std::shared_ptr<const bytes_t *> data;
    
    redis_reply(size_t n, std::shared_ptr<const bytes_t *> p) : size(n), data(std::move(p)){}
};

template<typename socket, typename endpoint, typename buffer, typename iterator, typename policy> 
class Redis {

    public:
        //constructor for tcp socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string ip_address, 
                int port) : 
            iocon(iocon),
            ep(boost::asio::ip::address::from_string(ip_address), 
            boost::lexical_cast<std::uint16_t>(port)), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr),
            logger(&std::cout, "Redis Client") {};

        //constructor for unix socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string unix_addr): 
            iocon(iocon),
            ep(unix_addr), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr),
            logger(&std::cout, "Redis Client") {};

        //decon
        virtual ~Redis(){
            if(sock->is_open()){
                sock->close();
            }
        }

        //start async operations
        void start(atom::error & err){
                sock->async_connect(ep, 
                        std::bind(&atom::Redis<socket, endpoint, buffer, iterator, policy>::on_connect, 
                                this, err));
        }
        
        //stop async operations
        void stop(){
            logger.info("closing socket");
            sock->close();
        }

        //sync connect - TODO TEST ERROR CASE
        void connect(atom::error & err){
            sock->connect(ep, err);
            if(!err){
                wrap_socket();
            }
        }

        //sync disconnect
        void disconnect(atom::error & err){
            if(sock->is_open()){
                sock->shutdown(socket::shutdown_send, err);
                if(!err){
                    sock->close(err);
                } 
            }
        }
        
        //release the read buffer - must be called AFTER user is finished using the data in the buffer
        void release_rx_buffer(size_t size){
            rx_buff.consume(size);
        }

        // xadd operation
        const atom::redis_reply xadd(std::string stream_name, std::string field, const bytes_t * data, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", field, data }, err);
            return read_reply(err);
        }

        // xadd operation - without automatically generated ids
        const atom::redis_reply xadd(std::string stream_name, std::string id, std::string field, const bytes_t * data, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, id, field, data }, err);
            return read_reply(err);
        }

        //xrange operation
        const atom::redis_reply xrange(std::string stream_name, std::string id_start, std::string id_end, std::string count, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XRANGE" , stream_name,  id_start, id_end, "COUNT", count}, err);
            return read_reply(err);
        }

        //xgroup operation
        const atom::redis_reply xgroup(std::string stream_name, std::string consumer_group_name, std::string last_id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XGROUP" , "CREATE", stream_name,  consumer_group_name, last_id, "MKSTREAM"}, err);
            return read_reply(err);
        }
        
        //xreadgroup operation
        const atom::redis_reply xreadgroup(std::string group_name, std::string consumer_name, std::string block, std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREADGROUP" , "GROUP", group_name, consumer_name, "BLOCK", block, "COUNT", count, "STREAMS", stream_name, id}, err);
            return read_reply(err);
        }

        //xread operation
        const atom::redis_reply xread( std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREAD" , "COUNT", count, "STREAMS", stream_name, id}, err);
            return read_reply(err);
        }

        //xack operation
        const atom::redis_reply xack(std::string stream_name, std::string group_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"XACK", stream_name, group_name, id}, err);
            return read_reply(err);
        }

        //set operation - TODO: unpack and handle opt args
        const atom::redis_reply set(std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"SET", stream_name, id}, err);
            return read_reply(err);
        }
        
        //xdel operation - TODO: unpack and handle opt args
        const atom::redis_reply xdel(std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"XDEL", stream_name, id}, err);
            return read_reply(err);
        }

        //load a script 
        const atom::redis_reply load_script(std::string script_file_location, atom::error & err){
            std::ifstream script_file(script_file_location, std::ios::binary);
            std::string script = std::string((std::istreambuf_iterator<char>(script_file)), std::istreambuf_iterator<char>());

            bredis_con->write(bredis::single_command_t{"SCRIPT", "LOAD", script}, err);
            return read_reply(err);
        }
    protected:
        //wrap socket as bredis connection - must be called after successful connection to redis
        virtual void wrap_socket(){
            bredis_con = std::make_shared<bredis::Connection<socket>>(std::move(*sock));
        }

        //read reply from redis
        const atom::redis_reply read_reply(atom::error & err){
            if(!err){
                auto result_markers = bredis_con->read(rx_buff, err);
                if(!err){
                    redis_check(result_markers, err);
                    if(!err){
                        const bytes_t * data = static_cast<const bytes_t *>(rx_buff.data().data());
                        return atom::redis_reply(result_markers.consumed, std::make_shared<const bytes_t *>(data));
                    }
                }
            }   
            return atom::redis_reply(0, std::make_shared<const bytes_t *>());
        }

    private:
        //connection callback
        void on_connect(const boost::system::error_code& err){
            if(err){
                logger.error("connection was unsuccessful: " +  err.message());   
                stop();        
            } else{
                logger.info("connection to Redis was successful.");
                if(!sock->is_open()){
                    logger.error("socket is closed.");
                } else{
                    wrap_socket();
                    //do_async_xadd();
                }
            }
        }

        //do error detection for redis error messages
        void redis_check(bredis::positive_parse_result_t<iterator, policy> result_markers, atom::error & err){
            error_extractor err_extractor;
            auto redis_error = boost::apply_visitor(err_extractor, result_markers.result);
            if(!redis_error.empty()){
                err.set_redis_error(redis_error);
            }
        }


        //members
        boost::asio::io_context& iocon;
        endpoint ep;
        std::shared_ptr<socket> sock;
        std::shared_ptr<bredis::Connection<socket>> bredis_con;
        buffer tx_buff;
        buffer rx_buff;
        atom::logger logger;

        //extractor for redis errors
        struct error_extractor : public boost::static_visitor<std::string> {
            template <typename T> std::string operator() (const T & /*value*/) const {return "";}

            std::string operator()(const bredis::markers::error_t<iterator> &value) const {     
                std::string error;
                auto str = value.string;
                std::string copy{str.from, str.to};
                error += copy;
                return error;
            }
        };
};

}

#endif // __ATOM_CPP_REDIS_H
