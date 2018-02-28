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
#include <cstdio>
#include <cstring>
#include <clocale>
#include <iostream>
#include <unordered_map>
#include <array>
#include <stdlib.h>
#include <memory>
#include <fstream>
#include <boost/serialization/vector.hpp>
#include <chrono>
#include <thread>
#include <memory>
#include <assert.h>
#include "semaphore.hpp"

using std::string;
using namespace std::chrono_literals;

struct th_args {
    int id = 0;
    benedias::binary_semaphore bs;
    th_args(int v, std::shared_ptr<benedias::binary_semaphore>&spbs):id(v),sp_bs(spbs){}
    std::shared_ptr<benedias::binary_semaphore>sp_bs;
    int stage = 0;
};

static void bsemtest_1(th_args& args)
{
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << std::this_thread::get_id() << " " << args.id << std::endl;
    ++args.stage;   // 1
    std::this_thread::sleep_for(2s);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end-start;
    ++args.stage;   // 2
    std::cout << std::this_thread::get_id() << " " << args.id << " Elapsed " << elapsed.count() << " ms\n";
    args.sp_bs->wait();
    end = std::chrono::high_resolution_clock::now();
    ++args.stage;   // 3
    elapsed = end-start;
    std::cout << std::this_thread::get_id() << " " << args.id << " Elapsed " << elapsed.count() << " ms\n";
    std::cout << std::this_thread::get_id() << " " << args.id << " DONE " << "\n";
}

static void bsemtest_2(th_args& args)
{
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << std::this_thread::get_id() << " " << args.id << std::endl;
    ++args.stage;   // 1
    args.bs.wait();
    auto end = std::chrono::high_resolution_clock::now();
    ++args.stage;   // 2
    std::chrono::duration<double, std::milli> elapsed = end-start;
    std::cout << std::this_thread::get_id() << " " << args.id << " Elapsed " << elapsed.count() << " ms\n";
    args.bs.wait();
    end = std::chrono::high_resolution_clock::now();
    ++args.stage;   // 3
    elapsed = end-start;
    std::cout << std::this_thread::get_id() << " " << args.id << " Elapsed " << elapsed.count() << " ms\n";
    args.sp_bs->wait();
    end = std::chrono::high_resolution_clock::now();
    elapsed = end-start;
    ++args.stage;   // 4
    std::cout << std::this_thread::get_id() << " " << args.id << " Elapsed " << elapsed.count() << " ms\n";
    std::cout << std::this_thread::get_id() << " " << args.id << " DONE " << "\n";
}

void bs_test()
{
    std::cout << "Binary Semaphore Test. " << std::endl;
    std::shared_ptr<benedias::binary_semaphore>spbs = std::make_shared<benedias::binary_semaphore>(false);
    th_args ta1(1, spbs);
    th_args ta2(2, spbs);
    // test:Multiple posts register as single event {
    ta2.bs.post();
    ta2.bs.post();
    ta2.bs.post();
    ta2.bs.post();
    ta2.bs.post();
    std::thread th1(bsemtest_1, std::ref(ta1));
    std::thread th2(bsemtest_2, std::ref(ta2));
    std::this_thread::sleep_for(4s);
    // test:Multiple posts register as single event } ta2 should be at stage 2
    assert(ta2.stage == 2);
    // test:Single post registers as single event {
    ta2.bs.post();
    std::this_thread::sleep_for(1s);
    // test:Single post registers as single event } ta2 should be at stage 3
    assert(ta2.stage == 3);
    // ta1 slept for 1 second should be at stage 2
    assert(ta1.stage == 2);
    spbs->post();
    std::this_thread::sleep_for(1s);
    assert(ta1.stage == 3 || ta2.stage == 4);
    spbs->post();
    std::this_thread::sleep_for(1s);
    assert(ta1.stage == 3 || ta2.stage == 4);
    th1.join();
    th2.join();
}

int main(int argc, char* argv[])
{
    bs_test();
    std::cout << "--------------------" << std::endl;
}
