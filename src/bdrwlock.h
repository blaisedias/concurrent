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
    2) read lock followed by write lock on the same thread will deadlock,
    because write locking waits for all current read locks to be unlocked.
    3) read lock followed by read lock on the same thread will deadlock,
    if a write lock starts between the read lock operations.
    4) write lock followed by read lock on the same thread, will deadlock
    because read locking after a write lock has started will block till the write lock
    is released.
    5) write lock followed by write lock, will deadlock since write locking waits for
    previous write locks to be released.
    6) locks are not explicitly bound to threads, it is the callers responsibility to
    ensure that matching calls are made on the same thread.
    This is easily achieved using std::lock_guard
    7) For now, to avoid confusion, all locks are non-copyable and non-movable.

    NOTE: This solution does not incorporate priority inversion.
    For priority inversion to work it would be necessary to raise the priority
    of all threads with reader locks, when a thread with higher priority attempts
    a write lock.
*/
#ifndef BENEDIAS_RWLOCK_H_INCLUDED
#define BENEDIAS_RWLOCK_H_INCLUDED

namespace benedias {
// @brief simple futex based read write lock
// Should have FIFO behaviour for threads of the same priority,
// if futex wait implements FIFO dequeing on thread wake,
// which is expected for fair thread scheduling.
class futex_rw_control
{
    // Non copyable
    futex_rw_control& operator=(const futex_rw_control&) = delete;
    futex_rw_control(futex_rw_control const&) = delete;

    // Non movable
    futex_rw_control& operator=(futex_rw_control&&) = delete;
    futex_rw_control(futex_rw_control&&) = delete;

    //@brief this is the futex variable used as a mutex.
    //for read lock the gate is acquired, the number of readers is incremented,
    //and then the gate is released.
    //for read unlock the gate is not acquired.
    //for write lock the gate is acquired for the duration of the write
    //for write lock the gate is released.
    int gate = 0;
    //@brief this is the count of readers and futex variable used to wake
    //writers if any.
    int nreaders = 0;

    public:
    futex_rw_control(){}
    ~futex_rw_control();
    //@brief acquires the gate, and atomically increments nreaders.
    void read_lock();
    //@brief atomically decrements nreaders and wakes pending writer if any.
    void read_unlock();
    //@brief acquires the gate, and wait for existing read locks to be released, if any.
    void write_lock();
    //@brief releases the gate, and wakes pending readers or writers if any.
    void write_unlock();

    // The following functions make it possible to used a single lock
    // for read modify write operations on a shared resource.

    //@change a read lock into a write lock if possible.
    //try to acquire the gate, if successful (there are no writers)
    //waits for existing read locks to be released.
    //note the lock MUST be released by calling write_unlock not read_unlock call.
    //On success, it is safe to proceeed with the write operation, else the complete
    //read modify write operation must be repeated.
    bool try_write_modify();
    //@change a read lock into a write lock,
    //
    void write_modify();
};

// @brief simple futex based write only lock using a futex_rw_control instance
// Do not use if read modify write lock is required.
class fu_write_lock
{
    // Non copyable
    fu_write_lock& operator=(const fu_write_lock&) = delete;
    fu_write_lock(fu_write_lock const&) = delete;

    // Non movable
    fu_write_lock& operator=(fu_write_lock&&) = delete;
    fu_write_lock(fu_write_lock&&) = delete;

    futex_rw_control* control;
    fu_write_lock(futex_rw_control* rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    inline void lock()
    {
        control->write_lock();
    }
    inline void unlock()
    {
        control->write_unlock();
    }
};

// @brief simple futex based read only lock using a futex_rw_control instance
// Do not use if read modify write lock is required.
class fu_read_lock
{
    // Non copyable
    fu_read_lock& operator=(const fu_read_lock&) = delete;
    fu_read_lock(fu_read_lock const&) = delete;

    // Non movable
    fu_read_lock& operator=(fu_read_lock&&) = delete;
    fu_read_lock(fu_read_lock&&) = delete;

    futex_rw_control* control;
    fu_read_lock(futex_rw_control* rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    inline void lock()
    {
        control->read_lock();
    }
    inline void unlock()
    {
        control->read_unlock();
    }
};

// @brief simple futex based read modify write lock using a futex_rw_control instance
// Use if read modify write lock is required.
class fu_read_modifiable_lock
{
    bool write_modified = false;
    futex_rw_control*  control;

    // Non copyable
    fu_read_modifiable_lock & operator=(const fu_read_modifiable_lock &) = delete;
    fu_read_modifiable_lock (fu_read_modifiable_lock  const&) = delete;

    // Non move assigable
    fu_read_modifiable_lock & operator=(fu_read_modifiable_lock &&) = delete;

    fu_read_modifiable_lock(futex_rw_control* rwcontrol):control(rwcontrol) {}
    friend  class fu_rw_lock;

    public:
    // Move constructible
    fu_read_modifiable_lock (fu_read_modifiable_lock &&) = default;

    inline void lock()
    {
        control->read_lock();
    }

    inline void unlock()
    {
        if (write_modified)
            control->write_unlock();
        else
            control->read_unlock();
    }

    inline bool try_write_modify()
    {
        // NOP if already write modified.
        if (!write_modified)
            write_modified = control->try_write_modify();
        return write_modified;
    }

    inline void write_modify()
    {
        control->write_modify();
        write_modified = true;
    }

    inline bool is_write_lock()
    {
        return write_modified;
    }
};

// @brief simple futex based read write lock using a futex_rw_control instance
// This class encapsulates the futex_rw_control instance,
// and associated futex_write_lock and futex_read_lock instances.
// casting operators make it possible to use instances of this class for
// std::lock_guard<fu_read_lock> and std::lock_guard<fu_write_lock>
// For read modify write locks per thread state is required, a new
// instance of fu_read_modifiable_lock can be created calling
// make_modifiable_lock.
class fu_rw_lock
{
    // Non copyable
    fu_rw_lock& operator=(const fu_rw_lock&) = delete;
    fu_rw_lock(fu_rw_lock const&) = delete;

    // Non movable
    fu_rw_lock& operator=(fu_rw_lock&&) = delete;
    fu_rw_lock(fu_rw_lock&&) = delete;

    futex_rw_control  control;
    class fu_write_lock write_lock;
    class fu_read_lock read_lock;
    public:
        fu_rw_lock():write_lock(&control),read_lock(&control){}
        ~fu_rw_lock(){}
        operator fu_write_lock& () { return write_lock; }
        operator fu_read_lock& () { return read_lock; }

        fu_read_modifiable_lock make_modifiable_lock()
        {
            return std::forward<fu_read_modifiable_lock>(fu_read_modifiable_lock(&control));
        }
};

}// namespace benedias
#endif
