#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <iomanip>
#include "MemoryPool.h"

// 测试用的数据结构
struct SmallObject {
    int data[4];  // 16 bytes
    SmallObject(int a, int b, int c, int d) {
        data[0] = a; data[1] = b; data[2] = c; data[3] = d;
    }
};

struct MediumObject {
    char data[128];  // 128 bytes
    MediumObject() {
        for (int i = 0; i < 128; i++) data[i] = i % 256;
    }
};

struct LargeObject {
    char data[300];  // 300 bytes
    LargeObject() {
        for (int i = 0; i < 300; i++) data[i] = i % 256;
    }
};

class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    long elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
    }
    
    long elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }
};

// 大规模单线程性能测试
void large_scale_single_thread_test() {
    std::cout << "=== 大规模单线程性能测试 ===" << std::endl;
    
    const size_t ntimes = 1000000;  // 100万次
    
    // 系统分配器测试
    std::cout << "系统分配器测试..." << std::endl;
    Timer timer;
    for (size_t i = 0; i < ntimes; i++) {
        SmallObject* p = new SmallObject(1, 2, 3, 4);
        delete p;
    }
    long system_time = timer.elapsed_ms();
    
    // 内存池测试
    std::cout << "内存池测试..." << std::endl;
    timer = Timer();
    for (size_t i = 0; i < ntimes; i++) {
        SmallObject* p = Pool::newElement<SmallObject>(1, 2, 3, 4);
        Pool::deleteElement(p);
    }
    long pool_time = timer.elapsed_ms();
    
    std::cout << "系统分配器: " << system_time << " ms" << std::endl;
    std::cout << "内存池: " << pool_time << " ms" << std::endl;
    
    if (system_time > 0 && pool_time > 0) {
        double improvement = (system_time - pool_time) * 100.0 / system_time;
        std::cout << "性能变化: " << std::fixed << std::setprecision(2) << improvement << "%" << std::endl;
        if (improvement > 0) {
            std::cout << "内存池更快!" << std::endl;
        } else {
            std::cout << "系统分配器更快!" << std::endl;
        }
    }
    std::cout << std::endl;
}

