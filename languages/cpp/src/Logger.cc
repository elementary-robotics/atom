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

atom::Logger::Logger(std::ostream * out, std::string name): name(name), out(out){
    std::string default_log_level = std::getenv("DEFAULT_LOG_LEVEL");
    initialize_map();
    set_level(default_log_level);
}

void atom::Logger::emergency(std::string message){
    if(log_level >= atom::Logger::level::EMERG){
        *out << "[ EMERGENCY ] [ " << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::alert(std::string message){
    if(log_level >= atom::Logger::level::ALERT){
        *out << "[ ALERT ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::critical(std::string message){
    if(log_level >= atom::Logger::level::CRIT){
        *out << "[ CRITICAL ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}
void atom::Logger::error(std::string message){
    if(log_level >= atom::Logger::level::ERR){
        *out << "[ ERROR ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::warning(std::string message){
    if(log_level >= atom::Logger::level::WARNING){
        *out << "[ WARNING ] [ " << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::notice(std::string message){
    if(log_level >= atom::Logger::level::NOTICE){
        *out << "[ NOTICE ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::info(std::string message){
    if(log_level >= atom::Logger::level::INFO){
        *out << "[ INFO ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::debug(std::string message){
    if(log_level >= atom::Logger::level::DEBUG){
        *out << "[ DEBUG ] [ "  << name <<" ] [ " << 
                boost::posix_time::second_clock::local_time() <<
                " ] " << message <<"\n";
    }
}

void atom::Logger::set_level(std::string level){
    if(level_map.find(level) != level_map.end()){
        log_level = level_map[level];
    } else{
        throw std::runtime_error("Invalid Log Level: " + level + " is not a recognized logging level for atom.");
    }
}

atom::Logger::level atom::Logger::get_level(){
    return log_level;
}


void atom::Logger::initialize_map(){
    level_map["EMERG"] = atom::Logger::level::EMERG;
    level_map["ALERT"] = atom::Logger::level::ALERT;
    level_map["CRIT"] = atom::Logger::level::CRIT;
    level_map["ERR"] = atom::Logger::level::ERR;
    level_map["WARNING"] = atom::Logger::level::WARNING;
    level_map["NOTICE"] = atom::Logger::level::NOTICE;
    level_map["INFO"] = atom::Logger::level::INFO;
    level_map["DEBUG"] = atom::Logger::level::DEBUG;
}
