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

    RedisTest() :  redis_con(io_con, "/shared/redis.sock", num_buffers, buffer_timeout){

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
    int num_buffers=20;
    int buffer_timeout=1000;


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

    atom::Redis<socket_t, endpoint_t, Buffer, Iterator, Policy> redis_con_bad(io_con, "/bad/bad.sock", num_buffers, buffer_timeout);
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
    atom::redis_reply<Buffer> reply = redis_con.xadd("test_stream", "test_val", test_data, xadd_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);

    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*data.first.get(), ::testing::ContainsRegex("\\d*\\-\\d?"));
    EXPECT_THAT(data.second, ::testing::Eq(15));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XADD with serialization - nominal
TEST_F(RedisTest, xadd_vector){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    std::vector<std::string> test_data;
    for(int i=0; i< 4; i++){
        test_data.push_back("key_" + std::to_string(i));
        test_data.push_back("value_" + std::to_string(i));
    }

    atom::error xadd_err;
    atom::redis_reply<Buffer> reply = redis_con.xadd("test_stream", "none", test_data, xadd_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);

    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*data.first.get(), ::testing::ContainsRegex("\\d*\\-\\d?"));
    EXPECT_THAT(data.second, ::testing::Eq(15));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XADD - redis error detection
TEST_F(RedisTest, xadd_redis_err){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    const char * test_data = "Hello world";
    std::string bad_id = "74d80474-e6f2-4c01-8e68-908a9f44b05f";
    atom::error xadd_err;

    atom::redis_reply<Buffer> reply = redis_con.xadd("test_stream", bad_id, "test_val", test_data, xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::redis_error);
    EXPECT_THAT(xadd_err.message(), ::testing::StrEq("atom has encountered a redis error"));
    EXPECT_THAT(xadd_err.redis_error(), "ERR Invalid stream ID specified as stream command argument");
    
    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XRANGE - nominal
TEST_F(RedisTest, xrange){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xrange_err;
    atom::redis_reply<Buffer> reply = redis_con.xrange("test_stream", "-", "+", "2", xrange_err); //TODO fix the case where count > 1
    auto data = boost::get<atom::reply_type::entry_response>(reply.parsed_reply);

    EXPECT_THAT(xrange_err.code(), atom::error_codes::no_error);
    for(auto m: data){
        EXPECT_THAT(m.first, ::testing::ContainsRegex("[0-9]{1,13}-[0-9]?"));
        for(auto p: m.second){
            EXPECT_THAT(p.second,::testing::Ge(0));
        }
    }

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XRANGE - nominal
TEST_F(RedisTest, xrevrange){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xrange_err;
    atom::redis_reply<Buffer> reply = redis_con.xrevrange("test_stream", "+", "-", "2", xrange_err); //TODO fix the case where count > 1
    auto data = boost::get<atom::reply_type::entry_response>(reply.parsed_reply);

    EXPECT_THAT(xrange_err.code(), atom::error_codes::no_error);
    for(auto m: data){
        EXPECT_THAT(m.first, ::testing::ContainsRegex("[0-9]{1,13}-[0-9]?"));
        for(auto p: m.second){
            EXPECT_THAT(p.second,::testing::Ge(0));
        }
    }

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XGROUP - nominal
TEST_F(RedisTest, xgroup){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xgroup_err;
    atom::redis_reply<Buffer> reply = redis_con.xgroup("test_stream", "my_group", "$", xgroup_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);

    EXPECT_THAT(xgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*data.first.get(), ::testing::ContainsRegex("OK"));
    EXPECT_THAT(data.second, ::testing::Eq(2));

    //release buffer
    redis_con.release_rx_buffer(reply);
}


//test stream command XREADGROUP - nominal
TEST_F(RedisTest, xreadgroup){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");

    atom::error xreadgroup_err;
    atom::redis_reply<Buffer> reply = redis_con.xreadgroup("my_group", "consumer_id", "1", "1",  "test_stream", "0", xreadgroup_err); //TODO handle (nil) from >
    auto data = boost::get<atom::reply_type::entry_response_list>(reply.parsed_reply);
    EXPECT_THAT(xreadgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(data.size(), 1);
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XGROUP DESTROY - nominal
TEST_F(RedisTest, xgroup_destroy){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xgroup_err;
    atom::redis_reply<Buffer> reply = redis_con.xgroup_destroy("test_stream", "my_group", xgroup_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);
    EXPECT_THAT(xgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(std::string(*data.first.get(), data.second), ::testing::Eq("1"));
    EXPECT_THAT(data.second, ::testing::Eq(1));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XREAD - nominal
TEST_F(RedisTest, xread){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xread_err;
    atom::redis_reply<Buffer> reply = redis_con.xread("2", "test_stream", "0-0", xread_err);
    auto data = boost::get<atom::reply_type::entry_response_list>(reply.parsed_reply);

    for(auto v: data){
        for(auto m: v){
        EXPECT_THAT(m.first, ::testing::ContainsRegex("[0-9]{1,13}-[0-9]?"));
        for(auto p: m.second){
            EXPECT_THAT(p.second,::testing::Ge(0));
        }
    }
    }

    EXPECT_THAT(xread_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XACK - nominal
TEST_F(RedisTest, xack){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error xack_err;
    atom::redis_reply<Buffer> reply = redis_con.xack("test_stream", "my_group", "0-0", xack_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);

    EXPECT_THAT(xack_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(std::string(*data.first.get(), data.second), ::testing::ContainsRegex("[0-9]"));
    EXPECT_THAT(data.second, ::testing::Eq(1));

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command SET - nominal
TEST_F(RedisTest, set){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    atom::error set_err;
    atom::redis_reply<Buffer> reply = redis_con.set("test_stream2", "42", set_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);
    EXPECT_THAT(set_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(std::string(*data.first.get(), data.second), ::testing::Eq("OK"));
    EXPECT_THAT(data.second, 2);

    //release buffer
    redis_con.release_rx_buffer(reply);
}

//test stream command XDEL - nominal
TEST_F(RedisTest, xdel){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    //add to a stream with xadd
    atom::error xadd_err;
    atom::redis_reply<Buffer> reply0 = redis_con.xadd("my_stream5", "test_field0", "my first data here", xadd_err);
    atom::redis_reply<Buffer> reply1 = redis_con.xadd("my_stream5", "test_field1", "my second data here", xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);

    //grab the id
    auto redis_reply = boost::get<atom::reply_type::flat_response>(reply1.parsed_reply);
    std::string id = std::string(*redis_reply.first.get(), redis_reply.second);

    //release buffer
    redis_con.release_rx_buffer(reply0);
    redis_con.release_rx_buffer(reply1);

    //delete id with xdel
    atom::error xdel_err;
    atom::redis_reply<Buffer> reply2 = redis_con.xdel("my_stream5", id, xdel_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply2.parsed_reply);
    EXPECT_THAT(xdel_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(std::string(*data.first.get(), data.second), ::testing::Eq("1"));
    EXPECT_THAT(data.second, ::testing::Eq(1));

    //release buffer
    redis_con.release_rx_buffer(reply2);
}

//test stream command SCRIPT LOAD - nominal
TEST_F(RedisTest, load_script){
    atom::error err;

    redis_con.connect(err);
    EXPECT_THAT(err.message(), "Success");
    

    atom::error script_err;
    atom::redis_reply<Buffer> reply = redis_con.load_script("/atom/lua-scripts/stream_reference.lua", script_err);
    auto data = boost::get<atom::reply_type::flat_response>(reply.parsed_reply);
    EXPECT_THAT(script_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(data.second, ::testing::Eq(40));

    //release buffer
    redis_con.release_rx_buffer(reply);
}