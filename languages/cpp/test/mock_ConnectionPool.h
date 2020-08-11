#include <gmock/gmock.h>

#include "ConnectionPool.h"

class MockConnectionPool : public atom::ConnectionPool {
    public:
    MockConnectionPool(boost::asio::io_context & iocon, std::string redis_ip) : 
        atom::ConnectionPool(iocon, 10, 1, redis_ip) {}
    
    MOCK_METHOD(std::shared_ptr<UNIX_Redis>, make_unix, ());
    MOCK_METHOD(std::shared_ptr<TCP_Redis>, make_tcp, (std::string));

};