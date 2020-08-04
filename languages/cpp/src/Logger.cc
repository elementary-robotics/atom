////////////////////////////////////////////////////////////////////////////////
//
//  @file Logger.cc
//
//  @brief Logger implementation
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"

#include <cstdlib>
#include <boost/date_time.hpp>

atom::logger::logger(std::ostream * out, std::string name): name(name), out(out){
    std::string default_log_level = std::getenv("DEFAULT_LOG_LEVEL");
    initialize_map();
    set_level(default_log_level);
}

void atom::logger::emergency(std::string message){
    if(log_level >= atom::logger::level::EMERG){
        *out << "[ EMERGENCY ] [ " << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::alert(std::string message){
    if(log_level >= atom::logger::level::ALERT){
        *out << "[ ALERT ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::critical(std::string message){
    if(log_level >= atom::logger::level::CRIT){
        *out << "[ CRITICAL ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}
void atom::logger::error(std::string message){
    if(log_level >= atom::logger::level::ERR){
        *out << "[ ERROR ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::warning(std::string message){
    if(log_level >= atom::logger::level::WARNING){
        *out << "[ WARNING ] [ " << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::notice(std::string message){
    if(log_level >= atom::logger::level::NOTICE){
        *out << "[ NOTICE ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::info(std::string message){
    if(log_level >= atom::logger::level::INFO){
        *out << "[ INFO ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::debug(std::string message){
    if(log_level >= atom::logger::level::DEBUG){
        *out << "[ DEBUG ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::logger::set_level(std::string level){
    if(level_map.find(level) != level_map.end()){
        log_level = level_map[level];
    } else{
        throw std::runtime_error("Invalid Log Level: " + level + " is not a recognized logging level for atom.");
    }
}

atom::logger::level atom::logger::get_level(){
    return log_level;
}


void atom::logger::initialize_map(){
    level_map["EMERG"] = atom::logger::level::EMERG;
    level_map["ALERT"] = atom::logger::level::ALERT;
    level_map["CRIT"] = atom::logger::level::CRIT;
    level_map["ERR"] = atom::logger::level::ERR;
    level_map["WARNING"] = atom::logger::level::WARNING;
    level_map["NOTICE"] = atom::logger::level::NOTICE;
    level_map["INFO"] = atom::logger::level::INFO;
    level_map["DEBUG"] = atom::logger::level::DEBUG;
}
