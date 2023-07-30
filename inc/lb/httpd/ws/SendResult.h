#ifndef LIB_LB_HTTPD_WS_SENDRESULT_H
#define LIB_LB_HTTPD_WS_SENDRESULT_H

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


namespace lb
{


namespace httpd
{


namespace ws
{


/** \brief The return code from the data and control sending methods on \a Senders. */
enum class SendResult
{
  /** \brief Data message or control frame has been sent succesfully. */
  eSuccess,

  /** \brief Data message or control frame has *not* been sent succesfully. */
  eFailure,

  /** \brief Connection has been closed, no further data can be sent. */
  eClosed,

  /** \brief The \a Senders object has been default constructed and not upgraded. */
  eNoImplementation
};


} // End of namespace ws


} // End of namespace httpd


} // End of namespace lb


#endif // LIB_LB_HTTPD_WS_SENDRESULT_H
