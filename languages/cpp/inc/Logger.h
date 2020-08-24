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
            ///Recognized log levels,
            ///used to determine the extent of logging.
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

            ///Constructor for Logger
            ///@param out stream to which to publish log messages to
            ///@param name name of the logger
            Logger(std::ostream * out, std::string name);
            virtual ~Logger(){};

            ///Emergency Level logger. A message will be logged if the log level set to EMERG or more.
            ///@param message text to log at EMERG level
            void emergency(std::string message);

            ///Alert Level logger. A message will be logged if the log level set to ALERT or more.
            ///@param message text to log at ALERT level
            void alert(std::string message);

            ///Criical Level logger. A message will be logged if the log level set to CRIT or more.
            ///@param message text to log at CRIT level
            void critical(std::string message);

            ///Error Level logger. A message will be logged if the log level set to ERR or more.
            ///@param message text to log at ERR level
            void error(std::string message);

            ///Warning Level logger. A message will be logged if the log level set to WARNING or more.
            ///@param message text to log at WARNING level
            void warning(std::string message);

            ///Notice Level logger. A message will be logged if the log level set to NOTICE or more.
            ///@param message text to log at NOTICE level
            void notice(std::string message);

            ///Info Level logger. A message will be logged if the log level set to INFO or more.
            ///@param message text to log at INFO level
            void info(std::string message);

            ///Debug Level logger. A message will be logged if the log level set to DEBUG.
            ///@param message text to log at DEBUG level
            void debug(std::string message);

            ///Set level of logger
            ///@param level level to which messages will be logged at
            void set_level(std::string level);

            ///View level of log
            ///@return current log level
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