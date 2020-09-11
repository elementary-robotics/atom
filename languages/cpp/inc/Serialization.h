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

#include "config.h"

#include <boost/tokenizer.hpp>
#include <boost/variant.hpp>
#include <msgpack.hpp>

#include "Logger.h"
#include "Error.h"




namespace atom {
    class Serialization {
    public:
        ///Serialization methods supported by atom
        enum method {
            none, ///< no serialization
            msgpack, ///< msgpack c++ serialization
            arrow ///< arrow (not supported yet)
        };

        Serialization() : method_strings({
                {method::none, "none"},
                {method::msgpack, "msgpack"},
                {method::arrow, "arrow"}
            }), logger(&std::cout, "Serializer"){}
        Serialization(std::ostream& logstream, std::string log_name) : method_strings({
                {method::none, "none"},
                {method::msgpack, "msgpack"},
                {method::arrow, "arrow"}
            }), logger(&logstream, log_name){}

        virtual ~Serialization(){}

        template<typename DataType>
        std::vector<std::string> serialize(DataType& entry_data, atom::Serialization::method ser_method, atom::error& err){
            //serialize
            switch(ser_method){
                case atom::Serialization::method::msgpack:
                {
                    //serialize the values (leave the keys alone)
                    std::vector<std::string> processed_data;
                    bool is_value = false;
                    for(auto m: entry_data){
                        if(is_value){
                            std::stringstream serialized_data;
                            serialize_msgpack(m, serialized_data);
                            processed_data.push_back(serialized_data.str());
                        } else{
                            std::string key = boost::get<std::string>(m);
                            processed_data.push_back(key);
                        }
                        is_value = !is_value;
                    }

                    return processed_data;
                }
                case atom::Serialization::method::none:
                {   
                    std::vector<std::string> processed_data;
                    try {
                    std::for_each(entry_data.cbegin(), entry_data.cend(), [&](const msgpack::type::variant & elem) {
                                    processed_data.push_back(boost::get<std::string>(elem)); });
                    } catch(boost::bad_get & e) {
                        logger.alert("Must supply data composed only of strings when Serialization::none is selected");
                        throw std::runtime_error("Must supply data composed only of strings when Serialization::none is selected");
                    }
                    return processed_data;
                }
                case atom::Serialization::method::arrow:
                {          
                    err.set_error_code(atom::error_codes::unsupported_command);
                    return std::vector<std::string>{"arrow unsupported"};
                }
                default:
                    throw std::runtime_error("Supplied serialization option is invalid.");
            }

        }

        //streamType should support write(const char*, size_t)
        template<typename MsgPackType, typename streamType> 
        void serialize_msgpack(MsgPackType data, streamType & stream){
            msgpack::pack(stream, data);
        }

        //deserialize msgpack & convert into original msgpack type
        template <typename MsgPackType, typename streamType>
        MsgPackType deserialize_msgpack(streamType & data){
            std::string str(data.str());
            msgpack::object_handle obj_handle = msgpack::unpack(str.data(), str.size());
            try{
                MsgPackType data = obj_handle.get().as<MsgPackType>();
                return data;
            } catch(msgpack::type_error & e){
                logger.alert("Type mismatch: " + std::string(e.what()));
                return MsgPackType();
            }
        }

    const std::map<method, std::string> method_strings;

    private:
        //members
        Logger logger;
    };
}

#endif // __ATOM_CPP_SERIALIZATION_H