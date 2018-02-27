# concurrent
Concurrency and thread synchronisation.

## Hazard Pointers
Hazard pointer classes are not thread safe, they aren't meant to be!

### Design and implementation
The implementation relies on thread safe linked lists implemented using atomic operations,
and so is non blocking, only insert at top, and remove from top can be implemented atomically,
so functionally the lists are stacks.

The trade off for this simplicity and atomicity is that the set of items belonging to a "hazard pointer domain" can only ever grow. This will become clearer below.

The list encapsulator class HazardPointerList has three thread safe lists:
* Complete set.
* Garbage collection (gc) list.
* Free list.

The items (HazardPointerNode) inserted into HazardPointerList have two list link fields:
* One for the complete set, this is used for enumerating all pointers in hazard pointers.
* One shared between gc and free list.

When an item is created it is added to the complete set and that link field is never changed until the HazardPointerList is destroyed. This makes it safe to enumerate, without resorting to locks.

The second linked list field can be in 
* The free list.
* The gc list.
* NULL for when the object it is pointing to is live and "protected".

The free list is treated like a stack, newly freed items are pushed, and items are popped when required.

Adding to the gc list is a push like operation, i.e. atomic insert at the head.
However after collection items which are not the head may now be free, the collection process is made thread safe by mutual exclusion, and ensuring that asynchronous pushes to the head of gc list do not affect gc list traversal, fortunately this is fairly straightforward.


They should be treated as thread local storage.
Sequencing of setting pointers atomically and enqueuing on collection or free lists is important.
We atomically set pointers 
## Internals

