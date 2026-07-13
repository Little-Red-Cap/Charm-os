# POSIX / net modules-ts 构建解阻记录

## 背景
- 时间点：在推进 POSIX process-control 与 real-ELF `kill_self` 信号烟测期间，`cmake-build-posix-qemu` 的全量构建被新的模块导入冲突阻塞。
- 现象：`posix-qemu-demo` 无法完成链接，导致 QEMU 回归无法继续作为 POSIX 主线验收手段。
- 影响面：POSIX 构建被阻塞，根因位于 `net` / `posix` 的模块导入边界，不是 POSIX 语义。

## 典型症状
- `Modules/io/net/net.reactor.cppm`、`Modules/io/posix/posix.api.cppm` 一带出现与 `std::span` / `std::array` 相关的 modules-ts 冲突。
- `Modules/io/net/net.stack.cppm` 会出现看似不合理的“默认构造函数重复声明”类错误。
- 清空构建目录中的 `.gcm` 缓存后仍可稳定复现，排除单纯缓存污染。

## 经验判断
- GCC `-fmodules-ts` 下的同类问题常来自：
  - 多个模块接口都直接暴露了标准库模板类型；
  - 同一组类型通过不同导入路径重复进入一个更高层模块；
  - 编译器在合并导入图时，对标准库模板实例或接口单元状态处理不稳定。
- 因此，看到 `std::span` / `std::array` 的冲突，不应只盯着单个报错点，而应优先审视：
  - 是否存在过宽的接口类型暴露；
  - 是否存在不必要的直接导入；
  - 是否可以把高层模块从某个“容易抖动”的中间模块上解耦。

## 本次成功做法

### 1. 先把 `net.common` 从 `std::span` 接口暴露上解耦
- 文件：`Modules/io/net/net.common.cppm`
- 做法：
  - 不再把 `net::ByteView` / `net::MutByteView` 直接定义为 `std::span` 别名；
  - 改为轻量自定义视图类型；
  - 只保留当前 `net` 子系统真实需要的最小接口：`data()` / `size()` / `empty()` / `operator[]` / `begin()` / `end()` / `subspan()`。
- 收益：
  - 把 `net` 公共接口从标准库模板实例细节里抽离出来；
  - 降低 `net.common` 被多个上层模块重复导入时的冲突概率。

### 2. 在 `net.posix` 里做显式桥接，而不是继续让类型系统隐式猜
- 文件：`Modules/io/net/net.posix.cppm`
- 做法：
  - 增加 `posix::ByteView -> net::ByteView` 和 `posix::MutByteView -> net::MutByteView` 的显式转换 helper；
  - 让 `SocketService` 的 `send/recv/sendto/recvfrom` 以及 `FdOps` trampoline 都通过这些 helper 进入 `net` 层。
- 收益：
  - 边界更清晰；
  - `posix.fd_table` 与 `net.common` 可以各自保留自己的最小视图契约，而不会互相污染。

### 3. 把 `net.posix` 从 `net.stack` 的硬依赖上解耦
- 文件：`Modules/io/net/net.posix.cppm`
- 做法：
  - `SocketService` 的内部状态由 `net::Stack` 改为 `net::SocketProviderRef`；
  - 新增 `bind_provider(...)`；
  - 保留模板形式的 `bind_stack(...)` 作为兼容 façade，但它只提取 `stack.provider()`，不再要求接口单元直接导入 `net.stack`。
- 收益：
  - 直接切断了 `posix.api -> net.posix -> net.stack` 这条容易触发重复导入的链；
  - 保持对现有调用点的兼容，不需要大面积改调用方。

### 4. 顺手去掉 `posix.api` 中不必要的直接导入
- 文件：`Modules/io/posix/posix.api.cppm`
- 做法：
  - 删除不再必要的 `import net.socket;`；
  - 保留所需的 `net.common` / `net.posix`。
- 收益：
  - 缩小高层接口单元的导入图；
  - 降低未来再触发类似冲突的概率。

## 验证路径
- 构建：
  - `cmake --build cmake-build-posix-qemu --target posix-qemu-demo -j 22`
- 回归：
  - `Examples/kernel/posix/qemu/run_qemu_ci.ps1 -ElfPath cmake-build-posix-qemu/posix-qemu-demo.elf -TimeoutSec 60`
- 结果：
  - 构建恢复通过；
  - QEMU 恢复为 `[ok] posix smoke + busybox phase2 smoke`；
  - 同轮推进的 real-ELF `kill_self` `SIGTERM` / `SIGINT` / `SIGKILL` 烟测也一并通过。

## 这次经验沉淀出的规则
- 在 GCC `modules-ts` 环境里，公共接口模块应尽量避免直接暴露复杂标准库模板别名，尤其是会被多路导入的基础模块。
- 如果两个子系统都需要“字节视图”语义，优先考虑：
  - 各自持有最小稳定视图；
  - 在边界上做显式桥接；
  - 而不是强行共享某个容易引发导入耦合的标准库别名。
- 当一个模块只需要“provider 能力”，就不要把“更高层包装类型”也硬塞进依赖图。
- 看到 modules-ts 报错时，优先做“导入图瘦身 + 接口收口”，不要急着把锅甩给缓存或单个报错行。

## 后续建议
- 如果后续 `net` 或 `posix` 再扩接口，优先沿用这次做法：
  - 先判断是“需要能力”还是“需要包装类型”；
  - 能依赖 `Ref/Ops/View` 的地方，不要直接依赖更重的 façade。
- 如果未来还要继续扩大 `net` 对外暴露面，可以考虑把“通用 byte view”进一步沉淀成更底层、专门为模块边界设计的轻量公共组件，而不是继续复用标准库模板别名。
