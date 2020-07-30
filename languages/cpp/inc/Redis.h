////////////////////////////////////////////////////////////////////////////////
//
//  @file Redis.h
//
//  @brief Header for the Redis class implementation
//
//  @copy 2018 Elementary Robotics. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ATOM_CPP_REDIS_H
#define __ATOM_CPP_REDIS_H

#include <iostream>
#include <chrono>
#include <string>
#include <vector>

#include <bredis.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>

#include <Error.h>

namespace atom {

template<typename socket, typename endpoint, typename buffer, typename iterator, typename policy> 
class Redis {

    public:
        //constructor for tcp socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string ip_address, 
                int port) : 
            iocon(iocon),
            ep(boost::asio::ip::address::from_string(ip_address), 
            boost::lexical_cast<std::uint16_t>(port)), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr) {};

        //constructor for unix socket connections
        Redis(boost::asio::io_context & iocon, 
                std::string unix_addr): 
            iocon(iocon),
            ep(unix_addr), 
            sock(std::make_shared<socket>(iocon)), 
            bredis_con(nullptr) {};

        //decon
        virtual ~Redis(){
            if(sock->is_open()){
                sock->close();
            }
        }

        //start async operations
        void start(){
                sock->async_connect(ep, 
                        std::bind(&atom::Redis<socket, endpoint, buffer, iterator, policy>::on_connect, 
                                this, std::placeholders::_1));
        }
        
        //stop async operations
        void stop(){
            std::cout<< "Closing Socket" <<std::endl;
            sock->close();
        }

        // sync connect
        void connect(atom::error err){
            boost::system::error_code ec;
            sock->connect(ep, ec);
            if(ec){
                err.set_error_code(atom::error::codes::internal_error);
                err.set_message(ec.message());
            } else {
                wrap_socket();
            }
        }
        
    protected:
        //wrap socket as bredis connection - must be called after successful connection to redis
        virtual void wrap_socket(){
                bredis_con = std::make_shared<bredis::Connection<socket>>(std::move(*sock));
        }

    private:
        //connection callback
        void on_connect(const boost::system::error_code& err){
            if(err){
                std::cout<<"Connection was unsuccessful: "<<  err.message() << std::endl;   
                stop();        
            } else{
                std::cout<<"Connection to Redis was successful: " << err.message() <<std::endl;
                if(!sock->is_open()){
                    std::cout<<"Socket closed... Connection timed out." <<std::endl;
                } else{
                    wrap_socket();
                    //do_async_xadd();
                }
            }
        }

        //do error detection for redis error messages
        atom::error redis_check(bredis::positive_parse_result_t<iterator, policy> result_markers){
            error_extractor err_extractor;
            atom::error err;
            auto redis_error = boost::apply_visitor(err_extractor, result_markers.result);
            if(!redis_error.empty()){
                err.set_error_code(atom::error::redis_error);
                err.set_message(redis_error);
            }
            return err;
        }


        //members
        boost::asio::io_context& iocon;
        endpoint ep;
        std::shared_ptr<socket> sock;
        std::shared_ptr<bredis::Connection<socket>> bredis_con;
        buffer xadd_tx_buff;
        buffer xadd_rx_buff;

        //extractor for redis errors
        struct error_extractor : public boost::static_visitor<std::string> {
            template <typename T> std::string operator() (const T & /*value*/) const {return "";}

            std::string operator()(const bredis::markers::error_t<iterator> &value) const {     
                std::string error;
                auto str = value.string;
                std::string copy{str.from, str.to};
                error += copy;
                return error;
            }
        };
};

}

#endif // __ATOM_CPP_REDIS_H
