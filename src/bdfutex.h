/*

Copyright (C) 2017,2018  Blaise Dias

This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This file is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BENEDIAS_BDFUTEX_H_INCLUDED
#define BENEDIAS_BDFUTEX_H_INCLUDED

#include <sys/types.h>

namespace benedias {

enum {
    futex_op_success = 0,
    futex_op_failed = -1,
    futex_op_invalid = -2,
};

typedef void (*critical_error)(const char*);
// Global
extern critical_error   futex_critical_error;

int futex_wake(int *uaddr, int wake_count, const char* txt);
int futex_wait(int *uaddr, int expected, const char *txt);

int futex_unlock_pi(pid_t *uaddr, const char *txt);
int futex_lock_pi(pid_t *uaddr, const char *txt);

} // namespace
#endif
