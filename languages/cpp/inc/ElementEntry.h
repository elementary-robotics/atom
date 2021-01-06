

#ifndef __ATOM_CPP_ELEMENTRY_H
#define __ATOM_CPP_ELEMENTRY_H

#include "Messages.h"

namespace atom {

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
struct msgpack_entry {
    std::string field;
    std::vector<atom::entry_type::object<MsgPackType> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    msgpack_entry(std::string field, 
        std::vector<atom::entry_type::object<MsgPackType> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    msgpack_entry(std::string field) : field(field){};

};

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType>
struct raw_entry {
    std::string field;
    std::vector<atom::entry_type::object<const char *> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    raw_entry(std::string field, 
        std::vector<atom::entry_type::object<const char *> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    raw_entry(std::string field) : field(field){};

};

///An entry is a portion of a reply from Redis. It encompasses a Redis ID and 
///@param field the ID of the reply
///@param data a pointer to the beginning of the data belonging to this entry.
template<typename BufferType, typename ArrowType = void> //TODO update that void when you actually employ arrow
struct arrow_entry {
    std::string field;
    std::vector<atom::entry_type::object<ArrowType> > data;
    std::shared_ptr<atom::redis_reply<BufferType>> base_reply;

    arrow_entry(std::string field, 
        std::vector<atom::entry_type::object<ArrowType> >& data,
        atom::redis_reply<BufferType> reply) : field(field),
                                                data(data),
                                                base_reply(std::make_shared<atom::redis_reply<BufferType>>(reply))
                                                {};
    arrow_entry(std::string field) : field(field){};

};

///Holds the different forms of entry_types that we could expect to see.
//TODO CHANGE THE ORDER OF TEMPLATE PARAMS
template<typename BufferType, typename MsgPackType = msgpack::type::variant>
class entry {
public:
    boost::variant<msgpack_entry<BufferType, MsgPackType>,raw_entry<BufferType>, arrow_entry<BufferType>> base_entry;

    entry() : base_entry(msgpack_entry<BufferType,MsgPackType>("empty")){};
    entry(msgpack_entry<BufferType, MsgPackType> base_entry) : base_entry(base_entry){};
    entry(raw_entry<BufferType> base_entry) : base_entry(base_entry){};
    entry(arrow_entry<BufferType> base_entry) : base_entry(base_entry){};


    msgpack_entry<BufferType, MsgPackType> get_msgpack(){
        return boost::get<msgpack_entry<BufferType, MsgPackType>>(base_entry);
    }

    std::vector<atom::entry_type::object<MsgPackType> > get_msgpack_data(){
        msgpack_entry<BufferType, MsgPackType> msgpack_e = get_msgpack();
        return msgpack_e.data;
    }
    
    raw_entry<BufferType> get_raw(){
        return boost::get<raw_entry<BufferType>>(base_entry);
    }

    arrow_entry<BufferType> get_arrow(){
        return boost::get<arrow_entry<BufferType>>(base_entry); //TODO implement and test
    }

    bool is_empty(){
        return base_entry.empty();
    }

};

}
#endif //__ATOM_CPP_ELEMENTRY_H