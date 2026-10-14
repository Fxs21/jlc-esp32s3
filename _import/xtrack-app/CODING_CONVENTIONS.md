# 编码规范

语言: C, 不使用 C++.

## 命名风格

**全部使用 snake_case.**

| 元素 | 示例 |
|------|------|
| 函数 | `account_publish`, `dbl_buf_init` |
| 类型（typedef） | `account_t`, `page_manager_t`, `account_err_t` |
| 枚举类型 | `account_event_t`, `account_err_t` |
| 枚举值 | `ACCOUNT_EVENT_PUB_PUBLISH`, `ACCOUNT_OK` |
| 结构体字段 | `write_index`, `read_available` |
| 文件 | `data_center.h`, `page_manager.c` |
| 宏 | `MAX_ACCOUNTS`, `ID_LEN` |

## 指针声明

指针星号附加在名字（函数名 / 参数名 / 变量名）一侧, 而不是类型名一侧.

```c
// 正确
int *func(char *str, void *data);
int  a, *b;       /* 声明多个变量时例外 */
char *ptr;

// 错误
int*   func(char* str, void* data);
char*  ptr;
```

例外: 在同一行声明多个变量且部分是指针时, 按需调整 (`int a, *b`).

## 输出参数

输出指针参数使用 `out_` 前缀.

```c
// 正确
int pingpong_buffer_get_read_buf(pingpong_buffer_t * ppbuf, void ** out_read_buf);

// 错误
int pingpong_buffer_get_read_buf(pingpong_buffer_t * ppbuf, void ** p_read_buf);
int pingpong_buffer_get_read_buf(pingpong_buffer_t * ppbuf, void ** read_buf);
```

例外: 局部变量名不需要前缀, 用简洁的描述性名称即可.

```c
// 局部变量, 正确
void * wbuf;
pingpong_buffer_get_write_buf(&self->pp, &wbuf);
```

## 头文件注释

每个头文件使用 Doxygen 风格, 每个函数使用完整的 @brief / @param / @retval.

```c
/**
  * @brief  Ping-pong buffer initialization
  * @param  ppbuf: Pointer to the ping-pong buffer structure
  * @param  buf0:  Pointer to the first buffer
  * @param  buf1:  Pointer to the second buffer
  * @retval None
  */
void pingpong_buffer_init(pingpong_buffer_t * ppbuf, void * buf0, void * buf1);
```

格式要求:
- `@brief` 后直接跟描述, 不换行
- `@param` 格式: `@param  name: 描述`, 缩进对齐
- `@retval None` 表示无返回值
- `@retval RES_OK` / `@return res_code_t` 表示有返回值
- `@note` 可选, 用于补充说明

## 字符编码

代码文件中只使用 ASCII 字符.

```c
// 正确
"demo -> main"

// 错误
"demo → main"
```

箭头用 `->` 代替 Unicode `→`.

注释中的流程箭头也用 `->`:

```c
// 正确
// on_load -> lv_screen_load -> on_appear

// 错误
// on_load → lv_screen_load → on_appear
```

## 文件组织

- 一个模块一个 `.h` + 一个 `.c`
- 结构体定义和 API 声明放在 `.h`, 实现放在 `.c`
- **每个 struct 类型必须附带 `typedef`**, 且 `struct` 标签名与 `typedef` 名一致（不加 `_s` 后缀）

```c
// 正确 — struct 标签与 typedef 同名
typedef struct dc_t dc_t;          // 前向声明（解决循环依赖）
struct dc_t { int count; };        // 完整定义

// 错误 — _s 后缀多余
typedef struct dc_data_s dc_data_t;
struct dc_data_s { int count; };
```

struct 的 `typedef` 在 `.h` 中紧跟在 include 之后、API 声明之前。如果存在循环依赖（如 `account_t` 和 `dc_t` 互相引用），前向声明 + typedef 放在前置类型定义之前。

## 内存模型

- 使用静态数组代替运行时动态分配
- 固定大小, 编译期已知上限
- 避免在运行时路径中使用 malloc (初始化阶段除外)

```c
#define MAX_ACCOUNTS 32
static account_t g_pool[MAX_ACCOUNTS];
```

## 守卫

头文件守卫使用全大写文件名 + `_H`:

```c
#ifndef DATA_CENTER_H
#define DATA_CENTER_H
...
#endif /* DATA_CENTER_H */
```

## 语言特性

- 使用 C99/C11 标准（ESP-IDF 默认 gnu11，全兼容）
- 使用 `stdint.h` 的定长类型 (`uint32_t`, `int8_t` 等)
- 使用 `stddef.h` 的 `size_t`
- `bool` 类型（`<stdbool.h>`）仅在语义为"是/否"查询的函数中使用, 错误码统一用 `int` (`res_code_t`)
- 使用 `//` 单行注释和 `/* */` 多行注释, 风格不限
