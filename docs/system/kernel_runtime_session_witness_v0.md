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

## 事实源与失败

机器 shape 与派生规则由
[`minimal_kernel.kernel_runtime_session.v0.schema.json`](../../schemas/minimal_kernel.kernel_runtime_session.v0.schema.json)
和 [`export_minimal_kernel_runtime_session.py`](../../scripts/export_minimal_kernel_runtime_session.py) 定义；ledger
语义见 [`minimal_kernel_runtime_ledger_fact_contract_v0.md`](minimal_kernel_runtime_ledger_fact_contract_v0.md)。

失败必须保留 code、domain、layer、focus、phase 等结构化来源。consumer 不得从 message、Markdown report、
text check 或 raw log 发明 verdict；人读投影也不是独立运行证据。

## 验证

聚合入口为
[`minimal_kernel_runtime_session_witness_smoke.ps1`](../../scripts/minimal_kernel_runtime_session_witness_smoke.ps1)。
参数与输出路径由脚本维护。没有当次 artifact 时，不能从本文或历史通过记录推断 Host、QEMU 或真实板状态。
