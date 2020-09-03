////////////////////////////////////////////////////////////////////////////////
//
//  @file Serialization.h
//
//  @brief Serialization for atom - msgpack only thus far
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////



#ifndef __ATOM_CPP_SERIALIZATION_H
#define __ATOM_CPP_SERIALIZATION_H

#include <iostream>
#include <typeinfo>

#include <boost/tokenizer.hpp>
#include <boost/variant.hpp>
#include <msgpack.hpp>

#include "Logger.h"





namespace atom {
    class Serialization {
    public:
        ///Serialization methods supported by atom
        enum method {
            none, ///< no serialization
            msgpack, ///< msgpack c++ serialization
            arrow ///< arrow (not supported yet)
        };

        Serialization() : logger(&std::cout, "Serializer"){}
        Serialization(std::ostream& logstream, std::string log_name) : logger(&logstream, log_name){}

        virtual ~Serialization(){}

        //streamType should support write(const char*, size_t)
        template<typename MsgPackType, typename streamType> 
        void serialize(MsgPackType data, streamType & stream){
            msgpack::pack(stream, data);
        }

        //deserialize msgpack & convert into original msgpack type
        template <typename MsgPackType, typename streamType>
        MsgPackType deserialize(streamType & data){
            std::string str(data.str());
            msgpack::object_handle obj_handle = msgpack::unpack(str.data(), str.size());
            try{
                MsgPackType original_type;
                msgpack::object deserialized = obj_handle.get();
                deserialized.convert(original_type);
                return original_type;
            } catch(msgpack::type_error & e){
                logger.alert("Type mismatch: " + std::string(e.what()));
                return MsgPackType();
            }
        }

    private:
        //members
        Logger logger;
    };
}

#endif // __ATOM_CPP_SERIALIZATION_H