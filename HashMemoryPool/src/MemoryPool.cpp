#include "MemoryPool.h"

namespace Pool
{

// MemoryPool 成员函数实现
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize)
    , SlotSize_(0)
    , blockListHead_(nullptr)
    , currentBlockEnd_(nullptr)
    , nextAvailableSlot_(nullptr)
    , freeListHead_(nullptr)
{}


// 析构函数 遍历 删除指针
MemoryPool::~MemoryPool() {
    Slot* cur = blockListHead_;
    while (cur) {
        Slot* next = cur->next;
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

// 初始化 Slot 的 size 为后面的 hashbucket 做准备
void MemoryPool::init(size_t slotSize) {
    assert(slotSize > 0);
    SlotSize_ = slotSize;
}

// 在块中分配内存
void* MemoryPool::allocate() {
    if (freeListHead_ != nullptr) {
        std::lock_guard<std::mutex> lock(mutexForFreeList_);

        Slot* result = reinterpret_cast<Slot*>(freeListHead_);
        freeListHead_ = freeListHead_->next;
        return result;
    } else {
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        
        if (nextAvailableSlot_ >= currentBlockEnd_) {
            allocateBlock();
        }
        Slot* result = nextAvailableSlot_;
        // 更新下一个 可用的内存槽
        nextAvailableSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<char*>(nextAvailableSlot_) + SlotSize_);
        return result; // return 一个可用的内存槽地址
    }
}

// 释放块中的一块内存
void MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) return;

    std::lock_guard<std::mutex> lock(mutexForFreeList_);

    reinterpret_cast<Slot*>(ptr)->next = freeListHead_;
    freeListHead_ = reinterpret_cast<Slot*>(ptr);
}


// 为内存池分配一个新的内存块
void MemoryPool::allocateBlock() {
    // 1. 分配一块大小为 BlockSize_ 的原始内存（不调用构造函数）
    // 强转为 char* 方便后续按字节偏移计算
    char* newBlock = reinterpret_cast<char*>(operator new(BlockSize_));

    // 2. 将新块加入内存块链表头部
    reinterpret_cast<Slot*>(newBlock)->next = blockListHead_;
    // 再将链表头更新为新块，完成新块的插入
    blockListHead_ = reinterpret_cast<Slot*>(newBlock);

    // 3. 计算内存块中实际可用区域的起始位置
    // 内存块布局：[块链表指针（Slot*）][实际存储槽位的区域]
    // 跳过块链表指针占用的空间（sizeof(Slot*)），得到可用区域起始地址
    char* body = newBlock + sizeof(Slot*);

    // 4. 计算可用区域的对齐填充字节数
    // 确保可用区域的起始地址满足 Slot 类型的对齐要求（alignof(Slot)）
    // padPointer 返回需要填充的字节数，使 body + bodyPadding 地址对齐
    size_t bodyPadding = padPointer(body, alignof(Slot));

    // 5. 设置下一个可分配槽位的地址
    // 将对齐后的地址强转为 Slot*，作为当前块的首个可用槽位
    nextAvailableSlot_ = reinterpret_cast<Slot*>(body + bodyPadding);

    // 6. 计算当前块的可用区域结束位置
    // 公式含义：新块总大小 - 最后一个槽位的大小 + 1（指向最后一个槽位的下一位）
    currentBlockEnd_ = reinterpret_cast<Slot*>(newBlock + BlockSize_ - SlotSize_ + 1);
}

// 计算从地址 p 开始按 align 对齐所需要的填充字节数（返回相对于 p 的偏移）
size_t MemoryPool::padPointer(char* p, size_t align) {
    uintptr_t result = reinterpret_cast<uintptr_t>(p);
    return (align - result) % align;
}

// HashBucket 静态成员定义和实现
MemoryPool HashBucket::pools_[MEMORY_POOL_NUM];  // 定义静态成员

void HashBucket::initMemoryPool() {
    for (int i = 0; i < MEMORY_POOL_NUM; ++i) {
        // 这里最多到 512 字节 如果超过 512 字节就直接用 new/malloc 这些系统调用
        // 内存池解决的是小内存带来的内存碎片问题
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

MemoryPool& HashBucket::getMemoryPool(int index) {
    // 注意：这里应该返回 pools_[index]，而不是新建一个
    return pools_[index];
}


void* HashBucket::useMemory(size_t size) {
    if (size <= 0) return nullptr;
    if (size > MAX_SLOT_SIZE) return operator new(size);

    // 向上取整
    return getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).allocate();
}

void HashBucket::freeMemory(void* ptr, size_t size) {
    if (ptr == nullptr) return;
    if (size > MAX_SLOT_SIZE) {
        operator delete(ptr);
        return;
    }
    getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).deallocate(ptr);
}

} // namespace Pool