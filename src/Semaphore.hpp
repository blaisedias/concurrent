/*

Copyright (C) 2016  Blaise Dias

Semaphore is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Semaphore is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Semaphore.  If not, see <http://www.gnu.org/licenses/>. * 
*/

#ifndef _SEMAPHORE_HPP_INCLUDED
#define _SEMAPHORE_HPP_INCLUDED

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <stdexcept>
#include <system_error>

namespace benedias {

#ifdef  USE_NATIVE_SEMAPHORES
/// \class Semaphore
/// Wrapper for counting semaphores c code.
class Semaphore
{
 protected:
        sem_t semaphore;

        bool HandleWaitFail()
        {
            if (errno == EINVAL)
                throw std::system_error(errno, std::system_category());
            assert(errno == EDEADLK);
            return false;
        }

        void HandlePostFail()
        {
                // Ignore overflow
                if (errno != EOVERFLOW)
                {
                    // if here something is very wrong,
                    // errno can only be EINVAL or ENOSYS,
                    // so instance should not exist, as construction has failed
                    // or we have scribbled the value of semaphore, in either
                    // case soldiering on is pointless.
                    assert(false);
                }
        }
 public:
        /// Default constructor, semaphore state is unsignalled.
        Semaphore()
        {
            int rv = sem_init(&semaphore, 0, 0);
            // use sterrror_r?
            if (rv)
                throw std::system_error(errno, std::system_category());
        }

        /// Construct semaphore state with signalled count.
        explicit Semaphore(unsigned int count)
        {
            int rv = sem_init(&semaphore, 0, count);
            // use sterrror_r?
            if (rv)
                throw std::system_error(errno, std::system_category());
        }

        virtual ~Semaphore()
        {
            sem_destroy(&semaphore);
        }

        /// Signal the semaphore
        inline void Post()
        {
            if (sem_post(&semaphore))
                HandlePostFail();
        }

        /// Wait for semaphore signal.
        virtual inline bool Wait()
        {
            int rv;

            do
            {
                rv = sem_wait(&semaphore);
            }while(rv == -1 && errno == EINTR);

            if (rv)
                return HandleWaitFail();

            return true;
        }
};

// Wrapper for process local semaphores, behaviour approximates binary
// semaphores.
class BinarySemaphore :  public Semaphore
{
 public:
        BinarySemaphore():Semaphore() {}
        explicit BinarySemaphore(bool signalled):Semaphore(signalled?1:0) {}

        /// Wait for semaphore signal, and clear all pending signals.
        /// Approximates behaviour for a binary semaphore.
        inline bool Wait()
        {
            if (!Semaphore::Wait())
                return false;

            do
            {
                if (sem_trywait(&semaphore))
                {
                    if (errno == EAGAIN)
                        break;
                    return HandleWaitFail();
                }
            }while(true);

            return true;
        }
};

#else 

/// \class Semaphore
/// Wrapper for counting semaphores c code.
class Semaphore
{
    // 0 => unavailable, 1=> available
    volatile int uword;

    inline int futex_wake()
    {
        return syscall(SYS_futex, &uword, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1,
                       NULL, NULL, 0);
    }

    inline int futex_wait(int val)
    {
        return syscall(SYS_futex, &uword, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                val, NULL, NULL, 0);
    }

    public:
    /// Default constructor, semaphore state is unsignalled.
    Semaphore()
    {
        uword = 0;
    }

    /// Construct semaphore state with signalled count.
    explicit Semaphore(int count)
    {
        uword = count;
    }

    virtual ~Semaphore()
    {
    }

    void Post()
    {
        if (__atomic_add_fetch(&uword, 1, __ATOMIC_ACQ_REL) == 0)
        {
            futex_wake();
        }
    }

    bool Wait()
    {
        // 2) We want to do futex_wait(v < 0)
        // 2) But we also do want to be exposed to missed futex_wake
        // !!!!!!!!!!! This code is incorrect
        int v = __atomic_sub_fetch(&uword, 1, __ATOMIC_ACQ_REL);
        if (v < 0)
        {
            futex_wait(v);
        }
        return true;
    }

    bool TryWait()
    {
        int expected;
        __atomic_load(&uword, &expected, __ATOMIC_ACQUIRE);
        if (expected > 0)
        {
            int desired = expected-1;
            if(__atomic_compare_exchange_n(&uword, &expected, desired,
                        false, __ATOMIC_ACQ_REL, __ATOMIC_CONSUME))
                return true;
        }
        return false;
    }
};

/// \class BinarySemaphore
/// Implements binary semaphores using futexes,
/// Semantics: a wait op consumes ALL preceding post ops,
/// since the previous wait op (if any).
class BinarySemaphore
{
    enum {
        BS_UNAVAILABLE,
        BS_AVAILABLE,
    };
    // 0 => unavailable, 1=> available
    volatile int uword;

    inline int futex_wake()
    {
        return syscall(SYS_futex, &uword, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1,
                       NULL, NULL, 0);
    }

    inline int futex_wait(int val)
    {
        return syscall(SYS_futex, &uword, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                val, NULL, NULL, 0);
    }

 public:
    BinarySemaphore():uword(BS_UNAVAILABLE) {}
    explicit BinarySemaphore(bool signalled):uword(
            signalled ? BS_AVAILABLE : BS_UNAVAILABLE) {}

    void Post()
    {
        int expected = BS_UNAVAILABLE;
        if (__atomic_compare_exchange_n(&uword, &expected, BS_AVAILABLE,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_CONSUME))
        {
            futex_wake();
        }
        else
            assert(expected == BS_AVAILABLE);
    }

    bool Wait()
    {
        int expected = BS_AVAILABLE;
        while (!__atomic_compare_exchange_n(&uword, &expected, BS_UNAVAILABLE,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_CONSUME))
        {
            futex_wait(expected);
            expected = BS_AVAILABLE;
        }
        return true;
    }

    bool TryWait()
    {
        int expected = BS_AVAILABLE;
        if(__atomic_compare_exchange_n(&uword, &expected, BS_UNAVAILABLE,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_CONSUME))
            return true;
        return false;
    }
};
#endif

} // namespace benedias
#endif  // _SEMAPHORE_HPP_INCLUDED
