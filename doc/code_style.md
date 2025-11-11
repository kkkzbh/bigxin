# 代码风格与规范（Source of Truth）

本文件是本仓库代码风格与工程约定的唯一权威文档。
所有关于代码风格 / 结构 / 约定的新增或修改，必须通过修改本文件完成。

所有贡献者在编写或评审代码前必须阅读并遵守本文件。

## 0. 总体原则

1. 若评审中出现“代码风格调整”意见，应优先判断是否应写入本文件：
  - 若是公共、可复用的规则，则更新本文件。
  - 若仅为个案或争议点，明确标记为“无需更新”的原因。
  - 避免无限增加细碎规则，保持文档紧凑且具可执行性。
2. 风格与行为以本文件为准；本文件与其他说明冲突时，以本文件为最终依据。
3. 本仓库代码假定：
  - 使用 C++26。
  - 使用 RAII 管理资源，禁止裸 `new/delete`。
  - 优先使用模块与协程（特别是与 Asio 配合）。
  - 禁用 C++ 异常（见“异常与错误”章节）。

## 1. 基本配置与格式化

1. 语言与工具
  - 目标语言版本：C++26。
  - 禁止裸 `new/delete`，资源管理通过 RAII / 智能指针 / 容器完成。
 2. 格式化
  - 使用 `clang-format`，基于 LLVM 风格，配置见 `.clang-format`。
  - 统一：4 空格缩进；最大行宽 120。
  - 所有变更必须执行：`clang-format -i`。
  - 当工具配置与本文件存在冲突时，以本文件为准（见“1.4 clang-format 冲突处理”）。
3. 文件命名
  - 源文件：`snake_case.cpp`。
  - 模块单元仍使用 `.cpp`；当前不使用 `.ixx`。
  - 仅非模块代码使用头文件；模块优先 `import`。

### 1.4 clang-format 冲突处理（文档优先）

1. 原则：当 `.clang-format` 与本文规则冲突时，严格以本文为准；必要时使用局部指令包裹，尽量将影响范围限制到最小。
2. 推荐做法：
   - 在需要与配置冲突的代码片段外包裹：
     - `// clang-format off`
     - 目标代码
     - `// clang-format on`
   - 仍需执行 `clang-format -i`，以保持其余代码的一致性。
3. 已知冲突（必须按本文执行）：
   - 多行参数格式时，函数名与 `(` 之间留 1 个空格（见第 16/17 节）。当前配置中 `SpaceBeforeParens: Never` 会移除此空格；请使用 `clang-format off/on` 包裹该调用。
   - 多行参数时，右括号与分号独占一行（通常与配置一致，此条为重申并确保不可被折叠）。
4. 示例：

    ```cpp
    // clang-format off
    foo (
        a,
        []{
          work();
        },
        b
    );
    // clang-format on
    ```

## 2. 命名规范

1. 类型：`PascalCase`(对只有首字母需要大写的情况用纯小写，对小于等于6个字符以下的用纯小写)。
2. 函数 / 变量：`snake_case`。
3. 常量（含全局常量）：`SCREAMING_SNAKE_CASE`。
4. 命名空间：全小写。
5. 名称要求简洁清晰，在上下文可读前提下偏短。

## 3. 模块、包含与翻译单元结构

### 3.1 模块单元结构

1. 仅当需要 `#include` 非模块头文件时，使用全局模块片段：

    ```cpp
    module;
    
    #include <...> // 仅此处允许
    
    export module name;
    import dep1;
    import dep2;
    ```

2. 若模块单元不需要 `#include`，则禁止写 `module;`，文件形式为：

    ```cpp
    
    export module name;
    
    import dep1;
    import dep2;
    ```

3. `import` 必须出现在 `export module` 之后，且集中排列。

4. `#include` 在模块单元中仅允许出现在 `module;` 与 `export module` 之间；之后严禁再出现 `#include`。

### 3.2 非模块翻译单元（.cpp）

1. 文件前两行留空，从第 3 行开始写内容。
2. 若使用 `import std;`，所有 `#include <...>` 必须出现在所有 `import` 之前。
3. 禁止在 `import` 之后追加新的 `#include`。

### 3.3 标准库与第三方库

1. 标准库优先使用：

    ```cpp
    import std;
    ```

   不使用标准库头的 `#include <...>`，除非工具链问题有明确说明。

2. 第三方库（如 Asio 等）必须通过 `#include`，位置遵守上述模块结构规则。

### 3.4 CMake 与模块组织

1. 模块文件通过 `FILE_SET CXX_MODULES` 加入目标：

    ```cmake
    target_sources(target PUBLIC FILE_SET CXX_MODULES FILES a.cpp b.cpp)
    ```

2. 模块命名与组织：

  - 使用“点号模块”按功能拆分：`utility`、`core`、`protocol`、`server`、`client` 等。
  - 当前不提供聚合模块；若未来新增，需评估工具链兼容性。
  - 模块分区仅用于拆分实现，尽量不将分区名暴露为对外接口的一部分。

