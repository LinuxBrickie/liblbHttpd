#ifndef LIB_LB_HTTPD_WS_RECEIVERSIMPL_H
#define LIB_LB_HTTPD_WS_RECEIVERSIMPL_H

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

#include <mutex>


namespace lb
{


namespace httpd
{


namespace ws
{


struct Receivers::Impl
{
  Impl( DataReceiver ds, ControlReceiver cs )
    : dataReceiver{ ds }
    , controlReceiver{ cs }
  {
  }

  /**
      \brief Should be called when the connection close handshake is initiated
             by either end.
   */
  void close()
  {
    std::scoped_lock l{ mutex };

    dataReceiver = {};
    controlReceiver = {};
  }

  std::mutex mutex;

  /**
      \brief An object for receiving WebSocket data messages.

      Once a close control frame is either sent or received \a close() should be
      called to reset this back to an invalid function.
   */
  DataReceiver dataReceiver;

  /**
      \brief An object for receiving WebSocket control frames.

      Once a close control frame is either sent or received \a close() should be
      called to reset this back to an invalid function.
   */
  ControlReceiver controlReceiver;
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_RECEIVERSIMPL_H
