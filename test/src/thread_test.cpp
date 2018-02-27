/*

Copyright (C) 2014-2018  Blaise Dias

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
*/
#include <iostream>
extern void lock_test();
extern void rwlock_test2();
extern void rwlock_mw_test2();
extern void rwlock_rmw_test2();
extern void bs_test();

int main(int argc, char* argv[])
{
//    bs_test();
//    std::cout << "--------------------" << std::endl;
    lock_test();
    std::cout << "--------------------" << std::endl;
    rwlock_test2();
    std::cout << "--------------------" << std::endl;
    rwlock_mw_test2();
    std::cout << "--------------------" << std::endl;
//    rwlock_rmw_test2();
//    std::cout << "--------------------" << std::endl;
    std::cout << "All Done. " << std::endl;
}

