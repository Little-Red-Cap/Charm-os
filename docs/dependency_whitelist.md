# 依赖白名单（编译期入口约束）

目标：把“分层规则”落到编译期约束，所有模块优先只 import 入口模块，禁止跨层直连。

## 入口模块

- Foundation：`charm.foundation`
- Runtime：`charm.runtime`
- Domains：`charm.domain`

## 分层规则（单向依赖）

```
Charm.Foundation  <-  Charm.Runtime  <-  Charm.Domains
```

### Foundation（能力基座）

允许：
- `import charm.foundation`

禁止：
- 任何 `charm.runtime` / `charm.domain`
- 任何 `kernel/*` / `fs/*` / `hal/*` / `ui/*` / `audio/*`

### Runtime（运行时与系统能力）

允许：
- `import charm.foundation`
- `import charm.runtime`

禁止：
- `charm.domain`
- 任何 `ui/*` / `audio/*` 直连

### Domains（领域系统）

允许：
- `import charm.foundation`
- `import charm.runtime`
- `import charm.domain`

禁止：
- 反向依赖 Foundation/Runtime 的具体实现细节（用入口模块代替）

## 例外（仅允许在平台/适配层）

平台/适配层可直接引入具体模块以实现绑定：

- `Modules/platform/*`
- `Examples/*`（示例允许直连，但优先入口）

## 代码示例

### Runtime 模块

```
import charm.foundation;
import charm.runtime;
```

### Domain 模块

```
import charm.foundation;
import charm.runtime;
import charm.domain;
```

## 执行规则

1. 新文件默认只 import 入口模块。
2. 若必须直连具体模块，先在本页“例外”说明并标注理由。
3. 违反规则的 import 视为构建失败。
4. CMake 将在配置阶段进行基础校验（Foundation/Runtime 违规直接失败）。

## 参考

- VSF 对照与可借鉴清单：`docs/vsf_comparison.md`
