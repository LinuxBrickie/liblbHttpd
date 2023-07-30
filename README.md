# liblbHttpd

An AGPL-3.0-or-later C++ library for serving HTTP requests.

lb is short for LinuxBrickie, my online handle.

## Dependencies

The main library dependencies are
- liblbEncoding (available from my github account, licensed under GPL-3.0-or-later)
- libmicrohttpd (licensed under LGPL-2.1-or-later)

In addition the gtest binary dependencies are
- googletest( licensed under BSD 3-Clause)

## Licensing

As the copyright holder I am happy to consider alternative licensing if
the AGPL-3.0-or-later licence does not suit you. Please feel free to reach
out to me at fotheringham.paul@gmail.com.

## Usage

Create a Server instance and install a RequestHandler on it to service
requests.

## Notes

Built and tested on Fedora 37 against
- libmicrohttpd 0.9.76

Not all libmicrohttpd functionality is exposed. Basic GET and POST handling
should work.

