////////////////////////////////////////////////////////////////////////////////
//
//  @file Client_Element.h
//
//  @brief Client_Element implementation, used to communicate with Server_Element elements
//         through Redis
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////



#ifndef __ATOM_CPP_CLIENT_ELEMENT_H
#define __ATOM_CPP_CLIENT_ELEMENT_H

#include <iostream>
#include <chrono>

#include "ConnectionPool.h"
#include "Serialization.h"
#include "Redis.h"
#include "Logger.h"
#include "Messages.h"


namespace atom{

template<typename ConnectionType, typename BufferType>
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
///@param num_buffs number of buffers to allocate for BufferPool
///@param buffer_timeout timeout in milliseconds to wait for a buffer to become available
///@param log_stream stream to which to publish log messages
///@param element_name name of the client element
Client_Element(boost::asio::io_context & iocon, int max_cons, int timeout, std::string redis_ip, 
                Serialization& serialization, int num_buffs, int buff_timeout,
                int num_tcp, int num_unix, 
                std::ostream& log_stream, std::string element_name);

///Destructor for Client_Element
virtual ~Client_Element();

///Read at most n entries from an element's stream
///@param element_name Name of the element the stream belongs to
///@param stream_name Name of the stream to read entries from
///@param num_entries Number of entries to get
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
///@tparam DataType Optional argument, specify if a serialization method other than atom::Serialization::none is desired
template<typename MsgPackType = msgpack::type::variant>
std::vector<atom::entry<BufferType, MsgPackType>> entry_read_n(std::string element_name, std::string stream_name, 
                                int num_entries, atom::error & err, 
                                atom::Serialization::method serialization=atom::Serialization::method::msgpack, 
                                bool force_serialization=false){

    std::vector<atom::entry<BufferType, MsgPackType>> entries;
    std::string stream_id = make_stream_id(element_name, stream_name);
    atom::redis_reply<BufferType> reply = connection->xrevrange(stream_id, "+", "-", std::to_string(num_entries), err);

    if(err){
        logger.error("Error: " + err.message());
        return entries;
    }

    atom::reply_type::entry_response data = reply.entry_response();
    ser.deserialize<BufferType, MsgPackType>(entries, serialization, data, err);

    connection->release_rx_buffer(reply);
    return entries;
}


///Read entries from an element's stream since a given last_id
///@param element_name Name of the element the stream belongs to
///@param stream_name Name of the stream to read entries from
///@param num_entries Number of entries to get
///@param last_id ID from which to begin getting entries from. 
///       If set to 0, all entries will be returned. If set to $, only new entries past the function call will be returned.
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
template<typename MsgPackType = msgpack::type::variant>
std::vector<atom::entry<BufferType, MsgPackType>> entry_read_since(std::string element_name, std::string stream_name, 
                                int num_entries, atom::error & err,  std::string last_id="$",
                                std::string block = "0",
                                atom::Serialization::method serialization=atom::Serialization::method::msgpack,
                                bool force_serialization=false){

    std::vector<atom::entry<BufferType, MsgPackType>> entries;
    std::string stream_id = make_stream_id(element_name, stream_name);
    atom::redis_reply<BufferType> reply = connection->xread("1", block, stream_id, last_id, err);

    if(err){
        logger.error("Error: " + err.message());
        return entries;
    }

    auto data = reply.entry_response_list();
    ser.deserialize<BufferType, MsgPackType>(entries, serialization, data, err);

    connection->release_rx_buffer(reply);
    return entries;
}

///Listens on specified streams for entries, passes entries to corresponding handlers
///@param stream_handlers vector of StreamHandlers
///@param n_loops Number of times to send a stream entry to the handlers
///               Defaults to 0 for infinite loop.
///@param timeout Time (ms) to block on the stream, and if surpassed, the function will return.
///@param serialization Optional argument, used for applying deserialziation method to entries
///@param force_serialization Optional argument, ignore default deserialization method in favor of serialization argument passed in
template<typename MsgPackType = msgpack::type::variant>
void entry_read_loop(std::vector<atom::StreamHandler<BufferType, MsgPackType>>& stream_handlers, 
                                int num_loops=0, std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
                                atom::Serialization::method serialization=atom::Serialization::method::msgpack,
                                bool force_serialization=false){
    
    std::vector<std::string> stream_timestamps;
    std::map<std::string, atom::Handler<BufferType, MsgPackType>> streams_map;
    for(auto & handler: stream_handlers){
        const std::string stream_id = make_stream_id(handler.element_name, handler.stream_name);
        stream_timestamps.push_back(stream_id);
        logger.debug("stream id: " + stream_id);
        stream_timestamps.push_back(get_redis_timestamp());
        streams_map.emplace(stream_id, handler.handler);
    }

    //counter to break out of while loop if num_loops =/= 0
    int counter = 0;
    while(true){
        if(num_loops !=0){
            if(counter == num_loops){
                break;
            }
            counter++;
        }

        atom::error err;
        atom::redis_reply<BufferType> data = connection->xread(stream_timestamps, err);
        auto stream_entries = data.entry_response_list();
        int vec_ind = 0;
        for(auto & stream: stream_entries){
            std::string stream_name = stream.first;
            std::vector<atom::entry<BufferType, MsgPackType>> deserialized_entries;
            for(auto & entry: stream.second){
                if(vec_ind % 2){
                    stream_timestamps[vec_ind] = entry.first; //update redis timestamp from which we read in the next loop
                }
                ser.deserialize(deserialized_entries, serialization, entry, err);
                atom::entry<BufferType, MsgPackType> & current_entry = deserialized_entries.back();
                logger.debug("entry.first: " + entry.first+ ", stream_name " + stream_name);
                streams_map.at(stream_name)(current_entry); //execute handler
                vec_ind++;
            }
        }
    }


}


///send commands to an element and optionally waits for acknowledgement ACK
///@param element_name Name of the element to command
///@param command_name Name of the command to execute
///@param block Optional argument, if true wait for an element response from the function
///@param timeout Optional argument, how long to wait for an acknowledgement before timing out
///@param serialization Optional argument, used for applying deserialziation method to entries
///@tparam DataType can be std::vector<msgpack::type::variant> or std::vector<std::string> for no serialization or msgpack serialization, and in the future arrow for arrow serialization
template<typename DataType, typename MsgPackType = msgpack::type::variant>
atom::element_response<DataType, MsgPackType> send_command(std::string element_name, std::string command_name, 
                                DataType data, atom::error err, bool block=true,
                                std::chrono::milliseconds timeout=std::chrono::milliseconds(atom::params::ACK_TIMEOUT),
                                atom::Serialization::method serialization=atom::Serialization::method::msgpack){
    std::string local_last_id = last_response_id;


    //serialize data
    std::vector<std::string> serialized_data = ser.serialize(data, serialization, err);

    //create a command
    atom::command<BufferType, MsgPackType> cmd(element_name, command_name, std::make_shared<atom::entry<BufferType, MsgPackType>>(data));
    
    //get connection from pool
    ConnectionType a_con = pool.get_connection<ConnectionType>();
    a_con.connect(err);
    if(err){
        logger.critical("Unable to connect to Redis.");
        throw std::runtime_error("Unable to connect to Redis.");
    }

    //send serialized data and grab command id
    atom::redis_reply<BufferType> reply = a_con.xadd(make_command_id(element_name), serialized_data, err, atom::params::STREAM_LEN);
    std::string command_id = atom::reply_type::to_string(reply.flat_response());
    a_con.release_rx_buffer(reply);

    //attempt to receive ACK from a server element until timeout is reached
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::duration elapsed_time(0);
    while(elapsed_time < timeout){
        elapsed_time = (start - std::chrono::high_resolution_clock::now()).count();
        std::vector<std::string> streams_timestamps {make_response_id(element_name), local_last_id};
        std::string block_time = std::to_string(std::max(std::chrono::duration_cast<std::chrono::milliseconds>(timeout - elapsed_time), std::chrono::milliseconds(1)).count());
        atom::redis_reply<BufferType> responses = a_con.xread(streams_timestamps, err, block_time);
        auto response_list = responses.entry_response_list();
        if(response_list.empty()){
            elapsed_time = (start - std::chrono::high_resolution_clock::now()).count();
            if(elapsed_time >= timeout){
                err.set_error_code(atom::error_codes::no_response);
                logger.error("Did not receive acknowledgement from " + element_name);
                return atom::element_response<DataType, MsgPackType>(nullptr, serialization, err);
            }
        }

    }

}

///Get timestamp from redis
std::string get_redis_timestamp(){
    atom::error err;
    atom::redis_reply<BufferType> reply = connection->time(err);
    auto entry = reply.entry_response();
    assert(entry.size() == 1);
    std::string timestamp;
    for(auto & m : entry){
        std::string microseconds(*m.second[0].first.get(), 3);
        timestamp = m.first + microseconds;
    }
    connection->release_rx_buffer(reply);
    return timestamp;
}

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
Serialization ser;

///logging
Logger logger;

///last reponse id - tracks the last entry where the client response stream was read
std::string last_response_id;

};



}

#endif //__ATOM_CPP_CLIENT_ELEMENT_H