////////////////////////////////////////////////////////////////////////////////
//
//  @file test_ClientElement.cc
//
//  @unit tests for ClientElement.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <thread>


#include "Client_Element.h"
#include "Serialization.h"

using my_type = msgpack::type::tuple<int, bool, std::string>;


class ClientElementTest : public testing::Test {

public:

    ClientElementTest() : 
        ip("172.20.0.2"),
        client_elem(iocon, 100, 1000, ip, ser, 10, 1000, 5, 5, std::cout, "ClientElement"),
        redis(iocon, "/shared/redis.sock", 5, 1000) {
    }

	virtual void SetUp() {
        atom::error err;
        redis.connect(err);
	};

	virtual void TearDown() {

	};

    ~ClientElementTest(){
    }
    
    boost::asio::io_context iocon;
    std::string ip;
    int num_buffers = 20;
    int buffer_timeout = 1;
    atom::Serialization ser;
    atom::Client_Element<atom::ConnectionPool::UNIX_Redis, atom::ConnectionPool::Buffer_Type> client_elem;
    atom::ConnectionPool::UNIX_Redis redis;

};

TEST_F(ClientElementTest, init_ClientElement){

}

TEST_F(ClientElementTest, entry_read_n){

    std::string stream_name = "stream:MyElem:client_stream";
    atom::error err;

    //serialize and write an entry like server element would
    std::vector<std::string> my_data = {"hello", "world", "ice_cream", "chocolate"};
    std::stringstream ss;
    msgpack::pack(ss, my_data[1]);
    my_data[1] = ss.str();
    std::stringstream ss2;
    msgpack::pack(ss2, my_data[3]);
    my_data[3] = ss2.str();

    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply1 = redis.xadd(stream_name,"msgpack", my_data, err);
    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply2 = redis.xrevrange(stream_name, "+", "-", "2", err);

    //read the entry - below gens error!
    auto entries = client_elem.entry_read_n<msgpack::type::variant>("MyElem", "client_stream", 2, err, "msgpack", false);

    //cleanup
    reply1.cleanup();
    reply2.cleanup();

}