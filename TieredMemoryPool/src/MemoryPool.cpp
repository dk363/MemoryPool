#pragma once

#include "../include/ThreadCache.h"
#include <cstddef>

namespace Pool 
{

class MemoryPool {
public:
    static void* allocate(std::size_t size) {
        return ThreadCache::getInstance()->allocate(size);
    }

    static void deallocate(void* ptr, size_t size) {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};

} // namespace Pool