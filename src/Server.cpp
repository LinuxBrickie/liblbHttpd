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

#include <lb/httpd/Server.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <lb/encoding/base64.h>
#include <lb/encoding/sha1.h>
#include <lb/encoding/websocket.h>

// Not available on my system at time of writing :(
//#include <microhttpd_ws.h>

#include "Poller.h"
#include "WebSocket.h"
#include "ws/SendersImpl.h"


namespace lb
{


namespace httpd
{


// Library-wide counter i.e. shared by all Server instances effectively.
ws::ConnectionID globalConnectionID{ 0 };


struct Server::Private
{
  Private( Config
         , RequestHandler rh
         , std::optional<ws::Handler> wsh );
  Private( Config
         , std::string httpsCert
         , std::string httpsPrivateKey
         , RequestHandler rh
         , std::optional<ws::Handler> wsh );
  ~Private();

  /** \brief Called on construction. Throws a runtime error if there is an issue. */
  static Config sanityCheck( Config );

  MHD_Response* maybeCreateWebSocketResponse( const char* url, Method, Version );

  bool isHeaderSet( const std::string& ) const;
  bool isHeaderSetTo( const std::string&, const std::string& ) const;

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

  static void upgradeHandler( void* userData
                            , MHD_Connection* connection
                            , void* connectionContext
                            , const char* extraData
                            , size_t extraDataSize
                            , MHD_socket socket
                            , MHD_UpgradeResponseHandle* upgradeHandle );

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

  void webSocketLoop();
  void webSocketClosed( ws::ConnectionID );


  Config config;

  MHD_Daemon*const mhd;

  RequestHandler requestHandler;
  std::optional<ws::Handler> webSocketHandler;

  // Should really be in a connection class but since we are explicitly single
  // threaded it does not matter yet.
  Headers       headers;
  PostKeyValues postKeyValues;

  using WebSockets = std::unordered_map< ws::ConnectionID, WebSocket >;
  WebSockets webSockets;

  using ClosedWebSockets = std::unordered_set< ws::ConnectionID >;
  ClosedWebSockets closedWebSockets;

  std::thread webSocketThread;
  std::mutex  webSocketMutex;
  std::atomic<bool> webSocketRunning{ true };

