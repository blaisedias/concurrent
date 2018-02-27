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

#include <atomic>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <stdexcept>
#include <system_error>

#include "bdfutex.h"
#include "stacktrace.h"


namespace benedias {

static void default_futex_critical_error(const char *txt)
{
    if (!txt)
        txt = "";
//    std::system_error syserr_exc(errno, std::system_category(), txt);
    print_stacktrace();
//    throw syserr_exc;
    abort();
}

// Global
critical_error   futex_critical_error = default_futex_critical_error;

static inline int futex(int *uaddr, int futex_op, int val,
        const struct timespec *timeout, int *uaddr2, int val3)
{
    return syscall(SYS_futex, uaddr, futex_op, val,
                   timeout, uaddr2, val3);
}


// Public
int futex_wake(int *uaddr, int wake_count, const char* txt)
{
    int rv = futex(uaddr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, wake_count, NULL, NULL, 0);
    if (rv < 0)
    {
        // Things really have gone wrong!
        // errno should only be EINVAL
        futex_critical_error(txt);
        return -1;
    }
    return rv;
}

// Public
int futex_wait(int *uaddr, int expected, const char *txt)
{
    int rv = futex(uaddr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expected, NULL, NULL, 0);
    if (rv != 0 && errno != EINTR && errno != EAGAIN)
    {
        // Things really have gone wrong!
        // errno should only be EACCES.
        futex_critical_error(txt);
        return -1;
    }
    return 0; // check and possibly try again
}


// Public
int futex_lock_pi(pid_t *uaddr, const char *txt)
{
    pid_t desired = syscall(SYS_gettid);
    pid_t expected = 0;
    int lock_op = FUTEX_LOCK_PI | FUTEX_PRIVATE_FLAG;
    if (!__atomic_compare_exchange(uaddr, &expected, &desired,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        // TODO: Make behaviour selectable, recovering a lock when a thread died whilst
        // holding the lock only makes sense if resource state can be rewound, or
        // set to a stable state, or reset.
        if (*uaddr & FUTEX_OWNER_DIED)
        {
            //    lock_op = FUTEX_TRYLOCK_PI | FUTEX_PRIVATE_FLAG;
        }
        int rv = futex(uaddr, lock_op, 0, NULL, NULL, 0);
        if (rv != 0 && errno != EINTR && errno != EAGAIN)
        {
            // Things really have gone wrong!
            // errno should only be EINVAL, ENOMEM, ENOSYS, EPERM, ESRCH
            futex_critical_error(txt);
            return -1;
        }
    }
    return 0; // check and possibly try again
}

#define MASK_OUT_FUTEX_WAITERS  (~(FUTEX_WAITERS))
#define MASK_OUT_FUTEX_OWNER_DIED  (~(FUTEX_OWNER_DIED))
// Public
int futex_unlock_pi(pid_t *uaddr, const char *txt)
{
    pid_t tid , expected;
    pid_t desired = 0;
    tid = expected = syscall(SYS_gettid);

    if (!__atomic_compare_exchange(uaddr, &expected, &desired,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        expected &= MASK_OUT_FUTEX_WAITERS;
        if (expected != tid)
        {
            fprintf(stderr, "invalid futex unlock\n");
            return futex_op_invalid;
        }

        int rv = futex(uaddr, FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG, 0, NULL, NULL, 0);
        if (rv != 0 && errno != EINTR && errno != EAGAIN)
        {
            // Things really have gone wrong!
            // errno should only be EINVAL, ENOSYS, ESRCH
            futex_critical_error(txt);
            return -1;
        }
    }
    return 0; // check and possibly try again
}


};
