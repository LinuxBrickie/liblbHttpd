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

    This is provided to you *by* the \a Handler in the \a Connection object
    which is passed to your \a ConnectionEstablished callback.
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

  /**
      \brief Send text data to the WebSocket. Binary not yet supported.
      \param message The WebSocket frame payload.
      \param maxFrameSize Maximum frame size. Zero implies unlimited.

      If a frame's size exceeds \a maxFrameSize then the server will split the
      frame up into multiple frames and send a fragmented message.
   */
  SendResult sendData( std::string message, size_t maxFrameSize );

  /**
      \brief Send a close control frame with close code and optional reason.
      \note If the client sends a close control frame then the server will
            automatically respond with a matching close frame. This method is
            intended for when the server wants to intiate the close.
   */
  SendResult sendClose( encoding::websocket::closestatus::PayloadCode, std::string reason = {} );

  /** \brief Send a ping control frame. */
  SendResult sendPing( std::string payload ) const;

  /**
      \brief Send a pong control frame.
      \note The server automatically sends a pong frame in response to a ping
            so generally this should not be needed.
   */
  SendResult sendPong( std::string payload ) const;

  struct Impl; //!< Opaque implementation detail.

private:
  std::shared_ptr<Impl> d;
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_SENDERS_H
