# POSIX 三层执行边界

状态：supporting contract。

本文件约束 POSIX 兼容执行面中的 System Image、Runtime 与 User Program。三层是责任边界，
不是 Charm Core 类型，也不要求每个平台使用相同目录结构。

源码事实优先于本页：

- `Modules/io/posix/posix.program_image.cppm`
- `Modules/io/posix/posix.proc.cppm`
- `Modules/io/posix/posix.user_runtime.cppm`
- `Modules/io/posix/posix.user_context.cppm`

## System Image

System Image 持有启动与平台事实，例如：

- startup、向量表和链接布局
- 可执行内存与 image buffer
- board/QEMU bring-up
- console、VFS、时钟和其它 runtime 依赖的装配
- 最终可启动 ELF/bin 产物

这些事实由具体平台工程提供。POSIX 模块不得猜测板级地址、启动顺序或设备归属，
User Program 也不得依赖这些细节。

当前 Cortex-M7 QEMU 实例位于 `Examples/kernel/posix/qemu/`。它是一个实现和验证入口，
不是 System Image 的通用类型定义。

## Runtime

Runtime 把平台能力组织为 same-address-space 程序执行语义。当前关键边界包括：

- `ProgramImage` 描述 image kind、名称、entry ABI、entry 与运行提示字段。
- `ProcService` 解析 image、建立子 fd 视图、组织 argv/envp、调用入口并保存退出状态。
- `posix::user::Runtime` 把活动 POSIX API 投影给用户态桥接。
- `StartupContext` 暴露当前调用的 argc/argv/envp。
- `ExecContext` 承担 errno 与显式退出收束。

Runtime 可以知道固定容量、same-address-space 和具体 loader，但不得把这些实现细节伪装成
MMU 进程隔离或完整 Linux ABI。

## User Program

POSIX `ProgramImage` 当前支持两种入口 ABI：

```cpp
int main(int argc, char** argv);
int main(int argc, char** argv, char** envp);
```

它们分别对应 `main_argv_v0` 与 `main_argv_envp_v1`。`validate_program_image()` 要求 ABI 与
唯一有效 entry 指针匹配；`invoke_program_main()` 按 ABI 调用。

User Program 可以依赖当前 POSIX façade 提供的 fd、stdio、argv/envp、errno 和进程结果语义，
但不能直接依赖：

- board memory map 或链接脚本路径
- runtime 内部固定容量和对象布局
- loader 的临时 buffer
- 当前退出收束实现
- 不在测试与接口中成立的 Linux 行为

## 执行链

当前主关系是：

```text
System Image 提供平台资源
-> ProcService 接收 SpawnConfig
-> resolve/load ProgramImage
-> 建立 fd 与 argv/envp 上下文
-> invoke_program_main
-> 保存 WaitStatus 并回收本地资源
```

registered、ELF、ModuleX 等 image kind 可以使用不同解析或加载路径，但进入 `ProcService`
后共享同一套 POSIX 程序入口和生命周期。

## 与 Resident App ABI 的边界

`Examples/app_abi` 中的 resident App 原型是另一条应用执行契约：

- POSIX：`ProgramImage` + `main(argc, argv[, envp])`
- Resident App：`AppImage` + `CharmAppMainFn` + `CharmAppApi`

二者可以共享 ELF 或 ModuleX 作为文件格式，但文件格式相同不代表 entry ABI、runtime context
或 capability surface 相同。loader 必须明确自己产出哪一种 loaded image，不得靠函数指针
强转把两条执行模型混接。

## 不在本契约中的内容

- 产品 bootloader、签名、rollback 或部署策略
- MMU 隔离、抢占调度、完整 signal/fork/session
- sysroot、完整 CRT 或 toolchain 交付形状
- Charm Core 身份与 capability 准入
- 未来 image format roadmap

当前模块清单与验证入口见
[`posix_support_overview.md`](posix_support_overview.md)。完整早期分层论证见
[`../archive/posix-v0/`](../archive/posix-v0/README.md)。
