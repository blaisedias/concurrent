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
#include <clocale>
#include <iostream>
#include <vector>
#include <chrono>
#include <climits>
#include <thread>
#include <mutex>

#include <assert.h>
#include "bdlock.h"

using benedias::fu_lock;
using std::string;
using namespace std::chrono_literals;

static std::vector<int> gv={1,2,3,4,5};
static fu_lock          gv_lock;

struct lck_args {
    bool run = true;
    unsigned int read_iterations = 0;
    unsigned int write_iterations = 0;
    unsigned int iterations = 0;
    unsigned int try_write_mods = 0;
    unsigned int write_mods = 0;
    unsigned int max_iterations = INT_MAX;
    bool inc = true;
    lck_args(){}
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


static void reader(lck_args& args)
{
    std::this_thread::sleep_for(1s);
    while(args.run && (++args.read_iterations < args.max_iterations))
    {
        {
            std::lock_guard<fu_lock> lg(gv_lock);
            args.read_test();
        }
        std::this_thread::yield();
    }
    args.run = false;
}

static void writer(lck_args& args)
{
    while(args.run && (++args.write_iterations < args.max_iterations))
    {
        {
            std::lock_guard<fu_lock> lg(gv_lock);
            args.write_test();
        }
        std::this_thread::sleep_for(1000us);
    }
    args.run = false;
}

void lock_test()
{
    std::cout << "Lock Test." << std::endl;
    std::vector<lck_args>reader_args;
    std::vector<std::thread>readers;
    std::vector<lck_args>writer_args;
    std::vector<std::thread>writers;

    for(auto i=0; i < 90; i++)
    {
        reader_args.emplace_back(lck_args());
    }
    for(auto &arg : reader_args)
    {
        readers.emplace_back(std::thread(reader, std::ref(arg)));
    }

    for(auto i=0; i < 4; i++)
    {
        writer_args.emplace_back(lck_args());
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
    double v=0;
    for(auto &arg : reader_args)
    {
//        std::cout << arg.read_iterations << " ";
        v += arg.read_iterations;
    }
    std::cout << (v/reader_args.size()) << std::endl;
    std::cout << "Writer iterations:" << std::endl;
    v = 0;
    for(auto &arg : writer_args)
    {
//        std::cout << arg.write_iterations << " ";
        v += arg.write_iterations;
    }
    std::cout << (v/writer_args.size()) << std::endl;
}

