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

#ifndef _HAZARDPOINTER_HPP_INCLUDED
#define _HAZARDPOINTER_HPP_INCLUDED
#include <assert.h>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>
#include "Semaphore.hpp"

#define ASSERT_CHECKS   1
#if     ASSERT_CHECKS
#define CHECK_ASSERT(x)  assert((x))
#else
#define CHECK_ASSERT(x)  void((x))
#endif

namespace benedias {
namespace concurrent {

template <typename T> class HazardPointerList;
template <typename T> class HazardPointer;

/**
 * \class HazardPointerNode
 *
 * This class is the node class for HazardPointerList,
 * it does not have any public members or functions.
 * Each instance is "bound" to a single instance of HazardPointerList.
 * Instances of this class can only be created and destroyed by instances
 * of HazardPointerList.
 */
template <typename T> class HazardPointerNode {
    friend class HazardPointerList<T>;
    friend class HazardPointer<T>;

    /// object pointer
    T*  pointer = reinterpret_cast<T*>(0);
    T*  gc_pointer = reinterpret_cast<T*>(0);

    /// HazardPointerList instance to which this node belongs to.
    HazardPointerList<T>*   owner;
    /// Linked list member for the fixed list of hazard pointers.
    HazardPointerNode<T>*   fixed_link;
    /// Linked list member for the gc and free lists.
    HazardPointerNode<T>*   next;

    // Non copyable.
    HazardPointerNode(const HazardPointerNode&) = delete;
    HazardPointerNode& operator=(const HazardPointerNode&) = delete;
    // Non movable.
    HazardPointerNode(HazardPointerNode&& other) = delete;
    HazardPointerNode& operator=(const HazardPointerNode&&) = delete;

    /// Constructor.
    /// \param owner HazardPointerList instance to which the node is "bound".
    explicit HazardPointerNode(HazardPointerList<T>* owner)
    {
        this->owner = owner;
        next = NULL;
    }
    ~HazardPointerNode(){}

    /// utility function for assert checks.
    inline bool IsEnqueued()
    {
        return next != NULL;
    }

    /// utility function for assert checks.
    inline bool IsUnqueued()
    {
        return next == NULL;
    }

 public:
    /// Sets the object pointer object atomically, clients are responsible
    /// for checking that pointer has the expected value.
    /// The object pointed to is now "protected" until Release or Delete is
    /// called.
    /// If ptrptr is NULL, or the contents of ptrptr is NULL,
    /// then it is functionally equivalent to calling Release,
    /// except that the HazardPointer is still usable.
    /// \param ptrptr pointer to protected location pointing to the object.
    inline bool Acquire(T** ptrptr)
    {
        CHECK_ASSERT(IsUnqueued());
        CHECK_ASSERT(gc_pointer == NULL);
        
        if (NULL != gc_pointer)
            return false;
#if 0
        if (ptrptr == NULL)
            __atomic_store_n(&pointer, 0x0, __ATOMIC_RELEASE);
        else
            __atomic_store(&pointer, ptrptr, __ATOMIC_RELEASE);
#else
        __atomic_store(&pointer, ptrptr, __ATOMIC_RELEASE);
#endif
        return true;
    }

    /// Clears the object pointer.
    /// Functionally equivalent to calling Release,
    /// except that the HazardPointer is still usable.
    inline bool Clear()
    {
        CHECK_ASSERT(gc_pointer == NULL);
        __atomic_store_n(&pointer, 0x0, __ATOMIC_RELEASE);
        return true;
    }

    /// Releases "protection" on the object pointed to.
    /// After this calls to get_pointer will return NULL
    inline bool Release()
    {
        CHECK_ASSERT(gc_pointer == NULL);
        if (NULL == pointer)
            return false;

        __atomic_store_n(&pointer, 0x0, __ATOMIC_RELEASE);
        owner->EnqueueFreeRecord(this);
        return true;
    }

    /// Release "protection" on the object pointed to,
    /// and queues the object for deletion.
    inline bool Delete()
    {
        CHECK_ASSERT(gc_pointer == NULL);
        if (NULL == pointer)
            return false;

        (void)__atomic_exchange(&gc_pointer, &pointer, &pointer, __ATOMIC_ACQ_REL);
        owner->EnqueueRecordForCollection(this);
        CHECK_ASSERT(pointer == NULL);
        return true;
    }

    /// Get the pointer to the "protected" object.
    /// Do not cache the return value!
    inline T* get_pointer() const
    {
        return pointer;
    }

};

class CollectorThread;

/*
 * \class CollectorClientInterface
 *
 * This class implements the interface required for clients of the 
 * CollectorThread class
 */
class CollectorClientInterface {
 private:
        /// Client state, used to facilitate safe (de)registration
        /// of the client, whilst the CollectorThread is executing.
        /// This variable is atomically updated, by the CollectorThread
        /// class.
        volatile unsigned state = 0;
        friend class CollectorThread;

