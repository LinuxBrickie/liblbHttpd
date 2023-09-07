#ifndef LIB_LB_HTTPD_POLL_H
#define LIB_LB_HTTPD_POLL_H

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

#include <mutex>
#include <poll.h>
#include <unordered_map>
#include <vector>


/** \brief Wrapper around the system poll() call.

    Use \a add to enable polling of a file descriptor and register a callback to
    be invoked when data is available on that file descriptor.

    Use \a remove to remove the file descriptor from being polled.

    Use the function operator to perform a single poll of all registered file
    descriptors.

    Thread safe.
 */
class Poller
{
public:
  using Callback = std::function< bool() >;

  void add( int fd, Callback inCallback )
  {
    std::scoped_lock l{ pendingAddsMutex };
    pendingAdds.emplace( fd, inCallback ); // Actually added just prior to polling
  }

  void remove( int fd )
  {
    std::scoped_lock l{ pendingRemovalsMutex };
    pendingRemovals.push_back( fd );
  }

  int operator() ( int timeout )
  {
    processPendingRemovals();
    processPendingAdds();

    pollfd* pfd{ nullptr };
    if ( !pollFDs.empty() )
    {
      pfd = &pollFDs[0];
    }

    const int pollResult{ poll( pfd, pollFDs.size(), timeout ) };

    if ( pollResult < 0 )
    {
      std::cerr << "Error polling" << std::endl;
    }
    else if ( pollResult > 0 )
    {
      bool repoll{ false };
      int numFDsProcessed{ 0 };
      std::vector<int> toRemove;
      toRemove.reserve( pollFDs.size() );

      for ( PollFDs::size_type i = 0; i < pollFDs.size(); ++i )
      {
        if ( ( pollFDs[i].fd > -1 ) && ( pollFDs[i].revents & POLLIN ) )
        {
          if ( !pollCallbacks[i]() )
          {
            toRemove.push_back( pollFDs[i].fd );
          }
          ++numFDsProcessed;

          // Can terminate early if we've reached the number returned by poll.
          if ( numFDsProcessed == pollResult )
          {
            break;
          }
        }
      }

      bulkRemoval( toRemove );
    }

    return pollResult;
  }

private:
  void processPendingAdds()
  {
    std::scoped_lock l{ pendingAddsMutex };

    for ( auto&[ fd, callback ] : pendingAdds )
    {
      if ( nextAvailable == pollFDs.size() )
      {
        pollFDs.push_back( {} );
        pollCallbacks.emplace_back();
      }

      auto& pollfd{ pollFDs[ nextAvailable ] };
      pollfd.fd = fd;
      pollfd.events = POLLIN;
      pollCallbacks[ nextAvailable ] = std::move( callback );

      do
      {
        ++nextAvailable;
      }
      while( ( nextAvailable < pollFDs.size() ) && ( pollFDs[ nextAvailable ].fd < 0 ) );
    }

    pendingAdds.clear();
  }

  void bulkRemoval( const std::vector<int>& extraRemovals )
  {
    std::scoped_lock l{ pendingRemovalsMutex };

    pendingRemovals.insert( pendingRemovals.end()
                          , extraRemovals.begin()
                          , extraRemovals.end() );

    processPendingRemovalsNoLock();
  }

  void processPendingRemovals()
  {
    std::scoped_lock l{ pendingRemovalsMutex };

    processPendingRemovalsNoLock();
  }

  void processPendingRemovalsNoLock()
  {
    for ( auto& fd : pendingRemovals )
    {
      for ( PollFDs::size_type i = 0; i < pollFDs.size(); ++i )
      {
        auto& pfd{ pollFDs[i] };
        if ( pfd.fd == fd )
        {
          pfd.fd = -1;
          pfd.events = 0;
          pfd.revents = 0;

          pollCallbacks[i] = {};

          if ( i < nextAvailable )
          {
            nextAvailable = i;
          }

          break;
        }
      }
    }

    pendingRemovals.clear();
  }

  std::mutex pendingAddsMutex;
  using PendingAdds = std::unordered_map< int, Callback >;
  PendingAdds pendingAdds;

  std::mutex pendingRemovalsMutex;
  using PendingRemovals = std::vector< int >;
  PendingRemovals pendingRemovals;

  // For reference:
  //
  // struct pollfd {
  //   int   fd;         /* file descriptor */
  //   short events;     /* requested events */
  //   short revents;    /* returned events */
  // };

  // Keep the file descriptor vector separate from the callbacks as we need the
  // contiguous memory to pass to poll.
  using PollFDs = std::vector<pollfd>;
  PollFDs pollFDs;
  PollFDs::size_type nextAvailable{ 0 };

  using Callbacks = std::vector<Callback>;
  Callbacks pollCallbacks;
};


#endif // LIB_LB_HTTPD_POLL_H
