ThreeTieredMemoryPool note

这个项目我推荐从 `PageCache` 开始向下看。对外的接口封装在 `MemoryPool.h` 中，通过调用    `threadCache.h` 中的接口实现。缓存层数之间都做了良好的封装


下面是一些语法细节


# std::atomic
保证多线程在读写变量时的操作时不可分割的
在 CSAPP 中学到了汇编代码
对于这一行代码

```cpp
int addOne() {
    ++counter;
    return counter;
}
```

有这样的汇编代码

```
movq    %rdi    %rax
add     $1      %rax
ret
```

会发现这里`++counter`并不是一步完成的
因此当有所个线程执行函数时 会有这种情况：
线程A执行 `addOne()` 时，读取 `counter` 变量的同时，线程B执行 `addOne()` 时，读取 `counter` 变量，然后 `++counter` 将其放入 寄存器返回 然后写入，接着线程 B 也做了同样的操作。但是此时并没有更新 `counter` 的状态，也就是说写入操作之后 `counter` 还是 +1 而不是 +2
通过 `std::atomic` 将这一个操作变成了一个整体 也就是硬件原子级指令 在 CPU 层面保持原子性 避免了出现这种情况

## std::atomic_flag

多线程程序中表示一个简单的二元状态（已占位或未占位）细化锁的粒度

### 实际应用：

```cpp
// CentralCache.h
std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
```
实现自旋锁
```cpp
// CentralCache.cpp
// 不使用 yield() 的话：
while (locks_[index].test_and_set(std::memory_order_acquire)) {
    // 空循环
}
// 忙等待（busy-waiting），CPU 会不停执行测试，不做别的事，极度消耗性能。

// 内存序的设计是为了防止编译器或 CPU 把指令重排，导致锁内操作被提前执行。
while (locks_[index].test_and_set(std::memory_order_acquire)) {
    // 添加线程让步，避免忙等待，避免过度消耗CPU
    // 告诉操作系统 现在没事可做 将资源让给别人
    std::this_thread::yield();
}
```

```cpp
// 解锁 确保被锁期间所有操作完成
locks_[index].clear(std::memory_order_release);
```
| 部分                     | 含义                                                |
| ---------------------- | ------------------------------------------------- |
| `locks_[index]`        | 一个 `std::atomic_flag` 类型的锁（true 表示被占用，false 表示空闲） |
| `test_and_set()`       | 原子地把 flag 设为 `true`，并返回旧值                         |
| `while(...)`           | 如果返回值是 true（说明已经被占），就继续等待                         |
| `yield()`              | 暂时让出 CPU，避免忙等（burn CPU）                           |
| `memory_order_acquire` | 保证加锁后，不会把**锁内的操作**重排到加锁之前                         |

具体例子
```csharp
线程A               线程B
--------            --------
1. flag=false
2. test_and_set()→旧值false
3. flag变成true     test_and_set()→旧值true（被占）
4. 进入临界区        while循环等待 + yield()
5. clear()释放锁     看到flag=false→退出while()
6. 加锁成功
```

### std::memory_order_*

| 内存序                    | 含义                                | 常见用途            |
| :--------------------- | :-------------------------------- | :-------------- |
| `memory_order_relaxed` | 只保证原子性，不保证顺序                      | 性能最高，用于计数器、统计值等 |
| `memory_order_acquire` | 当前线程 **读取** 原子变量后，禁止后续读写重排到该操作之前  | 加锁时用（获取锁）       |
| `memory_order_release` | 当前线程 **写入** 原子变量前，禁止之前的读写重排到该操作之后 | 解锁时用（释放锁）       |
| `memory_order_acq_rel` | 同时具备 acquire 和 release 语义         | 既有读又有写的情况       |
| `memory_order_seq_cst` | 最强，保证全局顺序一致性                      | 默认但开销最大         |


# void*

void* 是一个通用指针
分配 void* 并不给出大小 只是记录指针

在通用分配器中`malloc free` 这很容易造成内存碎片的问题

但是本项目通过 list 实现对不同大小的内存分配不同的 list 减少了内存碎片出现 


# 内存在自身存储指针

```cpp
*reinterpret_cast<void**>(ptr) = freeList_[index];
freeList_[index] = ptr;
```

1. 将 ptr 转化为指向指针的指针
2. 解引用 ptr 将 ptr 指向的地方变为 list 的头节点
3. list 的头节点变成 ptr

相当于 
```cpp
FreeBlock* newHead = static_cast<FreeBlock*>(ptr);
newHead->next = freeList_[index]; 
freeList_[index] = newHead;
```
但是这里省去了 next 指针 直接利用内存块的空闲部分

## reinterpret_cast vs static_cast

| 特性   | `static_cast`      | `reinterpret_cast` |
| ---- | ------------------ | ------------------ |
| 主要用途 | **类型安全的编译期转换**     | **原始二进制级别的强制转换**   |
| 安全性  |  较安全（编译器检查）       |  极不安全（仅重新解释位）    |
| 转换原理 | 根据类型规则进行合法转换       | 仅改变“解释方式”，不改变底层位   |
| 可用场景 | 内建类型、继承类指针、void* 等 | 指针类型互转、强制类型 hack   |
| 检查   | 编译期检查类型兼容性         | 几乎无类型检查            |
| 本质   | “值意义转换”            | “内存解释转换”           |


# 概率产生的段错误

1. 修改 core dump 输出方式，让它生成普通文件
2. 确保允许生成 core 文件
3. 确保 core 文件生成在当前的文件夹
4. 写一个 `.sh` 脚本
5. 记录到 `.core` 文件中
6. 通过 gdb 查看 core 文件
    ```
    gdb ./perf_test core.perf_test.46804
    ```

## 重复工作

计算机最大的优势在于重复工作
对于 测试 来说，舍弃之前手动计时的习惯，用 sh 脚本反复执行 计算 用时 和 平均用时

# 延迟归还

在高并发环境中，对内存的释放会很频繁 
这里再增加一个缓冲层 在 **归还请求次数** 或 **时间** 积累到一定次数之后之后再将内存还回 pageCache 

# mmap

```cpp
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
```

1. addr = nullptr
让操作系统自动选择映射地址。
如果指定地址，则内核尝试在该地址映射内存（通常不指定，更安全）。

2. size
要申请的内存大小（字节数）。
必须是页大小（通常 4KB）的整数倍。

3. prot = PROT_READ | PROT_WRITE
内存权限标志：
PROT_READ → 可读
PROT_WRITE → 可写
组合起来就是“可读可写”。

4. flags = MAP_PRIVATE | MAP_ANONYMOUS
MAP_PRIVATE → 内存修改对其他进程不可见（写时拷贝）
MAP_ANONYMOUS → 不关联任何文件（fd 参数被忽略）
所以这段内存就是纯粹的堆内存，不映射任何文件。

5. fd = -1
由于 MAP_ANONYMOUS，fd 参数必须为 -1。
不使用文件描述符。

6. offset = 0
偏移量，针对文件映射有意义；匿名映射时必须为 0。