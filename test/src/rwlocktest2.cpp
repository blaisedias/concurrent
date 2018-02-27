/*

Copyright (C) 2017,2018  Blaise Dias

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
#include <clocale>
#include <iostream>
#include <vector>
#include <chrono>
#include <climits>
#include <mutex>

#include <assert.h>
#include "bdrwlock.h"

using benedias::fu_read_lock;
using benedias::fu_read_modifiable_lock;
using benedias::fu_write_lock;
using benedias::fu_rw_lock;
using std::string;
using namespace std::chrono_literals;

static std::vector<int> gv={1,2,3,4,5};
static fu_rw_lock    gv_lock;

struct rw_args {
    bool run = true;
    unsigned int read_iterations = 0;
    unsigned int write_iterations = 0;
    unsigned int iterations = 0;
    unsigned int try_write_mods = 0;
    unsigned int write_mods = 0;
    unsigned int max_iterations = INT_MAX;
    bool inc = true;
    rw_args(){}
    void read_test()
    {
        int len = gv.size();
        assert((len % 5) == 0);
        int xsum = 0;
        for(auto &x: gv)
        {
            xsum += x;
            --len;
        }
        assert(len == 0);
    }

    void write_test()
    {
        int len = gv.size();
        assert((len % 5) == 0);
        if (inc && len < 200)
        {
            for(int i = 1; i < 6; i++)
            {
                gv.push_back(i);
            }
        }
        else if (!inc && len)
        {
            for(int i = 1; i < 6; i++)
                gv.pop_back();
        }
    }
};


static void reader(rw_args& args)
{
    std::this_thread::sleep_for(1s);
    while(args.run && (++args.read_iterations < args.max_iterations))
    {
        {
            std::lock_guard<fu_read_lock> lg(gv_lock);
            args.read_test();
        }
        std::this_thread::yield();
    }
    args.run = false;
}

static void writer(rw_args& args)
{
    while(args.run && (++args.write_iterations < args.max_iterations))
    {
        {
            std::lock_guard<fu_write_lock> lg(gv_lock);
            args.write_test();
        }
        std::this_thread::sleep_for(1000us);
    }
    args.run = false;
}

static void m_writer(rw_args& args)
{
    std::this_thread::sleep_for(1s);
    while(args.run && (args.read_iterations < args.max_iterations))
    {
        {
            fu_read_modifiable_lock rmwlock = gv_lock.make_modifiable_lock();
            std::lock_guard<fu_read_modifiable_lock> lg(rmwlock);
            if (!rmwlock.try_write_modify())
            {
                rmwlock.write_modify();
                ++args.write_mods;
            }
            else
            {
                ++args.try_write_mods;
            }
            args.write_test();
            ++args.write_iterations;
        }
        std::this_thread::sleep_for(1000us);
    }
    args.run = false;
}


static void reader_writer(rw_args& args)
{
    std::this_thread::sleep_for(1s);
    while(args.run && (args.read_iterations < args.max_iterations))
    {
        bool write_pending = (0 == (args.read_iterations % 101));
        {
            fu_read_modifiable_lock rmwlock = gv_lock.make_modifiable_lock();
            std::lock_guard<fu_read_modifiable_lock> lg(rmwlock);
            do {
                ++args.read_iterations;
                args.read_test();
                if (write_pending)
                {
                    if (rmwlock.is_write_lock())
                    {
                        args.write_test();
                        ++args.write_iterations;
                        write_pending = false;
                    }
                    else
                    {
                        // repeat the read_test and then do the write test
                        if (!rmwlock.try_write_modify())
                        {
                            rmwlock.write_modify();
                            ++args.write_mods;
                        }
                        else
                        {
                            ++args.try_write_mods;
                        }
                    }
                }
            }while(write_pending);
        }
        std::this_thread::yield();
    }
    args.run = false;
}


void rwlock_rmw_test2()
{
    std::cout << "Read Modified Write Lock Test." << std::endl;
    std::vector<rw_args>reader_args;
    std::vector<std::thread>readers;
    std::vector<rw_args>writer_args;
    std::vector<std::thread>writers;
    std::vector<rw_args>reader_writer_args;
    std::vector<std::thread>reader_writers;

    for(auto i=0; i < 90; i++)
    {
        reader_args.emplace_back(rw_args());
    }
    for(auto &arg : reader_args)
    {
        readers.emplace_back(std::thread(reader, std::ref(arg)));
    }

    for(auto i=0; i < 10; i++)
    {
        reader_writer_args.emplace_back(rw_args());
    }
    for(auto &arg : reader_writer_args)
    {
        reader_writers.emplace_back(std::thread(reader_writer, std::ref(arg)));
    }

    for(auto i=0; i < 4; i++)
    {
        writer_args.emplace_back(rw_args());
    }
    writer_args[1].inc = false;
    writer_args[3].inc = false;
    for(auto &arg : writer_args)
    {
        writers.emplace_back(std::thread(writer, std::ref(arg)));
    }
    std::this_thread::sleep_for(10s);
    for(auto &arg : reader_args)
        arg.run = false;
    for(auto &arg : writer_args)
        arg.run = false;
    for(auto &arg : reader_writer_args)
        arg.run = false;
    for(auto &th : writers)
        th.join();
    for(auto &th : readers)
        th.join();
    for(auto &th : reader_writers)
        th.join();

    std::cout << "Concurrently accessed/modified vector size :" << gv.size() << std::endl;
    std::cout << "Reader iterations:" << std::endl;
    double v;
    v=0;
    for(auto &arg : reader_args)
    {
//        std::cout << arg.read_iterations << " ";
        v += arg.read_iterations;
    }
    std::cout << (v/reader_args.size()) << std::endl;
    std::cout << "Writer iterations:" << std::endl;
    v=0;
    for(auto &arg : writer_args)
    {
//        std::cout << arg.write_iterations << " ";
        v += arg.write_iterations;
    }
    std::cout << (v/writer_args.size()) << std::endl;
    std::cout << "Reader Writer iterations:" << std::endl;
    double vr=0;
    double vw=0;
    double vwm=0;
    double vtwm=0;
    for(auto &arg : reader_writer_args)
    {
//        std::cout << " rd:" << arg.read_iterations << " wr:" << arg.write_iterations << ") ";
//        std::cout << " wm:" << arg.write_mods << " trywm:" << arg.try_write_mods << std::endl;
        vr += arg.read_iterations;
        vw += arg.write_iterations;
        vwm += arg.write_mods;
        vtwm += arg.try_write_mods;
    }
    int sz = reader_writer_args.size();
    std::cout << (vr/sz) << ", " << (vw/sz) << ", " << (vwm/sz) << ", " << (vtwm/sz) << std::endl;
}


void rwlock_test2()
{
    std::cout << "Read Write Lock Test." << std::endl;
    std::vector<rw_args>reader_args;
    std::vector<std::thread>readers;
    std::vector<rw_args>writer_args;
    std::vector<std::thread>writers;

    for(auto i=0; i < 90; i++)
    {
        reader_args.emplace_back(rw_args());
    }
    for(auto &arg : reader_args)
    {
        readers.emplace_back(std::thread(reader, std::ref(arg)));
    }

    for(auto i=0; i < 4; i++)
    {
        writer_args.emplace_back(rw_args());
    }
    writer_args[1].inc = false;
    writer_args[3].inc = false;
    for(auto &arg : writer_args)
    {
        writers.emplace_back(std::thread(writer, std::ref(arg)));
    }
    std::this_thread::sleep_for(10s);
    for(auto &arg : reader_args)
        arg.run = false;
    for(auto &arg : writer_args)
        arg.run = false;
    for(auto &th : writers)
        th.join();
    for(auto &th : readers)
        th.join();

    std::cout << "Concurrently accessed/modified vector size :" << gv.size() << std::endl;
    std::cout << "Reader iterations:" << std::endl;
    double v;
    v=0;
    for(auto &arg : reader_args)
    {
//        std::cout << arg.read_iterations << " ";
        v += arg.read_iterations;
    }
    std::cout << (v/reader_args.size()) << std::endl;
    std::cout << "Writer iterations:" << std::endl;
    v=0;
    for(auto &arg : writer_args)
    {
//        std::cout << arg.write_iterations << " ";
        v += arg.write_iterations;
    }
    std::cout << (v/writer_args.size()) << std::endl;
}


void rwlock_mw_test2()
{
    std::cout << "Read Write Lock Test (all writes a read locks modified for writes)." << std::endl;
    std::vector<rw_args>reader_args;
    std::vector<std::thread>readers;
    std::vector<rw_args>writer_args;
    std::vector<std::thread>writers;

    for(auto i=0; i < 90; i++)
    {
        reader_args.emplace_back(rw_args());
    }
    for(auto &arg : reader_args)
    {
        readers.emplace_back(std::thread(reader, std::ref(arg)));
    }

    for(auto i=0; i < 4; i++)
    {
        writer_args.emplace_back(rw_args());
    }
    writer_args[1].inc = false;
    writer_args[3].inc = false;
    for(auto &arg : writer_args)
    {
        writers.emplace_back(std::thread(m_writer, std::ref(arg)));
    }
    std::this_thread::sleep_for(10s);
    for(auto &arg : reader_args)
        arg.run = false;
    for(auto &arg : writer_args)
        arg.run = false;
    for(auto &th : writers)
        th.join();
    for(auto &th : readers)
        th.join();

    std::cout << "Concurrently accessed/modified vector size :" << gv.size() << std::endl;
    std::cout << "Reader iterations:" << std::endl;
    double v;
    v=0;
    for(auto &arg : reader_args)
    {
//        std::cout << arg.read_iterations << " ";
        v += arg.read_iterations;
    }
    std::cout << (v/reader_args.size()) << std::endl;
    std::cout << "Writer iterations:" << std::endl;
    v=0;
    for(auto &arg : writer_args)
    {
//        std::cout << arg.write_iterations << " ";
        v += arg.write_iterations;
    }
    std::cout << (v/writer_args.size()) << std::endl;
}
