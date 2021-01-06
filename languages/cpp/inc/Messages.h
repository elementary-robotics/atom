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
    {"response_keys", {"data", "err_code", "err_str", "element", "cmd", "cmd_id", "ser", "cmd_list"}},
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

    atom::reply_type::array_response array_response(){
        return boost::get<atom::reply_type::array_response>(parsed_reply);
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

        std::shared_ptr<DataType> shared_value(){
            return boost::get<std::shared_ptr<DataType>>(pair.first);
        }

        template<typename CustomType = DataType>
        CustomType value(){
            return *boost::get<std::shared_ptr<CustomType>>(pair.first).get();
        }

        std::string svalue(){
            std::shared_ptr<DataType> data = shared_value();
            return atom::reply_type::to_string(data, pair.second);
        }

    };
}



template<typename BufferType>
class serialized_entry {
public:
    std::string field;
    std::vector<std::string> data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;
    
    serialized_entry(): field(""){data.push_back("");};
    serialized_entry(std::string field, 
        std::vector<std::string>& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    serialized_entry(std::string field, std::vector<std::string>& data) : field(field), data(data){};

};

///Holds serialized or deserialized data that accompanies a command.
template<typename BufferType = boost::asio::streambuf>
class command {
public:
    command(std::string element_name, std::string command_name, std::vector<std::string> ser_data) :
                                        element_name(element_name), command_name(command_name), ser_data(ser_data){};
    std::string element_name;
    std::string command_name;
    std::vector<std::string> ser_data;

    std::vector<std::string> data(){
        std::vector<std::string> cmd_vector = std::vector<std::string>{element_name, command_name};
        cmd_vector.insert(cmd_vector.end(), ser_data.begin(), ser_data.end());
        return cmd_vector;
    }
};


struct reference {

};






}

#endif //__ATOM_CPP_MESSAGES_H