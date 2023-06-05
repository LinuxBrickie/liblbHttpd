/*
    Copyright (C) 2023  Paul Fotheringham (LinuxBrickie)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef LIB_LB_HTTPD_SERVER_H
#define LIB_LB_HTTPD_SERVER_H

#include <microhttpd.h>

#include <functional>
#include <string>
#include <unordered_map>


namespace lb
{


namespace httpd
{


/** \brief A web server C++ wrapper round the C library libmicrohttpd.

    Runs on its own internal thread. There is _no_ thread per request.
 */
class Server
{
public:
  enum class Method
  {
    eInvalid,
    eGet,
    eHead,
    ePost,
    ePut,
    eDelete
  };

  enum class Version
  {
    eUnknown,
    e0_9,
    e1_0,
    e1_1,
    e2_0,
  };

  using PostKeyValues = std::unordered_map< std::string, std::string >;

  struct Response
  {
    unsigned int code;
    std::string content;
  };

  using RequestHandler = std::function< Response( std::string, // url
                                                  Method,
                                                  Version,
                                                  std::string,
                                                  const PostKeyValues& ) >; // request payload

  Server( int port, RequestHandler );
  ~Server();

private:
  static MHD_Result keyValueIterator( void* userData
                                    , enum MHD_ValueKind kind
                                    , const char* key
                                    , const char* value );

  static MHD_Result postDataIterator( void* userData
                                    , MHD_ValueKind kind
                                    , const char* key
                                    , const char* filename
                                    , const char* content_type
                                    , const char* transfer_encoding
                                    , const char* data
                                    , uint64_t off
                                    , size_t size );

  static MHD_Result accessHandlerCallback( void* cls
                                         , MHD_Connection*
                                         , const char* url
                                         , const char* method
                                         , const char* version
                                         , const char* upload_data
                                         , size_t* upload_data_size
                                         , void** connectionContext );

  Response invokeRequestHandler( MHD_Connection*
                               , std::string
                               , Method
                               , Version
                               , std::string );

  MHD_Daemon*const mhd;

  RequestHandler requestHandler;

  PostKeyValues postKeyValues;
};


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_SERVER_H
