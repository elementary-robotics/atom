////////////////////////////////////////////////////////////////////////////////
//
//  @file test_ConnectionPool.cc
//
//  @unit tests for ConnectionPool.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <vector>


#include "mock_Redis.h"
#include "mock_ConnectionPool.h"


class ConnectionPoolTest : public testing::Test {

public:

    ConnectionPoolTest() :  mock_connection_pool(iocon, 100, "172.24.0.2"){
    }

	virtual void SetUp() {
        EXPECT_CALL(mock_connection_pool, make_unix())
            .Times(5)
            .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::UNIX_Redis>(iocon, "/shared/redis.sock") ));

        EXPECT_CALL(mock_connection_pool, make_tcp(::testing::StrEq("172.24.0.2")))
            .Times(5)
            .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::TCP_Redis>(iocon, "172.24.0.2", 6379) ));
        
        mock_connection_pool.init(5, 5);

        ASSERT_THAT(mock_connection_pool.number_open_unix(), 5);
        ASSERT_THAT(mock_connection_pool.number_open_tcp(), 5);
	};

	virtual void TearDown() {

	};

    ~ConnectionPoolTest(){
    }
    
    boost::asio::io_context iocon;
    MockConnectionPool mock_connection_pool;
    
};

TEST_F(ConnectionPoolTest, init_ConnectionPool){
    MockConnectionPool mock_CP(iocon, 10, "172.24.0.2");

    EXPECT_CALL(mock_CP, make_unix())
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::UNIX_Redis>(iocon, "/shared/redis.sock") ));

    EXPECT_CALL(mock_CP, make_tcp(::testing::StrEq("172.24.0.2")))
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::TCP_Redis>(iocon, "172.24.0.2", 6379) ));

    mock_CP.init(5, 5);

    ASSERT_THAT(mock_CP.number_open_unix(), 5);
    ASSERT_THAT(mock_CP.number_open_tcp(), 5);

    EXPECT_THAT(mock_CP.number_available_unix(), 5);
    EXPECT_THAT(mock_CP.number_available_tcp(), 5);
}


TEST_F(ConnectionPoolTest, get_tcp){


    std::thread thread1([&] {auto conn = mock_connection_pool.get_tcp_connection(); });
    std::thread thread2([&] {auto conn = mock_connection_pool.get_tcp_connection(); });

    thread1.join();
    thread2.join();

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 5);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 3);

}

TEST_F(ConnectionPoolTest, get_unix){


    std::thread thread1([&] {auto conn = mock_connection_pool.get_unix_connection(); });
    std::thread thread2([&] {auto conn = mock_connection_pool.get_unix_connection(); });

    thread1.join();
    thread2.join();

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 3);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 5);

}

TEST_F(ConnectionPoolTest, get_release_unix){

    auto existing_con = mock_connection_pool.get_unix_connection();
    std::thread thread1([&] {auto conn = mock_connection_pool.get_unix_connection(); });
    std::thread thread2([&] {mock_connection_pool.release_connection(existing_con); });

    thread1.join();
    thread2.join();

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 4);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 5);

}

TEST_F(ConnectionPoolTest, get_release_tcp){

    auto existing_con = mock_connection_pool.get_tcp_connection();
    std::thread thread1([&] {auto conn = mock_connection_pool.get_tcp_connection(); });
    std::thread thread2([&] {mock_connection_pool.release_connection(existing_con); });

    thread1.join();
    thread2.join();

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 5);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 4);

}

TEST_F(ConnectionPoolTest, test_tcp_resize){

    std::vector<std::thread> threads;

    //expect to double the number of connections available
    EXPECT_CALL(mock_connection_pool, make_tcp(::testing::StrEq("172.24.0.2")))
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::TCP_Redis>(iocon, "172.24.0.2", 6379) ));


    for(int i = 0; i < 6; i++){
        threads.push_back(std::thread([&] {auto conn = mock_connection_pool.get_tcp_connection();}));
    }
    
    for(auto& t : threads){
        t.join();
    }

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 5);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 4);

}

TEST_F(ConnectionPoolTest, test_unix_resize){

    std::vector<std::thread> threads;

    //expect to double the number of connections available
    EXPECT_CALL(mock_connection_pool, make_unix())
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::UNIX_Redis>(iocon, "/shared/redis.sock") ));


    for(int i = 0; i < 6; i++){
        threads.push_back(std::thread([&] {auto conn = mock_connection_pool.get_unix_connection();}));
    }
    
    for(auto& t : threads){
        t.join();
    }

    EXPECT_THAT(mock_connection_pool.number_available_unix(), 4);
    EXPECT_THAT(mock_connection_pool.number_available_tcp(), 5);

}

TEST_F(ConnectionPoolTest, test_tcp_wait){

    MockConnectionPool mock_CP(iocon, 10, "172.24.0.2");

    EXPECT_CALL(mock_CP, make_unix())
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::UNIX_Redis>(iocon, "/shared/redis.sock") ));

    EXPECT_CALL(mock_CP, make_tcp(::testing::StrEq("172.24.0.2")))
        .Times(5)
        .WillRepeatedly(::testing::Return(std::make_shared<atom::ConnectionPool::TCP_Redis>(iocon, "172.24.0.2", 6379) ));

    mock_CP.init(5, 5);

    std::vector<std::thread> threads;

    auto existing_con = mock_CP.get_unix_connection();

    for(int i = 0; i < 5; i++){
        threads.push_back(std::thread([&] {auto conn = mock_CP.get_unix_connection();}));
    }

    threads.push_back(std::thread([&] {mock_CP.release_connection(existing_con);}));

    
    for(auto& t : threads){
        t.join();
    }

    EXPECT_THAT(mock_connection_pool.number_open_unix(), 5);
    EXPECT_THAT(mock_connection_pool.number_open_tcp(), 5);

}