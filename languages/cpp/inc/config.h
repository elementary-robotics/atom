

namespace atom{

///General Parameters for atom
enum params {
    ACK_TIMEOUT=1000, ///<1000 timeout to wait for ACK from Redis
    COMMAND_DEFAULT_TIMEOUT_MS=1000, ///<1000 default timeout for commands
    RESPONSE_TIMEOUT=1000, ///<1000 response timeout
    BUFFER_CAP=20///<20 max number of buffers in BufferPool
};

}