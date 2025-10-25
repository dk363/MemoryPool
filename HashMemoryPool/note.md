# 基于hash_bucket实现内存池
参考 [这个项目](https://github.com/cacay/MemoryPool/tree/master)，我通过声明模板友元函数增加了哈希表实现了内存池
但是相对于 标准库的 `new` 来说，时空复杂度还是差很多。
我想了一下，应该有这几个原因：
1. 实现中 `#define MEMORY_POOL_NUM 64` 这里初始化hash_bucket 花费掉了时间，因此在测试中我加入了多种情况的测试， (~~先画靶再射箭~~)
2. `void HashBucket::freeMemory(void* ptr, size_t size)` 的底层实现还是用 `delete`


以下是我在这个项目中觉得新鲜的一点东西


# 内存对齐
> MOV 类由四条指令组成： movb 、movw 、movl 和
movq 。这些指令都执行同样的操作；主要区别在千它们操作的数据大小不同：分别是l 、
2 、4 和8 字节。
>  <div align="right">—— CSAPP</div>
如上所述，在寄存器的实现中规定了具体数据大小的实现。设想一下，如果不需要对齐，假设一个数据大小为 4 个字节，从地址 0x1 开始，那么在取数据时需要取 8 个字节，然后还去掉第一个字节的数据 因为这是无用的。对齐字节的工作在这样的操作面前不值一提。
# union vs struct
## 内存布局
对于 `union` 来说，所有成员共享一块内存 其总大小为最大的成员内存大小
而 `struct` 则相反，内存大小是所有成员加在一起的大小 
在这个[内存池](https://github.com/cacay/MemoryPool/blob/master/C-11/MemoryPool.h)的实现中有
```cpp
union Slot_ {
    T element;
    Slot_* next;
};
```
这里为什么用 `union` 而不是 `struct` 呢？
实现中使用了指针来指示当前可分配的位置，因此这里的`Slot`节点有两个用途：
1. 充当指针指示位置
2. 充当节点存储数据
因此通过`union`提升代码的复用性
# `void MemoryPool::allocateBlock()`的实现
在这个[cpp](src/MemoryPool.cpp)文件中我已经很详细地注解了如何为内存池分配一个新的内存块，这里就不再赘述了

总的来说，在有参考的情况下这个实现还是比较简单的
