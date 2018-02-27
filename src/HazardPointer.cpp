/*

Copyright (C) 2016  Blaise Dias

HazardPointer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

HazardPointer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Semaphore.  If not, see <http://www.gnu.org/licenses/>. *
*/
#include <assert.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "HazardPointer.hpp"

namespace benedias {
    namespace concurrent {

/// CollectorClientInterface state transitions
/// k_UNREGISTERED to k_REGISTERED
/// k_REGISTERED to k_COLLECTING
/// k_REGISTERED to k_DELETING
/// k_COLLECTING to k_REGISTERED
/// k_DELETING to k_UNREGISTERED
enum {
    k_UNREGISTERED = 0,
    k_REGISTERED,
    k_COLLECTING,
    k_DELETING,
};

CollectorThread& CollectorThread::RegisterClient(
        CollectorClientInterface& client)
{
    if (k_UNREGISTERED != __atomic_load_n(&client.state, __ATOMIC_CONSUME))
    {
        // TODO(Blaise Dias):
        CHECK_ASSERT(false);
    }
    CHECK_ASSERT(client.collector_thread == NULL);

    std::lock_guard<std::mutex>  lockg(data_lock);
    clients.push_back(&client);
    clientsID.push_back(++id_value);
    __atomic_store_n(&client.state, k_REGISTERED, __ATOMIC_RELEASE);
    client.collector_thread = this;
    return *this;
}

void CollectorThread::DeregisterClient(CollectorClientInterface& client)
{
    unsigned expected = k_REGISTERED;
    while (!__atomic_compare_exchange_n(&client.state, &expected, k_DELETING,
                false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
    {
        std::this_thread::yield();
        // TODO(Blaise Dias): if expected is not k_COLLECTING do something!
        CHECK_ASSERT(expected == k_COLLECTING);
        expected = k_REGISTERED;
    }

    std::lock_guard<std::mutex>  lockg(data_lock);
    int rm_ix_client = -1;
    for (unsigned ix_client = 0; ix_client < clients.size(); ++ix_client)
    {
        if (clients[ix_client] == &client)
        {
            rm_ix_client = ix_client;
            break;
        }
    }

    if (rm_ix_client < 0)
    {
        // TODO(Blaise Dias):
        CHECK_ASSERT(false);
        return;
    }
    clients.erase(clients.cbegin() + rm_ix_client);
    clientsID.erase(clientsID.cbegin() + rm_ix_client);
    __atomic_store_n(&client.state, k_UNREGISTERED, __ATOMIC_RELEASE);
}

bool CollectorThread::Collect()
{
//    std::lock_guard<std::mutex>  lockg(exec_lock);
    unsigned curr_id = 0;
    bool pending = false;
    CollectorClientInterface* clientp;

    // Scan each client for GC, reduce data lock time by looping on client id.
    // * acquiring the data lock
    // * finding the first client candidate, id > 
    // * setting its state to collect
    // * releasing the lock
    // * recording the client id
    // * performing collect on the client
    // * until we have exhausted all
    do
    {
        clientp = NULL;
        unsigned expected = k_REGISTERED;
        // Reduce contention on the data lock....
        // find the first candidate client, change its state to 
        // collecting and exit the loop.
        {
            std::lock_guard<std::mutex>  lockg(data_lock);
            for (unsigned ix_client = 0;
                    (ix_client < clientsID.size()) && (clientp == NULL);
                    ++ix_client)
            {
                if (clientsID[ix_client] <= curr_id)
                    continue;
                curr_id = clientsID[ix_client];
                if (!clients[ix_client]->HaveDeletes())
                    continue;
                expected = k_REGISTERED;
                if (!__atomic_compare_exchange_n(
                            &clients[ix_client]->state, &expected, k_COLLECTING,
                            false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
                {
#ifdef  DEBUG                    
printf("CollectorThread::Collect expected %d\n", expected);
#endif
                    CHECK_ASSERT(expected == k_DELETING);
                }
                else
                    clientp = clients[ix_client];
            }
        }

        if (clientp)
        {
            if (!clientp->Collect())
                pending = true;
            expected = k_COLLECTING;
            if (!__atomic_compare_exchange_n(
                            &clientp->state, &expected, k_REGISTERED,
                            false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            {
                CHECK_ASSERT(false);
            }
        }
    }while(clientp);
    return pending;
}

CollectorThread::~CollectorThread()
{
    Stop(true);
    std::lock_guard<std::mutex>  lockg(data_lock);
    for (unsigned ix_client = 0; ix_client < clients.size(); ++ix_client)
    {
        CHECK_ASSERT(clients[ix_client]->collector_thread == this);
        clients[ix_client]->collector_thread = NULL;
    }
    clients.clear();
    clientsID.clear();
}

} // namespace concurrent
} // namespace benedias

