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
#include <memory>
#include <utility>

#include <bredis.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>
#include <boost/variant.hpp>

#include "Error.h"
#include "Logger.h"
#include "Parser.h"
#include "BufferPool.h"
#include "Messages.h"

using bytes_t = char;

namespace atom {



template<typename socket, typename endpoint, typename buffer, typename iterator, typename policy> 
class Redis {

    public:
        //constructor for tcp socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string ip_address, 
                int port,
                int buffers_requested,
                int timeout) : 
            iocon(iocon),
            ep(boost::asio::ip::address::from_string(ip_address), 
            boost::lexical_cast<std::uint16_t>(port)), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr),
            logger(&std::cout, "Redis Client"),
            parser(),
            buffer_pool(buffers_requested, timeout) {
                buffer_pool.init();
            };

        //constructor for unix socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string unix_addr,
                int buffers_requested,
                int timeout): 
            iocon(iocon),
            ep(unix_addr), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr),
            logger(&std::cout, "Redis Client"),
            parser(),
            buffer_pool(buffers_requested, timeout) {
                buffer_pool.init();
            };

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
            } else{
                logger.alert("Socket connection error: " + err.message());
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

        //cleanup the associated data
        void release_rx_buffer(atom::redis_reply<buffer> & reply){
            reply.cleanup();
            buffer_pool.release_buffer(reply.data_buff, reply.size);
        }

        // xadd operation
        atom::redis_reply<buffer> xadd(std::string stream_name, std::string field, const bytes_t * data, atom::error & err, int max_len = 0){
            if(max_len > 0){
                bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "MAXLEN", "~", std::to_string(max_len), "*", field, data }, err);
            }
            else{
                bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", field, data }, err);
            }
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        // xadd operation - without automatically generated ids
        atom::redis_reply<buffer> xadd(std::string stream_name, std::string id, std::string field, const bytes_t * data, atom::error & err, int max_len = 0){
            if(max_len > 0){
                bredis::single_command_t cmd = bredis::single_command_t{ "XADD", stream_name, "MAXLEN", "~", std::to_string(max_len), id, field, data };
                bredis_con->write(cmd, err);
                redis_debug(cmd.arguments);
            }else{
                bredis::single_command_t cmd = bredis::single_command_t{ "XADD", stream_name, id, field, data };
                bredis_con->write(cmd, err);
                redis_debug(cmd.arguments);
            }
            return read_reply(atom::reply_type::options::flat_pair, err);
        }
        
        // xadd operation with stringstream - used for single msgpack serialized types - TODO: determine if this call should be removed
        atom::redis_reply<buffer> xadd(std::string stream_name, std::string field, std::stringstream & data, atom::error & err){
            std::string str = data.str();
            const char * msgpack_data = str.c_str();
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", "ser", "msgpack", field, msgpack_data }, err);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        // xadd operation with vector argument
        atom::redis_reply<buffer> xadd(std::string stream_name, std::string method, std::vector<std::string>& entry_data, atom::error & err, int max_len = 0){
            std::vector<std::string> command;
            if(max_len > 0){
                command = { "XADD", stream_name, "MAXLEN", "~", std::to_string(max_len), "*", "ser", method};
            }else{
                command = {"XADD", stream_name, "*", "ser", method};
            }
            command.insert(command.end(), entry_data.begin(), entry_data.end());
            
            bredis::single_command_t cmd = bredis::single_command_t{command.cbegin(), command.cend()};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        // xadd operation with multiple key value pairs - {NOT TESTED}
        template<typename ... ArgTypes>
        atom::redis_reply<buffer> xadd(std::string stream_name, atom::error & err, ArgTypes ... args){
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", &args...}, err);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        //xrange operation
        atom::redis_reply<buffer> xrange(std::string stream_name, std::string id_start, std::string id_end, std::string count, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"XRANGE" , stream_name,  id_start, id_end, "COUNT", count};
            bredis_con->write(cmd , err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_map, err);
        }

        //xrevrange operation
        atom::redis_reply<buffer> xrevrange(std::string stream_name, std::string id_start, std::string id_end,  atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XREVRANGE" , stream_name,  id_start, id_end};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_map, err);
        }
        
        //xrevrange operation - count specified
        atom::redis_reply<buffer> xrevrange(std::string stream_name, std::string id_start, std::string id_end, std::string count, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XREVRANGE" , stream_name,  id_start, id_end, "COUNT", count};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_map, err);
        }

        //xgroup operation
        atom::redis_reply<buffer> xgroup(std::string stream_name, std::string consumer_group_name, std::string last_id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XGROUP" , "CREATE", stream_name,  consumer_group_name, last_id, "MKSTREAM"};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }
        
        //xgroup destroy operation
        atom::redis_reply<buffer> xgroup_destroy(std::string stream_name, std::string consumer_group_name, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XGROUP" , "DESTROY", stream_name,  consumer_group_name};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        //xreadgroup operation
        atom::redis_reply<buffer> xreadgroup(std::string group_name, std::string consumer_name, std::string block, std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XREADGROUP" , "GROUP", group_name, consumer_name, "BLOCK", block, "COUNT", count, "STREAMS", stream_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_maplist, err);
        }

        //xread operation
        atom::redis_reply<buffer> xread( std::vector<std::string>& streams_timestamps, atom::error & err, int block = 0){
            std::vector<std::string> command; 
            if(block > 0){
                command = {"XREAD" , "BLOCK", std::to_string(block), "STREAMS"};
            } else{
                command = {"XREAD" , "STREAMS"};
            }
            command.insert(command.end(), streams_timestamps.begin(), streams_timestamps.end());
            
            bredis::single_command_t cmd = bredis::single_command_t{command.cbegin(), command.cend()};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_maplist, err);
        }
        
        //xread operation
        atom::redis_reply<buffer> xread( std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XREAD" , "COUNT", count, "STREAMS", stream_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_maplist, err);
        }
        
        //xread operation
        atom::redis_reply<buffer> xread( std::string count, std::string block, std::string stream_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{ "XREAD" , "BLOCK", block, "COUNT", count, "STREAMS", stream_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_maplist, err);
        }

        //xack operation
        atom::redis_reply<buffer> xack(std::string stream_name, std::string group_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"XACK", stream_name, group_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        //set operation - TODO: unpack and handle opt args
        atom::redis_reply<buffer> set(std::string stream_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"SET", stream_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }
        
        //xdel operation - TODO: unpack and handle opt args
        atom::redis_reply<buffer> xdel(std::string stream_name, std::string id, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"XDEL", stream_name, id};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        //load a script 
        atom::redis_reply<buffer> load_script(std::string script_file_location, atom::error & err){
            std::ifstream script_file(script_file_location, std::ios::binary);
            std::string script = std::string((std::istreambuf_iterator<char>(script_file)), std::istreambuf_iterator<char>());
            script_file.close();
            bredis_con->write(bredis::single_command_t{"SCRIPT", "LOAD", script}, err);
            return read_reply(atom::reply_type::options::flat_pair, err);
        }

        // time operation
        atom::redis_reply<buffer> time(atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"TIME"};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::entry_map, err);
        }
        
        // keys operation
        atom::redis_reply<buffer> keys(std::string & match, atom::error & err){
            bredis::single_command_t cmd = bredis::single_command_t{"KEYS", match};
            bredis_con->write(cmd, err);
            redis_debug(cmd.arguments);
            return read_reply(atom::reply_type::options::array, err);
        }

        // helper function for tokenizing redis replies
        std::vector<std::string> tokenize(std::string s, std::string delimiter) {
            size_t pos_start = 0, pos_end, delim_len = delimiter.length();
            std::string token;
            std::vector<std::string> res;

            while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
                token = s.substr(pos_start, pos_end - pos_start);
                pos_start = pos_end + delim_len;
                res.push_back(token);
            }

            if(!s.substr(pos_start).empty()){
                res.push_back(s.substr(pos_start));
            }
            return res;
        }

    protected:
        //wrap socket as bredis connection - must be called after successful connection to redis
        virtual void wrap_socket(){
            bredis_con = std::make_shared<bredis::Connection<socket>>(std::move(*sock));
        }

        //read reply from redis
        atom::redis_reply<buffer> read_reply(atom::reply_type::options parse_option, atom::error & err, bool process_resp=true){
            if(!err){
                auto pooled_buffer = buffer_pool.get_buffer();
                auto result_markers = bredis_con->read(pooled_buffer->io_buff, err);
                if(!err){
                    redis_check(result_markers, err);
                    if(!err){
                        const bytes_t * data = static_cast<const bytes_t *>(pooled_buffer->io_buff.data().data());
                        logger.debug("raw data: " + std::string(data));
                        if(process_resp){
                            atom::reply_type::parsed_reply parsed = parser.process(pooled_buffer->io_buff, parse_option, err);
                            return atom::redis_reply<buffer>(result_markers.consumed, 
                                        std::make_shared<atom::pooled_buffer<buffer>>(pooled_buffer), parsed);
                        }
                        return atom::redis_reply<buffer>(result_markers.consumed, 
                                std::make_shared<atom::pooled_buffer<buffer>>(pooled_buffer));
                    }
                    logger.error(err.redis_error());  
                }
            }
            logger.error(err.message());   
            return atom::redis_reply<buffer>(0, std::make_shared<atom::pooled_buffer<buffer>>());
        }

        void redis_debug(bredis::args_container_t & args){
            std::string debug_str;
            for(auto & arg: args){
                debug_str += (arg.to_string() + " ");
            }
            logger.debug(debug_str);
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
        Logger logger;
        Parser<buffer> parser;
        BufferPool<buffer> buffer_pool;

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
