#ifndef LIB_LB_HTTPD_WEBSOCKET_H
#define LIB_LB_HTTPD_WEBSOCKET_H

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

#include <functional>
#include <mutex>
#include <optional>

#include <lb/httpd/Server.h>

#include <lb/encoding/websocket.h>


namespace lb
{


namespace httpd
{


/** \brief Handles a valid, connected WebSocket, allowing two-way communication.

    If \a Server is configured to accept WebSockets (via its constructor) then a
    \a WebSocket instance will be created by \a Server when it receives a valid
    ws or wss protocol request and successfully upgrades the HTTP connection to
    a WebSocket connection.

    The send methods on this class are the ones used to implement the sending of
    data and control messages in \a Senders.

    It stores a copy of the \a Receivers object returned from the connection
    established callback. This is set directly by \a Server.
 */
struct WebSocket
{
  using CloseCallback = std::function< void(ws::ConnectionID) >;

  /** \brief Creates a manager for a single, established WebSocket connection.
      \param connectinoID The ID assigned by \a Serrver to this connection
      \param urlPath The URL path of the original request
      \param socket The MHD socket of the establisehd connection through which
                    we can \a send and \a recv.
      \param urh The MHS upgrade response handler that we need to close the
                 connection.
   */
  WebSocket( ws::ConnectionID connectionID
           , size_t maxBytesToReceive
           , std::string urlPath
           , MHD_socket socket
           , MHD_UpgradeResponseHandle* urh
           , CloseCallback closeCallback = {} );
  ~WebSocket();

  void closeSocket();

  bool receive();

  bool parseFrame( char* buffer, size_t numBytesReceived );

  std::optional<encoding::websocket::Header> parseHeader( const char* buffer
                                                        , size_t numBufferBytes );

  /** \brief Send a complete message through the WebSocket.

      A message may be split into multiple frames if a send limit has been set
      and the message size (including header) would exceed it.
   */
  ws::SendResult sendMessage( std::string, size_t maxFrameSize );

  ws::SendResult sendClose( encoding::websocket::closestatus::PayloadCode, std::string );

  ws::SendResult sendPing( std::string payload );
  ws::SendResult sendPong( std::string payload );

  ws::SendResult sendFrame( const encoding::websocket::Header& header
                          , const char* remainingData );

  /** \brief Send a close control frame to the client and close our socket.

      From RFC 6455:

      "As such, when a server is instructed to _Close the WebSocket Connection_
      it SHOULD initiate a TCP Close immediately, and when a client is instructed
      to do the same, it SHOULD wait for a TCP Close from the server.
   */
  void closeConnection( encoding::websocket::closestatus::ProtocolCode statusCode
                      , const std::string& reason = {} );


  std::mutex mutex;

  const ws::ConnectionID connectionID;
  const size_t maxBytesToReceive;

  const std::string urlPath;
  MHD_socket socket;
  MHD_UpgradeResponseHandle* upgradeResponseHandle;

  CloseCallback closeCallback;

  ws::Receivers receivers; //!< Provided via Handler::connectionEstablished

  encoding::websocket::Decoder frameParser;

  struct Fragmented
  {
    ws::Receivers::DataOpCode dataOpCode;
    std::string payload;
  };
  std::optional<Fragmented> fragmented;
};

} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WEBSOCKET_H
