# Dependency Contract (Foundation / Runtime / IO / Domain)

目标：把“散落的子系统”真正耦合成统一架构，不再各用各的。

## 1) 层级定义

- Foundation: `Modules/core/*`, `Modules/out/*`, `Modules/system/trace_core`
- Runtime: `Modules/system/kernel/*`, `Modules/system/modulex/*`, `Modules/system/boot/*`
- IO: `Modules/io/*` (HAL / FS / USB / Shell / AT / Input / VFS)
- Domain: `Modules/media/*`, `Modules/ui/*`

## 2) 硬规则 (Must)

- Foundation 不得依赖 Runtime / IO / Domain。
- Runtime 只依赖 Foundation，不得依赖 IO / Domain。
- IO 只依赖 Foundation / Runtime，不得依赖 Domain。
- Domain 只能依赖 Foundation / Runtime / IO，不得反向渗透。

## 3) 统一入口 (Single Source of Truth)

- 格式化与日志: 只允许 `out.*` / `trace_core`。
- 时间/计时: 只允许 `charm.system.clock` / `kernel` 的时间源。
- 容器与池: 只允许 `core/service/*` 固定容量容器。
- 事件/调度: 只允许 `kernel` 的 EDA / Sync / Timer。
- IO 能力: 只允许 `io/*` 入口，不允许 Domain 直接碰 HAL。

## 4) 禁止清单 (Anti-patterns)

- Domain 自建环形队列/池/日志/计时器/线程。
- UI/Audio 直接 include HAL 或底层驱动。
- 任何模块直接使用 `printf/snprintf` 或 `std::cout`。

## 5) 迁移准则

- 每次“能力回收”必须满足三步:
  1) 仅改使用侧 (调用路径替换，不删旧实现)
  2) 依赖检查生效 (违反依赖的 include/import 直接失败)
  3) 最小回归 (编译 + 一个最小行为验证)

## 6) 构建期约束

- CMake 依赖白名单必须开启 (`CHARM_ENABLE_DEPENDENCY_WHITELIST=ON`)。
- 违反依赖层级时，配置阶段或编译阶段直接失败。

