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

#include <lb/httpd/ws/Senders.h>
#include "SendersImpl.h"


namespace lb
{


namespace httpd
{


namespace ws
{


SendResult Senders::sendData( std::string message, size_t maxFrameSize )
{
  if ( d )
  {
    return d->sendData( message, maxFrameSize );
  }

  return SendResult::eNoImplementation;
}

SendResult Senders::sendClose( encoding::websocket::closestatus::PayloadCode code
                             , std::string reason )
{
  if ( d )
  {
    return d->sendClose( code, reason );
  }
  return SendResult::eNoImplementation;
}

SendResult Senders::sendPing( std::string payload ) const
{
  if ( d )
  {
    return d->sendPing( payload );
  }
  return SendResult::eNoImplementation;
}

SendResult Senders::sendPong( std::string payload ) const
{
  if ( d )
  {
    return d->sendPong( payload );
  }
  return SendResult::eNoImplementation;
}

} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb
