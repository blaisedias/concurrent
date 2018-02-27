/*

Copyright (C) 2018  Blaise Dias


This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This file is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sqzbsrv.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifndef BENEDIAS_LOCK_H_INCLUDED
#define BENEDIAS_LOCK_H_INCLUDED

#include <sys/types.h>

namespace benedias {

class fu_lock
{
    // Non copyable
    fu_lock& operator=(const fu_lock&) = delete;
    fu_lock(fu_lock const&) = delete;

    // Non movable
    fu_lock& operator=(fu_lock&&) = delete;
    fu_lock(fu_lock&&) = delete;

    pid_t gate = 0;
    public:
        fu_lock() {}
        ~fu_lock(){}
        void lock();
        void unlock();
};

}// namespace benedias
#endif
