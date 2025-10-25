#pragma once

#include "Common.h"

#include <mutex>
#include <unordered_map>
#include <array>
#include <atomic>
#include <chrono>

namespace Pool
{

// atomic 原子操作
struct SpanTracker {
    std::atomic<void*> spanAddr{nullptr};
    std::atomic<size_t> numPages{0};
    std::atomic<size_t> blockCount{0};
    std::atomic<size_t> freeCount{0}; 
};

class CentralCache {
public:
    static CentralCache& getInstance() {
        static CentralCache instance;
        return instance;
    }

    void* fetchRange(size_t index);
    void returnRange(void* start, size_t size, size_t index);

private:
    CentralCache();

    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

    // getter
    SpanTracker* getSpanTracker(void* blockAddr);

    void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

    // 判断是否应该归还 
    // 两个情况
    // 已经触发了 应该归还的 次数
    // 上一次真正归还的时间
    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, 
        std::chrono::steady_clock::time_point currentTime);

    // 归还给 pageCache
    void performDelayReturn(size_t index);

private:
    std::array<std::atomic<void*>, FREE_LIST_SIZE>                      centralFreeList_;

    // 每一个 list 都有属于自己的锁 如果只用一个锁负责全部的list 在多线程实现中竞态严重
    std::array<std::atomic_flag, FREE_LIST_SIZE>                        locks_;

    // 使用数组存储信息
    std::array<SpanTracker, 1024>                                       spanTrackers_;
    // spanCount 的数量
    std::atomic<size_t>                                                 spanCount_;

    // 延迟归还
    static const size_t                                                 MAX_DELAY_COUNT = 48; // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE>                     delayCounts_; // 每个大小类的延迟计数 已经触发了多少次应该归还的情况
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE>   lastReturnTime_; // 上一次真正归还的时间点
    static const std::chrono::milliseconds                              DELAY_INTERVAL; // 延迟间隔

};

} // namespace Pool