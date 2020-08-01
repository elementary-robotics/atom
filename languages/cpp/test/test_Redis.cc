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
    EXPECT_THAT(xadd_err.message(), ::testing::StrEq("ERR Invalid stream ID specified as stream command argument"));
}