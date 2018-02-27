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
along with this file.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include "bdlock.h"
#include "bdfutex.h"

#include <stdexcept>

namespace benedias {

void fu_lock::lock()
{
    futex_lock_pi(&gate, "benedias::fu_lock::lock()");
}

void fu_lock::unlock()
{
    int rv = futex_unlock_pi(&gate, "benedias::fu_lock::unlock()");
    switch(rv)
    {
        case futex_op_success:
            break;
        case futex_op_failed:
            // should never get here.
            assert(true);
            break;
        case futex_op_invalid:
            throw std::runtime_error("unlock attempted without prior locking");
            break;
    }
}

} // namespace
