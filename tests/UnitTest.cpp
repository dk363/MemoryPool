#include <iostream>
#include <thread>
#include <vector>

#include "../include/MemoryPool.h"


class P1 {
    int id_;
};

class P2 {
    int id_[5];
};

class P3 {
    int id_[10];
};

class P4 {
    int id_[20];
};


void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t nrounds) {
    std::vector<std::thread> vthread(nworks);

    size_t total_costtime = 0;
    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&]() {
            for (size_t j = 0; j < nrounds; ++j) {
                size_t start = clock();
                for (size_t i = 0; i < ntimes; ++i) {
                    P1* p1 = Pool::newElement<P1>(); // 内存池对外接口
                    Pool::deleteElement<P1>(p1);
                    P2* p2 = Pool::newElement<P2>();
                    Pool::deleteElement<P2>(p2);
                    P3* p3 = Pool::newElement<P3>();
                    Pool::deleteElement<P3>(p3);
                    P4* p4 = Pool::newElement<P4>();
                    Pool::deleteElement<P4>(p4);
                }

                size_t end = clock();

                total_costtime += end - start;
            }
        });
    
    }
    // Awaiting the completion of all worker threads.
    for (auto& t : vthread) {
        t.join();
    }

    printf("%lu threads concurrently execute %lu rounds, each round performs newElement & Pool::deleteElement %lu times, total time spent: %lu ms\n", 
        nworks, nrounds, ntimes, total_costtime);
    
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t nrounds) {
    std::vector<std::thread> vthread(nworks);
    size_t total_costtime = 0;
    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&]() {
            for (size_t j = 0; j < nrounds; ++j) {
                size_t start = clock();
                for (size_t i = 0; i < ntimes; ++i) {
                    P1* p1 = new P1;
                    delete p1;
                    P2* p2 = new P2;
                    delete p2;
                    P3* p3 = new P3;
                    delete p3;
                    P4* p4 = new P4;
                    delete p4;
                }
                size_t end = clock();

                total_costtime += end - start;
            }
        });
    }

    for (auto& t : vthread) {
        t.join();
    }

    printf("%lu threads concurrently execute %lu rounds, each round performs newElement & Pool::deleteElement %lu times, total time spent: %lu ms\n", 
        nworks, nrounds, ntimes, total_costtime);
    
}

int main() {
    Pool::HashBucket::initMemoryPool();

    BenchmarkMemoryPool(100, 100, 100);
    std::cout << "==" << std::endl;
    BenchmarkNew(100, 100, 100);
    
    return 0;
}