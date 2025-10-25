#include <atomic>
#include <cassert>
#include <cstddef>
#include <thread>
#include <chrono>

#include "../include/CentralCache.h"
#include "../include/PageCache.h"

namespace Pool
{

const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};

static const size_t SPAN_PAGES = 8;

// initial
CentralCache::CentralCache() {
    for (auto& ptr : centralFreeList_) {
        ptr.store(nullptr, std::memory_order_relaxed);
    }
    for (auto& lock : locks_) {
        lock.clear();
    }
    for (auto& count : delayCounts_) {
        count.store(0, std::memory_order_relaxed);
    }
    for (auto& time : lastReturnTime_) {
        time = std::chrono::steady_clock::now();
    }
    spanCount_.store(0, std::memory_order_relaxed);
}


// 从中心缓存获取内存块 传入 index 查找 list 中是否有空闲
// 如果没有那么进入 页缓存 申请
void* CentralCache::fetchRange(size_t index) {
    if (index >= FREE_LIST_SIZE) {
        return nullptr;
    }

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    void* result = nullptr;
    try {
        // 尝试从 centralFreeList 获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);

        if (!result) {
            size_t size = (index + 1) * ALIGNMENT;
            // 从 PageCache 中获取内存块
            result = fetchFromPageCache(size);

            // 失败
            if (!result) {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            char* start = static_cast<char*>(result);
            // 计算分配页数
            // 如果大于 最小的标准 即 SPAN_PAGES * PageCache::PAGE_SIZE
            // 那么将 size / PAGE_SIZE 向上取整
            size_t numPages = (size <= SPAN_PAGES * PageCache::PAGE_SIZE) ? 
                                SPAN_PAGES : (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

            size_t blockNum = (numPages * PageCache::PAGE_SIZE) / size;

            if (blockNum > 1) {
                // 构建链表
                for (size_t i = 1; i < blockNum; ++i) {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                // 链表末尾
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                void* next = *reinterpret_cast<void**>(result);
                *reinterpret_cast<void**>(result) = nullptr;

                // 更新中心缓存 释放锁
                centralFreeList_[index].store(
                    next,
                    std::memory_order_release
                );

                size_t trackerIndex = spanCount_++;
                if (trackerIndex < spanTrackers_.size()) {
                    spanTrackers_[trackerIndex].spanAddr.store(start, std::memory_order_release);
                    spanTrackers_[trackerIndex].numPages.store(numPages, std::memory_order_release);
                    spanTrackers_[trackerIndex].blockCount.store(blockNum, std::memory_order_release);
                    spanTrackers_[trackerIndex].freeCount.store(blockNum - 1, std::memory_order_release);
                }
            }
        } else {
            void* next = *reinterpret_cast<void**>(result);
            *reinterpret_cast<void**>(result) = nullptr;

            centralFreeList_[index].store(next, std::memory_order_release);

            SpanTracker* tracker = getSpanTracker(result);
            if (tracker) {
                tracker->freeCount.fetch_sub(1, std::memory_order_release);
            }
        }
    } catch (...) {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
    return result;
}

// 接受从 threadCache 中归还的内存块
void CentralCache::returnRange(void* start, size_t size, size_t index) {
    if (!start || index >= FREE_LIST_SIZE) return;

    size_t blockSize = (index + 1) * ALIGNMENT;
    size_t blockCount = size / blockSize;

    while (locks_[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    try {
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        // 头插法
        void* current = centralFreeList_[index].load(std::memory_order_relaxed); // 这里只是读取 所以用 relaxed 
        *reinterpret_cast<void**>(end) = current;
        centralFreeList_[index].store(start, std::memory_order_release); // 这里是写操作 在写操作之后 保证数据都是最新的

        size_t currentCount = delayCounts_[index].fetch_add(1, std::memory_order_relaxed) + 1;
        auto currentTime = std::chrono::steady_clock::now();

        if (shouldPerformDelayedReturn(index, currentCount, currentTime)) {
            performDelayReturn(index);
        }
    } catch(...) {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

// 判断是否应该延迟退还
bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCount, 
        std::chrono::steady_clock::time_point currentTime) {
    if (currentCount >= MAX_DELAY_COUNT) {
        return true;
    }

    auto lastTime = lastReturnTime_[index];
    return (currentTime - lastTime) >= DELAY_INTERVAL;
}

// 执行延迟归还
void CentralCache::performDelayReturn(size_t index) {
    delayCounts_[index].store(0, std::memory_order_relaxed);
    lastReturnTime_[index] = std::chrono::steady_clock::now();

    // 统计每一个 span 中 freeBlock 块数
    std::unordered_map<SpanTracker*, size_t> spanFreeCounts;
    void* currentBlock = centralFreeList_[index].load(std::memory_order_relaxed);

    while (currentBlock) {
        SpanTracker* tracker = getSpanTracker(currentBlock);
        if (tracker) {
            ++spanFreeCounts[tracker];
        }
        currentBlock = *reinterpret_cast<void**>(currentBlock);
    }

    for (const auto& [tracker, newFreeBlocks] : spanFreeCounts) {
        updateSpanFreeCount(tracker, newFreeBlocks, index);
    }
}

// 更新 span 的空闲块数
void CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index) {
    size_t oldFreeCount = tracker->freeCount.load(std::memory_order_relaxed);
    size_t newFreeCount = oldFreeCount + newFreeBlocks;

    tracker->freeCount.store(newFreeCount, std::memory_order_release);

    if (newFreeCount == tracker->blockCount.load(std::memory_order_relaxed)) {
        void* spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
        size_t numPages = tracker->numPages.load(std::memory_order_relaxed);

        void* head = centralFreeList_[index].load(std::memory_order_relaxed);
        void* newHead = nullptr;
        void* prev = nullptr;
        void* current = head;

        while (current) {
            void* next = *reinterpret_cast<void**>(current);
            if (current >= spanAddr && 
                current < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE) {
                if (prev) {
                    *reinterpret_cast<void**>(prev) = next;
                } else {
                    newHead = next;
                }
            } else {
                prev = current;
            }
            current = next;
        }

        centralFreeList_[index].store(newHead, std::memory_order_release);
        PageCache::getInstance().deallocateSpan(spanAddr, numPages);
    }
}

// 从页缓存中攫取 Cache
void* CentralCache::fetchFromPageCache(size_t size) {
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    size_t pagesToAlloc = std::max(numPages, PageCache::PAGE_SIZE);

    return PageCache::getInstance().allocateSpan(pagesToAlloc);
}


// 根据 block 返回他所在的 span
SpanTracker* CentralCache::getSpanTracker(void* blockAddr) {
    for (size_t i = 0; i < spanCount_.load(std::memory_order_relaxed); ++i) {
        void* spanAddr = spanTrackers_[i].spanAddr.load(std::memory_order_relaxed);
        size_t numPages = spanTrackers_[i].numPages;

        // 通过起始地址 和 span总的内存大小算出区间 
        // 然后判断 block 的地址是否在这个区间之内
        if (blockAddr >= spanAddr &&
            blockAddr < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE) {
                return &spanTrackers_[i];
            }
    }
    return nullptr;
}

} // namespace Pool