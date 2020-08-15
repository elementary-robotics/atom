////////////////////////////////////////////////////////////////////////////////
//
//  @file Logger.h
//
//  @brief Logging capabilities for atom
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////



#ifndef __ATOM_CPP_LOGGER_H
#define __ATOM_CPP_LOGGER_H

#include <iostream>
#include <map>

namespace atom {

    class Logger {
        public:
            //recognized log levels
            enum level {
                EMERG,
                ALERT,
                CRIT,
                ERR,
                WARNING,
                NOTICE,
                INFO,
                DEBUG
            };

            Logger(std::ostream * out, std::string name);
            virtual ~Logger(){};

            //level loggers
            void emergency(std::string message);
            void alert(std::string message);
            void critical(std::string message);
            void error(std::string message);
            void warning(std::string message);
            void notice(std::string message);
            void info(std::string message);
            void debug(std::string message);

            //set level of log
            void set_level(std::string level);

            //view level of log
            level get_level();
        private:
            //initialize the underlying log_level map
            void initialize_map();

            //members
            std::string name;
            std::ostream * out;
            level log_level;
            std::map<std::string, atom::Logger::level> level_map;
    };
}


#endif // __ATOM_CPP_LOGGER_H