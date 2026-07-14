# Input 文档入口

> `status`: `supporting`
>
> `scope`: raw input、sampling、pump 与 domain adapter 的文档路由

当前分层、生命周期、背压、已知限制和验证入口见
[`input_layering_decision.md`](input_layering_decision.md)。实现以
[`Modules/io/input`](../../Modules/io/input/) 和 [`hal_input.cppm`](../../Modules/io/hal/hal_input.cppm)
为准。

IO primitive 与 backend/adapter ownership 从 [`../io/README.md`](../io/README.md) 进入；UI intent、
gesture、focus 和控件行为不由本目录定义。
