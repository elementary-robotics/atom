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
#include <msgpack.hpp>

#include "Error.h"
#include "Parser.h"

namespace atom {

struct longest{
    template<class T> bool operator()(T const &one, T const& two){
        return one.size() < two.size();
    }
};

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
            cleanup(map.second);
        }
    }
    
};



namespace entry_type {
    template<typename DataType>
    using deser_data = std::shared_ptr<DataType>; //for the deserialized value

    using str_data = std::shared_ptr<std::string>; //for the string key (which is not serialized OR deserialized)

    template<typename DataType>
    struct object {
        std::pair<boost::variant<deser_data<DataType>, str_data>, size_t> pair;
    
        object(std::shared_ptr<DataType> ptr, size_t size) : pair(ptr,size) { }
        object(std::shared_ptr<std::string> ptr, size_t size) : pair(ptr,size) { }
        object() {}

        inline object operator=(object other){
            pair.first = other.pair.first;
            pair.second = other.pair.second;
        }

        virtual ~object(){
            //pair.first.reset();
        }

        std::string key(){
            return *boost::get<std::shared_ptr<std::string>>(pair.first).get();
        }

        std::shared_ptr<DataType> value(){
            return boost::get<std::shared_ptr<DataType>>(pair.first);
        }

        std::string svalue(){
            std::shared_ptr<DataType> data = value();
            return atom::reply_type::to_string(data, pair.second);
        }

    };
}

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
struct msgpack_entry {
    std::string field;
    std::vector<atom::entry_type::object<MsgPackType> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    msgpack_entry(std::string field, 
        std::vector<atom::entry_type::object<MsgPackType> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    msgpack_entry(std::string field) : field(field){};

};

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType>
struct raw_entry {
    std::string field;
    std::vector<atom::entry_type::object<const char *> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    raw_entry(std::string field, 
        std::vector<atom::entry_type::object<const char *> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    raw_entry(std::string field) : field(field){};

};

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType, typename ArrowType = void> //TODO update that void when you actually employ arrow
struct arrow_entry {
    std::string field;
    std::vector<atom::entry_type::object<ArrowType> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    arrow_entry(std::string field, 
        std::vector<atom::entry_type::object<ArrowType> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    arrow_entry(std::string field) : field(field){};

};

///Holds the different forms of entry_types that we could expect to see.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
class entry {
public:
    boost::variant<msgpack_entry<BufferType, MsgPackType>,raw_entry<BufferType>, arrow_entry<BufferType>> base_entry;

    entry(msgpack_entry<BufferType, MsgPackType> base_entry) : base_entry(base_entry){};
    entry(raw_entry<BufferType> base_entry) : base_entry(base_entry){};
    entry(arrow_entry<BufferType> base_entry) : base_entry(base_entry){};


    msgpack_entry<BufferType, MsgPackType> get_msgpack(){
        return boost::get<msgpack_entry<BufferType, MsgPackType>>(base_entry);
    }
    
    raw_entry<BufferType> get_raw(){
        return boost::get<raw_entry<BufferType>>(base_entry);
    }

    arrow_entry<BufferType> get_arrow(){
        return boost::get<arrow_entry<BufferType>>(base_entry); //TODO implement and test
    }

    bool is_empty(){
        return base_entry.empty();
    }

};

template<typename BufferType>
class serialized_entry {
public:
    std::string field;
    std::vector<std::string> data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;
    
    serialized_entry(){};
    serialized_entry(std::string field, 
        std::vector<std::string>& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    serialized_entry(std::string field, std::vector<std::string>& data) : field(field), data(data){};

};

///Holds the different forms of entry_types that we could expect to see.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
class command {
public:
    command(std::string element_name, std::string command_name, std::shared_ptr<atom::serialized_entry<BufferType>> ser_entry) :
                                        element_name(element_name), command_name(command_name), ser_entry(std::move(ser_entry)){};
    std::string element_name;
    std::string command_name;
    std::shared_ptr<atom::serialized_entry<BufferType>> ser_entry;
};

///Holds a response from a Server_Element. 
///@param data response data
///@param serialization_method string that indicates which serialization method to use. Currently only msgpack is supported.
///@param err holds error code and msg information, if any errors occur.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
struct element_response {
    std::shared_ptr<std::vector<atom::entry<BufferType, MsgPackType>>> data;
    std::string serialization_method;
    atom::error err;
    bool filled = false;

    element_response(atom::error err): err(err){};

    element_response(std::shared_ptr<std::vector<atom::entry<BufferType, MsgPackType>>> data, 
                    std::string method, atom::error & err) : data(data),
                                                        serialization_method(method),
                                                        err(err), filled(true){};
    void fill(std::shared_ptr<std::vector<atom::entry<BufferType, MsgPackType>>> data, std::string method, atom::error & err){
        data = data;
        serialization_method = method;
        err = err;
        filled = true;
    }
};


struct reference {

};


template<typename BufferType, typename MsgPackType = msgpack::type::variant>
using Handler = void(*)(atom::entry<BufferType, MsgPackType>);

///function signature for a stream handler, params are element_name and stream_name
template<typename BufferType = boost::asio::streambuf, typename MsgPackType = msgpack::type::variant>
class StreamHandler{

public:

    StreamHandler(std::string element_name, std::string stream_name, Handler<BufferType, MsgPackType> handler) : element_name(element_name),
                                                                    stream_name(stream_name),
                                                                    handler(handler){};
    
    std::string element_name;
    std::string stream_name;

    Handler<BufferType, MsgPackType> handler;
};

///function signature for read handler, params are an entry and user-supplied data
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
using ReadHandler = std::function<bool(atom::entry<BufferType, MsgPackType>&, void*)>;

///function signature for command handler, params are redis reply, element response, and error 
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
using CommandHandler = std::function<void(atom::redis_reply<BufferType>&, element_response<BufferType, MsgPackType>&, atom::error&)>;

///function signature for health checking - TODO: MOVE THIS TO THE SERVER ELEMENT
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
using HealthChecker = std::function<element_response<BufferType, MsgPackType>(void *)>;

}

#endif //__ATOM_CPP_MESSAGES_H