  Poller poller;
};


static
Server::Method parseMethod( const char* methodStr )
{
  if ( strcmp( methodStr, "GET" ) == 0 )
  {
    return Server::Method::eGet;
  }
  else if ( strcmp( methodStr, "HEAD" ) == 0  )
  {
    return Server::Method::eHead;
  }
  else if ( strcmp( methodStr, "POST" ) == 0  )
  {
    return Server::Method::ePost;
  }
  else if ( strcmp( methodStr, "PUT" ) == 0  )
  {
    return Server::Method::ePut;
  }
  else if ( strcmp( methodStr, "DELETE" ) == 0  )
  {
    return Server::Method::eDelete;
  }

  return Server::Method::eInvalid;
}

static
Server::Version parseVersion( const char* versionStr )
{
  if ( strcmp( versionStr, "HTTP/0.9" ) == 0 )
  {
    return { 0, 9 };
  }
  else if ( strcmp( versionStr, "HTTP/1.0" ) == 0 )
  {
    return { 1, 0 };
  }
  else if ( strcmp( versionStr, "HTTP/1.1" ) == 0 )
  {
    return { 1, 1 };
  }
  else if ( strcmp( versionStr, "HTTP/2.0" ) == 0 )
  {
    return { 2, 0 };
  }

  return { -1, -1 };
}


Server::Private::Private( Config config
                        , RequestHandler rh
                        , std::optional<ws::Handler> wsh )
  : config{ sanityCheck( std::move( config ) ) }
  , mhd{ MHD_start_daemon( MHD_USE_INTERNAL_POLLING_THREAD
                         | MHD_USE_ERROR_LOG
                         | MHD_ALLOW_UPGRADE
                         | MHD_ALLOW_SUSPEND_RESUME
                         , config.port
                         , nullptr // accept policy callback not required
                         , nullptr // accept policy callback user data
                         , &accessHandlerCallback
                         , this
                         , MHD_OPTION_END ) }
  , requestHandler{ std::move( rh ) }
  , webSocketHandler{ std::move( wsh ) }
{
  if ( !requestHandler )
  {
    throw std::runtime_error( "No HTTP request handler specified" );
  }

  if ( webSocketHandler )
  {
    webSocketThread = std::move( std::thread{ std::bind( &Private::webSocketLoop
                                                       , this ) } );
  }

  if ( !mhd )
  {
    throw std::runtime_error( "Failed to create HTTP MHD server" );
  }
}

Server::Private::Private( Config config
                        , std::string httpsCert
                        , std::string httpsPrivateKey
                        , RequestHandler rh
                        , std::optional<ws::Handler> wsh )
  : config{ sanityCheck( std::move( config ) ) }
  , mhd{ MHD_start_daemon( MHD_USE_INTERNAL_POLLING_THREAD
                         | MHD_USE_ERROR_LOG
                         | MHD_ALLOW_UPGRADE
                         | MHD_ALLOW_SUSPEND_RESUME
                         | MHD_USE_TLS
                         , config.port
                         , nullptr // accept policy callback not required
                         , nullptr // accept policy callback user data
                         , &accessHandlerCallback
                         , this
                         , MHD_OPTION_HTTPS_MEM_CERT, httpsCert.c_str()
                         , MHD_OPTION_HTTPS_MEM_KEY, httpsPrivateKey.c_str()
                         , MHD_OPTION_END ) }
  , requestHandler{ std::move( rh ) }
  , webSocketHandler{ std::move( wsh ) }
{
  if ( !requestHandler )
  {
    throw std::runtime_error( "No HTTPS request handler specified" );
  }

  if ( webSocketHandler )
  {
    webSocketThread = std::move( std::thread{ std::bind( &Private::webSocketLoop
                                                       , this ) } );
  }

  if ( !mhd )
  {
    throw std::runtime_error( "Failed to create HTTPS MHD server" );
  }
}

Server::Private::~Private()
{
  // Stop polling for data
  webSocketRunning = false;
  if ( webSocketThread.joinable() )
  {
    webSocketThread.join();
  }

  // Close any WebSocket connections that have not been closed by the client.
  for ( auto&[id, ws] : webSockets )
  {
    ws.closeConnection( encoding::websocket::CloseStatusCode::eGoingAway );
  }
  webSockets.clear();

  MHD_stop_daemon( mhd );
}

// static
Server::Config Server::Private::sanityCheck( Config config )
{
  if ( ( config.port < 1 ) || ( config.port > 65535 ) )
  {
    throw std::runtime_error{ "Invalid port number specified. Needs to be in the range 1 to 65535." };
  }

  if ( config.maxSocketBytesToReceive == 0 )
  {
    throw std::runtime_error{ "Invalid maximum socket bytes to receive. Needs to be greater than zero." };
  }

  return config;
}

MHD_Response* Server::Private::maybeCreateWebSocketResponse( const char* url
                                                           , Method method
                                                           , Version version )
{
  if ( !webSocketHandler )
  {
    return nullptr;
  }

  if ( !webSocketHandler->isHandled( url ) )
  {
    return nullptr;
  }

  // Examine headers. Should look something like:
  //
  // Host: localhost:4567
  // Accept: */*
  // Upgrade: websocket
  // Connection: Upgrade
  // Sec-WebSocket-Version: 13
  // Sec-WebSocket-Key: ZHEJMUkToewFjjdufVsStQ==
  if ( method != Method::eGet )
  {
    return nullptr;
  }
  else if ( !isHeaderSet( MHD_HTTP_HEADER_HOST ) )
  {
    return nullptr;
  }
  else if ( !isHeaderSetTo( MHD_HTTP_HEADER_UPGRADE, "websocket" ) )
  {
    return nullptr;
  }
  else if ( !isHeaderSetTo( "Connection", "Upgrade" ) )
  {
    return nullptr;
  }
  else if ( !isHeaderSet( "Sec-WebSocket-Version" ) )
  {
    return nullptr;
  }
  else if ( !isHeaderSet( MHD_HTTP_HEADER_SEC_WEBSOCKET_KEY ) )
  {
    return nullptr;
  }
  else if ( ( version.major  < 1 )
         || ( version.major == 1 ) && ( version.minor < 1 ) )
  {
    return nullptr;
  }

  MHD_Response*const mhdResponse
  {
    MHD_create_response_for_upgrade( &upgradeHandler, this )
  };

  /**
   * For the response we need at least the following headers:
   * 1. "Connection: Upgrade"
   * 2. "Upgrade: websocket"
   * 3. "Sec-WebSocket-Accept: <base64value>"
   * The value for Sec-WebSocket-Accept can be generated with MHD_websocket_create_accept_header.
   * It requires the value of the Sec-WebSocket-Key header of the request.
   * See also: https://tools.ietf.org/html/rfc6455#section-4.2.2
   */
  MHD_add_response_header( mhdResponse
                         , MHD_HTTP_HEADER_UPGRADE
                         , "websocket" );
  // Header is known to exist from check above
  std::string acceptResponse{ headers[ MHD_HTTP_HEADER_SEC_WEBSOCKET_KEY ] };

  // If we had websocket support we could use MHD_websocket_create_accept_header
  acceptResponse.append( "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" );
  acceptResponse = encoding::sha1::encode( acceptResponse );
  acceptResponse = encoding::base64::encode( acceptResponse );
  MHD_add_response_header( mhdResponse
                         , MHD_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT
                         , acceptResponse.c_str() );

  return mhdResponse;
}


bool Server::Private::isHeaderSet( const std::string& header ) const
{
  return headers.count( header ) > 0;
}

bool Server::Private::isHeaderSetTo( const std::string& header
                                   , const std::string& value ) const
{
  const auto I{ headers.find( header ) };
  return ( I != headers.end() ) && ( I->second == value );
}

struct ConnectionContext
{
  std::string url;
  MHD_PostProcessor* pp{ nullptr };
};

// static
MHD_Result Server::Private::accessHandlerCallback( void* userData
                                                 , MHD_Connection* connection
                                                 , const char* url
                                                 , const char* methodStr
                                                 , const char* versionStr
                                                 , const char* uploadData
                                                 , size_t* uploadDataSize
                                                 , void** connectionContext )
{
  auto server = (Private*)userData;

//  std::cout << "URL:              " << url << '\n';
//  std::cout << "Method:           " << methodStr << '\n';
//  std::cout << "Version:          " << versionStr << '\n';
//  std::cout << "Upload data size: " << *uploadDataSize << '\n';
//  std::cout << "Upload data:      ";
//  if ( uploadData )
//  {
//    std::cout << std::string{ uploadData, *uploadDataSize } << '\n';
//  }
//  else
//  {
//    std::cout << "<null>\n";
//  }
//  std::cout << std::flush;

  const auto method{ parseMethod( methodStr ) };
  if ( method == Method::eInvalid )
  {
    return MHD_NO;
  }

  const auto version{ parseVersion( versionStr ) };
  if ( version.major < 0 )
  {
    return MHD_NO;
  }

  //std::cout << "HEADERS:" << std::endl;
  MHD_get_connection_values( connection, MHD_HEADER_KIND, &keyValueIterator, server );

  if ( !*connectionContext )
  {
    // First invocation for this connection so set things up as required.
    ConnectionContext*const cc{ new ConnectionContext };
    *connectionContext = cc;

    cc->url = url;
    if ( method == Method::ePost )
    {
      cc->pp = MHD_create_post_processor( connection
                                        , 1024 * 32
                                        , &postDataIterator
                                        , userData );
      if ( !cc->pp )
      {
        std::cerr << "Failed to create POST processor!" << std::endl;
      }
    }

    // Return now and we get called again. No, I don't know either.
    return MHD_YES;
  }

  // Handle upgrade to a WebSocket connection. This can only be over GET and
  // must be at least HTTP 1.1
  MHD_Response* mhdResponse
  {
    server->maybeCreateWebSocketResponse( url, method, version )
  };
  if ( mhdResponse )
  {
    const auto result
    {
      MHD_queue_response( connection, MHD_HTTP_SWITCHING_PROTOCOLS, mhdResponse )
    };

    MHD_destroy_response( mhdResponse );

    return result;
  }

  ConnectionContext*const cc{ (ConnectionContext*)(*connectionContext) };

  // Note that passing MHD_POSTDATA_KIND to MHD_get_connection_values does
  // nothing, even for small POST data, contrary to the documentation. It
  // appears that you must use the post processor in all cases. This would
  // appear to be backed up by a quick inspection of the libmicrohttpd source.
  //
  // Note that if uploadDataSize is non-zero then we are processing POST data
  // and must not queue a response.
  if ( ( method == Method::ePost ) && ( *uploadDataSize != 0 ) )
  {
    const auto result
    {
      MHD_post_process( cc->pp, uploadData, *uploadDataSize )
    };
    *uploadDataSize = 0;
    return result;
  }

  // Ought to be safe to destroy this now. Sample code does this in a request
  // completed callback but I don't see why we can't do it now.
  MHD_destroy_post_processor( cc->pp );
  cc->pp = nullptr;

  const auto response
  {
    server->invokeRequestHandler( connection
                                , url
                                , method
                                , version
                                , std::move( std::string{ uploadData
                                                        , *uploadDataSize } ) )
  };

  mhdResponse
    = MHD_create_response_from_buffer( response.content.size()
                                     , (void*)response.content.c_str()
                                     , MHD_RESPMEM_MUST_COPY );

  const auto result
  {
    MHD_queue_response( connection, response.code, mhdResponse )
  };

  MHD_destroy_response( mhdResponse );

  delete cc;

  return result;
}


// static
MHD_Result Server::Private::keyValueIterator( void* userData
                                            , enum MHD_ValueKind kind
                                            , const char *key
                                            , const char *value )
{
  //std::cout << "key: " << key << ", value: " << value << std::endl;

  auto server = (Private*)userData;

  server->headers[ key ] = value;

  return MHD_YES;
}

// static
MHD_Result Server::Private::postDataIterator( void* userData
                                            , MHD_ValueKind kind
                                            , const char* key
                                            , const char* filename
                                            , const char* contentType
                                            , const char* transferEncoding
                                            , const char* data
                                            , uint64_t off
                                            , size_t size )
{
//  std::cout << "PP ITERATOR \n";
//  std::cout << " key: " << key << '\n';
//  if ( filename )
//  {
//    std::cout << " filename: " << filename << '\n';
//  }
//  if ( contentType )
//  {
//    std::cout << " content type: " << contentType << '\n';
//  }
//  if ( transferEncoding )
//  {
//    std::cout << " transfer encoding: " << transferEncoding << '\n';
//  }
//  std::cout << " data: " << std::string{ data, size } << '\n';
//  std::cout << " offset: " << off << '\n';
//  std::cout << " size: " << size << '\n';
//  std::cout << std::flush;

  auto server = (Private*)userData;

  const auto I{ server->postKeyValues.find( key ) };
  if ( I != server->postKeyValues.end() )
  {
    I->second.append( data, size );
  }
  else
  {
    server->postKeyValues.emplace( std::piecewise_construct
                                 , std::forward_as_tuple( key )
                                 , std::forward_as_tuple( data, size ) );
  }

  return MHD_YES;
}

// static
void Server::Private::upgradeHandler( void* userData
                                    , MHD_Connection* connection
                                    , void* connectionContext
                                    , const char* extraData
                                    , size_t extraDataSize
                                    , MHD_socket socket
                                    , MHD_UpgradeResponseHandle* upgradeHandle )
{
  auto server = (Private*)userData;
  auto cc{ (ConnectionContext*)connectionContext };
  if ( !cc )
  {
    std::cerr << "Missing connection context in upgrade handler" << std::endl;
    return;
  }

  const auto connectionID{ globalConnectionID++ };

  std::scoped_lock l{ server->webSocketMutex };

  const auto emplacePair
  {
    server->webSockets.emplace( std::piecewise_construct
                              , std::forward_as_tuple( connectionID )
                              , std::forward_as_tuple( connectionID
                                                     , server->config.maxSocketBytesToReceive
                                                     , cc->url
                                                     , socket
                                                     , upgradeHandle
                                                     , std::bind( &Private::webSocketClosed, server, std::placeholders::_1 ) ) )
  };
  if ( !emplacePair.second )
  {
    std::cerr << "Failed to create WebSocket for " << cc->url << std::endl;
    return;
  }

  WebSocket& webSocket{ emplacePair.first->second };

  auto receivers
  {
    server->webSocketHandler->connectionEstablised(
      { connectionID
      , cc->url
      , ws::Senders::Impl::create( std::bind( &WebSocket::sendMessage
                                            , &webSocket
                                            , std::placeholders::_1
                                            , std::placeholders::_2 )
                                 , std::bind( &WebSocket::sendClose
                                            , &webSocket
                                            , std::placeholders::_1
                                            , std::placeholders::_2 )
                                 , std::bind( &WebSocket::sendPing
                                            , &webSocket
                                            , std::placeholders::_1 )
                                 , std::bind( &WebSocket::sendPong
                                            , &webSocket
                                            , std::placeholders::_1 ) )
      } )
  };

  webSocket.receivers = std::move( receivers );

  if ( extraDataSize > 0 )
  {
// TODO - this is not quite right. It may be data or control and it will have
// a Header that needs decoding to tell us.
//    webSocket.receivers.receiveData( connectionID, std::string{ extraData, extraDataSize } );
  }

  server->poller.add( webSocket.socket, std::bind( &WebSocket::receive, &webSocket ) );
}

Server::Response Server::Private::invokeRequestHandler( MHD_Connection* connection
                                                      , std::string url
                                                      , Method method
                                                      , Version version
                                                      , std::string payload )
{
  Headers       headers;
  PostKeyValues postKeyValues;

  headers.swap( this->headers );
  postKeyValues.swap( this->postKeyValues );

  auto response
  {
    requestHandler( std::move( url )
                  , method
                  , version
                  , std::move( headers )
                  , std::move( payload )
                  , std::move( postKeyValues ) )
  };

  return response;
}

void Server::Private::webSocketLoop()
{
  while ( webSocketRunning )
  {
    // Note that the addition of new WebSocket instances does not affect the
    // poller as it is already mutex protected internally.
    //
    // If any WebSocket gets closed as a result of the poll then it will end up
    // in the closedWebSockets container.
    const int pollResult{ poller( 500 ) };
    if ( pollResult < 0 )
    {
      std::cerr << "Error while polling" << std::endl;
      std::this_thread::sleep_for( std::chrono::seconds( 2 ) ); // Keep trying every 2 seconds
      continue;
    }

    // Now see if any WebSocket needs removed from the list.
    for ( const auto& connectionID : closedWebSockets )
    {
      std::scoped_lock l{ webSocketMutex };

      const auto W{ webSockets.find( connectionID ) };
      if ( W == webSockets.end() )
      {
        std::cerr << "Unknown WebSocket closed!" << std::endl;
        return;
      }
      webSockets.erase( W );
    }
    closedWebSockets.clear();
  }
}

void Server::Private::webSocketClosed( ws::ConnectionID connectionID )
{
  closedWebSockets.insert( connectionID );
}


Server::Server( Config config
              , RequestHandler rh
              , std::optional<ws::Handler> wsh )
  : d{ std::make_unique<Private>( std::move( config )
                                , std::move( rh )
                                , std::move( wsh ) ) }
{
}

Server::Server( Config config
              , std::string httpsCert
              , std::string httpsPrivateKey
              , RequestHandler rh
              , std::optional<ws::Handler> wsh )
  : d{ std::make_unique<Private>( std::move( config )
                                , std::move( httpsCert )
                                , std::move( httpsPrivateKey )
                                , std::move( rh )
                                , std::move( wsh ) ) }
{
}

Server::~Server()
{
}

Server::Server( Server&& ) = default;


} // End of namespace httpd


} // End of namespace lb
