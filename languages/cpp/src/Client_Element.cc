 
#include "Client_Element.h"

#include <iostream>


template<typename ConnectionType, typename BufferType>
atom::Client_Element<ConnectionType, BufferType>::Client_Element(boost::asio::io_context & iocon,
                                    int max_cons, int timeout, std::string redis_ip,
                                    atom::Serialization& serialization, 
                                    int num_buffs, int buff_timeout,
                                    int num_tcp, int num_unix,
                                    std::ostream& log_stream, 
                                    std::string element_name) : pool(iocon, max_cons, timeout, redis_ip, num_buffs, buff_timeout), 
                                                            connection(nullptr),
                                                            ser(std::move(serialization)),
                                                            logger(&log_stream, element_name),
                                                            last_response_id(""){
    //initialize connection pool
    pool.init(num_unix, num_tcp);
    connection = pool.get_connection<ConnectionType>();
    //connect to Redis server
    atom::error err;
    connection->connect(err);
    if(err){
        logger.error("Unable to connect to Redis: " + err.message());
    }
}

template<typename ConnectionType, typename BufferType>
atom::Client_Element<ConnectionType, BufferType>::~Client_Element(){
    //TODO cleanup the pool and close the connection
};


template<typename ConnectionType, typename BufferType>
std::string  atom::Client_Element<ConnectionType, BufferType>::make_stream_id(std::string element_name, std::string stream_name){
    return "stream:" + element_name + ":" + stream_name;
}

template<typename ConnectionType, typename BufferType>
std::string atom::Client_Element<ConnectionType, BufferType>::make_stream_id(std::string stream_name){
    return stream_name;
}



template class atom::Client_Element<atom::ConnectionPool::UNIX_Redis, atom::ConnectionPool::Buffer_Type>;

template class atom::Client_Element<atom::ConnectionPool::TCP_Redis, atom::ConnectionPool::Buffer_Type>;