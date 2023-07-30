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

#include <mutex>
#include <stdexcept>


namespace lb
{


namespace httpd
{


namespace ws
{


struct Handler::Private
{
  mutable std::mutex mutex;

  /** \brief Return true if the URL is valid for a WebSocket request.

      If false is returned then the upgrade to a WebSocket connection fails.

      This is optional. If left invalid then all URLs will be assumed to be
      valid and a WebSocket connection will be opened.
   */
  IsHandled isHandled;

  ConnectionEstablished connectionEstablished;

  Private( IsHandled ih, ConnectionEstablished ce )
    : isHandled{ std::move( ih ) }
    , connectionEstablished{ std::move( ce ) }
  {
    if ( !isHandled )
    {
      throw std::runtime_error( "IsHandled function is invalid" );
    }
    if ( !connectionEstablished )
    {
      throw std::runtime_error( "ConnectionEstablished function is invalid" );
    }
  }
};

Handler::Handler( IsHandled ih
                , ConnectionEstablished ce )
  : d{ std::make_shared<Private>( std::move( ih ), std::move( ce ) ) }
{
}

bool Handler::isHandled( const std::string& url ) const
{
  std::scoped_lock<std::mutex> l{ d->mutex };

  return d->isHandled( url );
}

Receivers Handler::connectionEstablised( Connection c ) const
{
  std::scoped_lock<std::mutex> l{ d->mutex };

  return d->connectionEstablished( std::move( c ) );
}

void Handler::stopHandling()
{
  std::scoped_lock<std::mutex> l{ d->mutex };

  d->isHandled = {};
  d->connectionEstablished = {};
}


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb
