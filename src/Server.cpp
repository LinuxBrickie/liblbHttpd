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

#include <cstring>
#include <iostream>


namespace lb
{


namespace httpd
{


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
  if ( versionStr == "HTTP/0.9" )
  {
    return Server::Version::e0_9;
  }
  else if ( versionStr == "HTTP/1.0" )
  {
    return Server::Version::e1_0;
  }
  else if ( versionStr == "HTTP/1.1" )
  {
    return Server::Version::e1_1;
  }
  else if ( versionStr == "HTTP/2.0" )
  {
    return Server::Version::e2_0;
  }

  return Server::Version::eUnknown;
}


Server::Server( int port , RequestHandler rh )
  : mhd{ MHD_start_daemon( MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG
                         , port
                         , nullptr // accept policy callback not required
                         , nullptr // accept policy callback user data
                         , &accessHandlerCallback
                         , this
                         , MHD_OPTION_END ) }
  , requestHandler{ rh }
{
}

Server::~Server()
{
  MHD_stop_daemon( mhd );
}

struct ConnectionContext
{
  MHD_PostProcessor* pp{ nullptr };
};

// static
MHD_Result Server::accessHandlerCallback( void* userData
                                        , MHD_Connection* connection
                                        , const char* url
                                        , const char* methodStr
                                        , const char* version
                                        , const char* uploadData
                                        , size_t* uploadDataSize
                                        , void** connectionContext )
{
  auto server = (Server*)userData;

//  std::cout << "URL:              " << url << '\n';
//  std::cout << "Method:           " << methodStr << '\n';
//  std::cout << "Version:          " << version << '\n';
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

  if ( !*connectionContext )
  {
    // First invocation for this connection so set things up as required.
    ConnectionContext*const cc{ new ConnectionContext };
    *connectionContext = cc;

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
                                , std::move( url )
                                , method
                                , parseVersion( version )
                                , std::move( std::string{ uploadData
                                                        , *uploadDataSize } ) )
  };

  MHD_Response*const mhdResponse
  {
    MHD_create_response_from_buffer( response.content.size()
                                   , (void*)response.content.c_str()
                                   , MHD_RESPMEM_MUST_COPY )
  };

  const auto result
  {
    MHD_queue_response( connection, response.code, mhdResponse )
  };

  MHD_destroy_response( mhdResponse );

  delete cc;

  return result;
}


// static
MHD_Result Server::keyValueIterator( void* userData
                                   , enum MHD_ValueKind kind
                                   , const char *key
                                   , const char *value )
{
  std::cout << "key: " << key << ", value: " << value << std::endl;
  return MHD_YES;
}

// static
MHD_Result Server::postDataIterator( void* userData
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

  auto server = (Server*)userData;

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

Server::Response Server::invokeRequestHandler( MHD_Connection* connection
                                             , std::string url
                                             , Method method
                                             , Version version
                                             , std::string payload )
{
//  std::cout << "HEADERS:" << std::endl;
//  std::cout << MHD_get_connection_values( connection, MHD_HEADER_KIND, &keyValueIterator, this ) << std::endl;

  auto response
  {
    requestHandler( std::move( url ), method, version, std::move( payload ), std::move( postKeyValues ) )
  };

  postKeyValues.clear();

  return response;
}


} // End of namespace httpd


} // End of namespace lb