 protected:
        /// Lock for serialisation of the Collect function execution.
        std::recursive_mutex  collector_client_lock;
        /// Pointer to the collector thread this client is registered
        /// with, this field is update by the CollectorThread instance.
        CollectorThread* collector_thread = NULL;

 public:
        /// Garbage collection function
        /// \returns false is garbage collection is incomplete and needs
        /// retrying at some later point, true otherwise.
        virtual bool Collect() = 0;
        /// Garbage collection function
        /// \returns true if items are deletable.
        virtual bool HaveDeletes() = 0;
        virtual ~CollectorClientInterface(){}
};

/*
 * \class CollectorThread
 *
 * This class implements the logic for Safe Memory Reclamation,
 * using HazardPointers in a dedicated thread.
 *
 * Typically a single instance of this class will be required.
 * Multiple instances may improve collection response time and reduce
 * latency. When collection is required, a client "signals" the instance,
 * and all clients are scanned for collection. If collection was not complete
 * the thread sleeps for 100 milliseconds and retries the collection again.
 * This means that collection is attempted on lists which may not have
 * any objects to be reclaimed. 
 * The main overhead for each of these attempts
 * is the locking required for thread safe operation.
 */
class CollectorThread {
 private:
        std::vector<CollectorClientInterface*> clients;
        std::vector<unsigned> clientsID;
        std::mutex  data_lock;
        unsigned id_value = 0;
        volatile bool active = false;
        std::thread *thrd = NULL;
        bool Collect();
        void CollectOne(CollectorClientInterface& client);
        BinarySemaphore sema;

 public:
        /// On construction a thread is created, which runs the 
        /// collect fuction for each HazardPointerList registered
        /// with this instance of CollectorThread.
        CollectorThread()
        {
            active = true;
            thrd = new std::thread(Run, this);
        }

        ~CollectorThread();

        /// Register a client for garbage collection by this thread.
        /// \param client the client to register with the collector.
        CollectorThread& RegisterClient(CollectorClientInterface& client);

        /// Deregister a client for garbage collection by this thread.
        /// This function must be called by the destructor.
        /// \param client the client to deregister from the collector.
        void DeregisterClient(CollectorClientInterface& client);

        static void Run(CollectorThread* ct)
        {
            while (ct->active)
            {
                if (ct->Collect())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ct->sema.Post();
                }
                ct->sema.Wait();
            }
            ct->thrd = NULL;
        }

        /// Stops the collector thread.
        /// \param join default false, it true this function returns
        /// after performing a join on the thread.
        void Stop(bool join=false)
        {
            active = false;
            Signal();
            if (join && (thrd != NULL))
                thrd->join();
        }

