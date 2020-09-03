////////////////////////////////////////////////////////////////////////////////
//
//  @file Server_Element.h
//
//  @brief Server_Element header, represents a hardware elements
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////



#ifndef __ATOM_CPP_SERVER_ELEMENT_H
#define __ATOM_CPP_SERVER_ELEMENT_H

#include <iostream>

#include "ConnectionPool.h"
#include "Serialization.h"
#include "Redis.h"
#include "Logger.h"
#include "Messages.h"


namespace atom{

template<typename ConnectionType, typename BufferType>
class Server_Element {
    public:
        Server_Element(boost::asio::io_context & iocon, int max_cons, int timeout, std::string redis_ip, 
                    Serialization& serialization, int num_buffs, int buff_timeout,
                    int num_tcp, int num_unix, 
                    std::ostream& log_stream, std::string element_name);

        virtual ~Server_Element();

        ///Writes an entry to Redis server
        ///@tparam ConnectionType determines if TCP or UNIX socket is used
        ///@tparam BufferType determines the underlying buffer type
        ///@param stream_name name of the stream to write to
        ///@param entry_data vector of redis keys and values to write to redis. Key value pairs must be specified in sequence.
        ///@param ser_method the serialization method to use
        ///@param err to hold errors that may occur during this operation
        atom::redis_reply<BufferType> entry_write(std::string stream_name,  
                                std::vector<std::string>& entry_data, atom::Serialization::method ser_method, atom::error& err);

    private:
        std::string name;
        std::map<std::string, atom::reference> references;
        std::map<std::string, atom::CommandHandler<BufferType>> command_handlers;
        std::vector<std::string> streams;
        std::string atom_version;
        std::string atom_language;
        ConnectionPool pool;
        std::shared_ptr<ConnectionType> connection;
        Serialization ser;
        Logger logger;
};

};

#endif //__ATOM_CPP_SERVER_ELEMENT_H