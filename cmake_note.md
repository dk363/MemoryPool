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