### 3.5 导出策略

1. 优先逐项导出：

    ```cpp
    export auto foo() -> int;
    export struct Bar { ... };
    ```

2. 避免 `export namespace ...`，除非该命名空间边界稳定且确实需要完整导出。

3. 分区中默认不导出内部工具：

  - 内部共享工具仅在同一模块内部可见。
  - 若需对外暴露，由主模块统一导出。

4. 跨分区共享常量：

  - 使用 `inline constexpr`。
  - 仅当需要对外暴露时添加 `export`。

5. 使用 `using` 在使用点简化常用符号，例如：

    ```cpp
    using util::use_awaitable_noexcept;
    ```

## 4. 类型与声明风格

1. 函数返回类型：

  - 统一使用尾置返回类型：

      ```cpp
      auto func(args) -> ReturnType;
      ```

2. `inline` 顺序：

  - 使用 `auto inline f(...) -> R`，不写 `inline auto`。

3. 构造函数修饰顺序：

  - `explicit constexpr Ctor() {}`。

4. 变量修饰顺序：

  - `auto const x = ...;`
  - `auto static constexpr X = ...;`
  - `noexcept` 在形参列表之后。

5. 模板：

  - 优先使用 concepts：

      ```cpp
      template<std::unsigned_integral T>
      auto f(T v) -> ...
      ```

    或：

      ```cpp
      auto f(std::unsigned_integral auto v) -> ...
      ```

  - 未使用 concepts 时，统一使用 `typename`，不使用 `class` 关键字作为模板形参。

6. const 引用与指针：

  - 使用右侧 `const`：
    - `Type const&` / `Type const*`。
    - 对 `auto` 一致：`auto const&` / `auto const*`。

## 5. struct / class 使用约定

1. 一律使用 `struct`，不使用 `class`。

2. 访问控制：

  - `struct` 默认 `public`，不要额外写 `public:`。
  - 如需私有成员，仅添加一个 `private:` 分区。

3. 继承：

  - 使用默认公有继承形式：

      ```cpp
      struct D : Base { ... };
      ```

    不写 `public Base`。

## 6. const, constexpr 与初始化

1. 能 `constexpr` 则 `constexpr`，能 `const` 则 `const`（针对引用/指针）。

2. 列表初始化：

  - 非空列表使用 `{ value }`，空列表使用 `{}`。
  - 优先使用列表初始化避免窄化。

3. 字符串与视图：

  - 只读短期使用 `std::string_view`：`"..."sv`。

  - 需要所有权或拼接使用 `std::string`：`"..."s`。

  - 需启用：

      ```cpp
      using namespace std::string_view_literals;
      using namespace std::string_literals;
      ```

4. 整型常量：

  - 避免窄化，优先：

      ```cpp
      auto n = std::size_t{ 256 };
      ```

  - 需要有意窄化时使用圆括号：

      ```cpp
      auto port = std::uint16_t(7777);
      ```

5. 从 `string_view` 构造 `std::string`：

  - 直接：

      ```cpp
      auto s = std::string{ view };
      ```

  - 禁止冗余构造链。

6. C API：

  - 与 C API 交互优先 `.data()`，视为与 `.c_str()` 等价足够。

7. CTAD：

  - 能用则用，例如：

      ```cpp
      auto w = std::weak_ptr{ sp };
      auto p = std::pair{ a, b };
      ```

## 7. 所有权、性能与视图

1. 仅读取且不持久化的字符串参数优先使用 `std::string_view`。
2. 用于“落地存储”的入参可以按值接收，在实现中 `std::move` 进成员。
3. 传递可移动对象或临时对象时统一使用 `std::move`，移动后变量不再使用。
4. 当回调签名为 `T const&` 时，不强制使用 `std::move`。
5. 视图生命周期：
  - `string_view` / `span` 指向容器内部存储时，禁止在其存活期内对容器做可能导致重分配或移动的修改。
  - 可选模式：先使用视图完成解析，再修改容器；或拷贝出需要的片段。

## 8. 时间与日期

1. 仅使用 `std::chrono`，禁止使用 C 时间 API：

  - 禁止：`std::time` / `std::localtime` / `std::ctime` / `strftime` 等。

2. 当前时间：

    ```cpp
    auto now = std::chrono::system_clock::now();
    ```

3. 输出格式：

  - 使用 `std::format` 的 `chrono` 支持：

      ```cpp
      std::format("{:%H:%M:%S}", tp);
      ```

4. 时区策略：

  - 默认不依赖时区数据库。
  - 需要本地时间且工具链支持时，可使用 `std::chrono::zoned_time` / `current_zone()`。
  - 否则使用 UTC，并在输出中标注。

5. 不引入第三方时间库。

## 9. `_` 占位符使用（C++26）

