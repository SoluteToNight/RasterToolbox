# RasterToolbox

RasterToolbox 是一个轻量级 GIS 栅格数据管理工具初始化工程，采用 C++20 + Qt6 Widgets + GDAL。

## 环境要求

- C++20
- CMake >= 3.20
- Ninja

## Linux 快速开始（Pixi）

```bash
pixi run configure
pixi run build
pixi run test
xvfb-run -a pixi run ./build/dev/rastertoolbox --smoke-startup
```

## Windows 编译（vcpkg）

Windows 依赖统一通过 `vcpkg` manifest（`vcpkg.json`）管理。

1. 先准备 `vcpkg`：

```powershell
git clone https://github.com/microsoft/vcpkg C:\\vcpkg
cd C:\\vcpkg
.\\bootstrap-vcpkg.bat -disableMetrics
```

2. 设置环境变量并使用 Windows 预设构建：

```powershell
$env:VCPKG_ROOT = "C:\\vcpkg"
cmake --preset windows-msys2
cmake --build --preset windows-msys2
ctest --preset windows-msys2
```

## Windows 发布（GitHub Actions）

- 发布入口：推送版本标签 `v*`（例如 `v0.1.0`）。
- 工作流文件：`/.github/workflows/release-windows.yml`。
- 发布环境：`MSVC + vcpkg`（依赖由 `vcpkg.json` 管理）。
- 发布产物：`RasterToolbox-<tag>-windows-x64.zip`（示例：`RasterToolbox-v0.1.0-windows-x64.zip`）。
- 同一 tag 重跑：使用 `gh release upload --clobber` 替换同名资产，Release 保持单份同名 zip。
- 失败语义：
  - `smoke` 失败时不会进入 Release 写入步骤。
  - 进入 `--clobber` 上传后若失败，可能需要 rerun 以恢复到单资产一致状态。

## 交叉编译（Linux -> Windows, 可选）

```bash
cmake --preset mingw-debug
cmake --build --preset mingw-debug
```

依赖 `x86_64-w64-mingw32-*` 工具链；默认工具链文件：`cmake/toolchains/mingw-w64.cmake`。

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
