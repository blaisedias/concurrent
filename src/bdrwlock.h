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

Classes implementing simple read write locks, and possibly faster because of the simplicity.
References:
1) "Fast Userspace Read/Write locks, built on top of mutexes. Paul Mackerras and Rusty Russel."
2) "Futexes are tricky by Ulrich Drepper"

The rules are simple and harsh:
    1) the locks are non-recursive.
    2) read lock followed by write lock, will deadlock beacuse write locking waits for
    current reads to end.
    3) read lock followed by read lock, will deadlock if a write lock starts between the
    the read lock operations.
    4) write lock followed by read lock, will deadlock because read locking after a write
    lock has started will block till the write has completed.
    5) write lock followed by write lock, will deadlock since write locking waits for
    previous writes to complete.
    6) locks are not bound to threads, so it is the callers responsibility to ensure
    that matching calls are made on the same thread. This is easily achieved using
    std::lock_guard
    7) For now, to avoid confusion all locks are non-copyable and non-movable,
    this may not be tenable if it makes sense to have std::collections of locks.

    This is good enough for our purposes!
    The expectation is that the  simplicity makes the lock lightweight.
*/
#ifndef BENEDIAS_RWLOCK_H_INCLUDED
#define BENEDIAS_RWLOCK_H_INCLUDED
#include <thread>
#include <semaphore.h>

namespace benedias {
#if 0
// Simple FIFO read write lock, using a futexes, a semaphore and atomic ops,
// writers are not starved.
class sem_rw_lock
{
    // Non copyable
    sem_rw_lock& operator=(const sem_rw_lock&) = delete;
    sem_rw_lock(sem_rw_lock const&) = delete;

    // Non movable
    sem_rw_lock& operator=(sem_rw_lock&&) = delete;
    sem_rw_lock(sem_rw_lock&&) = delete;

    protected:
        sem_t  sem;
        int nreaders;
    public:
        sem_rw_lock()
        {
            sem_init(&sem, 0, 1);
            nreaders = 0;
        }

        ~sem_rw_lock()
        {
            sem_destroy(&sem);
        }

        void read_lock();
        void read_unlock();
        void write_lock();
        void write_unlock();
};
#endif

// Simple futex based read write lock
// Should have FIFO behaviour for threads of the same priority,
// if futex wait implements FIFO dequeing on thread wake,
// which is expected for fair thread scheduling.
struct futex_rw_control
{
    int gate = 0;
    int nreaders = 0;
    void read_lock();
    void read_unlock();
    void write_lock();
    void write_unlock();
    bool try_write_modify();
    void write_modify();
};

class fu_write_lock
{
    // Non copyable
    fu_write_lock& operator=(const fu_write_lock&) = delete;
    fu_write_lock(fu_write_lock const&) = delete;

    // Non movable
    fu_write_lock& operator=(fu_write_lock&&) = delete;
    fu_write_lock(fu_write_lock&&) = delete;

    futex_rw_control &control;
    fu_write_lock(futex_rw_control& rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    inline void lock()
    {
        control.write_lock();
    }
    inline void unlock()
    {
        control.write_unlock();
    }
};

class fu_read_lock
{
    // Non copyable
    fu_read_lock& operator=(const fu_read_lock&) = delete;
    fu_read_lock(fu_read_lock const&) = delete;

    // Non movable
    fu_read_lock& operator=(fu_read_lock&&) = delete;
    fu_read_lock(fu_read_lock&&) = delete;

    futex_rw_control &control;
    fu_read_lock(futex_rw_control& rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    inline void lock()
    {
        control.read_lock();
    }
    inline void unlock()
    {
        control.read_unlock();
    }
};

class fu_read_modifiable_lock
{
    bool write_modified = false;
    futex_rw_control  &control;

    fu_read_modifiable_lock(futex_rw_control& rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    inline void lock()
    {
        control.read_lock();
    }

    inline void unlock()
    {
        if (write_modified)
            control.write_unlock();
        else
            control.read_unlock();
    }

    inline bool try_write_modify()
    {
        // NOP if already write modified.
        if (!write_modified)
            write_modified = control.try_write_modify();
        return write_modified;
    }

    inline void write_modify()
    {
        control.write_modify();
        write_modified = true;
    }

    inline bool is_write_lock()
    {
        return write_modified;
    }
};

class fu_rw_lock
{
    // Non copyable
    fu_rw_lock& operator=(const fu_rw_lock&) = delete;
    fu_rw_lock(fu_rw_lock const&) = delete;

    // Non movable
    fu_rw_lock& operator=(fu_rw_lock&&) = delete;
    fu_rw_lock(fu_rw_lock&&) = delete;

    futex_rw_control  control;
    public:
        class fu_write_lock write_lock;
        class fu_read_lock read_lock;
        fu_rw_lock():write_lock(control),read_lock(control){}
        ~fu_rw_lock(){}
        operator fu_write_lock& () { return write_lock; }
        operator fu_read_lock& () { return read_lock; }

        fu_read_modifiable_lock make_modifiable_lock()
        {
            return fu_read_modifiable_lock(control);
        }
};

}// namespace benedias
#endif
