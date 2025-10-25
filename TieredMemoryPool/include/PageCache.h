#pragma once

#include <cstddef>
#include <map>
#include <mutex>

#include "Common.h"

namespace Pool 
{
class PageCache {
public:
    // 4Kb 
    static const std::size_t PAGE_SIZE = 4096;

    static PageCache& getInstance() {
        static PageCache instance;
        return instance;
    }

    void* allocateSpan(size_t numPages);

    void deallocateSpan(void* ptr, size_t numPages);

private:
    PageCache() = default;

    void* systemAlloc(size_t numPages);

private:
    struct Span {
        void*   pageAddr;
        size_t  numPages;
        Span*   next;
    };

    // 记录空闲的 span 的地址
    std::map<size_t, Span*> freeSpans_;
    // 记录已经分配的 span 的地址 方便归还
    std::map<void*, Span*>  spanMap_;
    std::mutex              mutex_;
};

} // namespace Pool