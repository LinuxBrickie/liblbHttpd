#ifndef LIB_LB_HTTPD_WS_RECEIVERS_H
#define LIB_LB_HTTPD_WS_RECEIVERS_H

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

#include <lb/httpd/ws/ConnectionID.h>

#include <functional>
#include <memory>
#include <string>


namespace lb
{


namespace httpd
{


namespace ws
{


/** \brief The means of receiving from the WebSocket.

    This is provided *to* the \a Handler via the return value of its
    \a ConnectionEstablished callback.

    Fragmented (data) messages are reassembled by Requester so that what you
    receive via \a receiveData is the complete message, you do not get access
    to the individual frames.

    Control messages are never fragmented so you receive the payload of the
    control frame directly in \a receiveControl. Note that control messages
    are for your information only and do not need replied to as they will be
    handled for you appropriately. Indeed in the case of a connection close
    control frame you will not be able to send anything back as the \a Senders
    will have been closed off to further sends.
 */
class Receivers
{
public:
  using DataReceiver = std::function< void( ConnectionID, std::string ) >;
  enum class ControlOpCode
  {
    eClose,
    ePing,
    ePong
  };
  using ControlReceiver = std::function< void( ConnectionID, ControlOpCode, std::string ) >;

  /** \brief Create an invalid object. Both receive methods will immediately return false. */
  Receivers() = default;

  Receivers( DataReceiver, ControlReceiver );

  Receivers( Receivers&& ) = default;
  Receivers& operator=( Receivers&& ) = default;
  Receivers( const Receivers& ) = default;
  Receivers& operator=( const Receivers& ) = default;

  bool receiveData( ConnectionID, std::string );
  bool receiveControl( ConnectionID, ControlOpCode, std::string );

  /**
      \brief Call this to ensure your function objects are not invoked again.

      This is intended to be used when the functions passed in to the
      constructor are no longer safe to call.
   */
  void stopReceiving();

private:
  struct Impl;
  std::shared_ptr<Impl> d;
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_RECEIVERS_H
