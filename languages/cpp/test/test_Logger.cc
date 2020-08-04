////////////////////////////////////////////////////////////////////////////////
//
//  @file test_Logger.cc
//
//  @unit tests for Logger.cc
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Logger.h"


class LoggerTest : public testing::Test {

public:

    LoggerTest() :  logger(&stream, "Test Logger"){

    }

	virtual void SetUp() {

	};

	virtual void TearDown() {
	};

    ~LoggerTest(){
    }
    
   atom::logger logger;
   std::ostringstream stream;
};

TEST_F(LoggerTest, default_logger){
    //default logger should be at info level
    EXPECT_THAT(logger.get_level(), atom::logger::level::INFO);
}

TEST_F(LoggerTest, bad_logger){
    //unsupported logging levels should throw
    EXPECT_THROW(logger.set_level("BOGUS"), std::runtime_error);
}

TEST_F(LoggerTest, emergency){
    std::string message = "This is an emergency message.";
    logger.emergency(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, alert){
    std::string message = "This is an alert message.";
    logger.alert(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, critical){
    std::string message = "This is a critical message.";
    logger.critical(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, error){
    std::string message = "This is an error message.";
    logger.error(message);
    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, warning){
    std::string message = "This is a error message.";
    logger.error(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, notice){
    std::string message = "This is a notice message.";
    logger.notice(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, info){
    std::string message = "This is an info message.";
    logger.info(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}

TEST_F(LoggerTest, debug){
    std::string message = "This is a debug message.";
    logger.debug(message);

    EXPECT_THAT(stream.str(), ::testing::Not(::testing::HasSubstr(message)));
}

TEST_F(LoggerTest, change_level){
    std::string message = "This is a debug message.";
    logger.set_level("DEBUG");
    logger.debug(message);

    EXPECT_THAT(stream.str(), ::testing::HasSubstr(message));
}