#include "Server_Element.h"

#include <iostream>
#include <msgpack.hpp>


template<typename ConnectionType, typename BufferType, typename MsgPackType>
atom::Server_Element<ConnectionType, BufferType, MsgPackType>::Server_Element(boost::asio::io_context & iocon, int max_cons, int timeout, std::string redis_ip, 
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

    
    add_command(atom::VERSION_COMMAND, [](){
        return atom::element_response<BufferType, MsgPackType>({"language", atom::LANGUAGE, "version", atom::VERSION}, "msgpack");
    });

}


//TODO
template<typename ConnectionType, typename BufferType, typename MsgPackType>
atom::Server_Element<ConnectionType, BufferType, MsgPackType>::~Server_Element(){}


template class atom::Server_Element<atom::ConnectionPool::UNIX_Redis, atom::ConnectionPool::Buffer_Type, msgpack::type::variant>;
template class atom::Server_Element<atom::ConnectionPool::TCP_Redis, atom::ConnectionPool::Buffer_Type, msgpack::type::variant>;
