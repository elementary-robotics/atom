////////////////////////////////////////////////////////////////////////////////
//
//  @file Messages.h
//
//  @brief Header-only implementation of atom types
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_MESSAGES_H
#define __ATOM_CPP_MESSAGES_H

#include <iostream>
#include <memory>
#include <functional>
#include <vector>

#include <boost/variant.hpp>

#include "Error.h"
#include "Parser.h"

namespace atom {

const std::map<std::string, std::vector<std::string>> reserved_keys = {
    {"command_keys", {"data", "cmd", "element", "ser"}},
    {"response_keys", {"data", "err_code", "err_str", "element", "cmd", "cmd_id", "ser"}},
    {"entry_keys", {"ser"}}
};

template<typename buffer>
class pooled_buffer : public std::enable_shared_from_this<pooled_buffer<buffer>> {
public:
    ///Constructor for pooled_buffer, the class
    /// used in BufferPool
    pooled_buffer() : ref_counter(0){};
    pooled_buffer(const pooled_buffer&) = delete;
    pooled_buffer(std::shared_ptr<pooled_buffer> move) : ref_counter(move->ref_counter) {};

    virtual ~pooled_buffer(){};

    ///Update reference counter for pooled_buffer.
    ///Increment by 1.
    void add_ref(){
        ref_counter++;
    }

    ///Update reference counter for pooled_buffer.
    ///Decrement by 1.
    void remove_ref(){
        if(ref_counter > 0){
            ref_counter--;
        }
        else{
            //TODO: how to handle this case?
        }
    }

    ///Get reference information on pooled_buffer
    ///@return number of entities that hold a reference to pooled_buffer instance
    int get_refs(){
        return ref_counter;
    }

    ///Consume buffer
    ///@param size bytes to consume in buffer
    void consume(size_t size){
        io_buff.consume(size);
    }

    buffer io_buff;

private:
    
    int ref_counter;
};


///Stores replies from Redis read into the buffer
///@param size total size of the read data
///@param data_buffer pointer to the underlying pooled_buffer
///@param parsed_reply the object containing pointers to relevant portions of the underlying buffer
template<typename buffer>
class redis_reply {
    public:
    const size_t size;
    std::shared_ptr<atom::pooled_buffer<buffer>> data_buff;
    reply_type::parsed_reply parsed_reply;
    
    redis_reply(size_t n, std::shared_ptr<atom::pooled_buffer<buffer>> b) : size(n), data_buff(std::move(b)){}
    redis_reply(size_t n, std::shared_ptr<atom::pooled_buffer<buffer>> b, reply_type::parsed_reply parsed_reply) : size(n), 
                                                                                        data_buff(std::move(b)),
                                                                                        parsed_reply(parsed_reply){}

    atom::reply_type::flat_response flat_response(){
        return boost::get<atom::reply_type::flat_response>(parsed_reply);
    }

    atom::reply_type::entry_response entry_response(){
        return boost::get<atom::reply_type::entry_response>(parsed_reply);
    }

    atom::reply_type::entry_response_list entry_response_list(){
        return boost::get<atom::reply_type::entry_response_list>(parsed_reply);
    }

    ///Release the ownership of parsed_reply.
    ///This function will cleanup the member parsed_reply without requiring the user to specify its type.
    void cleanup(){
        try {
           cleanup(boost::get<atom::reply_type::flat_response>(parsed_reply));
        } catch(boost::bad_get & ec){
            try {
                cleanup(boost::get<atom::reply_type::entry_response>(parsed_reply));
            } catch(boost::bad_get & ec){
                cleanup(boost::get<atom::reply_type::entry_response_list>(parsed_reply));
            }
        }
    }

    private:

    ///release the ownership of shared pointers
    ///@param flat object to clean up
    void cleanup(atom::reply_type::flat_response & flat){
        flat.first.reset();
    }

    ///release the ownership of shared pointers
    ///@param entry_map object to clean up
    void cleanup(atom::reply_type::entry_response & entry_map) {
        for(auto it = entry_map.begin(); it != entry_map.end(); it++){
            auto vec = it->second;
            for(auto in = vec.begin(); in != vec.end(); in++){
                in->first.reset();
            }
            vec.clear();
        }
        entry_map.clear();
    }

    ///release the ownership of shared pointers
    ///@param entries_list object to clean up
    void cleanup(atom::reply_type::entry_response_list & entries_list){
        for(auto & map : entries_list){
            cleanup(map);
        }
    }
    
};



namespace entry_type {
    template<typename DataType>
    using deser_data = std::shared_ptr<DataType>;
    using str_data = std::shared_ptr<std::string>;
    using buff_data = std::shared_ptr<const char *>;

    template<typename DataType>
    struct object {
        std::pair<boost::variant<deser_data<DataType>, str_data, buff_data>, size_t> data;

        object(std::shared_ptr<DataType> ptr, size_t size) {
            data.first = ptr;
            data.second = size;
        }
        object(std::shared_ptr<std::string> ptr, size_t size) {
            data.first = ptr;
            data.second = size;
        }
        object(std::shared_ptr<const char *> ptr, size_t size) {
            data.first = ptr;
            data.second = size;
        }
    };
}

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename DataType, typename BufferType>
struct entry {
    std::string field;
    std::vector<atom::entry_type::object<DataType> > data;
    std::shared_ptr<BufferType> base_buffer;

    entry(std::string field, 
        std::vector<atom::entry_type::object<DataType> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_buffer()
                                                {};
    entry(std::string field) : field(field){};

};

///Holds a response from a Server_Element. 
///@param data response data
///@param serialization_method string that indicates which serialization method to use. Currently only msgpack is supported.
///@param err holds error code and msg information, if any errors occur.
struct element_response {
    std::shared_ptr<const char *> data;
    std::string serialization_method;
    atom::error err;

    element_response(std::shared_ptr<const char *> data, 
                    std::string method, atom::error err) : data(data),
                                                        serialization_method(method),
                                                        err(err){};
};


struct reference {

};

///function signature for a stream handler, params are element_name and stream_name
typedef std::function<void(std::string, std::string)> StreamHandler;

///function signature for read handler, params are an entry and user-supplied data
template<typename DataType, typename BufferType>
using ReadHandler = std::function<bool(entry<DataType, BufferType>&, void*)>;

///function signature for command handler, params are redis reply, element response, and error 
template<typename BufferType>
using CommandHandler = std::function<element_response(atom::redis_reply<BufferType>&, element_response&, atom::error&)>;

///function signature for health checking - TODO: MOVE THIS TO THE SERVER ELEMENT
typedef std::function<element_response(void *)> HealthChecker;

}

#endif //__ATOM_CPP_MESSAGES_H