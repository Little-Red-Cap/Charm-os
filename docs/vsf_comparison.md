# VSF 对照与可借鉴清单

本页只对标 VSF 的“接口形状与分层思路”，不借鉴宏体系与实现细节。

## 1. 分层与初始化顺序（可直接借鉴）
- VSF 分层：service（底座能力）→ component（功能子系统）→ app
- 典型 init 顺序：`service_init` → `hal_init` → `component_init`（由应用显式决定）
- 在 Charm 的对应：把 core/service 视为 Foundation，并在应用层显式启用组件 init

落地建议：
- 文档中明确 “组件初始化由 app 决定”
- 为 component 提供可选的统一入口（不强制调用）

## 2. 依赖开关与构建期校验（强烈建议借鉴）
- VSF 在 cfg 中明确依赖关系：组件依赖 service/hal，编译期拒绝不满足的组合
- Charm 方向：依赖白名单 + CMake/静态断言校验（不靠宏驱动生成）

落地建议：
- 在 `docs/dependency_whitelist.md` 中补组件依赖关系
- CMake 增加 build-time 检查（缺依赖即失败）

## 3. 内核事件队列多后端策略（可借鉴）
- VSF 有“环形队列/链表队列”两套实现，API 统一，按配置选择
- Charm 方向：保持 EDA API 不变，调度策略可切换

落地建议：
- 维护统一 API
- 允许“低开销后端”和“动态优先级后端”并存

## 4. HAL 统一 API + 驱动模板（强烈建议借鉴）
- VSF 的 HAL API 统一，驱动只做核心逻辑，时钟/复位/中断由平台 hook
- Charm 已在做 vtable/registry，适合补“驱动模板规范”

落地建议：
- 为每个 HAL 外设提供 “驱动实现范围”与“平台 hook 范围”说明
- 形成最小模板与示例

## 5. MAL 抽象（Memory Abstraction Layer）（可借鉴）
- VSF 的 MAL 让 block/file/flash 统一为设备抽象
- 对你当前 VFS + FatFs + BlockDevice 很有收益

落地建议：
- 统一 block/file 设备抽象，再挂载到 FS
- 为虚拟磁盘/远端磁盘准备统一入口

## 6. 可复现测试与模板检查（建议借鉴）
- VSF 在 HAL 侧提供模板与测试脚本，避免“接口形状漂移”
- Charm 方向：小而确定的测试 + 模板检查

落地建议：
- 为 HAL/USB/FS 建立最小功能回归
- 添加接口形状检查（API 是否完整、符号是否齐全）

## 7. Charm 可执行清单（优先级）
1) 分层初始化顺序落地（文档 + 可选 init 入口）
2) 依赖白名单 + 构建期检查
3) HAL 驱动模板规范
4) MAL 抽象定义
5) EDA 调度后端可切换策略
6) 最小回归测试/模板检查

