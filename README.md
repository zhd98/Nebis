# MdPad

极简、快速的 Markdown 编辑器。**纯 Win32 + Scintilla**，没有任何 WebView / 浏览器内核，
单窗口、类 Notepad 的体验，启动快、占用低。直接在编辑器里写 Markdown，**没有预览窗**——
所见即 `.md` 源文本，只是做了「行内语法淡化」：

- `#` / `##` 等 Markdown 符号显示为**灰色**、视觉上"隐藏"；
- 标题按级别**逐级放大、彩色、加粗**；
- 行内代码绿色等宽，链接蓝色。

编辑体验：多文档标签页、撤销/重做（Ctrl+Z / Ctrl+Y）、查找 / 替换、
UTF-8 读写（中文友好）。

## 技术说明

- 编辑器内核是 **Scintilla**（窗口类 `Scintilla`），通过 Win32 的 `CreateWindowEx` + `SendMessage` 调用，**零 C++ 链接依赖**（不链接 scintilla.lib）。
- Markdown 语法高亮使用 **Lexilla** 的 `markdown` 词法器：加载 `Lexilla.dll` →
  `CreateLexer("markdown")` → `SCI_SETILEXER` 挂到每个 Scintilla 实例上。
- 头文件（`src/scintilla/` 下的 `Scintilla.h` / `SciLexer.h` / `ILexer.h` / `Lexilla.h` / `Sci_Position.h`）
  取自 Scintilla 5.6.6 / Lexilla 官方源码，随仓库提交，编译期可见。
- 运行时只需两个 DLL：`Scintilla.dll` 与 `Lexilla.dll`（已随 SciTE 5.6.6 发布）。

> 为什么不是经典的单体 `SciLexer.dll`？Scintilla 5.x 起把词法器拆成了独立的 `Lexilla.dll`，
> 当前版本不再提供单体 `SciLexer.dll`，所以采用「Scintilla.dll + Lexilla.dll」的现代用法。

## 本地构建（可选）

需要 MSYS2 的 MinGW-w64 环境：

```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

然后把 `Scintilla.dll` 和 `Lexilla.dll`（从 https://www.scintilla.org/wscite566.zip 解压得到）
放到 `build/mdpad.exe` 同级目录即可运行。

## 通过 GitHub Actions 构建（推荐）

把仓库推送到 GitHub 后，Actions 会自动：

1. 下载 SciTE 5.6.6 全量包并取出 `Scintilla.dll` / `Lexilla.dll`；
2. 用 MinGW-w64 编译 `mdpad.exe`（静态链接，无额外运行时依赖）；
3. 把 `mdpad.exe` + 两个 DLL 打包为名为 **`mdpad-win64`** 的 artifact 供下载。

> 在仓库 **Actions** 页面手动触发（`workflow_dispatch`）或推送即触发。

## 使用

- 文件 → 新建 / 打开 / 保存 / 另存为（Ctrl+N / Ctrl+O / Ctrl+S）
- 编辑 → 撤销 / 重做（Ctrl+Z / Ctrl+Y）、查找（Ctrl+F）、替换（Ctrl+H）
- 多文档：顶部标签切换，文件 → 关闭标签
- 拖入或打开 `.md` 即可开始编写

## 文件结构

```
src/
  main.cpp         主框架窗口、菜单、标签页、消息循环
  editor.cpp/.h    Scintilla 封装：建窗、Markdown 样式、UTF-8 读写
  findreplace.cpp/.h 查找 / 替换窗口
  scintilla/       官方头文件（Scintilla.h / SciLexer.h / ILexer.h / Lexilla.h / Sci_Position.h）
CMakeLists.txt     MinGW 构建（静态链接运行时）
.github/workflows/build.yml   GitHub Actions 编译 exe
```
