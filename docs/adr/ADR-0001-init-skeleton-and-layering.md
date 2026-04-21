# ADR-0001: 初始化骨架与四层分界

- Status: Accepted
- Date: 2026-04-20

## Context

RasterToolbox 当前从空仓库起步，需要优先建立可验证工程骨架，并且与架构文档定义的 UI / Dispatcher / Engine / Config 四层边界一致。

## Decision

采用单仓单应用起步：

- 一个主程序 target：`rastertoolbox`
- `include/rastertoolbox` 与 `src` 同构分层
- `ProgressSignalBridge` 归属 `engine`
- SQLite 仅保留扩展点，不在 init v0 落 schema
- Linux 作为 CI 首平台，Widgets smoke 使用 `xvfb-run -a`

## Consequences

- 优点：快速形成 configure/build/test/smoke 闭环
- 代价：早期需通过 include 方向和 code review 维护边界纪律
