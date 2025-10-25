#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace Pool
{

void* ThreadCache::allocate(size_t size) {
    // 
    #ifdef DEBUG_MODE
        throw std::invalid_argument("ThreadCache::allocate(): size cannot be 0");
    #else
        size = ALIGNMENT;
    #endif

    if (size > MAX_BYTES) {
        malloc(size)
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

// TODO: 实现动态标准
bool ThreadCache::shouldReturnToCentralCache(size_t index) {
    size_t threshold = 256;
    return (freeListSize_[index] > threshold);
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

    // TODO: batchNum 可以改进 再 fetchRange 时可以直接告知
    // freeListSize_ 是记录总的申请的块的数量
    freeListSize_[index] += batchNum;

    return result;
}   

// 将内存块还给 CentralCache
void ThreadCache::returnToCentralCache(void* start, size_t size) {
    size_t index = SizeClass::getIndex(size);

    size_t alignedSize = SizeClass::roundUp(size);

    size_t batchNum = freeListSize_[index];
    
    // 如果只有一个块 则不归还
    if (batchNum <= 1) return; 

    // 保留 1 / 4 
    size_t keepNum = std::max(batchNum / 4, size_t(1));
    size_t returnNum = batchNum - keepNum; // returnNum >= 1

    char* current = static_cast<char*>(start);
    // 要保留的最后一个节点
    char* splitNode = current;
    for (size_t i = 0; i < keepNum - 1; ++i) {
        splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode));
        
        // 提前结束
        // 如果注释掉这个函数 有概率发生段错误
        // TODO: 这里为什么注释掉会有段错误
        if (splitNode == nullptr) {
            returnNum = batchNum - (i + 1);
            break;
        }
    }

    if (splitNode != nullptr) {
        void* nextNode = *reinterpret_cast<void**>(splitNode);
        *reinterpret_cast<void**>(splitNode) = nullptr;

        freeList_[index] = start;
        freeListSize_[index] = keepNum;

        if (returnNum > 0 && nextNode != nullptr) {
            CentralCache::getInstance().returnRange(void* nextNode, returnNum * alignedSize, index);
        }
    }
}
// TODO: 统计更改前后的用时 sh 脚本

}