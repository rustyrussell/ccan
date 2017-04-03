/*
   Unix SMB/CIFS implementation.
   Simplistic implementation of tap interface.

   Copyright (C) Rusty Russell 2012

     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <ccan/err/err.h>
#include "no-fsync.h"

#ifndef __location__
#define __TAP_STRING_LINE1__(s)    #s
#define __TAP_STRING_LINE2__(s)   __TAP_STRING_LINE1__(s)
#define __TAP_STRING_LINE3__  __TAP_STRING_LINE2__(__LINE__)
#define __location__ __FILE__ ":" __TAP_STRING_LINE3__
#endif

extern unsigned tap_ok_count, tap_ok_target;
#define plan_tests(num) do { tap_ok_target = (num); } while(0)
#define ok(e, ...) ((e) ? (printf("."), tap_ok_count++, true) : (warnx(__VA_ARGS__), false))
#define ok1(e) ok((e), "%s:%s", __location__, #e)
#define pass(...) (printf("."), tap_ok_count++)
#define fail(...) warnx(__VA_ARGS__)
#define diag(...) do { printf(__VA_ARGS__); printf("\n"); } while(0)
#define exit_status() (tap_ok_count == tap_ok_target ? 0 : 1)
