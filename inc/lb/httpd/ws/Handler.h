#ifndef LIB_LB_HTTPD_WS_HANDLER_H
#define LIB_LB_HTTPD_WS_HANDLER_H

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


#include <memory>

#include <lb/httpd/ws/ConnectionID.h>
#include <lb/httpd/ws/Receivers.h>
#include <lb/httpd/ws/Senders.h>


namespace lb
{


namespace httpd
{


namespace ws
{


/** \brief Object for handling incoming WebSocket connections.

  WebSocket connections start with an HTTP GET request that is then upgraded to
  a two-way WebSocket connection.

  By installing a ws::Handler on \a Server at construction time these two-way
  connections can be accepted or not based on URL. If accepted they can then be
  managed by receiving data and control messages with \a Receivers and sending
  data and control messages with \a Senders. Note that this obejct is a lightweight
  handle to a shared implemenation so you can and should keep your own copy
  after passing it into the constructor.

  A Handler consists of two functions, \a IsHandled and \a ConnectionEstablished,
  passed to the constructor.

  When a new connection is made your \a IsHandled function is called with the
  URL to see if it should be accepted or not. If it is accepted then the upgrade
  is performed and, if successful, your \a ConnectionEstablished function is
  called to provide you with a \a Connection object containing a \a Senders
  object with which you can send data on the connection. The return value of
  \a ConnectionEstablished is a \a Receivers object that you create so that
  Handler can pass received data back to you.

  Once you are no longer able to handle requests, typically on destruction of
  your function objects, then you should call \a stopHandling to ensure they
  are not invoked again.
 */
struct Handler
{
  using IsHandled = std::function< bool( const std::string& ) >;

  struct Connection
  {
    const ConnectionID id;
    const std::string url;

    Senders senders;
  };
  using ConnectionEstablished = std::function< Receivers( Connection ) >;

  Handler( IsHandled, ConnectionEstablished );

  Handler( Handler&& ) = default;
  Handler& operator=( Handler&& ) = default;
  Handler( const Handler& ) = default;
  Handler& operator=( const Handler& ) = default;

  bool isHandled( const std::string& url ) const;

  Receivers connectionEstablised( Connection ) const;

  /**
      \brief Call this to ensure your function objects are not invoked again.

      This is intended to be used when the functions passed in to the
      constructor are no longer safe to call.
   */
  void stopHandling();

private:
  struct Private;
  std::shared_ptr<Private> d;
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_HANDLER_H
