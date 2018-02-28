/*

Copyright (C) 2016-2018  Blaise Dias

This file is part of sqzbsrv.

sqzbsrv is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

sqzbsrv is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sqzbsrv.  If not, see <http://www.gnu.org/licenses/>.

Classes implementing simple read write locks,
based on 
1) "Fast Userspace Read/Write locks, built on top of mutexes. Paul Mackerras and Rusty Russel."
2) "Futexes are tricky by Ulrich Drepper"
*/
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <climits>
#include <assert.h>
#include <string.h>
static const bool futex_throw_on_error = false;
static const bool futex_assert_on_error = true;

#include "bdfutex.h"
#include "bdrwlock.h"
#include "stacktrace.h"

namespace benedias {
const bool verbose = false;


static void enter_gate(int *gate, const char* _fn_err_txt) 
{
    // gate values can only be
    // 0 : unlocked
    // 1 : locked
    // 2 : locked contended
    // Transitions on lock 0 -> 1, 1 -> 2, 0 -> 2
    // Transitions on unlock 1 -> 0, 2 -> 0 (wake)
    int expected=0;
    if (!__atomic_compare_exchange_n(gate, &expected, 1,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        do
        {
            if (expected == 2 
                    || __atomic_compare_exchange_n(gate, &expected, 2,
                       false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            {
                futex_wait(gate, 2, _fn_err_txt);
            }
            expected = 0;
        } while (!__atomic_compare_exchange_n(gate, &expected, 2,
                   false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    }
}

static bool try_enter_gate(int *gate, const char* _fn_err_txt) 
{
    // gate values can only be
    // 0 : unlocked
    // 1 : locked
    // 2 : locked contended
    int expected=0;
    if (__atomic_compare_exchange_n(gate, &expected, 1,
               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return true;
    return false;
}


static void leave_gate(int* gate, const char* _fn_err_txt)
{
    int vsampled;
    if ((vsampled = __atomic_fetch_sub(gate, 1,  __ATOMIC_ACQ_REL)) != 1)
    {
#ifdef  TESTING
        assert(vsampled == 2);
#endif
        // at least one thread is waiting.
        __atomic_store_n(gate, 0, __ATOMIC_RELEASE);
        futex_wake(gate, 1, _fn_err_txt);
    }
    else
    {
#ifdef  TESTING
        assert(vsampled == 0);
#endif
    }
}


//============================================================================

#if 0
void sem_rw_lock::read_lock()
{
    sem_wait(&sem);
    // @here if there are no writers
    __atomic_add_fetch(&nreaders, 1, __ATOMIC_ACQ_REL);
    sem_post(&sem);
}

void sem_rw_lock::read_unlock()
{
    static const char* _fn_err_txt = " sem_rw_lock::read_unlock";
    // Negative if a) this is the last reader
    // b) there is a writer.
    if(0 > __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL))
    {
        futex_wake(&nreaders, 1, _fn_err_txt);
    }
}

void sem_rw_lock::write_lock()
{
    static const char* _fn_err_txt = " sem_rw_lock::write_lock";
    int val_nreaders;
    sem_wait(&sem);
    // sem is unavailable for the duration of the write,
    // including the wait for readers to complete.
    if (0 <= (val_nreaders = __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL)))
    {
        do
        {
            futex_wait(&nreaders, val_nreaders, _fn_err_txt);
            __atomic_load(&nreaders, &val_nreaders, __ATOMIC_ACQUIRE);
        }while(val_nreaders >= 0);
    }
}

void sem_rw_lock::write_unlock()
{
    __atomic_store_n(&nreaders, 0, __ATOMIC_RELEASE);
    sem_post(&sem);
}

#endif

//============================================================================
//
void futex_rw_control::read_lock()
{
    static const char* _fn_err_txt = " fu_read_lock::lock";
    enter_gate(&gate, _fn_err_txt);
    // @here if there are no active writers
    __atomic_add_fetch(&nreaders, 1, __ATOMIC_RELEASE);
    leave_gate(&gate, _fn_err_txt);
}

void futex_rw_control::read_unlock()
{
    static const char* _fn_err_txt = " fu_read_lock::unlock";
    // Negative if both
    // a) this is the last reader
    // b) there is a writer.
    int result = __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL);
    if(0 > result)
    {
        assert(result == -1);
        futex_wake(&nreaders, 1, _fn_err_txt);
    }
}

void futex_rw_control::write_lock()
{
    static const char* _fn_err_txt = " fu_write_lock::lock";
    int val_nreaders;
    enter_gate(&gate, _fn_err_txt);
    // gate is unavailable for the duration of the write,
    // including the wait for readers to complete.
    // atomically decrement of nreaders, 
    // if at least 1 reader final value is >= 0, so we must wait.
    // readers detect that there is a waiter when decrementing nreaders
    // yields a negative value.
    if (0 <= (val_nreaders = __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL)))
    {
        do
        {
            futex_wait(&nreaders, val_nreaders, _fn_err_txt);
            __atomic_load(&nreaders, &val_nreaders, __ATOMIC_CONSUME);
        }while(val_nreaders >= 0);
    }
}

void futex_rw_control::write_unlock()
{
    static const char* _fn_err_txt = " fu_write_lock::unlock";
    __atomic_store_n(&nreaders, 0, __ATOMIC_RELEASE);
    leave_gate(&gate, _fn_err_txt);
}

bool futex_rw_control::try_write_modify()
{
    static const char* _fn_err_txt = " futex_rw_control::try_write_modify";
    if (try_enter_gate(&gate, _fn_err_txt))
    {
        __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL);
        int val_nreaders;
        if (0 <= (val_nreaders = __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL)))
        {
            do
            {
                futex_wait(&nreaders, val_nreaders, _fn_err_txt);
                __atomic_load(&nreaders, &val_nreaders, __ATOMIC_CONSUME);
            }while(val_nreaders >= 0);
        }
        return true;
    }
    return false;
}

void futex_rw_control::write_modify()
{
    static const char* _fn_err_txt = " futex_rw_control::write_modify";
    // Negative if a) this is the last reader
    // b) there is a writer.
    if(0 > __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL))
    {
        futex_wake(&nreaders, 1, _fn_err_txt);
    }
    int val_nreaders;
    enter_gate(&gate, _fn_err_txt);
    // gate is unavailable for the duration of the write,
    // including the wait for readers to complete.
    if (0 <= (val_nreaders = __atomic_sub_fetch(&nreaders, 1, __ATOMIC_ACQ_REL)))
    {
        do
        {
            futex_wait(&nreaders, val_nreaders, _fn_err_txt);
            __atomic_load(&nreaders, &val_nreaders, __ATOMIC_CONSUME);
        }while(val_nreaders >= 0);
    }
}

futex_rw_control::~futex_rw_control()
{
    static const char* _fn_err_txt = " ~futex_rw_control";
    if (nreaders)
    {
        std::cerr << _fn_err_txt << " readers count is not 0 " << std::endl;
        print_stacktrace();
        abort();
    }

    if (!try_enter_gate(&gate, _fn_err_txt))
    {
        std::cerr << _fn_err_txt << " writer active " << std::endl;
        print_stacktrace();
        abort();
    }
}

} // namespace benedias
