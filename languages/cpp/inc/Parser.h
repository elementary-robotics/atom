////////////////////////////////////////////////////////////////////////////////
//
//  @file Parser.h
//
//  @brief Header-only implementation of the Parser class
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <memory>
#include <map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/variant.hpp>

#include "Error.h"
#include "Logger.h"

#ifndef __ATOM_CPP_PARSER_H
#define __ATOM_CPP_PARSER_H

namespace atom {

namespace reply_type {
    ///flat response: holds replies for XADD, XDEL, SET, XACK, etc.
    ///The shared pointer points to buffer where the data begins, and size_t indicates the number of characters in the data.
    using flat_response = std::pair<std::shared_ptr<const char *>, size_t>;

    ///entry response: holds replies from XRANGE, XREVRANGE, etc. 
    ///map key is the Redis ID of the entry,
    ///vector of flat_responses hold pointers to Redis keys and values with associated data sizes
    using entry_response = std::map<std::string, std::vector<flat_response>>;

    ///entry response list: holds replies from XREAD, XREADGROUP, etc.
    ///vector of entry maps, indexed by streams requested in outgoing command to Redis
    using entry_response_list = std::vector<entry_response>;

    ///parsed reply: boost::variant of flat_response, entry_response, or entry_response_list
    ///used as return type by Parser::process
    using parsed_reply = boost::variant<flat_response, entry_response, entry_response_list>;

    ///Parsing options
    enum options {
        flat_pair = 0, ///< flat pair
        entry_map = 1, ///< map of entries
        entry_maplist = 2 ///< vector of entry maps
    };
}


template<typename buffer>
class Parser {

using buf_iter = boost::asio::buffers_iterator<
                        typename buffer::const_buffers_type, char>;

public:

    ///Constructor for Parser.
    ///default logger stream set to std out and default logger name is "Parser"
    Parser() : logger(&std::cout, "Parser"){};

    ///Constructor for Parser.
    ///@param logstream stream to which to publish log messages to
    ///@param logger_name name of log with which to identify messages that originate from Parser
    Parser(std::ostream & logstream, std::string logger_name): logger(&logstream, logger_name){};
    virtual ~Parser(){};


    ///process raw buffer
    ///@param buff buffer to parse
    ///@param parse_option desired parsing type
    ///@param err holds errors that may occur during parsing
    ///@return parsed object, variant of atom::reply_type::flat_response, atom::reply_type::entry_response, 
    ///or atom::reply_type::entry_response_list, depending on outgoing Redis command.
    atom::reply_type::parsed_reply 
    process(const buffer &buff, atom::reply_type::options parse_option, atom::error & err){
      
      auto begin = buf_iter::begin(buff.data());
      auto end = buf_iter::end(buff.data());

      switch(static_cast<atom::reply_type::options>(parse_option))
      {
          case atom::reply_type::options::flat_pair: 
          {
              auto resp =  process_flat(buff, err);
              flat_dbg(resp);
              return resp;
          }
          case atom::reply_type::options::entry_map: 
          {
              auto resp = process_entry(begin, end, err);
              map_dbg(resp);
              return resp;
          }
          case atom::reply_type::options::entry_maplist: 
          {
              auto resp = process_entrylist(begin, end, err);
              maplist_dbg(resp);
              return resp;
              break;
          }
          default:
          {
            logger.emergency("Unsupported reply type requested.");
            throw std::runtime_error("Unsupported reply type requested.");
          }

      }
    }

    ///Debug helper: logs contents of vector of entry maps
    ///@param pointers_map object to debug
    void maplist_dbg(std::vector<std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>> pointers_maplist){
        int counter=0;
        for(auto& v: pointers_maplist){
            logger.debug("-------------" + std::to_string(counter) + "--------------");
            map_dbg(v);
            counter++;
            logger.debug("----------------------------");
        }
    }

    ///Debug helper: logs entry map
    ///@param pointers_map object to debug
    void map_dbg(std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> pointers_map){
        logger.debug("...........begin................");
        for(auto& m: pointers_map){
            auto pairs = m.second;
            logger.debug("KEY: "+ m.first);
            for(auto &p: pairs){
                flat_dbg(p);
            }
        }
        logger.debug("............end................");
    }

