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
    std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> data_map;
    
    redis_reply(size_t n, std::shared_ptr<const bytes_t *> p) : size(n), data(std::move(p)){}
    redis_reply(size_t n, std::shared_ptr<const bytes_t *> p, 
                std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>) : size(n), 
                                                                                        data(std::move(p)),
                                                                                        data_map(std::move(data_map)){}

    //release the ownership of shared pointers
    void cleanup(){
        data.reset();
        for(auto it = data_map.begin(); it != data_map.end(); it++){
            auto vec = it->second;
            for(auto in = vec.begin(); in != vec.end(); in++){
                in->first.reset();
            }
        }
    }
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

        //cleanup the associated data
        void release_rx_buffer(atom::redis_reply & reply){
            reply.cleanup();
            rx_buff.consume(reply.size);
        }

        // xadd operation
        atom::redis_reply xadd(std::string stream_name, std::string field, const bytes_t * data, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", field, data }, err);
            return read_reply(field, err);
        }

        // xadd operation - without automatically generated ids
        atom::redis_reply xadd(std::string stream_name, std::string id, std::string field, const bytes_t * data, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, id, field, data }, err);
            return read_reply(field, err);
        }
        
        // xadd operation with stringstream - used for msgpack serialized types
        atom::redis_reply xadd(std::string stream_name, std::string field, std::stringstream & data, atom::error & err){
            std::string str = data.str();
            const char * msgpack_data = str.c_str();
            bredis_con->write(bredis::single_command_t{ "XADD", stream_name, "*", field, msgpack_data }, err);
            return read_reply(field, err);
        }

        //xrange operation
        atom::redis_reply xrange(std::string stream_name, std::string id_start, std::string id_end, std::string count, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XRANGE" , stream_name,  id_start, id_end, "COUNT", count}, err);
            return read_reply(stream_name, err);
        }

        //xrevrange operation
        atom::redis_reply xrevrange(std::string stream_name, std::string id_start, std::string id_end,  atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREVRANGE" , stream_name,  id_start, id_end}, err);
            return read_reply(stream_name, err);
        }
        
        //xrevrange operation - count specified
        atom::redis_reply xrevrange(std::string stream_name, std::string id_start, std::string id_end, std::string count, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREVRANGE" , stream_name,  id_start, id_end, "COUNT", count}, err);
            return read_reply(stream_name, err);
        }

        //xgroup operation
        atom::redis_reply xgroup(std::string stream_name, std::string consumer_group_name, std::string last_id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XGROUP" , "CREATE", stream_name,  consumer_group_name, last_id, "MKSTREAM"}, err);
            return read_reply(consumer_group_name, err);
        }
        
        //xreadgroup operation
        atom::redis_reply xreadgroup(std::string group_name, std::string consumer_name, std::string block, std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREADGROUP" , "GROUP", group_name, consumer_name, "BLOCK", block, "COUNT", count, "STREAMS", stream_name, id}, err);
            return read_reply(consumer_name, err);
        }

        //xread operation
        atom::redis_reply xread( std::string count, std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{ "XREAD" , "COUNT", count, "STREAMS", stream_name, id}, err);
            return read_reply(id, err);
        }

        //xack operation
        atom::redis_reply xack(std::string stream_name, std::string group_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"XACK", stream_name, group_name, id}, err);
            return read_reply(id, err);
        }

        //set operation - TODO: unpack and handle opt args
        atom::redis_reply set(std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"SET", stream_name, id}, err);
            return read_reply(id, err);
        }
        
        //xdel operation - TODO: unpack and handle opt args
        atom::redis_reply xdel(std::string stream_name, std::string id, atom::error & err){
            bredis_con->write(bredis::single_command_t{"XDEL", stream_name, id}, err);
            return read_reply(id, err);
        }

        //load a script 
        atom::redis_reply load_script(std::string script_file_location, atom::error & err){
            std::ifstream script_file(script_file_location, std::ios::binary);
            std::string script = std::string((std::istreambuf_iterator<char>(script_file)), std::istreambuf_iterator<char>());
            script_file.close();
            bredis_con->write(bredis::single_command_t{"SCRIPT", "LOAD", script}, err);
            return read_reply(script_file_location, err, false);
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

        //print map during debug
        void map_dbg(std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> pointers_map){
            logger.debug("---------Validate Redis Reply Map ---------");
            for(auto& m: pointers_map){
                logger.debug("...........begin................");
                auto map = m.second;
                logger.debug("KEY: "+ m.first);
                for(auto &p: map){
                    logger.debug("\tDATA: "+ std::string(*(p.first),p.second) +", SIZE: "+std::to_string(p.second));
                }
                logger.debug("............end................");

            }
        }

        //read reply from redis
        atom::redis_reply read_reply(std::string key, atom::error & err, bool process_resp=true){
            if(!err){
                auto result_markers = bredis_con->read(rx_buff, err);
                if(!err){
                    redis_check(result_markers, err);
                    if(!err){
                        const bytes_t * data = static_cast<const bytes_t *>(rx_buff.data().data());
                        if(process_resp){
                            std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> pointers_map = process(key, rx_buff, err);
                            map_dbg(pointers_map);
                        }
                        return atom::redis_reply(result_markers.consumed, std::make_shared<const bytes_t *>(data));
                    }
                    logger.error(err.redis_error());  
                }
            }
            logger.error(err.message());   
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
        Logger logger;

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

        //parser for redis replies - maps key to map of pointers to underlying buffer and associated data length
        std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>
            process(std::string key, const buffer &buff, atom::error & err){
                using iterator_t = boost::asio::buffers_iterator<
                            typename buffer::const_buffers_type, char>;
                std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> parsed_map;
                std::vector<std::pair<std::shared_ptr<const char *>, size_t>> inner_data;
                auto end = iterator_t::end(buff.data());

                if(iterator_t::begin(buff.data()) == end){
                    logger.debug("Empty response from Redis.");
                    return parsed_map;
                }

                bool break_out = false;
                for(auto it = iterator_t::begin(buff.data()); it != end; ++it){
                    switch(*it) {
                    case '+':  // simple string
                    case '-':  // nill or error
                    case ':': { // integer
                        it++; //move past indicator char
                        size_t str_len = find_data(it, end);
                        std::shared_ptr<const char *> str = std::make_shared<const char *>(&*(it));
                        inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(str, str_len)); 
                        it += (str_len);
                        break;
                    }
                    case '$': {// bulk string - will carry on until /r/n is hit
                        it++; //move past the indicator char
                        std::tuple<size_t,size_t> sizes = find_data_len(it, end); // <num_bytes, num characters in data string>
                        size_t num_bytes = std::get<0>(sizes);
                        size_t data_len = std::get<1>(sizes);
                        std::shared_ptr<const char *> data = std::make_shared<const char *>(&*(it+=num_bytes));
                        inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(data, data_len));
                        it+= (data_len); //move pointer past the data string
                        break;
                    }
                    case '*': { //array
                        it++; //move past the indicator char
                        std::tuple<size_t,size_t> sizes = find_data_len(it, end);
                        size_t num_bytes = std::get<0>(sizes);
                        int num_elems = std::get<1>(sizes); // num elems in array
                        it+=num_bytes; //move past the chars that indicate # of array elements
                        size_t data_len = process_array(it, iterator::end(buff.data()), num_elems, parsed_map);
                        it += (data_len);
                        if(it==end){
                            break_out = true;
                        }
                        break;
                    }
                    }
                    if(break_out) { break; };
                }
                if(!inner_data.empty()){
                    parsed_map.insert({key, inner_data});
                }
                return parsed_map;
            }  

        //parser for arrays
        size_t process_array(const iterator &begin, const iterator &end, int num_elems,
                    std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> & parsed_map) {
            bool id_read = false;
            std::string id;
            size_t total_data_len = 0;
            std::vector<std::pair<std::shared_ptr<const char *>, size_t>> inner_data;
            for(int entry=0; entry < num_elems; entry++){ //iterate through the entries
                for(auto it = begin; it != end; it++){
                    switch(*it) {
                    case '+':
                    case '-':
                    case ':': {
                        it++; //move past indicator char
                        size_t str_len = find_data(it, end);
                        std::shared_ptr<const char *> str = std::make_shared<const char *>(&*(it));
                        inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(str, str_len)); 
                        total_data_len+=str_len;
                        it += (str_len);
                        break;
                    }
                    case '$': { //is a bulk string - will carry on until /r/n is hit
                        it++; //move past the indicator char
                        std::tuple<size_t,size_t> sizes = find_data_len(it, end);
                        size_t num_bytes = std::get<0>(sizes); //offset where the data starts from current iter
                        size_t data_len = std::get<1>(sizes); //num characters in data string
                        total_data_len+=data_len;
                        std::shared_ptr<const char *> data = std::make_shared<const char *>(&*(it+=num_bytes));
                                                
                        if(!id_read){
                            id = std::string(*data, data_len);
                            id_read=true;
                        } /* else{ */
                            inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(data, data_len));
                        /* } */
                        it+= (data_len); //move pointer past the data string
                        //break; //TODO test why this causes failures
                    }
                    default:
                        total_data_len+=1;
                    }
                }
            }
            if(!inner_data.empty()){
                parsed_map.insert({id, inner_data});
            }
            return total_data_len;            
        }

        //returns number of bytes the data length is stored in, and the value of the data length itself
        std::tuple<size_t, size_t> find_data_len(const iterator &begin, const iterator &end){
            size_t data_len, size = 0;
            std::stringstream len;
            for(auto it = begin; it != end; it++){
                if(*it != '\r'){
                    size++;
                    len << *it;
                } else{
                    data_len = size_t(std::stoul(len.str()));
                    size+=2; //move it past the \r\n chars
                    return std::tuple<size_t, size_t>(size, data_len);
                }
            }
            return std::tuple<size_t, size_t>(size, data_len);
        }

        //used for resp types that do not provide data length before the data
        size_t find_data(const iterator &begin, const iterator &end){
            size_t size = 0;
            for(auto it = begin; it != end; it++){
                if(*it != '\r'){
                    size++;
                } else{
                    return size;
                }
            }
            return size;
        }

};

}

#endif // __ATOM_CPP_REDIS_H