1. `_` 用于显式丢弃值：

    ```cpp
    auto [ec, _] = f();
    auto _ = expr;
    ```

2. 可以在同一作用域重复声明 `_`，旧 `_` 禁止继续使用。

3. 仅在“有意识丢弃”场景使用 `_`，禁止将其当作常规变量名。

## 10. 类型与转换

1. 避免无意义的 `static_cast` / `reinterpret_cast`，优先隐式转换。

## 11. 变量声明（统一使用尾置类型 + auto）

1. 有初始化的局部变量统一为：

    ```cpp
    auto [const] name = Type{ ... };
    ```

2. 禁止在局部作用域使用前置类型初始化形式：

    ```cpp
    // 禁止：
    Type name{};
    Type name = { ... };
    ```

3. 仅当 `Type x;` / `Type x{}` 语义确实不同且需要该行为时，才直接写前置类型。

4. 字面量后缀：

  - 使用小写字母：如 `123ll`、`10uz`。

## 12. Ranges / Views

1. 优先使用现代接口：
  - 如可用，优先使用 `std::erase_if`。
2. 其余算法优先使用 `std::ranges::*` 与 `std::views::*` 管道风格。
3. 旧式迭代器循环在无特殊原因时不推荐。

## 13. 控制结构与格式

1. `if/for/while/switch/try` 左大括号不换行：

    ```cpp
    if (cond) {
      ...
    }
    ```

2. 禁止单行压缩：

  - `if/else`、`for`、`while`、`switch` 的语句体统一使用多行。

3. 无限循环：

  - 使用 `while (true)`，禁止 `for(;;)`。

4. 函数定义：

    ```cpp
    auto f(...) -> T
    {
      ...
    }
    ```

5. lambda：

  - 左大括号不换行，体使用多行：

      ```cpp
      auto f = [x]{
        ...
      };
      ```

  6. namespace
  - 左大括号换行，内部一个缩进
  7. struct
  - 左大括号换行，内部一个缩进

## 14. 协程与 lambda 细则

1. `co_spawn` 等场景下的 lambda：

  - 无参数时省略 `()`：

      ```cpp
      [cap] -> awaitable<void> {
        co_await ...;
      }
      ```

  - 仅在确有参数需求时才写 `()`

2. 返回 `awaitable<void>` 且已有 `co_await`/`co_yield` 时，末尾可省略 `co_return;`。

3. 若 lambda 体没有任何 `co_await`，必须保留 `co_return;` 形成协程。

4. lambda 体禁止压一行。

### 14.1 捕获规则（强制）

1. 禁止默认按引用捕获 `[&]`。

2. 优先按值捕获，必要状态用聚合对象承载，例如：

    ```cpp
    auto ui = std::make_shared<State>();
    auto h = Renderer([ui]{
      use(ui->x);
    });
    ```

3. 仅当可严格证明被捕获对象生命周期长于回调时，才允许显式引用捕获，如：

    ```cpp
    on_enter = [&state, &client] { ... };
    ```

4. 需要可选所有权时使用 `std::weak_ptr`，在回调内：

    ```cpp
    if (auto p = weak.lock()) {
      ...
    }
    ```

## 15. 逻辑运算符

1. 布尔逻辑使用关键字形式：
  - `not` 替代 `!`
  - `and` 替代 `&&`
  - `or` 替代 `||`
2. 位运算仍使用符号形式：`& | ^ ~`。

## 16. 函数调用与多行参数格式

1. 需要换行时使用多行参数格式：

  - 函数名与 `(` 之间留 1 个空格。
  - 每个实参单独一行，逗号结尾。
  - 右括号与分号独占一行。

2. 初始化列表作为实参时：

  - `({` 必须在同一行，`})` 必须在同一行：

      ```cpp
      f({
        ...
      });
      ```

3. 包裹长表达式的括号：

  - `= (` 与 `)` 保持结构清晰：

      ```cpp
      auto panel = (
          make_widget(...)
          | decorate(...)
      )
      | color(...);
      ```

## 17. 含内联 lambda 的调用

1. 只要参数中包含内联 lambda，一律使用多行参数格式：
  - 函数名与 `(` 之间留 1 个空格。
  - 每个参数单独一行。
  - lambda 体使用多行。
  - 右括号与分号独占一行。

## 18. 异常与错误处理

1. 禁用 C++ 异常：
  - 禁止 `throw` / `try` / `catch`。
  - `main` 不捕获异常。
2. 错误传递：
  - 使用错误码或 `std::expected` 等显式错误返回机制。
3. 数字解析：
  - 禁止使用 `std::stoi` / `std::stol` / `std::stoul` 等。
  - 统一使用 `std::from_chars`，检查返回值并确保完全消费输入。

## 19. 其他文档

- Async I/O 规范：`doc/async_io.md`
- CLI UI 规范：`doc/cli_ui.md`
