#include <iostream>

#include <boost/system/error_code.hpp>


namespace atom{
    class error{
        public:
            error(){
                error_code = atom::error::codes::no_error;
                msg_ = "Success";
            };

            virtual ~error(){};

            const std::string message(){
                return msg_;
            };
            
            void set_message(std::string msg){
                msg_ = msg;
            }
            
            void set_error_code(int code){
                error_code = code;
            }
            
            enum codes {
                no_error,
                internal_error,
                redis_error,
                no_response,
                invalid_command,
                unsupported_command,
                callback_failed
            };
        private:
            //members
            int error_code;
            std::string msg_;
    };
}