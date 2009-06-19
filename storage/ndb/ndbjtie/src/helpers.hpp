/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * helpers.hpp
 */

#ifndef helpers_hpp
#define helpers_hpp

//#include <cstdio>
#include <cstdlib>
#include <iostream>

//using namespace std;
using std::cout;
using std::endl;
using std::flush;

/************************************************************
 * Helper Macros & Functions
 ************************************************************/

#define ABORT_ERROR(message)                                            \
    do { cout << "!!! error in " << __FILE__ << ", line: " << __LINE__  \
              << ", msg: " << message << "." << endl;                   \
        exit(-1);                                                       \
    } while (0)

// an output stream for debug messages
#if DEBUG
#define CDBG if (0); else cout
#else
#define CDBG if (1); else cout
#endif

// some macros for tracing
#define ENTER(name)                             \
    CDBG << "--> " << name << endl;

#define LEAVE(name)                             \
    CDBG << "<-- " << name << endl;

#define TRACE(name)                             \
    Tracer _tracer_(name);

// use as:
// myfunction() {
//   TRACE("myfunction()");
//   ...
// }
class Tracer
{
    const char* const name;
public:
    Tracer(const char* name) : name(name) {
        ENTER(name);
    }

    ~Tracer() {
        LEAVE(name);
    }
};

#endif // helpers_hpp
