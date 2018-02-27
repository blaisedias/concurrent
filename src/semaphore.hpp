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
#ifndef BENEDIAS_SEMAPHORE_INCLUDED
#define BENEDIAS_SEMAPHORE_INCLUDED

#include <semaphore.h>
#include <pthread.h>

namespace benedias {
// semaphore implemented using futex calls.
// Key feature difference from a semaphore, multiple posts are accepted
// TODO: implement timed_wait.
class binary_semaphore
{
    // Non copyable
    binary_semaphore& operator=(const binary_semaphore&) = delete;
    binary_semaphore(binary_semaphore const&) = delete;

    // Non movable
    binary_semaphore& operator=(binary_semaphore&&) = delete;
    binary_semaphore(binary_semaphore&&) = delete;

    int gate=0;
    public:
        binary_semaphore() {}
        binary_semaphore(bool initial_state);
        ~binary_semaphore();
        void post();
        void wait();
        bool try_wait();
        int  get_value() { return gate; }
};


// Wrapper around UNIX counting semaphore
// TODO: error handling
class semaphore
{
    sem_t   sem;

    // Non copyable
    semaphore& operator=(const semaphore&) = delete;
    semaphore(semaphore const&) = delete;

    // Non movable
    semaphore& operator=(semaphore&&) = delete;
    semaphore(semaphore&&) = delete;

    public:
    semaphore(int count)
    {
        sem_init(&sem, 0, count);
    }

    ~semaphore()
    {
        sem_destroy(&sem);
    }

    void post()
    {
        sem_post(&sem);
    }

    void wait()
    {
        sem_wait(&sem);
    }

    void try_wait()
    {
        sem_trywait(&sem);
    }

    int get_value()
    {
        int v;
        sem_getvalue(&sem, &v);
        return v;
    }

};

}// namespace benedias
#endif
