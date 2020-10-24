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
#include <stdlib.h>

#include "Client_Element.h"
#include "Serialization.h"

using my_type = msgpack::type::tuple<int, bool, std::string>;
int callback_counter = 0;

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

TEST_F(ClientElementTest, entry_read_n_msgpack){

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

    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply1 = redis.xadd(stream_name, "msgpack", my_data, err);
    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply2 = redis.xrevrange(stream_name, "+", "-", "2", err);

    //read the entry
    auto entries = client_elem.entry_read_n<msgpack::type::variant>("MyElem", "client_stream", 1, err, atom::Serialization::method::msgpack, false);

    //verify keys
    EXPECT_THAT(entries[0].get_msgpack().data[0].key(), my_data[0]);
    EXPECT_THAT(entries[0].get_msgpack().data[2].key(), my_data[2]);

    //verify values
    auto deser_data = entries[0].get_msgpack().data[1].value();
    msgpack::type::variant  expected("world");
    msgpack::type::variant received = *deser_data.get();
    EXPECT_THAT(received, expected);

    auto deser_data1 = entries[0].get_msgpack().data[3].value();
    msgpack::type::variant  expected1("chocolate");
    msgpack::type::variant received1 = *deser_data1.get();
    EXPECT_THAT(received1, expected1);

    //cleanup
    reply1.cleanup();
    reply2.cleanup();

}

TEST_F(ClientElementTest, entry_read_n_none){

    std::string stream_name = "stream:MyElem:client_stream";
    atom::error err;

    //serialize and write an entry like server element would
    std::vector<std::string> my_data = {"hello", "world", "ice_cream", "chocolate"};
    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply1 = redis.xadd(stream_name, "none", my_data, err);
    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply2 = redis.xrevrange(stream_name, "+", "-", "1", err);

    //read the entry
    auto entries = client_elem.entry_read_n<>("MyElem", "client_stream", 1, err, atom::Serialization::method::none, false);

     //verify keys
    EXPECT_THAT(entries[0].get_raw().data[0].key(), my_data[0]);
    EXPECT_THAT(entries[0].get_raw().data[2].key(), my_data[2]);

    //verify values
    auto received = entries[0].get_raw().data[1].svalue();
    std::string expected = "world";
    EXPECT_THAT(received, expected);

    auto received1 = entries[0].get_raw().data[3].svalue();
    std::string expected1 = "chocolate";
    EXPECT_THAT(received1, expected1);

    //cleanup
    reply1.cleanup();
    reply2.cleanup();

}

TEST_F(ClientElementTest, entry_read_since){

    std::string stream_name = "stream:MyElem:client_stream";
    atom::error err;

    //serialize and write an entry like server element would
    std::vector<std::string> my_data = {"hello", "world", "ice_cream", "chocolate"};
    atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply1 = redis.xadd(stream_name, "none", my_data, err);

    auto flat = reply1.flat_response();
    std::string id = atom::reply_type::to_string(flat);
    reply1.cleanup();
    
    std::vector<atom::entry<boost::asio::streambuf, msgpack::type::variant>> entries;

    //write the entry
    std::thread t1([&]{
        sleep(2);
        atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply2 = redis.xadd(stream_name, "none", my_data, err);
        reply2.cleanup();
    });

    //read entries
    std::thread t2([&]{
        entries = client_elem.entry_read_since<>("MyElem", "client_stream", 1, err, "$", "10000", atom::Serialization::method::none, false);
    });

    t2.join();
    t1.join();

    //verify keys
    EXPECT_THAT(entries[0].get_raw().data[0].key(), my_data[0]);
    EXPECT_THAT(entries[0].get_raw().data[2].key(), my_data[2]);

    //verify values
    auto received = entries[0].get_raw().data[1].svalue();
    std::string expected = "world";
    EXPECT_THAT(received, expected);

    auto received1 = entries[0].get_raw().data[3].svalue();
    std::string expected1 = "chocolate";
    EXPECT_THAT(received1, expected1);    
}

TEST_F(ClientElementTest, entry_read_since_timeout){

    atom::error err;
    std::vector<atom::entry<boost::asio::streambuf, msgpack::type::variant>> entries;
    auto start = std::chrono::high_resolution_clock::now();
    entries = client_elem.entry_read_since<>("MyElem", "client_stream", 1, err, "$", "1000", atom::Serialization::method::none, false);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> dur = end - start;

    EXPECT_GT(dur.count(), 1);
    EXPECT_EQ(entries.size(), 0);
}

TEST_F(ClientElementTest, entry_read_since_block0){

    std::string stream_name = "stream:MyElem:client_stream";
    atom::error err;
    std::vector<std::string> data{"yaba", "daba", "doo", "!"};
    std::vector<atom::entry<boost::asio::streambuf, msgpack::type::variant>> entries;
    auto start = std::chrono::high_resolution_clock::now();
    

    //write the entry
    std::thread t1([&]{
        sleep(2);
        atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply2 = redis.xadd(stream_name, "none", data, err);
        reply2.cleanup();
    });

    //read entries
    std::thread t2([&]{
        entries = client_elem.entry_read_since<>("MyElem", "client_stream", 1, err, "$", "0", atom::Serialization::method::none, false);
    });

    t2.join();
    t1.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur = end - start;

    EXPECT_GT(dur.count(), 2);
    EXPECT_EQ(entries.size(), 1);
}

void my_handler(atom::entry<boost::asio::streambuf> entry){
    callback_counter++;
    std::cout<<"Hey I'm a stream handler!"<<std::endl;
    entry.get_msgpack();
}


TEST_F(ClientElementTest, entry_read_loop){

    std::string stream_name = "stream:MyElem:client_stream";
    atom::error err;

    atom::StreamHandler<> handler1("MyElem", "client_stream", &my_handler);

    std::string current_time = client_elem.get_redis_timestamp();
    std::string id = current_time;
    for(int i = 0; i < 3; i++){
        id = std::to_string(std::stol(id) + 10 + i);
        const char * data = ("data " + std::to_string(i)).c_str();
        atom::redis_reply<atom::ConnectionPool::Buffer_Type> reply = redis.xadd(stream_name, id, "key_" + std::to_string(i), data, err);
        redis.release_rx_buffer(reply);
    }

    std::vector<atom::StreamHandler<>> my_stream_handlers{handler1};
    
    client_elem.entry_read_loop(my_stream_handlers, 1);

    EXPECT_THAT(callback_counter, 3);
    callback_counter = 0;
}

TEST_F(ClientElementTest, command_send){

    atom::error err;
    std::cout<<"Entry_read_n"<<std::endl;
    auto entries = client_elem.entry_read_n<msgpack::type::variant>("MyElem", "client_stream", 1, err, atom::Serialization::method::msgpack, false);

    std::cout<<"Element_Response"<<std::endl;
    atom::element_response<boost::asio::streambuf, msgpack::type::variant> response = client_elem.send_command("MyElem", "my_command", entries[0], err);

}