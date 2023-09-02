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

#include <atomic>
#include <iostream>
#include <csignal>

#include <lb/httpd/Server.h>


std::atomic<bool> running{ true };

void signalHander( int s )
{
  running = false;
}

void installSignalHandlers()
{
  struct sigaction sa{};
  sa.sa_handler = signalHander;
  sigaction( SIGTERM
           , &sa
           , NULL );
  sigaction( SIGINT
           , &sa
           , NULL );
}

lb::httpd::Server::Response requestHandler( std::string url,
                                            lb::httpd::Server::Method method,
                                            lb::httpd::Server::Version version,
                                            lb::httpd::Server::Headers headers,
                                            std::string payload,
                                            lb::httpd::Server::PostKeyValues )
{
  return { 404, "This is a websocket echo server only. Regular http ignored." };
}


struct WSInfo
{
  std::string url;
  lb::httpd::ws::Senders dataSender;
};
using WSInfoLookup = std::unordered_map< lb::httpd::ws::ConnectionID, WSInfo >;
WSInfoLookup wsInfoLookup;


void dataReceiver( lb::httpd::ws::ConnectionID id
                 , lb::httpd::ws::Receivers::DataOpCode dataOpCode
                 , std::string data )
{
  if ( dataOpCode == lb::httpd::ws::Receivers::DataOpCode::eBinary )
  {
    // Only echo back text messages.
    return;
  }

  const auto I{ wsInfoLookup.find( id ) };
  if ( I != wsInfoLookup.end() )
  {
    WSInfo& wsInfo{ I->second };

    const lb::httpd::ws::SendResult result{ wsInfo.dataSender.sendData( std::move( data ), 0 ) };
    switch ( result )
    {
    case lb::httpd::ws::SendResult::eSuccess:
      break;
    default:
      std::cerr << "Failed to send data frame!" << std::endl;
      break;
    }
  }
  else
  {
    // Can't send response as we have no DataSender.
    std::cerr << "Unrecognised WebSocket connection ID" << std::endl;
  }
}

lb::httpd::ws::Receivers connectionEstablished( lb::httpd::ws::Handler::Connection connection )
{
  wsInfoLookup[ connection.id ] = { connection.url, connection.senders };
  return { dataReceiver, {} };
}


lb::httpd::ws::Handler wsHandler( []( const std::string& ) { return true; }
                                , connectionEstablished );


int main( int argc, char** argv )
{
  installSignalHandlers();

  lb::httpd::Server server( { { 2345 } }
                          , requestHandler
                          , wsHandler );

  while( running )
  {
    sleep( 1 );
  }

  return 0;
}