    ///Debug helper: logs contents of flat pairs
    ///@param pointers_map object to debug
    void flat_dbg(atom::reply_type::flat_response pair){
        logger.debug("DATA: "+ std::string(*(pair.first),pair.second) +", SIZE: "+std::to_string(pair.second));
    }

private: 

    //flat parser for redis replies - for XADD, XDEL, SET, XACK
    std::pair<std::shared_ptr<const char *>, size_t>
    process_flat(const buffer &buff, atom::error & err){
        logger.debug("process_flat()");        
        std::pair<std::shared_ptr<const char *>, size_t> response;
        auto end = buf_iter::end(buff.data());

        for(auto it = buf_iter::begin(buff.data()); it != end; ++it){
            switch(*it) {
            case '+':  // simple string
            case '-':  // nill or error
            case ':': { // integer
                logger.debug("process_flat| simple strings");
                it++; //move past indicator char
                size_t str_len = find_data(it, end);
                logger.debug("process_flat| str_len: " + std::to_string(str_len));
                std::shared_ptr<const char *> str = std::make_shared<const char *>(&*(it));
                response = std::pair<std::shared_ptr<const char *>,size_t>(str, str_len); 
                it += (str_len);
                break;
            }
            case '$': {// bulk string - will carry on until /r/n is hit
                logger.debug("process_flat| bulk strings");
                it++; //move past the indicator char
                std::tuple<size_t,size_t> sizes = find_data_len(it, end); // <num_bytes, num characters in data string>
                size_t num_bytes = std::get<0>(sizes);
                size_t data_len = std::get<1>(sizes);
                logger.debug("process_flat| data_len: " + std::to_string(data_len));
                std::shared_ptr<const char *> data = std::make_shared<const char *>(&*(it+=num_bytes));
                response = std::pair<std::shared_ptr<const char *>,size_t>(data, data_len);
                it+= (data_len); //move pointer past the data string
                break;
            }
            }
        }
        return response;
    } 

    //parser for entries - each entry represented as a map with ID as map key, which map to 
    //points to buffer which hold redis keys and values mapped to their size
    std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>
        process_entry(buf_iter & data, buf_iter & end, atom::error & err){
            logger.debug("process_entry()");
            std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> parsed_map;

            //find number of entries
            assert(*data == '*');
            data++; //move past the * indicator char
            std::tuple<size_t,size_t> sizes = find_data_len(data, end); // <num_bytes, num entries in response>
            size_t num_bytes = std::get<0>(sizes);
            size_t num_entries = std::get<1>(sizes);
            data+=num_bytes; //move past the chars that indicate # of array elements
            logger.debug("process_entry| found entries: " + std::to_string(num_entries));

            //auto start = data;
            for(size_t entry = 0; entry < num_entries; entry++){
                std::vector<std::pair<std::shared_ptr<const char *>, size_t>> inner_data;
                bool id_read = false;
                bool entry_read = false;
                std::string id;
                size_t num_elems;
                size_t counter=0;
                for(auto it = data; it != end; ++it){
                    switch(*it) {
                    case '+':  // simple string
                    case '-':  // nill or error
                    case ':': { // integer
                        std::cout<<"iter val: "<<*it<<std::endl;
                        assert(id_read);
                        logger.debug("process_entry| simple strings");
                        it++; //move past indicator char
                        size_t str_len = find_data(it, end);
                        logger.debug("process_entry| str_len: " + std::to_string(str_len));
                        std::shared_ptr<const char *> str = std::make_shared<const char *>(&*(it));
                        inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(str, str_len)); 
                        counter+=1;
                        if(counter==num_elems){entry_read=true;}
                        logger.debug("process_entry| updating inner data...");
                        it += (str_len);
                        break;
                    } //case +-:
                    case '$': {// bulk string - will carry on until /r/n is hit
                        logger.debug("process_entry| bulk strings");
                        it++; //move past the indicator char
                        std::tuple<size_t,size_t> sizes = find_data_len(it, end); // <num_bytes, num characters in data string>
                        size_t num_bytes = std::get<0>(sizes);
                        size_t data_len = std::get<1>(sizes);
                        logger.debug("process_entry| data_len: " + std::to_string(data_len));
                        it+=num_bytes;
                        std::shared_ptr<const char *> bulk_str = std::make_shared<const char *>(&*it);
                        it+= (data_len); //move pointer past the data string

                        if(!id_read){
                            id = std::string(*bulk_str, data_len);
                            logger.debug("process_entry| found id: " + id);
                            id_read=true;
                        } else{
                            logger.debug("process_entry| updating inner data...");
                            inner_data.push_back(std::pair<std::shared_ptr<const char *>,size_t>(bulk_str, data_len));
                            counter+=1;
                            if(counter==num_elems){entry_read=true;}
                        }
                        break;
                    } //case $
                    case '*': { //array
                        it++; //move past the indicator char
                        logger.debug("process_entry| arrays");
                        if(id_read){
                            std::tuple<size_t,size_t> sizes = find_data_len(it, end);
                            num_elems = std::get<1>(sizes); // num redis keys + values in entry
                            logger.debug("process_entry| num_elems: " + std::to_string(num_elems));
                        }
                        break;
                    } //case *
                    } //end switch
                    if(entry_read) { logger.debug("process_entry| done with entry."); data = it; break; } //leave iteration loop if we've read an entry
                } //for each it

                if(!inner_data.empty()){
                    logger.debug("process_entry| updating parsed map: ");
                    parsed_map.insert({id, inner_data});
                    map_dbg(parsed_map);
                }
                
            }//for each entry
            
