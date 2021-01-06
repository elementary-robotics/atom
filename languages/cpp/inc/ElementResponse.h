
#ifndef __ATOM_CPP_ELEMRESPONSE_H
#define __ATOM_CPP_ELEMRESPONSE_H

#include "Error.h"
#include "Serialization.h"
#include "ElementEntry.h"

namespace atom {


///Holds a response from a Server_Element. 
///@param data response data
///@param serialization_method string that indicates which serialization method to use. Currently only msgpack is supported.
///@param err holds error code and msg information, if any errors occur.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
struct element_response {
    

    element_response(atom::error err): err(err){};

    element_response(std::shared_ptr<atom::entry<BufferType, MsgPackType>> data, 
                    std::string method, atom::error & err) : data(data),
                                                        serialization_method(method),
                                                        err(err), filled(true){};
    element_response(std::vector<std::string> simple_data, std::string method) : simple_data(simple_data),
                                                        serialization_method(method),
                                                        filled(true){};


    atom::entry<BufferType, MsgPackType> entry(){
        if(filled){
            return *data.get();
        }
        if(serialization_method == "msgpack"){
            return atom::entry<BufferType, MsgPackType>();
        }
        if(serialization_method == "none"){
            atom::raw_entry<BufferType> base_entry("empty");
            return atom::entry<BufferType, MsgPackType>(base_entry);
        }
        if(serialization_method == "arrow"){
            atom::arrow_entry<BufferType> base_entry("empty");
            return atom::entry<BufferType, MsgPackType>(base_entry);
        }
        throw std::runtime_error("Unknown serialization method requested from element response.");
    }

    std::vector<std::string> entry_data(){
        atom::entry<BufferType, MsgPackType> e = *data.get();
        auto ser_method = ser.get_method(serialization_method);
        atom::serialized_entry<BufferType> ser_entry = ser.serialize(e, ser_method, err);
        return ser_entry.data;
    }

    std::vector<std::string> sdata(){
        return simple_data;
    }

    void fill(std::shared_ptr<atom::entry<BufferType, MsgPackType>> entry_data, std::string method, atom::error & err){
        data = entry_data;
        serialization_method = method;
        err = err;
        filled = true;
    }

    void fill(atom::entry<BufferType, MsgPackType> entry_data, std::string method, atom::error & err){
        data = std::make_shared<atom::entry<BufferType, MsgPackType>>(entry_data);
        serialization_method = method;
        err = err;
        filled = true;
    }
    
    void dbg(){
        auto out = entry_data();
        for(auto & d : out){
            std::cout<<d<<", ";
        }
    }

    atom::error err;
    bool filled = false;
    std::string serialization_method;

private:
    std::shared_ptr<atom::entry<BufferType, MsgPackType>> data;
    std::vector<std::string> simple_data;
    atom::Serialization ser;
    
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

///function signature for command handler, data as vector of strings and serialization method as argument
// -> nope: params are redis reply, element response, and error TODO CLEAN UP
template<typename BufferType = boost::asio::streambuf, typename MsgPackType = msgpack::type::variant>
using CommandHandler = atom::element_response<BufferType, MsgPackType>(*)(/* std::vector<std::string>, std::string */); //std::function<void(atom::redis_reply<BufferType>&, element_response<BufferType, MsgPackType>&, atom::error&)>;

///function signature for health checking - TODO: MOVE THIS TO THE SERVER ELEMENT
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
using HealthChecker = std::function<atom::element_response<BufferType, MsgPackType>(void *)>;

}

#endif //__ATOM_CPP_ELEMRESPONSE_H