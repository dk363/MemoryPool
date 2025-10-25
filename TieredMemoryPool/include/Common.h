#pragma once

#include <cstddef>
#include <atomic>
#include <array>

namespace Pool
{
constexpr size_t ALIGNMENT = 8; // 对齐数 可分配的最小缓存
constexpr size_t MAX_BYTES = 256 * 1024; // 一个块中的总容量
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // list 大小

// 内存块头部信息 
struct BlockHeader {
    BlockHeader* next;
};

class SizeClass {
public:
    // 将给定的 bytes 向上取整
    static size_t roundUp(size_t bytes) {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    // 分配内存块时计算索引
    static size_t getIndex(size_t bytes) {
        bytes = std::max(bytes, ALIGNMENT);
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }

    // 给定 size 返回对应的块的大小
    static size_t SizeForIndex(size_t size) {
        return roundUp(size);
    }
};

} // namespace Pool