            return parsed_map;
        }  



    //parser for reading entry maps on a per-stream basis
    std::vector<std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>>
    process_entrylist(buf_iter data, buf_iter end, atom::error & err) {
        std::vector<std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>>> parsed_response;
        //find number of streams
        assert(*data == '*');
        data++; //move past the * indicator char
        std::tuple<size_t,size_t> sizes = find_data_len(data, end); // <num_bytes, num streams in response>
        size_t num_bytes = std::get<0>(sizes);
        size_t num_streams = std::get<1>(sizes);
        data+=num_bytes; //move past the chars that indicate # of array elements
        logger.debug("process_entry| found streams: " + std::to_string(num_streams));

        auto start = data;
        for(int stream=0; stream < num_streams; stream++){ //iterate through the streams
            std::string stream_name;
            bool stream_name_read = false;
            bool stream_read = false;

            for(auto it = start; it != end; it++){
                logger.debug("process array| it: ");
                switch(*it) {
                case '*': {
                logger.debug("process array| array");
                    if(stream_name_read){
                        std::map<std::string, std::vector<std::pair<std::shared_ptr<const char *>, size_t>>> 
                        entry_map = process_entry(it, end, err);
                        parsed_response.push_back(entry_map);
                        stream_read=true;
                    }
                    break;
                }
                case '$': { //is a bulk string - will carry on until /r/n is hit
                    logger.debug("process array|" + std::to_string(stream) + "| bulk strings");
                    it++; //move past the indicator char
                    std::tuple<size_t,size_t> sizes = find_data_len(it, end);
                    size_t num_bytes = std::get<0>(sizes); //offset where the data starts from current iter
                    size_t data_len = std::get<1>(sizes); //num characters in data string
                    std::shared_ptr<const char *> data = std::make_shared<const char *>(&*(it+=num_bytes));
                    logger.debug("process array| " + std::to_string(stream) + "| data_len: " + std::to_string(data_len));                                            
                    if(!stream_name_read){
                        stream_name = std::string(*data, data_len);
                        stream_name_read=true;
                    }
                    it+= (data_len); //move pointer past the data string
                    break;
                }
                } //switch end
                if(stream_read) { logger.debug("process array| done with stream."); start = it; break; } //leave iteration loop if we've read an entry
            } //iter loop
        } //stream loop
        
        return parsed_response;            
    }

    //returns number of bytes the data length is stored in, and the value of the data length itself
    std::tuple<size_t, size_t> find_data_len(const buf_iter &begin, const buf_iter &end){
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
    size_t find_data(const buf_iter &begin, const buf_iter &end){
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

    //members
    Logger logger;

};

}

#endif //__ATOM_CPP_PARSER_H