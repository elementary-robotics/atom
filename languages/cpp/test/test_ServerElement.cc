////////////////////////////////////////////////////////////////////////////////
//
//  @file test_ServerElement.cc
//
//  @unit tests for ServerElement.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <thread>


#include "Server_Element.h"
#include "Serialization.h"

using my_type = msgpack::type::tuple<int, bool, std::string>;


class ServerElementTest : public testing::Test {

public:

    ServerElementTest() : 
        ip("172.20.0.2"),
        server_elem(iocon, 100, 1000, ip, ser, 10, 1000, 5, 5, std::cout, "ServerElement"),
        redis(iocon, "/shared/redis.sock", 5, 1000) {
    }

	virtual void SetUp() {
        atom::error err;
        redis.connect(err);
	};

	virtual void TearDown() {

	};

    ~ServerElementTest(){
    }
    
    boost::asio::io_context iocon;
    std::string ip;
    int num_buffers = 20;
    int buffer_timeout = 1;
    atom::Serialization ser;
    atom::Server_Element<atom::ConnectionPool::UNIX_Redis, atom::ConnectionPool::Buffer_Type> server_elem;
    atom::ConnectionPool::UNIX_Redis redis;

};

TEST_F(ServerElementTest, init_ServerElement){

}

TEST_F(ServerElementTest, entry_write){

    atom::error err;
    std::vector<std::string> entry_data = {"hello", "world", "I like", "cake"};
    server_elem.entry_write("server_stream", entry_data, atom::Serialization::method::none, err);
    if(err){
        std::cout << err.message() << std::endl;
    }
}

TEST_F(ServerElementTest, entry_write_msgpack){

    atom::error err;
    std::vector<std::string> entry_data = {"hello", "world", "I like", "cake"};
    server_elem.entry_write("server_stream", entry_data, atom::Serialization::method::msgpack, err);
    if(err){
        std::cout << err.message() << std::endl;
    }
}

TEST_F(ServerElementTest, entry_write_invalid_key){

    atom::error err;
    std::vector<std::string> entry_data = {"ser", "my_ser_method", "I like", "cake"};
    server_elem.entry_write("server_stream", entry_data, atom::Serialization::method::msgpack, err);
    const int err_code = err.code();
    EXPECT_EQ(err_code, static_cast<atom::error_codes>(atom::error_codes::invalid_command));
}

TEST_F(ServerElementTest, entry_write_keyval_pair_error){

    atom::error err;
    std::vector<std::string> entry_data = {"hello", "world", "I like"};
    server_elem.entry_write("server_stream", entry_data, atom::Serialization::method::msgpack, err);
    const int err_code = err.code();
    EXPECT_EQ(err_code, static_cast<atom::error_codes>(atom::error_codes::invalid_command));
}

TEST_F(ServerElementTest, entry_write_empty_vector){

    atom::error err;
    std::vector<std::string> entry_data;
    server_elem.entry_write("server_stream", entry_data, atom::Serialization::method::msgpack, err);
    const int err_code = err.code();
    EXPECT_EQ(err_code, static_cast<atom::error_codes>(atom::error_codes::invalid_command));
}

