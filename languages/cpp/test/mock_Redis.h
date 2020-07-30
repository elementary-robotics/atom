#include <gmock/gmock.h>

#include <Redis.h>

template<typename socket, typename endpoint, typename buffer, typename iterator, typename policy> 
class MockRedis : public atom::Redis<socket, endpoint, buffer, iterator, policy> {
    public:
    MockRedis(boost::asio::io_context & iocon, std::string unix_addr) : 
        atom::Redis<socket, endpoint, buffer, iterator, policy>(iocon, unix_addr) {}
    
    using atom::Redis<socket, endpoint, buffer, iterator, policy>::wrap_socket;
    MOCK_METHOD(void, wrap_socket, (), (override));
};