        /// Signal the collector thread, that work needs to be done.
        void Signal()
        {
            sema.Post();
        }
};

/**
 * \class HazardPointerList
 *
 * This class implements the management for hazard pointers.
 * Typically a single instance of this class is required for each 
 * set of objects to be protected or container class.
 */
template <typename T> class HazardPointerList: public CollectorClientInterface {
    friend class HazardPointerNode<T>;

    /// Ending node for all lists, all links point to itself.
    HazardPointerNode<T>*  end_node;

    /// Thread safe linked list of ALL hazard pointer records,
    /// never deleted from.
    /// This list is only ever added to, and this restriction facilitates
    /// concurrency, but nevertheless is  major limitation of this implementation.
    HazardPointerNode<T>*   nodes = end_node;
    /// Thread safe linked list of hazard pointer records to be collected.
    HazardPointerNode<T>*   gc_list = end_node;
    /// Thread safe linked list of hazard pointer records that are free.
    HazardPointerNode<T>*   free_list = end_node;

    std::vector<T*>  all_haz_ptrs;

    /// Add a new HazardPointerNode to the set of nodes belonging to
    /// this HazardPointerList.
    /// \param node the node to add.
    void Add(HazardPointerNode<T>* node)
    {
        HazardPointerNode<T>* desired;
        do
        {
            node->fixed_link = nodes;
            desired = node;
        }while (!__atomic_compare_exchange(
                    &nodes, &node->fixed_link, &desired,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    }

    /// Thread safe push a node onto either one of the free or gc list.
    /// \param node the node to push.
    /// \param head pointer to the head node of the list.
    void push(HazardPointerNode<T>* node, HazardPointerNode<T>** head)
    {
        CHECK_ASSERT(node->IsUnqueued());
        HazardPointerNode<T>* desired;
        do
        {
            node->next = *head;
            desired = node;
        }while (!__atomic_compare_exchange(head, &node->next, &desired,
                   false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    }

    /// Thread safe remove of a node from the free or gc list.
    /// In practice this function is only called to remove items from
    /// the gc list.
    /// \param node the node to remove.
    /// \param head pointer to the head node of the list.
    void remove(HazardPointerNode<T>* node, HazardPointerNode<T>** head)
    {
        while (true)
        {
            HazardPointerNode<T>** ppnode = head;
            while (*ppnode != node && *ppnode != end_node)
            {
                ppnode = &((*ppnode)->next);
            }
            if (*ppnode == end_node)
            {
                CHECK_ASSERT(false);
                return;
            }
            HazardPointerNode<T>* expected = *ppnode;
            HazardPointerNode<T>* desired = expected->next;
            if (__atomic_compare_exchange(ppnode, &expected, &desired,
                        false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            {
                /// Record that the node is not on the gc or free list.
                node->next = NULL;
                return;
            }
        }
    }

    /// Thread safe pop of a node from the free or gc list.
    /// \param head pointer to the head node of the list.
    HazardPointerNode<T>* pop(HazardPointerNode<T>** head)
    {
        HazardPointerNode<T>* node;
        HazardPointerNode<T>* desired;
        do
        {
            node = *head;
            desired = node->next;
        }while(!__atomic_compare_exchange(head, &node, &desired,
                    false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
        if (node != end_node)
        {
            /// Record that the node is not on the gc or free list.
            node->next = NULL;
        }
        return node;
    }

    /// Enqueue a record on the free list.
    /// \param node the newly "freed" node.
    inline void EnqueueFreeRecord(HazardPointerNode<T>* node)
    {
        CHECK_ASSERT(node->IsUnqueued());
        CHECK_ASSERT(node->pointer == NULL);
        push(node, &free_list);
    }

    /// Enqueued a record on the gc list.
    /// \param node the node scheduled for garbage collection.
    inline void EnqueueRecordForCollection(HazardPointerNode<T>* node)
    {
        CHECK_ASSERT(node->IsUnqueued());
        CHECK_ASSERT(node->pointer == NULL);
        CHECK_ASSERT(node->gc_pointer != NULL);
        push(node, &gc_list);
        if (collector_thread)
        {
            collector_thread->Signal();
        }
    }

    void init()
    {
        end_node = new HazardPointerNode<T>(this);
        end_node->fixed_link = end_node;
        end_node->next = end_node;
        nodes = end_node;
        gc_list = end_node;
        free_list = end_node;
    }

 public:
    HazardPointerList()
    {
        init();
    }

    explicit HazardPointerList(CollectorThread* th_collector)
    {
        init();
        th_collector->RegisterClient(*this);
    }

    /// HazardPointerList destructor is non trivial and NOT thread safe.
    /// Clients MUST ensure that the HazardPointerList is NOT being accessed
    /// by other threads.
    /// To prevent leaks all objects in the gclist MUST be deleted.
    /// Release is invoked on all nodes not in the free or gc lists,
    /// and the nodes are deleted.
    /// This will leave associated HazardPointer instances with dangling
    /// pointers. One fix would be to maintain a list of all associated
    /// HazardPointer instances, and invalidate them here.
    /// That would be complex and expensive.
    /// For now program structure and sequencing must work around this 
    /// limitation.
    ~HazardPointerList()
    {
        if (collector_thread)
            collector_thread->DeregisterClient(*this);

        HazardPointerNode<T>* node = nodes;
        while(node != end_node)
        {
            if (node->pointer)
                node->Release();
            node = node->fixed_link;
        }

        Collect();
        gc_list = end_node;
        free_list = end_node;
        node = nodes;
        nodes = end_node;
        while (node != end_node)
        {
            HazardPointerNode<T>*   del_node = node;
            node = node->fixed_link;
            delete del_node;
        }
        delete end_node;
    }

    /// Add nodes to the free list.
    /// One strategy to avoid blocking on HazardPointerNode acquisition
    /// is to "prime" the free list by adding nodes to it.
    /// Working out an optimal value for an application requires
    ///  1) foreknowledge of the (maximum) number of threads 
    ///  2) foreknowledge of the number of Hazard Pointers used by each thread.
    ///  3) a guaranteed object collection rate.
    /// The 3rd is non trivial to arrange, especially when collection occurs asynchronously
    /// in another thread.
    /// An alternatively is to use a non-blocking allocator.
    void Prime(size_t node_count)
    {
        while (node_count--)
        {
            HazardPointerNode<T>* node = new HazardPointerNode<T>(this);
            Add(node);
            push(node, &free_list);
        }
    }

    /// Acquire a hazard pointer record.
    /// The "new" record may be a "recycled" instance or created anew which may 
    /// block on memory allocation.
    HazardPointerNode<T>*  AcquireNode()
    {
        HazardPointerNode<T>* node = pop(&free_list);
        if (node == end_node)
        {
            node = NULL;
            node = new HazardPointerNode<T>(this);
            Add(node);
        }
        return node;
    }

    /// Garbage collector function, objects which have been scheduled for
    /// deletion, are safely deleted by this function.
    /// The function is thread safe, at any one time only one thread may
    /// execute this function, other threads will be blocked.
    /// Returns true if all objects scheduled for delete, have been deleted
    /// false otherwise.
    bool Collect()
    {
        if (gc_list == end_node)
            return false;

        std::lock_guard<std::recursive_mutex> lockg(collector_client_lock);

        HazardPointerNode<T> *node = nodes;
        while (node != end_node)
        {
            if (node->pointer)
                all_haz_ptrs.push_back(node->pointer);
            node = node->fixed_link;
        }
        std::sort(all_haz_ptrs.begin(), all_haz_ptrs.end());

        HazardPointerNode<T> *gc_node = gc_list;
        while (gc_node != end_node)
        {
            CHECK_ASSERT(NULL != gc_node);
            HazardPointerNode<T> *next_gc_node = gc_node->next;

            T *obj_ptr = gc_node->gc_pointer;
            CHECK_ASSERT(NULL != obj_ptr);
            CHECK_ASSERT(NULL == gc_node->pointer);
            if (!std::binary_search(all_haz_ptrs.begin(),
                        all_haz_ptrs.end(),
                        obj_ptr))
            {
                __atomic_store_n(&gc_node->gc_pointer, 0x0, __ATOMIC_RELEASE);
                delete obj_ptr;
#ifdef  DEBUG                
printf("***** Collect:Deleted %p\n", obj_ptr);
#endif
                remove(gc_node, &gc_list);
                CHECK_ASSERT(node->pointer == NULL);
                push(gc_node, &free_list);
            }
            gc_node = next_gc_node;
        }

        all_haz_ptrs.clear();
        return gc_list == end_node;
    }

    inline bool HaveDeletes()
    {
        return gc_list != end_node;
    }
};

/**
 * \class HazardPointer
 *
 * This class is the preferred method, for creating and using hazard pointers.
 * This class is not designed for concurrent access by multiple threads.
 * The implementation is simplified by binding the scope of the associated
 * HazardPointerNode to the scope of the instance.
 * Instances are non copyable and almost non movable,
 * To facilitate construction of arrays of HazardPointer, we enable construction
 * of new instance using another instance - the contents of which
 * are moved to the new instance - and the previous instance points to nothing.
 */
template <typename T> class HazardPointer {
    HazardPointerNode<T>*  hp_node;

 public:
    static void *operator new(size_t) = delete;
    static void *operator new[](size_t) = delete;
    static void operator delete(void *) = delete;
    static void operator delete[](void *) = delete;

    // Non copyable.
    HazardPointer(const HazardPointer&) = delete;
    HazardPointer& operator=(const HazardPointer&) = delete;
    /// Somewhat movable, to facilitate declaration of arrays of HazardPointer.
    /// To support these semantics correctly we must now, check that the associated
    /// HazardPointerNode is non-null before using it, a loss in efficiency :-(.
    HazardPointer(HazardPointer&& other)
    {
        other.hp_node = std::move(hp_node);
        hp_node = NULL;
    }
    HazardPointer& operator=(const HazardPointer&&) = delete;

    /// Constructor, HazardPointer instances are bound to a
    /// HazardPointerNode instance, which can only be created
    /// by an instance of HazardPointerList.
    /// \param hplist the HazardPointerList for HazardPointerNode
    /// acquisition.
    explicit HazardPointer(HazardPointerList<T>& hplist)
    {
        hp_node = hplist.AcquireNode();
    }

    /// Destructor, if bound to a HazardPointerNode,
    /// removes the object pointer from the list of hazard pointers.
    ~HazardPointer()
    {
        Release();
    }

    /// Reuse, if bound to a HazardPointerNode, the associated
    /// object pointeris removed from the list of protected objects,
    /// and may now be subject to collection.
    /// Binds to a new HazardPointerNode instance if required.
    /// Typically called to reuse the HazardPointer instance,
    /// after a call to Delete or Release.
    /// TODO(Blaise Dias): re-evaluate: Is it cleaner to recycle after
    /// a call to Release or Delete?
    /// \param hplist the HazardPointerList for HazardPointerNode
    /// acquisition.
    inline bool Recycle(HazardPointerList<T>& hplist)
    {
        if (NULL != hp_node)
        {
            hp_node->Acquire(NULL);
            return true;
        }
        else
        {
            hp_node = hplist.AcquireNode();
        }
    }

    /// Sets the object pointer in the hazard pointer record,
    /// the object pointed to can be safely used after return from this
    /// function, until Release or Delete is invoked.
    /// Can be called multiple times with different values.
    /// If called with NULL pointer, the object pointer is set to NULL.
    /// \param ptrptr the location of the object pointer.
    inline bool Acquire(T** ptrptr)
    {
        if (NULL == hp_node)
            return false;
        hp_node->Acquire(ptrptr);
        return true;
    }

    /// Removes the object pointer from the list of hazard pointers,
    /// the object pointed to by the pointer, may now be subject
    /// to collection and is no longer accessible using this
    /// instance.
    inline void Release()
    {
        if (NULL != hp_node)
            hp_node->Release();
        hp_node = NULL;
    }

    /// Removes the object pointer from the list of hazard pointers,
    /// the object pointed to by the pointer, is queued for deletion,
    /// and is not accessible using this instance.
    inline void Delete()
    {
        if (NULL != hp_node)
            hp_node->Delete();
        hp_node = NULL;
    }

    /// Gets pointer to the object.
    /// If the hazard pointer has been released or deleted,
    /// the NULL pointer will be returned.
    inline T* operator()() const
    {
        if (hp_node)
            return hp_node->get_pointer();
        return NULL;
    }
};

} // namespace concurrent
} // namespace benedias
#endif  // _HAZARDPOINTER_HPP_INCLUDED
