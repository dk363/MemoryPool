#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

#include <cstddef>
#include <cstdlib>

namespace Pool
{

void* ThreadCache::allocate(size_t size) {
    // 
    #ifdef DEBUG_MODE
        throw std::invalid_argument("ThreadCache::allocate(): size cannot be 0");
    #else
        size = ALIGNMENT;
    #endif

    if (size == 0) {
        size = ALIGNMENT;
    }

    if (size > MAX_BYTES) {
        malloc(size);
    }

    size_t index = SizeClass::getIndex(size);

    --freeListSize_[index];

    if (void* ptr = freeList_[index]) {

        freeList_[index] = *reinterpret_cast<void**>(ptr);
        return ptr;
    }

    return fetchFromCentralCache(index);
}

// ptr --> address
// 释放指定地点 指定大小的内存
void ThreadCache::deallocate(void* ptr, size_t size) {
    if (size > MAX_BYTES) {
        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);

    // 头插法
    // 将 ptr 变成一个指向指针的指针 
    // 解引用 ptr 也就是 ptr 指针指向 list 的头部
    // 然后更新 list 的头部 头部写入 ptr 的地址
    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;

    ++freeListSize_[index];

    // 是否需要将这一个内存块回收
    if (shouldReturnToCentralCache(index)) {
        returnToCentralCache(freeList_[index], size);
    }
}

// 这里动态标准的效果似乎并不好
// 判断是否需要将内存回收给中心缓存
// bool ThreadCache::shouldReturnToCentralCache(size_t index)
// {
//     // 基准阈值（可调）
//     constexpr size_t baseThreshold = 64;

//     // 动态阈值：base + recent_access / factor
//     // recent_access 越大，阈值越高（表示该 size-class 更热，应保留更多）
//     size_t dynamicThreshold = baseThreshold + (allocCount_[index] / 64);

//     // 为避免过大，可以加个上限（可选）
//     constexpr size_t maxThreshold = 1024;
//     if (dynamicThreshold > maxThreshold) dynamicThreshold = maxThreshold;

//     return (freeListSize_[index] > dynamicThreshold);
// }
// 我使用了 struct 为 list 记录了 初始化计数
// 最大的 freelist 大小
// 但是在实际应用中 小块的缓存list 应该更大一些
bool ThreadCache::shouldReturnToCentralCache(size_t index) {
    size_t maxListSize = 256;
    return (freeListSize_[index] > maxListSize);
}

// 从中心缓存获取内存
void* ThreadCache::fetchFromCentralCache(size_t index) {
    // 从中心缓存获取内存块 传入 index 查找 list 中是否有空闲
    void* start = CentralCache::getInstance().fetchRange(index);

    // 再上层封装的时候 注意可以捕捉 nullptr 然后停止程序
    if (!start) return nullptr;

    // 取一个返回 剩余的放入 freelist
    void* result = start;
    freeList_[index] = *reinterpret_cast<void**>(start);

    // 计算
    size_t batchNum = 0;
    void* current = start;

    while (current != nullptr) {
        ++batchNum;
        current = *reinterpret_cast<void**>(current);
    }

    // TODO: batchNum 可以改进 在 fetchRange 时可以直接告知
    // freeListSize_ 是记录总的申请的块的数量
    freeListSize_[index] += batchNum;

    return result;
}   

// 将内存块还给 CentralCache
void ThreadCache::returnToCentralCache(void* start, size_t size) {
    if (start == nullptr) return;

    size_t index = SizeClass::getIndex(size);

    // 对齐后的实际块大小
    size_t alignedSize = SizeClass::roundUp(size);

    size_t actualCount = 0;
    void* cur = start;
    while (cur != nullptr) {
        ++actualCount;
        cur = *reinterpret_cast<void**>(cur);
    }

    size_t batchNum = actualCount;
    
    // 如果只有一个块 则不归还
    if (batchNum <= 1) return; 

    // 保留 1 / 4 
    size_t keepNum = std::max(batchNum / 4, size_t(1));

    // 将内存块串成 list 并找到分割点
    void* splitNode = start;
    for (size_t i = 0; i < keepNum - 1; ++i) {
        // 这里是不应该 return 的
        if (splitNode == nullptr) {
            return;
        }
        splitNode = *reinterpret_cast<void**>(splitNode);
    }

    if (splitNode == nullptr) {
        return;
    }

    void* nextNode = *reinterpret_cast<void**>(splitNode);
    // 断开连接
    *reinterpret_cast<void**>(splitNode) = nullptr;
    // 头插法
    freeList_[index] = start;
    freeListSize_[index] = keepNum;

    size_t returnNum = batchNum - keepNum;
    if (returnNum > 0 && nextNode != nullptr) {
        CentralCache::getInstance().returnRange(nextNode, returnNum * alignedSize, index);
    }
}

} // namespace Pool