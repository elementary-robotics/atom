#include <gmock/gmock.h>

#include "ConnectionPool.h"

class MockConnectionPool : public atom::ConnectionPool {
    public:
    MockConnectionPool(boost::asio::io_context & iocon, int max, std::string redis_ip) : 
        atom::ConnectionPool(iocon, max, 1, redis_ip, 20, 1) {}
    
    MOCK_METHOD(std::shared_ptr<UNIX_Redis>, make_unix, ());
    MOCK_METHOD(std::shared_ptr<TCP_Redis>, make_tcp, (std::string));

};