////////////////////////////////////////////////////////////////////////////////
//
//  @file Client_Element.h
//
//  @brief Client_Element implementation, used to communicate with other elements
//         through Redis
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////



#ifndef __ATOM_CPP_CLIENT_ELEMENT_H
#define __ATOM_CPP_CLIENT_ELEMENT_H

#include <iostream>

#include "ConnectionPool.h"
#include "Serialization.h"
#include "Redis.h"
#include "Logger.h"


namespace atom{

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
struct entry {
    std::string field;
    std::shared_ptr<const char *> data;

    entry(std::string field, std::shared_ptr<const char *> data) : field(field),
                                                        data(std::move(data)){};
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

///function signature for a stream handler, params are element_name and stream_name
typedef std::function<void(std::string, std::string)> StreamHandler;

///function signature for read handler, params are an entry and user-supplied data
typedef std::function<bool(entry&, void*)> ReadHandler;

///function signature for command handler, params are redis reply, element response, and error 
typedef std::function<element_response(atom::redis_reply&, element_response&, atom::error&)> CommandHandler;

///function signature for health checking - TODO: MOVE THIS TO THE SERVER ELEMENT
typedef std::function<element_response(void *)> HealthChecker;

template<typename ConnectionType>
class Client_Element {

public:

///Constructor for Client_Element.
///Client Element provides functionality to read from and interact with with Server Elements.
///@tparam ConnectionType for UNIX or TCP socket as connection type used with Redis. 
/// Values can be atom::ConnectionPool::UNIX_Redis or atom::ConnectionPool::TCP_Redis, which are defined in ConnectionPool.h
///@param iocon io context 
///@param max_cons maximum number of connections to be made to Redis Server
///@param timeout timeout in milliseconds to wait for a connection to Redis Server to be released via the connection pool
///@param serialization instance of serialization class
///@param log_stream stream to which to publish log messages
///@param element_name name of the client element
Client_Element(boost::asio::io_context & iocon, int max_cons, int timeout, std::string redis_ip, 
                Serialization serialization, std::ostream& log_stream, std::string element_name);
~Client_Element();

///Read at most n entries from an element's stream
///@param element_name Name of the element the stream belongs to
///@param stream_name Name of the stream to read entries from
///@param num_entries Number of entries to get
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
atom::redis_reply entry_read_n(std::string element_name, std::string stream_name, 
                                int num_entries, atom::error & err, std::string serialization="", 
                                bool force_serialization=false);


///Read entries from an element's stream since a given last_id
///@param element_name Name of the element the stream belongs to
///@param stream_name Name of the stream to read entries from
///@param num_entries Number of entries to get
///@param last_id ID from which to begin getting entries from. 
///       If set to 0, all entries will be returned. If set to $, only new entries past the function call will be returned.
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
atom::redis_reply entry_read_since(std::string element_name, std::string stream_name, 
                                int num_entries, atom::error & err, std::string last_id="0",
                                std::string serialization="", bool force_serialization=false);


///Blocking read entries from an element's stream since a given last_id
///@param element_name Name of the element the stream belongs to
///@param stream_name Name of the stream to read entries from
///@param num_entries Number of entries to get
///@param last_id ID from which to begin getting entries from. 
///       If set to 0, all entries will be returned. If set to $, only new entries past the function call will be returned.
///@param block Optional argument, defaults to 0 to block forever. specifies the time (ms) to block on the read
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
atom::redis_reply entry_read_since(std::string element_name, std::string stream_name, 
                                int num_entries, atom::error & err, std::string last_id="0",
                                std::chrono::milliseconds block = std::chrono::milliseconds(0),
                                std::string serialization="", bool force_serialization=false);

///Listens on specified streams for entries, passes entries to corresponding handlers
///@param read_streams Maps element name to list of stream name, handler pairs
///@param n_loops Number of times to send a stream entry to the handlers
///               Defaults to 0 for infinite loop.
///@param timeout Time (ms) to block on the stream, and if surpassed, the function will return.
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
void entry_read_loop(std::map<std::string, std::map<std::string, atom::StreamHandler>> read_streams, 
                                int num_loops=0, std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
                                std::string serialization="", bool force_serialization=false);


///send commands to an element and optionally waits for acknowledgement ACK
///@param element_name Name of the element to command
///@param command_name Name of the command to execute
///@param block Optional argument, if true wait for an element response from the function
///@param timeout Optional argument, how long to wait for an acknowledgement before timing out
///@param serialization Optional argument, used for applying deserialziation method to entries
atom::element_response send_command(std::string element_name, std::string command_name, bool block=true,
                                std::chrono::milliseconds timeout=std::chrono::milliseconds(atom::params::ACK_TIMEOUT),
                                std::string serialization="");

///Get names of all Elements connected to Redis
///@return vector of element names
std::vector<std::string> get_all_elements();

///Get information about all streams belonging to an Element connected to Redis
///@return vector of element names
std::vector<std::string> get_all_streams(std::string element_name);

///Get information about all streams of all Elements connected to Redis
///@return maps element name to vector of stream names
std::map<std::string, std::vector<std::string>> get_all_streams();

///Get names of all commands belonging to an Element connected to Redis
///@return vector of command names
std::vector<std::string> get_all_commands(std::string element_name);

///Get names of all commands belonging to all Element connected to Redis
///@return maps element names to vector of command names
std::map<std::string, std::vector<std::string>> get_all_commands();

///Get information about references
///@param keys one or more keys of references to get
///@return maps reference key to corresponding data
std::map<std::string, std::shared_ptr<const char *>> get_reference(std::vector<std::string> keys);

///Get information about references - deserialized
///@param keys one or more keys of references to get
///@return maps reference key to corresponding msgpack type
template<typename MsgPackType>
std::map<std::string, MsgPackType> get_deserialized_reference(std::vector<std::string> keys);

///create a reference from stream - return value maps stream key to reference key
///@param element_name name of element to which the stream belongs to
///@param stream_name name of stream from which to create a reference
///@param stream_id optional arguemnt, id of stream
///@param timeout optional argument, time in milliseconds to time out. Default value is 0, for no timeout.
std::map<std::string, std::string> create_reference_from_stream(std::string element_name, std::string stream_name, 
                                std::string stream_id="", std::chrono::milliseconds timeout=std::chrono::milliseconds(0));

///get timeout information on a given reference
///@param key redis stream key to get reference timeout information on
std::chrono::milliseconds get_reference_timeout(std::string key);

///update the timeout for a reference
///@param key redis stream key to set reference timeout
///@param timeout time in milliseconds to set timeout
void update_reference_timeout(std::string key, std::chrono::milliseconds timeout);

///wait until all elements specified pass their respective health checks
///@param element_names vector of element names for which to wait for healthcheck
void wait_for_healthcheck(std::vector<std::string> element_names);


private:

///response id creation helper
std::string make_response_id(std::string element_name);

///command id creation helper
std::string make_command_id(std::string element_name);

///stream id creation helper
std::string make_stream_id(std::string element_name);

///stream id creation helper
std::string make_stream_id(std::string element_name, std::string stream_name);

///connection pool, manages connections to redis
ConnectionPool pool;

///main connection to redis
std::shared_ptr<ConnectionType> connection;

///serialization
Serialization serialization;

///logging
Logger logger;

};



}

#endif //__ATOM_CPP_CLIENT_ELEMENT_H