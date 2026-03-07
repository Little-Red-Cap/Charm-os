# Rules 导读

本目录存放 Charm 项目中**长期稳定、跨任务生效**的规则文件。
这些规则不是一次性 prompt，也不是某个技能的内部说明，而是 AI 在 Charm 项目中需要长期遵守的协作与工程边界。

## 文件列表

### `collaboration.md`
定义协作方式、沟通节奏与讨论优先级。

适用：需求不明确、多方案讨论、先讨论后实现、需要 review 或共识对齐。

### `embedded-modern-cpp.md`
定义现代嵌入式 C++ 的技术立场与设计哲学。

适用：接口设计、抽象方向、是否回退传统写法、零成本抽象判断。

### `charm-architecture.md`
定义 Charm 项目的具体工程纪律与架构约束。

适用：分层、初始化、IO/错误模型/时间源一致性判断。

## 读取建议

### 场景 1：需求不清，需要先对齐
1. `collaboration.md`
2. 视情况补充 `embedded-modern-cpp.md`
3. 视情况补充 `charm-architecture.md`

### 场景 2：讨论设计与接口
1. `embedded-modern-cpp.md`
2. 视情况补充 `charm-architecture.md`

### 场景 3：代码审查 / 架构评审
1. `charm-architecture.md`
2. `embedded-modern-cpp.md`
3. 需要时再补 `collaboration.md`

## 总体原则

- 不要默认一次性加载全部规则。
- 先加载最相关的一层，不足再逐步补充。
- 规则的目标是帮助 AI 做出一致判断，而不是制造噪声。
