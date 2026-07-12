# 能力回收执行规则

## 文档状态

- `status`: `supporting`
- `scope`: 将已有实现收敛为可发现、可接入、可验证的默认路径
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与 [`charm_core_contract.md`](charm_core_contract.md)

“能力回收”是实现整理方法，不是 Core 准入机制。任何名称和接口仍需通过 Constitution 审判。

## 硬规则

- 先证明真实消费方和行为契约，再选择或新增实现；
- 先替换使用路径，再删除旧接口；迁移期旧接口必须标明禁止新增或例外范围；
- 默认路径必须进入 profile、binding、template 或 build check，仅写文档不算完成；
- early/fault 直连路径不得无界外溢到正常运行期业务；
- 共享协调层优先消费只读 snapshot，不直接接管 audio/display/net/storage 等重 runtime；
- 真实板 workaround 先作为 EvidenceRig 事实记录，不直接推广为跨平台 contract；
- Foundation 只接收可跨领域证明的最小机制，不回收领域策略、格式或生命周期。

## 三段式流程

### 1. 使用层

- 列出当前消费者、旧依赖和目标依赖；
- 新代码只使用目标路径；
- 记录仍需保留旧路径的调用方和退出条件。

### 2. 依赖层

- 用 source/CMake 检查约束非法依赖；
- 不通过 forward declaration、全局 locator 或平台宏绕过边界；
- 确认目标实现没有反向创造新的 Core 语义。

### 3. 证据层

- 至少执行一次覆盖改动边界的编译检查；
- 至少保留一个成功行为证据和一个关键失败/缺失证据；
- Host、QEMU、real board 分别声明，不汇总成未经证明的“全平台通过”。

## 记录字段

- consumer 与所需行为；
- 目标 contract / implementation；
- 旧路径状态与退出条件；
- build/source 检查；
- 正例、负例及日志位置；
- 例外和剩余风险。

旧 UI/Board 优先级、状态矩阵和局部术语定义归档于
[`../archive/architecture-inventory-v0/`](../archive/architecture-inventory-v0/)。
