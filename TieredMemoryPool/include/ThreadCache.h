#pragma once

#include "Common.h"

namespace Pool 
{

class ThreadCache {
public:
// 这表明 instance 是线程局部存储的，意味着每个线程都会有自己的 ThreadCache 实例。
// 通过 static 关键字修饰成员函数 getInstance，确保每次调用该函数时只会初始化一次 instance。
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance;
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);

private:
    ThreadCache() {
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }

    // 从中心缓存获取内存
    void* fetchFromCentralCache(size_t index);
    // 归还内存到中心缓存
    void returnToCentralCache(void* start, size_t size);

    bool shouldReturnToCentralCache(size_t index);

private:
    // 用数组实现 自由链表
    // 相同大小的缓存放在一个块中 
    // 再用一个数组保存 块的大小 和已经放了多少个
    std::array<void*, FREE_LIST_SIZE>   freeList_;
    std::array<size_t, FREE_LIST_SIZE>  freeListSize_;
};

} // namespace Pool