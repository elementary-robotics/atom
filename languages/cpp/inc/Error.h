////////////////////////////////////////////////////////////////////////////////
//
//  @file Error.h
//
//  @brief Header-only implementation of classes that support atom error reporting
//
//  @copy 2020 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_ERROR_H
#define __ATOM_CPP_ERROR_H

#include <iostream>
#include <string>

#include <boost/system/error_code.hpp>

//define the error codes specific to atom
namespace atom {
    enum error_codes {
        no_error,
        internal_error,
        redis_error,
        no_response,
        invalid_command,
        unsupported_command,
        callback_failed
    };
} //namespace atom

namespace boost
{
  namespace system
  {
    // Tell the C++ 11 STL metaprogramming that the atom::error_codes enum
    // is registered with the standard error code system
    template <> struct is_error_code_enum<atom::error_codes> : std::true_type
    {
    };
  }  // namespace system
}  // namespace boost

namespace detail
{
  // Define atom's custom error code category
  class atom_error_category : public boost::system::error_category
  {
  public:
    //initialize error category
    atom_error_category() : error_category(){}
    
    //initialize error category with error code id
    atom_error_category(int code) : error_category(code){}

    //initialize error category with error code id and message
    atom_error_category(int code, std::string msg) : error_category(code) {/* redis_error_msg = msg; */}

    // Return a short descriptive name for the category
    virtual const char *name() const noexcept override final { return "atom error"; }

    // Return what each enum means in text
    virtual std::string message(int code) const override final
    {
      switch(static_cast<atom::error_codes>(code))
      {
      case atom::error_codes::no_error:
        return "Success";
      case atom::error_codes::internal_error:
        return "atom has encountered an internal error";
      case atom::error_codes::redis_error:
        return "atom has encountered a redis error"; //TODO: redis_error_msg
      case atom::error_codes::no_response:
        return "atom was unable to get a response";
      case atom::error_codes::invalid_command:
      case atom::error_codes::unsupported_command:
        return "atom does not support this command";
      case atom::error_codes::callback_failed:
        return "atom callback has failed";
      default:
        return "unknown";
      }
    }

    // Allow generic error conditions to be compared to atom errors
    virtual boost::system::error_condition default_error_condition(int code) const noexcept override final
    {
      switch(static_cast<atom::error_codes>(code))
      {
      case atom::error_codes::no_error:
        return  make_error_condition(boost::system::errc::success);
      case atom::error_codes::internal_error:
        return  make_error_condition(boost::system::errc::io_error);
      case atom::error_codes::no_response:
        return  make_error_condition(boost::system::errc::no_message);
      case atom::error_codes::invalid_command:
      case atom::error_codes::unsupported_command:
        return make_error_condition(boost::system::errc::not_supported);
      default:
        // default, no mapping for the error codes:
        // atom::error_codes::redis_error:
        // atom::error_codes::callback_failed:
        return boost::system::error_condition(code, *this);
      }
    }
  };
}  // namespace detail

namespace atom{
    class error : public boost::system::error_code {
        public:
            error() : boost::system::error_code(), msg_("Success"){};

            virtual ~error(){};

            //get the error code
            const int code(){
                return value();
            }

            std::string redis_error(){
                return msg_;
            }
            
            //set the error code
            void set_error_code(int code){
                err_cat = std::make_shared<detail::atom_error_category>(code);
                assign(code, *err_cat);
            }

            //set redis specific error code
            void set_redis_error(std::string msg){
                int code =  atom::error_codes::redis_error;
                err_cat = std::make_shared<detail::atom_error_category>(code);
                assign(code, *err_cat);
                msg_= msg;
            }

        private:
            //members
            std::string msg_;
            std::shared_ptr<detail::atom_error_category> err_cat;
    };
} //namespace atom

#endif // __ATOM_CPP_ERROR_H
