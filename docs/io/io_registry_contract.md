# io.registry 契约

## 文档状态

- `status`: `canonical`
- `scope`: Channel endpoint 的固定容量注册、查找和生命周期
- `source`: [`io.registry.cppm`](../../Modules/io/registry/io.registry.cppm)

## 容量与注册

- `Registry<MaxEndpoints>` 使用编译期固定容量，不在运行时分配内存。
- name 为空或 cap 为 `0` 时，`register_channel()` 返回 `invalid_arg`。
- name 或 cap 重复时返回 `exist`；容量耗尽时返回 `buffer_overflow`。
- `EndpointDesc::name` 是 `string_view`，其 backing storage 必须覆盖注册生命周期。

## Ownership 与查找

- Registry 不拥有 `Channel` 或 `Reactor`，只保存调用方提供的指针。
- `open_channel(name|cap)` 未命中时返回 `nullptr`。
- `find_channel()` 返回 endpoint metadata；`list_channels()` 只遍历已注册项。
- `unregister_channel()` 未命中时返回 `noent`，且不销毁底层对象。

从 Registry 取得的裸指针是否继续有效，由对象 owner 决定；注销只移除后续查找入口。

## Replace

`replace_channel()` 以 cap 定位现有项：

- cap 不存在时返回 `noent`；
- cap 命中但 name 不同返回 `exist`；
- cap 与 name 均匹配时替换 Channel/Reactor 指针和 metadata。

## 运行期设备

短生命周期设备不得把对象生命周期转交给 Registry。需要稳定 capability name 的动态 Channel 使用
[`io.channel.slot`](../../Modules/io/channel/io.channel.slot.cppm) 与
[`io.channel.slot_export`](../../Modules/io/channel/io.channel.slot_export.cppm)；detach 后既有 slot 调用返回
`noent`，Registry 中的公开入口保持稳定。
