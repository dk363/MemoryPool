#include <cstddef>
#include <cstring>
#include <mutex>
#include <sys/mman.h>

#include "../include/PageCache.h"

namespace Pool 
{
void* PageCache::allocateSpan(std::size_t numPages) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end()) {
        Span* span = it->second;

        if (span->next) {
            freeSpans_[it->first] = span->next;
        } else {
            freeSpans_.erase(it);
        }

        // 如果找的的 span 有多余的 那就只分配需要的部分
        if (span->numPages > numPages) {
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + 
                                numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
        }

        spanMap_[span->pageAddr] = span;
        return span->pageAddr;
    }

    void* memory = systemAlloc(numPages);

    if (!memory) return nullptr;

    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    spanMap_[memory] = span;
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return;

    Span* span = it->second;
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);

    if (nextIt != spanMap_.end()) {
        Span* nextSpan = nextIt->second;

        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];
        Span* dummy;
        dummy->next = nextList;
        Span* prev = dummy;

        while (prev->next) {
            if (prev->next == nextSpan) {
                found = true;
                prev->next = nextSpan->next;
            }
            prev = prev->next;
        }

        nextList = dummy->next;

        if (found) {
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
        }
    }
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAlloc(size_t numPages) {
    size_t size = numPages * PAGE_SIZE;

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;

    memset(ptr, 0, size);
    return ptr;
}

} // namespace Pool
