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

#include <mock_Redis.h>


class RedisTest : public testing::Test {

public:

    /* unix socket types */
    using socket_t = boost::asio::local::stream_protocol::socket;
    using endpoint_t = boost::asio::local::stream_protocol::endpoint;

    /* bredis communication types */
    using Buffer = boost::asio::streambuf;
    using Iterator = typename bredis::to_iterator<Buffer>::iterator_t;
    using Policy = bredis::parsing_policy::keep_result;

    RedisTest() :  mock_redis(io_con, "/shared/redis.sock"){

    }

	virtual void SetUp() {

	};

	virtual void TearDown() {
        atom::error err;
        mock_redis.disconnect(err);
        if(err){
            std::cout<<"Error: " << err.message()<<std::endl;
        }
	};

    ~RedisTest(){
    }

    //members
    boost::asio::io_context io_con;
    
    MockRedis<socket_t, endpoint_t, Buffer, Iterator, Policy> mock_redis;

};

//test the synchronous unix connection
TEST_F(RedisTest, sync_connection) {
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
}

//test error detection during bad sync unix connection 
TEST_F(RedisTest, bad_sync_connection){
    atom::error err;

    MockRedis<socket_t, endpoint_t, Buffer, Iterator, Policy> mock_redis_bad(io_con, "/bad/bad.sock");
    mock_redis_bad.connect(err);
    EXPECT_THAT(err.code(), atom::error_codes::redis_error);
    EXPECT_THAT(err.message(), "No such file or directory");}

//test the asynchronous unix connection
TEST_F(RedisTest, async_connection) {
    atom::error err;
    mock_redis.start(err);
    
    io_con.run();
    EXPECT_THAT(err.code(), atom::error_codes::no_error);
}

//test stream command XADD - nominal
TEST_F(RedisTest, xadd){
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    const char * test_data = "hello world";

    atom::error xadd_err;
    const atom::redis_reply reply = mock_redis.xadd("test_stream", "test_key", test_data, xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("\\d*\\-\\d?"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    mock_redis.release_rx_buffer(reply.size);
}

//test stream command XADD - redis error detection
TEST_F(RedisTest, xadd_redis_err){
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
    
    const char * test_data = "Hello world";
    std::string bad_id = "74d80474-e6f2-4c01-8e68-908a9f44b05f";
    atom::error xadd_err;

    const atom::redis_reply reply = mock_redis.xadd("test_stream", bad_id, "test_key", test_data, xadd_err);
    EXPECT_THAT(xadd_err.code(), atom::error_codes::redis_error);
    EXPECT_THAT(xadd_err.message(), ::testing::StrEq("atom has encountered a redis error"));
    EXPECT_THAT(xadd_err.redis_error(), "ERR Invalid stream ID specified as stream command argument");

    mock_redis.release_rx_buffer(reply.size);
}

//test stream command XRANGE - nominal
TEST_F(RedisTest, xrange){
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
    

    atom::error xrange_err;
    const atom::redis_reply reply = mock_redis.xrange("test_stream", "-", "+", "1", xrange_err);
    EXPECT_THAT(xrange_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("\\d*\\-\\d?"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    mock_redis.release_rx_buffer(reply.size);
}

//test stream command XGROUP - nominal
TEST_F(RedisTest, xgroup){
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
    

    atom::error xgroup_err;
    const atom::redis_reply reply = mock_redis.xgroup("test_stream", "my_group", "$", xgroup_err);
    EXPECT_THAT(xgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(*reply.data.get(), ::testing::ContainsRegex("OK"));
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    mock_redis.release_rx_buffer(reply.size);
}


//test stream command XREADGROUP - nominal
TEST_F(RedisTest, xreadgroup){
    atom::error err;

    mock_redis.connect(err);
    EXPECT_THAT(err.message(), "Success");
    

    atom::error xreadgroup_err;
    const atom::redis_reply reply = mock_redis.xreadgroup("my_group", "consumer_id", "1", "1",  "test_stream", ">", xreadgroup_err);
    EXPECT_THAT(xreadgroup_err.code(), atom::error_codes::no_error);
    EXPECT_THAT(reply.size, ::testing::Ge(0));

    mock_redis.release_rx_buffer(reply.size);
}