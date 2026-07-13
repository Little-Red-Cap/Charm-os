# Kernel Runtime Session Witness v0

> `status`: `supporting`

`kernel_runtime_session` 汇总一次 minimal-kernel 的 Host 语义证据与 ARMv7-A QEMU 机器证据，供
witness/compare consumer 使用。它避免上层重新解析私有日志，但不替代
[`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)，
也不证明真实板运行。

```text
host semantic summary + QEMU machine summary
  -> session exporter
  -> kernel_runtime_session.summary.json + runtime_ledger.json
  -> witness / compare consumer
```

## 证据边界

- semantic witness 投影 Host cold/warm summary，只证明 runtime glue、trap/syscall、task message 与
  session API 的语义断言，不证明真实 ARM exception entry 或寄存器 writeback。
- machine witness 投影 QEMU lower-half summary，覆盖 exception、interrupt、timer、trap、context 与
  runtime loop；它不证明真机内存、时钟、外设、BootROM 或板级启动。
- runtime facts 是 exporter 对输入 summary 的投影，不因 schema 或 report 存在而成立。
- `handoff_continuity` 只是 session continuity fact；只有 handoff 开始承担 image、slot、rollback、
  boot medium 等契约时才重新划分边界。
- ledger 记录 exporter 已消费事实的顺序，不重判 session verdict。

## Ledger 边界

`runtime_ledger.json` 由同一 session exporter 从 runtime evidence summary 派生，不解析 Host/QEMU
raw log，也不拥有 verdict。其 event count 必须与 session summary 一致；phase、status 和顺序只由
exporter 决定。

`standing` 表示输入 fact 被接受，`missing` 表示输入不足且不单独决定 session 失败；`collapsed`
只反映 exporter 已判定失败的 session。`arch.ingress.seam` 是新 QEMU summary 的 preferred anchor，
旧输入缺失时下游不得推断为 standing。

消费者只能把 ledger 作为 supporting artifact 或 explain context，不得绕过 session verdict、修改
phase/status，或将 events 解释为 scheduler timeline、profiler 或新 failure taxonomy。Ledger 当前没有
独立 schema/validator/gate；提升为交换格式前必须另行定义兼容与失败语义。

## 事实源与失败

机器 shape 与派生规则由
[`minimal_kernel.kernel_runtime_session.v0.schema.json`](../../schemas/minimal_kernel.kernel_runtime_session.v0.schema.json)
和 [`export_minimal_kernel_runtime_session.py`](../../scripts/export_minimal_kernel_runtime_session.py) 定义。

失败必须保留 code、domain、layer、focus、phase 等结构化来源。consumer 不得从 message、Markdown report、
text check 或 raw log 发明 verdict；人读投影也不是独立运行证据。

## 验证

聚合入口为
[`minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/minimal_kernel_runtime_session_witness_smoke.ps1)。
参数与输出路径由脚本维护。没有当次 artifact 时，不能从本文或历史通过记录推断 Host、QEMU 或真实板状态。
