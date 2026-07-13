# Config Module 草案

> `status`: `archived`
>
> 本文保存 typed 配置组织原则。仓库当前没有统一 Config Module、生成器或 consumer 证据。

## 目标

- 将构建输入转换为源码可消费的 typed 配置；
- 分离稳定公开 module 与生成实现 module；
- 兼容别名只做迁移，不拥有独立事实源；
- 减少业务代码直接读取散装宏和平铺常量。

## 分层

### 稳定公开表面

提供长期 import 名称和 typed API，不复制构建输入或生成 module 中的配置事实。

### 生成实现

将 CMake 的 profile、board、toolchain 和 feature 输入转换为源码值。生成文件可以随构建系统
演进，不作为业务代码的默认 import。

### 兼容层

旧 module、字段或常量名只转发到 typed 表面，并明确标为迁移层；不得重新硬编码事实或承载新功能。

## 构建边界

CMake 选择 target、toolchain、profile 与生成输入；源码消费转换后的语义配置。宏只保留给：

- C、汇编或 vendor SDK 必需定义；
- target/source 是否参与编译的开关；
- compiler、platform 和 architecture 探测。

产品身份、buffer/queue 容量和 feature 裁剪等业务语义，不应先设计成宏再包一层 typed facade。

## 评审问题

1. 配置属于构建选择、平台事实还是应用语义？
2. 是否已有 typed 表面？
3. 公开 module 与生成 module 是否只有一个事实源？
4. 兼容名称是否有明确退出边界？
5. 源码、CMake、生成文件和验证入口是否同步？

这些原则不授予 `Profile`、`Board`、`Product` 或 Config Module 公共身份；重新推进前必须先提供
真实 consumer、失败语义和至少两个环境的证据。
