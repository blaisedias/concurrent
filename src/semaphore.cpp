/*

Copyright (C) 2018  Blaise Dias

This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This file  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.

Classes implementing semaphore support.
*/
#include <assert.h>

#include <string.h>
#include <stdexcept>
#include <atomic>
#include <climits>

static const bool futex_throw_on_error = false;
static const bool futex_assert_on_error = true;

#include "bdfutex.h"
#include "semaphore.hpp"

namespace benedias {
const bool verbose = false;
// Binary semaphore
binary_semaphore::binary_semaphore(bool initial_state)
{
    if (initial_state)
        gate = 1;
}

binary_semaphore::~binary_semaphore()
{
    static const char* _fn_err_txt = " benedias::binary_semaphore::~binary_semaphore";
    futex_wake(&gate, INT_MAX, _fn_err_txt);
}

void binary_semaphore::post()
{
    static const char* _fn_err_txt = " benedias::binary_semaphore::post";
    int expected=0;
    if (__atomic_compare_exchange_n(&gate, &expected, 1,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        futex_wake(&gate, 1, _fn_err_txt);
    }
}

void binary_semaphore::wait()
{
    static const char* _fn_err_txt = " binary_semaphore::wait";
    int expected=1;
    while (!__atomic_compare_exchange_n(&gate, &expected, 0,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        futex_wait(&gate, 0, _fn_err_txt);
        expected = 1;
    }
}

bool binary_semaphore::try_wait()
{
    int expected=1;
    if (!__atomic_compare_exchange_n(&gate, &expected, 0,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        return false;
    }
    return true;
}


} // namespace
