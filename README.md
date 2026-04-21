# RasterToolbox

RasterToolbox 是一个轻量级 GIS 栅格数据管理工具初始化工程，采用 C++20 + Qt6 Widgets + GDAL。

## 环境要求

- C++20
- CMake >= 3.20
- Pixi（推荐）

## 快速开始（Pixi）

```bash
pixi run configure
pixi run build
pixi run test
xvfb-run -a pixi run ./build/dev/rastertoolbox --smoke-startup
```

## Phase 6 一键验收（真实样本）

```bash
pixi run phase6-acceptance
```

执行后会在 `.omx/plans/` 下生成一份带时间戳的验收证据文档。

## 目录结构

- `src/`：实现代码
- `include/`：公共头文件
- `resources/`：Qt 资源与默认预设
- `tests/`：单元与集成测试
- `docs/`：架构与 ADR

## UI 主题

- 在菜单 `视图 -> 主题` 中可切换 `深色 / 浅色`。

## 编译预设

- 当前环境（Linux）：`cmake --preset host-linux && cmake --build --preset host-linux`
- MinGW 交叉编译（Windows）：`cmake --preset mingw-debug && cmake --build --preset mingw-debug`
  - 依赖 `x86_64-w64-mingw32-*` 工具链；默认工具链文件：`cmake/toolchains/mingw-w64.cmake`

## 架构文档 Canonical Path

`docs/architecture/rastertoolbox-architecture.md`