// 多线程压力测试
void multithread_stress_test() {
    std::cout << "=== 多线程压力测试 ===" << std::endl;
    
    const size_t nthreads = 4;
    const size_t nrounds = 1000;
    const size_t ntimes = 1000;  // 总共 4百万次操作
    
    std::cout << nthreads << " 线程 × " << nrounds << " 轮 × " << ntimes << " 次 = " 
              << nthreads * nrounds * ntimes << " 次分配释放操作" << std::endl;
    
    std::atomic<size_t> completed_rounds{0};
    std::vector<std::thread> threads;
    
    Timer timer;
    
    auto worker = [&completed_rounds, nrounds, ntimes](int thread_id) {
        for (size_t round = 0; round < nrounds; round++) {
            std::vector<SmallObject*> objects;
            objects.reserve(ntimes);
            
            // 分配阶段
            for (size_t i = 0; i < ntimes; i++) {
                objects.push_back(Pool::newElement<SmallObject>(thread_id, round, i, 0));
            }
            
            // 释放阶段
            for (size_t i = 0; i < ntimes; i++) {
                Pool::deleteElement(objects[i]);
            }
            
            completed_rounds.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    // 创建线程
    for (size_t i = 0; i < nthreads; i++) {
        threads.emplace_back(worker, i);
    }
    
    // 显示进度
    auto progress_monitor = [&completed_rounds, nthreads, nrounds]() {
        while (completed_rounds < nthreads * nrounds) {
            size_t current = completed_rounds.load();
            double progress = (current * 100.0) / (nthreads * nrounds);
            std::cout << "\r进度: " << std::fixed << std::setprecision(1) << progress << "% (" 
                      << current << "/" << nthreads * nrounds << ")" << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << std::endl;
    };
    
    std::thread monitor(progress_monitor);
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    monitor.join();
    
    long total_time = timer.elapsed_ms();
    size_t total_operations = nthreads * nrounds * ntimes * 2; // 分配和释放
    
    std::cout << "总耗时: " << total_time << " ms" << std::endl;
    std::cout << "总操作次数: " << total_operations << " 次" << std::endl;
    std::cout << "吞吐量: " << (total_operations / total_time) << " 次操作/ms" << std::endl;
    std::cout << "平均每次操作耗时: " << (total_time * 1000.0 / total_operations) << " μs" << std::endl;
    std::cout << std::endl;
}

// 不同对象大小性能测试
void different_size_performance_test() {
    std::cout << "=== 不同对象大小性能测试 ===" << std::endl;
    
    const size_t ntimes = 100000;  // 10万次
    
    auto test_size = [ntimes](const std::string& name, auto constructor) {
        std::cout << "测试 " << name << "..." << std::endl;
        Timer timer;
        for (size_t i = 0; i < ntimes; i++) {
            auto p = constructor();
            Pool::deleteElement(p);
        }
        long time = timer.elapsed_ms();
        std::cout << name << ": " << time << " ms" << std::endl;
        return time;
    };
    
    long time_small = test_size("SmallObject (16B)", []() { 
        return Pool::newElement<SmallObject>(1, 2, 3, 4); 
    });
    
    long time_medium = test_size("MediumObject (128B)", []() { 
        return Pool::newElement<MediumObject>(); 
    });
    
    long time_large = test_size("LargeObject (300B)", []() { 
        return Pool::newElement<LargeObject>(); 
    });
    
    std::cout << "性能对比 (基准为SmallObject):" << std::endl;
    std::cout << "MediumObject: " << (time_medium * 100.0 / time_small) << "%" << std::endl;
    std::cout << "LargeObject: " << (time_large * 100.0 / time_small) << "%" << std::endl;
    std::cout << std::endl;
}

// 内存碎片抗性测试
void fragmentation_resistance_test() {
    std::cout << "=== 内存碎片抗性测试 ===" << std::endl;
    
    const size_t initial_allocation = 50000;
    const size_t fragmentation_cycles = 1000;
    
    std::vector<SmallObject*> objects(initial_allocation, nullptr);
    
    Timer timer;
    
    // 初始分配
    std::cout << "初始分配 " << initial_allocation << " 个对象..." << std::endl;
    for (size_t i = 0; i < initial_allocation; i++) {
        objects[i] = Pool::newElement<SmallObject>(i, i, i, i);
    }
    
    // 碎片化操作
    std::cout << "进行 " << fragmentation_cycles << " 轮碎片化操作..." << std::endl;
    for (size_t cycle = 0; cycle < fragmentation_cycles; cycle++) {
        // 释放一部分对象
        for (size_t i = cycle % 5; i < initial_allocation; i += 5) {
            if (objects[i]) {
                Pool::deleteElement(objects[i]);
                objects[i] = nullptr;
            }
        }
        
        // 重新分配
        for (size_t i = cycle % 5; i < initial_allocation; i += 5) {
            if (!objects[i]) {
                objects[i] = Pool::newElement<SmallObject>(i, cycle, 0, 0);
            }
        }
        
        if (cycle % 100 == 0) {
            std::cout << "\r已完成 " << cycle << "/" << fragmentation_cycles << " 轮" << std::flush;
        }
    }
    std::cout << std::endl;
    
    // 清理
    std::cout << "清理内存..." << std::endl;
    for (auto& ptr : objects) {
        if (ptr) Pool::deleteElement(ptr);
    }
    
    long total_time = timer.elapsed_ms();
    std::cout << "碎片测试完成! 总耗时: " << total_time << " ms" << std::endl;
    std::cout << std::endl;
}

// 极端压力测试
void extreme_stress_test() {
    std::cout << "=== 极端压力测试 ===" << std::endl;
    
    const size_t nthreads = 8;
    const size_t ntimes = 50000;  // 每个线程5万次
    
    std::cout << nthreads << " 个线程，每个线程 " << ntimes << " 次快速分配释放" << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<bool> error_occurred{false};
    
    Timer timer;
    
    auto worker = [&error_occurred, ntimes](int thread_id) {
        try {
            for (size_t i = 0; i < ntimes; i++) {
                SmallObject* p = Pool::newElement<SmallObject>(thread_id, i, 0, 0);
                if (p == nullptr) {
                    std::cerr << "线程 " << thread_id << " 分配失败!" << std::endl;
                    error_occurred.store(true);
                    return;
                }
                Pool::deleteElement(p);
            }
        } catch (const std::exception& e) {
            std::cerr << "线程 " << thread_id << " 异常: " << e.what() << std::endl;
            error_occurred.store(true);
        }
    };
    
    for (size_t i = 0; i < nthreads; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    long total_time = timer.elapsed_ms();
    
    if (!error_occurred) {
        std::cout << "极端压力测试通过! 总耗时: " << total_time << " ms" << std::endl;
        std::cout << "吞吐量: " << (nthreads * ntimes * 2 / total_time) << " 次操作/ms" << std::endl;
    } else {
        std::cout << "极端压力测试失败!" << std::endl;
    }
    std::cout << std::endl;
}

void extreme_stress_test_new() {
    std::cout << "=== 极端压力测试 ===" << std::endl;
    
    const size_t nthreads = 8;
    const size_t ntimes = 50000;  // 每个线程5万次
    
    std::cout << nthreads << " 个线程，每个线程 " << ntimes << " 次快速分配释放" << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<bool> error_occurred{false};
    
    Timer timer;
    
    auto worker = [&error_occurred, ntimes](int thread_id) {
        
    }
}

int main() {
    std::cout << "开始完整内存池性能测试..." << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // 初始化内存池
    std::cout << "初始化内存池..." << std::endl;
    Pool::HashBucket::initMemoryPool();
    std::cout << "内存池初始化完成" << std::endl << std::endl;
    
    try {
        large_scale_single_thread_test();
        different_size_performance_test();
        fragmentation_resistance_test();
        multithread_stress_test();
        extreme_stress_test();
        
        std::cout << "==========================================" << std::endl;
        std::cout << "所有性能测试完成!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "未知异常!" << std::endl;
        return 1;
    }
    
    return 0;
}