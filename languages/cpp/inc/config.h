////////////////////////////////////////////////////////////////////////////////
//
//  @file config.h
//
//  @brief config header, holds atom configuration information
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////


#ifndef __ATOM_CPP_CONFIG_H
#define __ATOM_CPP_CONFIG_H

namespace atom{

///General Parameters for atom
enum params {
    ACK_TIMEOUT=1000, ///<1000 timeout to wait for ACK from Redis
    COMMAND_DEFAULT_TIMEOUT_MS=1000, ///<1000 default timeout for commands
    RESPONSE_TIMEOUT=1000, ///<1000 response timeout
    BUFFER_CAP=20///<20 max number of buffers in BufferPool
};

///Version and Language information for atom (TODO what version is this?)
#define LANGUAGE "c++11"
#define VERSION "0"

//Serialization: enable msgpack to use boost. Necessary to pack and unpack variant types
#define MSGPACK_USE_BOOST 1

}

#endif //__ATOM_CPP_CONFIG_H