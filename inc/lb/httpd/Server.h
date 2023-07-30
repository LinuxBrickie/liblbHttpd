#ifndef LIB_LB_HTTPD_SERVER_H
#define LIB_LB_HTTPD_SERVER_H

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

#include <lb/httpd/ws/Handler.h>

#include <microhttpd.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>


namespace lb
{


namespace httpd
{


/** \brief A web server C++ wrapper round the C library libmicrohttpd.

    Can be started in either HTTP mode or HTTPS mode. If you need both then you
    need two separate instances but that makes sense since the two protocols
    should be on separate ports.

    Runs on its own internal thread. There is _no_ thread per request.
 */
class Server
{
public:
  struct Config
  {
    /** \brief The port on which the Server will listen for incoming connections.

         As usual the valid port numbers are 1 through 65535.
     */
    int port{ -1 };

    /** \brief The maximum number of bytes to read from the port's socket at a time.

        Ultimately this is what is passed to recv(2).
     */
    size_t maxSocketBytesToReceive{ 1024 };
  };

  enum class Method
  {
    eInvalid,
    eGet,
    eHead,
    ePost,
    ePut,
    eDelete
  };

  struct Version
  {
    int major;
    int minor;
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

  /**
      \brief Constructor for plain HTTP only. Starts the server.
      \param config Server configuration, including the port number to listen on.
      \param rh A callback std::function for handling each URL request.
      \param wsh An optional std::function for handling WebSocket requests.
      \throw std::runtime_error if the server could not be started or if no
             request handler was specified.

      The request handler is not optional. All regular URL requests will be
      passed to this callback and the data in the \a Response that it returns
      will be what is returned to the client.

      The ws::Handler is optional. If it is left invalid then any URL requests
      to the ws: protocol will be ignored (wss: is handled by the HTTPS
      constructor). If you wish to handle WebSocket connections then pass in a
      valid object and see the documentation for \a ws::Handler for details on
      what it should do.
   */
  Server( Config config
        , RequestHandler rh
        , std::optional<ws::Handler> wsh = {} );

  /**
      \brief Constructor for HTTPS only. Starts the server.
      \param config Server configuration, including the port number to listen on.
      \param httpsCert The contents of the server's HTTPS certificate.
      \param httpsPrivateKey The contents of the server's private key.
      \param rh A callback std::function for handling each URL request.
      \param wsh An optional std::function for handling WebSocket requests.
      \throw std::runtime_error if the server could not be started or if no
             request handler was specified.

      The request handler is not optional. All regular URL requests will be
      passed to this callback and the data in the \a Response that it returns
      will be what is returned to the client.

      The ws::Handler is optional. If it is left invalid then any URL requests
      to the wss: protocol will be ignored (ws: is handled by the HTTP
      constructor). If you wish to handle WebSocket connections then pass in a
      valid object and see the documentation for \a ws::Handler for details on
      what it should do.
   */
  Server( Config config
        , std::string httpsCert
        , std::string httpsPrivateKey
        , RequestHandler rh
        , std::optional<ws::Handler> wsh = {} );

  /** \brief Destructor. Stops the server. */
  ~Server();

  Server( Server&& );

private:
  struct Private;
  std::unique_ptr<Private> d;
};


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_SERVER_H
