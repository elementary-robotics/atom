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
        arrow, ///< arrow (not supported yet)
        not_found
    };

    Serialization() : method_strings({
            {method::none, "none"},
            {method::msgpack, "msgpack"},
            {method::arrow, "arrow"},
            {method::not_found, "not found"}
        }), logger(&std::cout, "Serializer"){}
    Serialization(std::ostream& logstream, std::string log_name) : method_strings({
            {method::none, "none"},
            {method::msgpack, "msgpack"},
            {method::arrow, "arrow"},
            {method::not_found, "not found"}
        }), logger(&logstream, log_name){}

    virtual ~Serialization(){}

    template<typename BufferType, typename MsgPackType>
    atom::serialized_entry<BufferType> serialize(atom::entry<BufferType, MsgPackType>& entry_in, atom::Serialization::method ser_method, atom::error& err){
        std::string ID;
        std::vector<std::string> ser_data;
        switch(ser_method){
            case atom::Serialization::method::msgpack:
            {   
                auto msgpack_type_entry = entry_in.get_msgpack();
                ID = msgpack_type_entry.field;
                ser_data = serialize_object(msgpack_type_entry.data, ser_method, err);
                break;
            }
            case atom::Serialization::method::none:
            {   
                auto raw_type_entry = entry_in.get_raw();
                ID = raw_type_entry.field;
                ser_data = serialize_object(raw_type_entry.data, ser_method, err);
                break;
            }
            case atom::Serialization::method::arrow:
            {   
                logger.alert("Arrow serialization not supported!");
                err.set_error_code(atom::error_codes::unsupported_command);
                return atom::serialized_entry<BufferType>();
            }
        }
        return atom::serialized_entry<BufferType>(ID, ser_data);

    }


    template<typename DataType>
    std::vector<std::string> serialize_object(std::vector<atom::entry_type::object<DataType> >& entry_data, atom::Serialization::method ser_method, atom::error& err){
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
                        serialize_msgpack(*boost::get<atom::entry_type::deser_data<DataType>>(m.pair.first).get(), serialized_data);
                        processed_data.push_back(serialized_data.str());
                        logger.debug("serialized object, msgpack value: " + serialized_data.str());

                    } else{
                        std::string key = *boost::get<atom::entry_type::str_data>(m.pair.first).get();
                        processed_data.push_back(key);
                        logger.debug("serialized object, msgpack key: " + key);
                    }
                    is_value = !is_value;
                }

                return processed_data;
            }
            case atom::Serialization::method::none:
            {   
                std::vector<std::string> processed_data;
                try {
                std::for_each(entry_data.cbegin(), entry_data.cend(), [&](const atom::entry_type::object<DataType> & elem) {
                                processed_data.push_back(*boost::get<atom::entry_type::str_data>(elem.pair.first).get()); });
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
                        logger.debug("serialized msgpack value: " + serialized_data.str());
                        processed_data.push_back(serialized_data.str());
                    } else{
                        std::string key = boost::get<std::string>(m);
                        logger.debug("serialized msgpack key: " + key);
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

    template<typename BufferType, typename MsgPackType>
    void deserialize(std::vector<atom::entry<BufferType, MsgPackType>>& entries, atom::Serialization::method serialization, atom::reply_type::entry_response_list & data, atom::error & err){
        //Be aware that we can keep using the vector as-is only because we read from one stream at a time in our elements.
        for(auto & entry_response : data){
            deserialize(entries, serialization, entry_response.second, err);
        }
    }

    template<typename BufferType, typename MsgPackType>
    void deserialize(std::vector<atom::entry<BufferType, MsgPackType>>& entries, atom::Serialization::method serialization, atom::reply_type::entry_response & data, atom::error & err){
        for(auto & entry_map: data){
            deserialize(entries, serialization, entry_map, err);
        }
    }
    
    template<typename BufferType, typename MsgPackType>
    void deserialize(std::vector<atom::entry<BufferType, MsgPackType>>& entries, atom::Serialization::method serialization, 
                        std::pair<std::string, std::vector<atom::reply_type::flat_response>> entry_map, atom::error & err){
        
            std::string uid = entry_map.first;

            //find the serialization method
            std::tuple<atom::Serialization::method,int> method = get_serialization_method(entry_map.second);
            atom::Serialization::method ser_method = std::get<0>(method);
            logger.debug("Serialization method found: " + method_strings.at(ser_method) );

            //handle case where there is no serialization method found in the keys
            // if the serialization method is there ignore the serialization arg passed in
            if(ser_method == atom::Serialization::method::not_found){
                ser_method = serialization;
            }
            
            int position = std::get<1>(method);
            bool is_val = false;

            switch(ser_method){
                case atom::Serialization::msgpack:
                {   
                    atom::Serialization::msgpack_serialization<MsgPackType> obj;
                    atom::msgpack_entry<BufferType, MsgPackType> single_entry(uid);

                    for(size_t i = position; i < entry_map.second.size(); i++){
                        if(is_val){  
                            atom::entry_type::object<MsgPackType> deser = obj.deserialize(entry_map.second[i], ser_method, err);
                            single_entry.data.push_back(deser);
                        } else{
                            auto key = std::make_shared<std::string>(atom::reply_type::to_string(entry_map.second[i]));
                            single_entry.data.push_back(atom::entry_type::object<MsgPackType>(key, entry_map.second[i].second));
                        }
                        is_val = !is_val;
                    }
                    entries.push_back(single_entry);
                    break;
                }
                case atom::Serialization::none:
                {
                    atom::Serialization::none_serialization<const char *> obj;
                    atom::raw_entry<BufferType> single_entry(uid);

                    for(size_t i = position; i < entry_map.second.size(); i++){
                        if(is_val){  
                            atom::entry_type::object<const char *> deser = obj.deserialize(entry_map.second[i], ser_method, err);
                            single_entry.data.push_back(deser);
                        } else{
                            auto key = std::make_shared<std::string>(atom::reply_type::to_string(entry_map.second[i]));
                            single_entry.data.push_back(atom::entry_type::object<const char *>(key, entry_map.second[i].second));
                        }
                        is_val = !is_val;
                    }
                    entries.push_back(single_entry);
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
        
    
    }


    std::tuple<atom::Serialization::method, int> get_serialization_method(std::vector<atom::reply_type::flat_response>& entry_data){
        int counter = 0;
        for(auto & rediskey_val: entry_data){
            if(rediskey_val.second == 3){
                std::string candidate = std::string(*rediskey_val.first.get(), rediskey_val.second);
                if(candidate == "ser"){
                    counter++;
                    std::string method = std::string(*entry_data[counter].first.get(), entry_data[counter].second);
                    counter++;
                    atom::Serialization::method found_method = get_method(method);
                    return std::tuple<atom::Serialization::method, int>(found_method, counter);
                    throw std::runtime_error(method + " serialization not supported.");
                }
            }
            counter++;
        }
        return std::tuple<atom::Serialization::method, int>(atom::Serialization::method::not_found, 0);
    }

    atom::Serialization::method get_method(std::string method){
        if(method == method_strings.at(atom::Serialization::method::none)){
            return atom::Serialization::method::none;
        }
        if(method == method_strings.at(atom::Serialization::method::msgpack)){
            return atom::Serialization::method::msgpack;
        }
        if(method == method_strings.at(atom::Serialization::method::arrow)){
            return atom::Serialization::method::arrow;
        }
    }

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