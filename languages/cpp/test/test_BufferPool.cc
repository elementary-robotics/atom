////////////////////////////////////////////////////////////////////////////////
//
//  @file test_BufferPool.cc
//
//  @unit tests for test_BufferPool.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "BufferPool.h"


class BufferPoolTest : public testing::Test {

public:

    using Buffer = boost::asio::streambuf;

    BufferPoolTest() :  buffer_pool(5, 100){
        buffer_pool.init();
    }

	virtual void SetUp() {
        
	};

	virtual void TearDown() {

	};

    ~BufferPoolTest(){
    }
    
    atom::BufferPool<Buffer> buffer_pool;
    
};

TEST_F(BufferPoolTest, throwinit){
    try {
        atom::BufferPool<Buffer> BP(200, 100);
    } catch(std::runtime_error & e){
        EXPECT_THAT(e.what(), ::testing::HasSubstr("Maximum number of buffers in pool is limited"));
        return;
    }
    //shouldn't get here.
    FAIL();
}


TEST_F(BufferPoolTest, get_buffer){
    
    EXPECT_THAT(buffer_pool.buffers_available(), 5);


    std::thread thread1([&] {auto buf = buffer_pool.get_buffer();});
    std::thread thread2([&] {auto buf = buffer_pool.get_buffer();});

    thread1.join();
    thread2.join();

    EXPECT_THAT(buffer_pool.buffers_available(), 3);
}


TEST_F(BufferPoolTest, dyanamic_buffer_creation){
    
    EXPECT_THAT(buffer_pool.buffers_available(), 5);

    std::vector<std::thread> threads;

    for(int i = 0; i < 6; i++){
        threads.push_back(std::thread([&] {auto buf = buffer_pool.get_buffer();}));
    }

    for(auto& t : threads){
        t.join();
    }

    EXPECT_THAT(buffer_pool.buffers_available(), 0);
    EXPECT_THAT(buffer_pool.count_buffers(), 6);

}


TEST_F(BufferPoolTest, wait_for_buf){
    
    atom::BufferPool<Buffer> BP(20, 100);
    BP.init();

    EXPECT_THAT(BP.buffers_available(), 20);

    std::vector<std::thread> threads;

    auto buf = BP.get_buffer();

    for(int i = 0; i < 20; i++){
        threads.push_back(std::thread([&] {auto buf = BP.get_buffer();}));
    }
    
    threads.push_back(std::thread([&] {BP.release_buffer(buf);}));

    for(auto& t : threads){
        t.join();
    }

    EXPECT_THAT(BP.buffers_available(), 0);
    EXPECT_THAT(BP.count_buffers(), 20);
}