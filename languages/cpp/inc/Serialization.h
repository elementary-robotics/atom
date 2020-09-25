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
#include "Parser.h"
#include "Messages.h"



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
                logger.alert("Arrow serialization not supported!");
                err.set_error_code(atom::error_codes::unsupported_command);
                return std::vector<std::string>{"arrow unsupported"};
            }
            default:
                throw std::runtime_error("Supplied serialization option is invalid.");
        }

    }

    //TODO: move the logic over from the Client_Element.
    //template<typename BufferType, typename MsgPackType>
    //void deserialize(std::vector<atom::entry<BufferType, MsgPackType>>& entries, atom::reply_type::entry_response & entry_map){

    //}

    //TODO: clean that garbage up on the bottom.

/*         template<typename DataType>
    void deserialize(atom::reply_type::flat_response resp, atom::Serialization::method ser_method, atom::entry_type::object<DataType>& object, atom::error & err){
        switch(ser_method){
            case atom::Serialization::msgpack:
            {   
                //std::string data_str(*resp.first.get(), resp.second);
                //DataType deser_data = deserialize_msgpack<DataType>(data_str);
                //object.init(std::make_shared<DataType>(deser_data), resp.second);
                // return atom::entry_type::object<DataType>(std::make_shared<DataType>(deser_data), resp.second);
                //return boost::apply_visitor(msgpack_deserializer{}, resp);
                msgpack_serialization<DataType> obj;
                object = obj.deserialize(resp, ser_method, err);
                break;
            }
            case atom::Serialization::none:
            {
                //TODO: handle buffer-lifecycle - so that it exists and is updated via the buffer pool when the entry takes ownership of it
                //object = atom::entry_type::object<DataType>(resp.first, resp.second);
                //object.init(resp.first, resp.second);
                none_serialization<> obj;
                object = obj.deserialize(resp, ser_method, err);
                break;
            }
            case atom::Serialization::arrow:
            {
                logger.alert("Arrow deserialization not supported!");
                err.set_error_code(atom::error_codes::unsupported_command);
            }
            default:
                throw std::runtime_error("Supplied deserialization option is invalid.");
        }
    } */



    //streamType should support write(const char*, size_t)
    template<typename MsgPackType, typename streamType> 
    void serialize_msgpack(MsgPackType data, streamType & stream){
        msgpack::pack(stream, data);
    }
 
    const std::map<method, std::string> method_strings;

    template<typename MsgPackType>
    struct msgpack_serialization {
    
        msgpack_serialization(MsgPackType& val) : val(val){};
        msgpack_serialization(){};

        atom::entry_type::object<MsgPackType> deserialize(atom::reply_type::flat_response resp, 
                                                        atom::Serialization::method ser_method,
                                                        atom::error & err){
            if(ser_method != atom::Serialization::method::msgpack){
                throw std::runtime_error("Incompatible serialization method supplied!");
            }
            std::string data_str(*resp.first.get(), resp.second);

            try{
                MsgPackType deser_data = deserialize_msgpack(data_str);
                return atom::entry_type::object<MsgPackType>(std::make_shared<MsgPackType>(deser_data), resp.second);
            } catch(msgpack::type_error & e){
                err.set_error_code(atom::error_codes::invalid_command);
                return atom::entry_type::object<MsgPackType>();
            }

        }

        //deserialize msgpack & convert into original msgpack type       
        MsgPackType deserialize_msgpack(std::string& str){
            msgpack::object_handle obj_handle = msgpack::unpack(str.data(), str.size());
            MsgPackType data = obj_handle.get().as<MsgPackType>();
            return data;
        }
        
        //deserialize msgpack & convert into original msgpack type
        template <typename streamType>
        MsgPackType deserialize_msgpack(streamType & data){
            std::string str(data.str());
            return deserialize_msgpack(str);
        }

        private:
            MsgPackType val;
    };

    template<typename NoneType = const char *>
    struct none_serialization {
    
        none_serialization(NoneType& val) : val(val){};
        none_serialization(){};

        atom::entry_type::object<NoneType> deserialize(atom::reply_type::flat_response resp, 
                                                        atom::Serialization::method ser_method,
                                                        atom::error & err){
            if(ser_method != atom::Serialization::method::none){
                throw std::runtime_error("Incompatible serialization method supplied!");
            }
            return atom::entry_type::object<NoneType>(resp.first, resp.second);
        }
    private:
        NoneType val;
    };


    private:
        //members
        Logger logger;
                
    };
}

#endif // __ATOM_CPP_SERIALIZATION_H