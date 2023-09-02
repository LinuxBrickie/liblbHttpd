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

#include <lb/httpd/ws/Receivers.h>
#include "ReceiversImpl.h"

#include <unordered_map>


namespace lb
{


namespace httpd
{


namespace ws
{


Receivers::Receivers( DataReceiver dr, ControlReceiver cr )
  : d{ std::make_shared<Impl>( std::move( dr), std::move( cr ) ) }
{
}

bool Receivers::receiveData( ConnectionID id
                           , DataOpCode dataOpCode
                           , std::string message )
{
  if ( d )
  {
    d->receiveData( id, dataOpCode, message );
    return true;
  }

  return false;
}

bool Receivers::receiveControl( ConnectionID id
                              , ControlOpCode opCode
                              , std::string payload )
{
  if ( d )
  {
    d->receiveControl( id, opCode, payload );
    return true;
  }

  return false;
}

void Receivers::stopReceiving()
{
  if ( d )
  {
    d->close();
  }
}


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb
