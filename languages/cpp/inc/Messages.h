////////////////////////////////////////////////////////////////////////////////
//
//  @file Messages.h
//
//  @brief Header-only implementation of Messages class
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

#include "Error.h"
#include "Parser.h"

namespace atom {

const std::map<std::string, std::vector<std::string>> reserved_keys = {
    {"command_keys", {"data", "cmd", "element", "ser"}},
    {"response_keys", {"data", "err_code", "err_str", "element", "cmd", "cmd_id", "ser"}},
    {"entry_keys", {"ser"}}
};

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
struct entry {
    std::string field;
    std::vector<atom::reply_type::flat_response> data;

    entry(std::string field, 
        std::vector<atom::reply_type::flat_response>& data) : field(field),
                                                        data(data){};
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
typedef std::function<bool(entry&, void*)> ReadHandler;

///function signature for command handler, params are redis reply, element response, and error 
template<typename BufferType>
using CommandHandler = std::function<element_response(atom::redis_reply<BufferType>&, element_response&, atom::error&)>;

///function signature for health checking - TODO: MOVE THIS TO THE SERVER ELEMENT
typedef std::function<element_response(void *)> HealthChecker;

}

#endif //__ATOM_CPP_MESSAGES_H