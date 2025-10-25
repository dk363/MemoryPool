用 cpp 实现了一个简单的内存池

# 内存池

在读了[内存管理](https://xiaolincoding.com/os/3_memory/vmem.html#%E8%99%9A%E6%8B%9F%E5%86%85%E5%AD%98) 一章后，我想要动手实现一个简单的内存池

这里主要实现了用 hash_bucket 和 three_tiered 的方式实现内存池 

## hash_bucket

这个只是一个简单的通过 hash 函数对不同大小的内存进行分区，达到减少内存碎片的目的
但是在测试之后，只有在特定的测试环境下可以做到比 `malloc new` 更快。详情见[HashMemoryPool项目文档](./HashMemoryPool/note.md)

## three_tiered

这里主要想仿照 linux 的文件系统的 页表 ，通过三级缓存改进内存池的性能

参考了 Google 的 [TCMalloc](https://github.com/google/tcmalloc)

## 为什么要实现内存池

### 内存碎片

内存碎片主要分为 外碎片 和 内碎片

#### 外碎片

假设有 4MB 的内存，先后打开了三个程序，分别占用 1MB 2MB 1MB 接着第一个和第二个程序运行结束、退出。接着打开第四个程序，要用 1.5MB 大小的空间，虽然此时总内存大小是 2MB 的，但是是不连续的，因此第四个程序一定要等第二个程序运行结束才可以运行。当然了，此时也可以将第二个程序换入硬盘然后再换出，但是这里有两个缺点
1. 硬盘访问速度比内存慢
2. 此时会造成机器的卡顿

#### 内碎片

内存分配必须起始于 4 8 16 整除的地址，因此当内存分配非倍数内存的时候，会多出来一部分，这里多出来的部分就是 **内碎片** 通过对不同内存大小的程序的分类，可以做到减少内碎片

# CMake

忘了的时候看一看这个[教程](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)

下面补充一点教程中没有细细讲的

## function

```cmake
function(<name> [<arg1> ...])
  <commands>
endfunction()
```
注意：这里 function 之后第一个是函数名称 `ARGV` 保存传递给函数的所有参数列表，而 `ARGN` 保存超出最后一个预期参数之外的参数列表。
举例说明：
```cmake
function(FilterFoo OutVar)
#        Search all the variables in the argument list passed to FilterFoo,
#        and place those containing "Foo" into the list named by "OutVar"
  foreach(word IN LISTS ARGN)
    if(word MATCHES Foo)
      list(APPEND ${OutVar} ${word})
    endif()
  endforeach()
  set(${OutVar} ${${OutVar}} PARENT_SCOPE)
endfunction()
# OutVar 是传入用来存放 Foo 变量的 List
# 这里调用剩余的传入的参数用 ARGN
```

## 引号的用法

### 字符串中有空格

```cmake
set(GREETING Hello World)

# GREETING = ["Hello", "World"]
```

```cmake
set(GREETING "Hello World")

# GREETING = ["Hello World"]
```

### 展开列表时的区别

```cmake
set(LIST a;b;c)


message(${LIST}) 
# a;b;c
# 将其作为列表自动展开

message("${LIST}")
# a;b;c
# 分号作为字符串的一部分
```

```cmake
set(LIST a;b;c)

foreach(item ${LIST})
  message("Item = ${item}")
endforeach()

# output:
# Item = a
# Item = b
# Item = c


foreach(item "${LIST}")
  message("Item = ${item}")
endforeach()


# Item = a;b;c
```

## include

```cmake
include(<file|module> [OPTIONAL] [RESULT_VARIABLE <var>]
                      [NO_POLICY_SCOPE])
```
在当前 CMake 作用域中执行另一个 .cmake 文件的内容
