////////////////////////////////////////////////////////////////////////////////
//
//  @file test_Redis.cc
//
//  @unit tests for Redis.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Redis.h"


class RedisTest : public testing::Test {

public:

    /* unix socket types */
    using socket_t = boost::asio::local::stream_protocol::socket;
    using endpoint_t = boost::asio::local::stream_protocol::endpoint;

    /* bredis communication types */
    using Buffer = boost::asio::streambuf;
    using Iterator = typename bredis::to_iterator<Buffer>::iterator_t;
    using Policy = bredis::parsing_policy::keep_result;

    RedisTest() :  redis_con(io_con, "/shared/redis.sock"){

    }

	virtual void SetUp() {

	};

	virtual void TearDown() {
        atom::error err;
        redis_con.disconnect(err);
        if(err){
            std::cout<<"Error: " << err.message() << std::endl;
        }
	};

    ~RedisTest(){
    }

    //members
    boost::asio::io_context io_con;
    
    atom::Redis<socket_t, endpoint_t, Buffer, Iterator, Policy> redis_con;

};

//test the synchronous unix connection
TEST_F(RedisTest, sync_connection) {
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
}

//test error detection during bad sync unix connection 
TEST_F(RedisTest, bad_sync_connection){
    atom::error err;

    atom::Redis<socket_t, endpoint_t, Buffer, Iterator, Policy> redis_con_bad(io_con, "/bad/bad.sock");
    redis_con_bad.connect(err);
    EXPECT_THAT(err.code(), atom::error_codes::redis_error);
    EXPECT_THAT(err.message(), "No such file or directory");}

//test the asynchronous unix connection
TEST_F(RedisTest, async_connection) {
    atom::error err;
    redis_con.start(err);
    
    io_con.run();
    EXPECT_THAT(err.code(), atom::error_codes::no_error);
}

//test stream command XADD - nominal
TEST_F(RedisTest, xadd){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    const char * test_data = "hello world";

    atom::error xadd_err;
    const atom::redis_reply reply = redis_con.xadd("test_stream", "test_val", test_data, xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("\\d*\\-\\d?"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XADD - redis error detection
TEST_F(RedisTest, xadd_redis_err){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    const char * test_data = "Hello world";
    std::string bad_id = "74d80474-e6f2-4c01-8e68-908a9f44b05f";
    atom::error xadd_err;

    const atom::redis_reply reply = redis_con.xadd("test_stream", bad_id, "test_val", test_data, xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::redis_error);
    EXPECT_THAT(xadd_err.message(), ::testing::StrEq("atom has encountered a redis error"));
    EXPECT_THAT(xadd_err.redis_error(), "ERR Invalid stream ID specified as stream command argument");

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XRANGE - nominal
TEST_F(RedisTest, xrange){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xrange_err;
    const atom::redis_reply reply = redis_con.xrange("test_stream", "-", "+", "1", xrange_err);
    EXPECT_THAT(xrange_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("[0-9]{1,13}-[0-9]?"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XGROUP - nominal
TEST_F(RedisTest, xgroup){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xgroup_err;
    const atom::redis_reply reply = redis_con.xgroup("test_stream", "my_group", "$", xgroup_err);
    EXPECT_THAT(xgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("OK"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}


//test stream command XREADGROUP - nominal
TEST_F(RedisTest, xreadgroup){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");

    atom::error xreadgroup_err;
    const atom::redis_reply reply = redis_con.xreadgroup("my_group", "consumer_id", "1", "1",  "test_stream", ">", xreadgroup_err);
    EXPECT_THAT(xreadgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XREAD - nominal
TEST_F(RedisTest, xread){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xread_err;
    const atom::redis_reply reply = redis_con.xread("2", "test_stream", "0-0", xread_err);
    EXPECT_THAT(xread_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XACK - nominal
TEST_F(RedisTest, xack){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xack_err;
    const atom::redis_reply reply = redis_con.xack("test_stream", "my_group", "0-0", xack_err);
    EXPECT_THAT(xack_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("[0-9]"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command SET - nominal
TEST_F(RedisTest, set){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error set_err;
    const atom::redis_reply reply = redis_con.set("test_stream", "42", set_err);
    EXPECT_THAT(set_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("OK"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}

//test stream command XDEL - nominal
TEST_F(RedisTest, xdel){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    //add to a stream with xadd
    atom::error xadd_err;
    const atom::redis_reply reply0 = redis_con.xadd("my_stream5", "test_field0", "my first data here", xadd_err);
    const atom::redis_reply reply1 = redis_con.xadd("my_stream5", "test_field1", "my second data here", xadd_err);
    std::cout<< "REPLY 1 "<< *reply1.data.get()<<std::endl;
    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply1.data.get(), ::testing::ContainsRegex("[0-9]{1,13}-[0-9]?"));
    EXPECT_THAT(reply1.size, ::testing::Ge(0));

    //grab the id
    std::string redis_reply = *reply1.data.get();
    size_t position = redis_reply.find("\r\n");
    std::string id = redis_reply.substr(redis_reply.find("\r\n")+position-1);

    //release buffer
    redis_con.release_rx_buffer(reply1.size);

    //delete id with xdel
    atom::error xdel_err;
    const atom::redis_reply reply2 = redis_con.xdel("my_stream5", id, xdel_err);
    EXPECT_THAT(xdel_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply2.data.get(), ::testing::ContainsRegex("1"));
    EXPECT_THAT(reply2.size, ::testing::Ge(0));

    //release buffer
    redis_con.release_rx_buffer(reply2.size);
}

//test stream command SCRIPT LOAD - nominal
TEST_F(RedisTest, load_script){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    

    atom::error script_err;
    const atom::redis_reply reply = redis_con.load_script("/atom/lua-scripts/stream_reference.lua", script_err);
    EXPECT_THAT(script_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::HasSubstr("$"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    redis_con.release_rx_buffer(reply.size);
}