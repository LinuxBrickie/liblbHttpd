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
#include <memory>
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

  using Headers       = std::unordered_map< std::string, std::string >;
  using PostKeyValues = std::unordered_map< std::string, std::string >;

  struct Response
  {
    unsigned int code;
    std::string content;
  };

  using RequestHandler = std::function< Response( std::string, // url
                                                  Method,
                                                  Version,
                                                  Headers,
                                                  std::string,
                                                  PostKeyValues ) >; // request payload

  /** \brief Constructor for plain HTTP only. Starts the server. */
  Server( int port, RequestHandler );

  /** \brief Constructor for HTTPS only. Starts the server. */
  Server( int port, std::string httpsCert, std::string httpsPrivateKey, RequestHandler );

  /** \brief Destructor. Stops the server. */
  ~Server();

private:
  struct Private;
  std::unique_ptr<Private> d;
};


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_SERVER_H
