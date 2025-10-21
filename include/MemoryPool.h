#pragma once

#include <atomic>
#include <utility>
#include <mutex>    
#include <cstdint>  
#include <cassert>

namespace Pool 
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

// 不将 pool 作为模板类 因为最下面的两个函数才是作为对外的接口的
// 将最下面的两个函数作为模板即可
class MemoryPool
{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();

    void init(size_t slotSize);

    void* allocate();
    void deallocate(void*);

private:
    void allocateBlock();
    // 对齐
    size_t padPointer(char* p, size_t align);
    
private:
    union Slot
    {
        std::atomic<Slot*> next;
    };

    int                     BlockSize_; // 一整个内存块的大小
    int                     SlotSize_; // 一个插槽的大小

    Slot*                   blockListHead_;      // 最近分配的内存块链表头
    Slot*                   currentBlockEnd_;    // 当前块中最后一个可用 slot 的下一个位置 类似于 iterator 中的 end()
    Slot*                   nextAvailableSlot_;  // 当前块中下一个可分配的 slot
    Slot*                   freeListHead_;       // 空闲链表头，回收的 slot
    
    std::mutex              mutexForFreeList_;
    std::mutex              mutexForBlock_;
};

class HashBucket
{
public:
    static void initMemoryPool();
    // 从 Bucket 中取对应大小的 pool
    static MemoryPool& getMemoryPool(int index);

    // 分配内存
    // 只需要大小
    // 在 pool 内单独保存了 nextAvailableSlot_
    static void* useMemory(size_t size);

    // 释放内存
    // ptr 地址
    // size 大小
    static void freeMemory(void* ptr, size_t size);
private:
    static MemoryPool pools_[MEMORY_POOL_NUM];

    // 通过声明为模板友元函数 兼顾模板T和对类内私有成员的访问权限
    template<typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template<typename T>
    friend void deleteElement(T* p);
};

// 这里的
template<typename T, typename... Args>
T* newElement(Args&&... args) {
    T* p = nullptr;
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
        // 常用于完美转发（perfect forwarding）中。
        // 它的主要作用是保证参数按照调用站点的形式进行转发，
        // 不引入额外的类型转换。
        // 这对于实现函数模板尤其有用，
        // 可以确保接收泛型参数的函数能够正确地将这些参数传递给其他函数，
        // 而不会因为类型导致参数的丢失或误用。
        new(p) T(std::forward<Args>(args)...);
    }
    return p;
}

template<typename T>
void deleteElement(T* p) {
    if (p) {
        p->~T();
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

} // namespace Pool