#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include "Semaphore.hpp"

namespace benedias {

class TestThread
{
    protected:
        std::thread* t1;
    public:
        TestThread() {}
        virtual ~TestThread()
        {
        }
        void join()
        {
            t1->join();
        }
};

class BinarySemaphoreTest: public TestThread
{
    public:
        BinarySemaphore *sem;
        BinarySemaphoreTest(BinarySemaphore *semin):sem(semin)
        {
            t1 = new std::thread(*this);
        }
        void operator()()
        {
            std::cout << "BinarySemaphoreTest: " << ' ' << this << " sleeping 10secs, thread id:" << std::this_thread::get_id() << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
            sem->Post();
        }
};

class SemaphoreTest: public TestThread
{
    public:
        Semaphore *sem;
        SemaphoreTest(Semaphore *semin):sem(semin)
        {
            t1 = new std::thread(*this);
        }
        void operator()()
        {
            std::cout << "SemaphoreTest: " << ' ' << this << " sleeping 10secs, thread id:" << std::this_thread::get_id() << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
            sem->Post();
            sem->Post();
        }
};


void testBinarySemaphore()
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;
    BinarySemaphore s(true);
    start = std::chrono::system_clock::now();
    s.Wait(); puts("Waited on intially signalled binary semaphore");
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    printf("%G\n", elapsed_seconds.count());
    if (elapsed_seconds.count() < 1)
        puts("Passed");
    else
        puts("Failed");

    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    s.Post(); puts("Signalling binary semaphore");
    start = std::chrono::system_clock::now();
    s.Wait(); puts("Waited on multiply signalled binary semaphore");
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    printf("%G\n", elapsed_seconds.count());
    if (elapsed_seconds.count() < 1)
        puts("Passed");
    else
        puts("Failed");

    puts("Waiting again on the same binary semaphore, execution should stop for about 10 secs");
    start = std::chrono::system_clock::now();
    BinarySemaphoreTest bt(&s);
    s.Wait(); puts("Waited on binary semaphore");
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    printf("%G\n", elapsed_seconds.count());
    if (elapsed_seconds.count() >= 10.0)
        puts("Passed");
    else
        puts("Failed");
    puts("Thread Join");
    bt.join();
    }


void testSemaphore()
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;

    Semaphore s(2);
    start = std::chrono::system_clock::now();
    s.Wait(); puts("Waited on intially signalled semaphore");
    s.Wait(); puts("Waited again on intially signalled semaphore");
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    printf("%G\n", elapsed_seconds.count());
    if (elapsed_seconds.count() < 1)
        puts("Passed");
    else
        puts("Failed");

    puts("Waiting again on the semaphore, execution should stop for about 10 secs, semaphore is signalled twice");
    start = std::chrono::system_clock::now();
    SemaphoreTest st(&s);
    s.Wait(); puts("Waited on semaphore, signalled twice");
    s.Wait(); puts("Waited on semaphore");
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    printf("%G\n", elapsed_seconds.count());
    if (elapsed_seconds.count() >= 10.0)
        puts("Passed");
    else
        puts("Failed");
    puts("Thread Join");
    st.join();
}

} //namespace benedias

int main(int argc, char** argv)
{
    puts("Testing Binary Semaphore\n========================");
    benedias::testBinarySemaphore();
    puts("\nTesting Semaphore\n=================");
    benedias::testSemaphore();
    return 0;
}
