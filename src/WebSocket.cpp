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

#include "WebSocket.h"

#include <cstring>
#include <iostream>
#include <sys/socket.h>


namespace lb
{


namespace httpd
{


WebSocket::WebSocket( ws::ConnectionID connectionID
                    , size_t maxBytesToReceive
                    , std::string urlPath
                    , MHD_socket socket
                    , MHD_UpgradeResponseHandle* upgradeResponseHandle
                    , std::function< void(ws::ConnectionID) > closeCallback )
  : connectionID{ connectionID }
  , maxBytesToReceive{ maxBytesToReceive }
  , urlPath{ std::move( urlPath ) }
  , socket{ socket }
  , upgradeResponseHandle{ upgradeResponseHandle }
  , closeCallback{ std::move( closeCallback ) }
{
}

WebSocket::~WebSocket()
{
  closeSocket();
}

void WebSocket::closeSocket()
{
  if ( upgradeResponseHandle )
  {
    MHD_upgrade_action( upgradeResponseHandle, MHD_UPGRADE_ACTION_CLOSE );

    // No further need for this now and it is a simple way of ensuring we won't
    // try to close the socket again.
    upgradeResponseHandle = nullptr;

    closeCallback( connectionID );
  }
}

bool WebSocket::receive()
{
  // Socket ready for reading, might not get all the data in this read though.
  // The next poll() will get it (and get it immediately since the poll timeout
  // is just that, a timeout, not a fixed delay).
  char buffer[ maxBytesToReceive ];
  auto numBytesReceived{ recv( socket, buffer, maxBytesToReceive, 0 ) };
  if ( numBytesReceived < 0 )
  {
    std::cerr << "Error reading from socket " << socket << " for ID " << connectionID
              << " , errno: " << errno
              << " (" << strerror( errno ) << " )" << std::endl;
    return true;
  }
  else if ( numBytesReceived == 0 )
  {
    // Connection closed
    return true;
  }

  // numBytesReceived > 0
  //std::cout << "Received " << numBytesReceived << " bytes." << std::endl;

  return parseFrame( buffer, numBytesReceived );
}

bool WebSocket::parseFrame( char* p, size_t numBytes )
{
  encoding::websocket::Decoder::Result parseResult{ frameParser.decode( p, numBytes ) };

  for ( auto& frame : parseResult.frames )
  {
    if ( !frame.header.isMasked )
    {
      // RFC 6455 Section 5.1 states
      //
      // "The server MUST close the connection upon receiving a frame that is
      //  not masked.  In this case, a server MAY send a Close frame with a
      // status code of 1002 (protocol error) as defined in Section 7.4.1"
      closeConnection( encoding::websocket::CloseStatusCode::eProtocolError );
      return false;
    }

    switch ( frame.header.opCode )
    {
    case encoding::websocket::Header::OpCode::eText: // Fall through
    case encoding::websocket::Header::OpCode::eBinary: // Fall through
    case encoding::websocket::Header::OpCode::eContinuation:
      receivers.receiveData( connectionID, std::move( frame.payload ) );
      break;
    case encoding::websocket::Header::OpCode::eConnectionClose:
    {
      // Parrot back the payload as per the RFC. Note we can't pass frame.header
      // here as this will have the masking bit set.
      encoding::websocket::Header header;
      header.opCode = encoding::websocket::Header::OpCode::eConnectionClose;
      header.payloadSize = frame.payload.size();
      sendFrame( header, frame.payload.c_str() );
      closeSocket();
      return false;
    }
    case encoding::websocket::Header::OpCode::ePing:
    {
      // Parrot back the payload as per the RFC. Note we can't pass frame.header
      // here as this will have the masking bit set.
      encoding::websocket::Header header;
      header.opCode = encoding::websocket::Header::OpCode::ePong;
      header.payloadSize = frame.payload.size();
      sendFrame( header, frame.payload.c_str() );
      break;
    }
    case encoding::websocket::Header::OpCode::ePong:
      std::cout << "Pong received: " << frame.payload << std::endl;
      break;
    }
  }

  return true;
}

ws::SendResult
WebSocket::sendMessage( std::string payload, size_t maxFrameSize )
{
  const auto encodedHeaderSize
  {
    encoding::websocket::Header::encodedSizeInBytes( payload.size(), false )
  };
  if ( ( maxFrameSize != 0 ) && ( maxFrameSize <= encodedHeaderSize ) )
  {
    std::cerr << "Max frame size is too low" << std::endl;
    return ws::SendResult::eFailure;
  }

  size_t numPayloadBytesRemaining{ payload.size() };

  // Note that the server never masks the payload, only the client does.
  encoding::websocket::Header header;
  header.opCode = encoding::websocket::Header::OpCode::eText;

  bool sentFirstFrame{ false };
  const char* p{ payload.c_str() };
  if ( maxFrameSize > 0 )
  {
    const auto encodedHeaderSize
    {
      encoding::websocket::Header::encodedSizeInBytes( numPayloadBytesRemaining, false )
    };
    while ( numPayloadBytesRemaining + encodedHeaderSize > maxFrameSize )
    {
      // First or Continuation (if any)
      if ( sentFirstFrame )
      {
        header.opCode = encoding::websocket::Header::OpCode::eContinuation;
      }
      header.payloadSize = maxFrameSize - encodedHeaderSize;
      //std::cout << "Sending frame..." << std::endl;
      sendFrame( header, p );
      sentFirstFrame = true;
      numPayloadBytesRemaining -= header.payloadSize;
      p += header.payloadSize;
    }
  }

  // Final (maybe only) frame i.e. "fin"
  //std::cout << "Sending final frame..." << std::endl;
  header.fin = true;
  header.payloadSize = numPayloadBytesRemaining;
  sendFrame( header, p );

  return ws::SendResult::eSuccess;
}

ws::SendResult WebSocket::sendClose( encoding::websocket::CloseStatusCode code
                                   , std::string reason )
{
  encoding::websocket::Header header;
  header.opCode = encoding::websocket::Header::OpCode::eConnectionClose;

  std::string payload( "\0\0", 2 );
  encoding::websocket::encodePayloadCloseStatusCode( code, payload );
  payload += reason;

  header.payloadSize = payload.size();

  sendFrame( header, payload.c_str() );

  return ws::SendResult::eSuccess;
}

ws::SendResult WebSocket::sendPing( std::string payload )
{
  encoding::websocket::Header header;
  header.opCode = encoding::websocket::Header::OpCode::ePing;
  header.payloadSize = payload.size();

  sendFrame( header, payload.c_str() );

  return ws::SendResult::eFailure;
}

ws::SendResult WebSocket::sendPong( std::string payload )
{
  encoding::websocket::Header header;
  header.opCode = encoding::websocket::Header::OpCode::ePong;
  header.payloadSize = payload.size();

  sendFrame( header, payload.c_str() );

  return ws::SendResult::eFailure;
}

ws::SendResult
WebSocket::sendFrame( const encoding::websocket::Header& header
                    , const char* framePayload )
{
  const auto encodedHeaderSize{ header.encodedSizeInBytes() };
  const size_t numBytesToSend{ encodedHeaderSize + header.payloadSize };
  std::unique_ptr<char[]> sendBuffer
  {
    std::make_unique<char[]>( numBytesToSend )
  };

  char* p = sendBuffer.get();

  header.encode( p );

  p += encodedHeaderSize;
  memcpy( p, framePayload, header.payloadSize );

  const int flags{ 0 };
  size_t numBytesRemaningToBeSent{ numBytesToSend };
  while ( numBytesRemaningToBeSent > 0 )
  {
    const auto numSentBytes{ ::send( socket, sendBuffer.get(), numBytesToSend, flags ) };
    if ( numSentBytes < 0 )
    {
      // MHS socket always blocks so this ought to be redundant
      if ( errno == EAGAIN || errno == EWOULDBLOCK )
      {
        continue;
      }

      std::cerr << "Failed to send " << numBytesToSend << " of WebSocket data!" << std::endl;
      return ws::SendResult::eFailure;
    }
    else if ( numSentBytes < numBytesToSend )
    {
      std::cerr << "Did not send full frame!" << std::endl;
    }
    numBytesRemaningToBeSent -= numSentBytes;
  }

  return ws::SendResult::eSuccess;
}

void WebSocket::closeConnection( encoding::websocket::CloseStatusCode statusCode
                               , const std::string& reason )
{
  // RFC6455 Section 5.5.1 states
  //
  // "The Close frame MAY contain a body (the "Application data" portion of
  //  the frame) that indicates a reason for closing, such as an endpoint
  //  shutting down, an endpoint having received a frame too large, or an
  //  endpoint having received a frame that does not conform to the format
  //  expected by the endpoint.  If there is a body, the first two bytes of
  //  the body MUST be a 2-byte unsigned integer (in network byte order)
  //  representing a status code with value /code/ defined in Section 7.4.
  //  Following the 2-byte integer, the body MAY contain UTF-8-encoded data
  //  with value /reason/, the interpretation of which is not defined by
  //  this specification.  This data is not necessarily human readable but
  //  may be useful for debugging or passing information relevant to the
  //  script that opened the connection.  As the data is not guaranteed to
  //  be human readable, clients MUST NOT show it to end users."

  // Also note that control frames such as this are never fragmented.
  encoding::websocket::Header header;
  header.opCode = encoding::websocket::Header::OpCode::eConnectionClose;

  std::string payload( "\x00\x00", 2 );
  encoding::websocket::encodePayloadCloseStatusCode( statusCode, payload );
  payload.append( reason );
  header.payloadSize = payload.size();

  sendFrame( header, payload.c_str() );

  closeSocket();
}


} // End of namespace httpd


} // End of namespace lb
