#include "Server_Element.h"

#include <iostream>
#include <msgpack.hpp>

#include "Messages.h"

template<typename ConnectionType, typename BufferType>
atom::Server_Element<ConnectionType, BufferType>::Server_Element(boost::asio::io_context & iocon, int max_cons, int timeout, std::string redis_ip, 
                    Serialization& serialization, int num_buffs, int buff_timeout,
                    int num_tcp, int num_unix, 
                    std::ostream& log_stream, std::string element_name) : name(element_name),
                                                                        atom_version(VERSION),
                                                                        atom_language(LANGUAGE),
                                                                        pool(iocon, max_cons, timeout, redis_ip, num_buffs, buff_timeout), 
                                                                        connection(nullptr),
                                                                        ser(std::move(serialization)),
                                                                        logger(&log_stream, element_name)
    {
        //initialize the connection pool
        pool.init(num_unix, num_tcp);
        connection = pool.get_connection<ConnectionType>();

        //connect to Redis server
        atom::error err;
        connection->connect(err);
        if(err){
            logger.error("Unable to connect to Redis: " + err.message());
        }

    }


//TODO
template<typename ConnectionType, typename BufferType>
atom::Server_Element<ConnectionType, BufferType>::~Server_Element(){}


template<typename ConnectionType, typename BufferType>
atom::redis_reply<BufferType> atom::Server_Element<ConnectionType, BufferType>::entry_write(std::string stream_name,  
                        std::vector<std::string>& entry_data, atom::Serialization::method ser_method, atom::error& err){
   //make sure vector isn't empty
    if(!(entry_data.size() > 0)){
        logger.alert("Writing empty vector to Redis is not permitted.");
        err.set_error_code(atom::error_codes::invalid_command);
        return atom::redis_reply<BufferType>(0, nullptr);
    }

    //make sure each redis key has a corresponding value
    if((entry_data.size() % 2 ) > 0){
        logger.alert("Invalid entry data. Each redis key must have a corresponding value.");
        err.set_error_code(atom::error_codes::invalid_command);
        return atom::redis_reply<BufferType>(0, nullptr);
    }

    //check for invalid keys
    std::vector<std::string> reserved_keys = atom::reserved_keys.at("entry_keys");
    bool is_key = true;
    for(const auto & m: entry_data) {
        if(is_key){
            if(std::find(reserved_keys.begin(), reserved_keys.end(), m) != reserved_keys.end()){
                logger.alert("Invalid key: " + m + " is a reserved key.");
                err.set_error_code(atom::error_codes::invalid_command);
                return atom::redis_reply<BufferType>(0, nullptr);
            }
        }
        is_key = !is_key;
    }

    //add stream
    streams.push_back(stream_name);

    //serialize
    switch(ser_method){
        case atom::Serialization::method::msgpack:
        {               
            //serialize the values (leave the keys alone)
            bool is_value = false;
            for(auto & m: entry_data){
                if(is_value){
                    std::stringstream serialized_data;
                    ser.serialize<std::string>(m, serialized_data);
                    m = serialized_data.str();
                }
                is_value = !is_value;
            }

            return connection->xadd(stream_name, "msgpack", entry_data, err);
        }
        case atom::Serialization::method::none:
        {          
            return connection->xadd(stream_name, "msgpack", entry_data, err);
        }
        case atom::Serialization::method::arrow:
        {          
            err.set_error_code(atom::error_codes::unsupported_command);
            return atom::redis_reply<BufferType>(0, nullptr);
        }
        default:
            throw std::runtime_error("Supplied serialization option is invalid.");
    }
}



template class atom::Server_Element<atom::ConnectionPool::UNIX_Redis, atom::ConnectionPool::Buffer_Type>;
template class atom::Server_Element<atom::ConnectionPool::TCP_Redis, atom::ConnectionPool::Buffer_Type>;
