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
along with HazardPointer .  If not, see <http://www.gnu.org/licenses/>. *
*/

#include <assert.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <string>
#include "HazardPointer.hpp"

namespace benedias {
    namespace concurrent {

std::string* strings[10] {
    new std::string("abc"),
    new std::string("def"),
    new std::string("ghi"),
    new std::string("jkl"),
    new std::string("mno"),
    new std::string("pqr"),
    new std::string("stu"),
    new std::string("vwx"),
    new std::string("yz0"),
    new std::string("123"),
};

void populate_strings()
{
    static const char *cstr[10] =
    {
        "abc",
        "def",
        "ghi",
        "jkl",
        "mno",
        "pqr",
        "stu",
        "vwx",
        "yz0",
        "123",
    };
    for (int ix=0; ix < 10; ix++)
    {
        if (strings[ix] == NULL)
        {
            strings[ix] = new std::string(cstr[ix]);
//            printf("NEW: %d) %p\n", ix, strings[ix]);
        }
    }
}

class A {
 public:
        A() {
            puts("Create A");
        }
        virtual ~A()
        {
            puts("Destroy A");
        }
};


class B: public A {
    std::thread* t1;

 public:
        B()
        {
            std::cout << "Create B " << ' ' << this \
                << ' ' << std::this_thread::get_id() << '\n';
            t1 = new std::thread(*this);
        }
        virtual ~B()
        {
            puts("Destroy B");
        }
        void test()
        {
            std::cout << "I am B " << ' ' << this << \
                ' ' << std::this_thread::get_id() << '\n';
        }
        void operator()()
        {
            std::cout << "Operator B " << ' ' << this \
                << ' ' << std::this_thread::get_id() << '\n';
            test();
        }
        void join()
        {
            t1->join();
        }
};

void printhp(const HazardPointer<std::string>& hp)
{
    std::cout << hp() << " " << \
        reinterpret_cast<void *>(hp()) << std::endl;
}

int test(HazardPointerList<std::string>& hplist, bool verbose=true)
{
    {
        HazardPointer<std::string> hp0(hplist);
        HazardPointer<std::string> hp1(hplist);
        HazardPointer<std::string> hp9(hplist);

        hp0.Acquire(&strings[0]);
        hp1.Acquire(&strings[1]);
        hp9.Acquire(&strings[9]);
        if(verbose)
        {
            printf("0) "); printhp(hp0);
            printf("1) "); printhp(hp1);
            printf("9) "); printhp(hp9);
        }
        if(verbose)
        {
            printf("Deleting "); printhp(hp9);
        }
        hp9.Delete();
        strings[9]=NULL;
    }
//    hplist.Collect();
    {
        HazardPointer<std::string> hps[9] = {
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
            HazardPointer<std::string>(hplist),
        };
        for (unsigned x = 0; x < 9; ++x)
        {
            hps[x].Acquire(&strings[x]);
        }
        if (verbose)
        {
            for (unsigned x = 0; x < 9; ++x)
            {
                printhp(hps[x]);
            }
            printf("Deleting "); printhp(hps[0]);
        }
        hps[0].Delete();
        strings[0]=NULL;
    }
    return 0;
}

#define NTEST_ITERS 1000
void t0()
{
    CollectorThread* thCollector = new CollectorThread();
    HazardPointerList<std::string>   hplist(thCollector);
    for(int x = 0; x < NTEST_ITERS; x++)
    {
        populate_strings();
        test(hplist);
    }
    delete thCollector;
}

void t1()
{
    CollectorThread* thCollector = new CollectorThread();
    {
        HazardPointerList<std::string>   hplist(thCollector);
        for(int x = 0; x < NTEST_ITERS; x++)
        {
            populate_strings();
            test(hplist);
        }
    }
    delete thCollector;
}

void t2()
{
    CollectorThread thCollector;
    HazardPointerList<std::string>   hplist(&thCollector);
    for(int x = 0; x < NTEST_ITERS; x++)
    {
        populate_strings();
        test(hplist);
    }
}

    } // namespace concurrent
} // namespace benedias

int main(int argc, char** argv)
{
    benedias::concurrent::t0();
    benedias::concurrent::t1();
    benedias::concurrent::t2();
    return 0;
}

