////////////////////////////////////////////////////////////////////////////////
//
//  @file test_Redis.cc
//
//  @unit tests for Redis.cc
//
//  @copy 2018 Elementary Robotics. All rights reserved.
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

TEST_F(RedisTest, sync_connection) {
    atom::error err;

    EXPECT_CALL(mock_redis, wrap_socket());
    EXPECT_THAT(err.message(), "Success");
    mock_redis.connect(err);

}
