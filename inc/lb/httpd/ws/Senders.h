#ifndef LIB_LB_HTTPD_WS_SENDERS_H
#define LIB_LB_HTTPD_WS_SENDERS_H

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

#include <lb/httpd/ws/SendResult.h>

#include <lb/encoding/websocket.h>

#include <memory>
#include <string>


namespace lb
{


namespace httpd
{


namespace ws
{


/** \brief The means of writing to the WebSocket.

    This is provided *by* the \a Handler in the \a Connection object which is
    passed into the \a ConnectionEstablished callback.
 */
class Senders
{
public:
  /** \brief Create an invalid object. All send methods will immediately return eNoImplementation. */
  Senders() = default;
  Senders( Senders&& ) = default;
  Senders& operator=( Senders&& ) = default;
  Senders( const Senders& ) = default;
  Senders& operator=( const Senders& ) = default;

  SendResult sendData( std::string message, size_t maxFrameSize );
  SendResult sendClose( encoding::websocket::CloseStatusCode, std::string reason = {} );
  SendResult sendPing( std::string payload ) const;
  SendResult sendPong( std::string payload ) const;

  struct Impl; //!< Opaque implementation detail.

private:
  std::shared_ptr<Impl> d;
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_SENDERS_H
