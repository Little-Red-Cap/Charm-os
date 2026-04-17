# 鏈€灏忓唴鏍?task syscall frame 濂戠害锛堣崏妗堬級

杩欎唤鏂囨。鐢ㄤ簬鎶娾€滄渶灏?syscall handler table 宸茬粡瀛樺湪浠ュ悗锛屾€庢牱鎶婁竴涓灦鏋勬棤鍏崇殑 numbered syscall frame 绋冲畾鍦版帴杩涙潵鈥濆崟鐙敹鍙ｃ€?
瀹冨搴斿綋鍓嶆柊澧炵殑锛?
- `Modules/system/kernel/task_syscall_frame.cppm`

鐩爣涓嶆槸鐜板湪灏辨壙璇虹湡瀹?ARMv7-A SVC frame锛屼篃涓嶆槸椹笂瀹氫箟瀹屾暣鐢ㄦ埛鎬?syscall ABI锛岃€屾槸鍏堟妸涓嬮潰杩欐潯閾捐矾鍋氭垚涓€鏉¤杽鑰岀ǔ銆佸彲瑙傚療銆佸彲鍥炲綊鐨勬ˉ闈細

- `frame -> TaskSyscallRequest -> TaskSyscallTable -> TrapResult -> frame writeback`

## 涓€鍙ヨ瘽鐗堟湰

- `TaskSyscallTable` 璐熻矗鈥滀竴涓?syscall 鍙峰簲璇ヨ惤鍒板摢涓€涓?handler鈥?- `TaskSyscallFrame` 璐熻矗鈥滀竴涓渶灏?numbered frame 鎬庢牱 decode銆乨ispatch銆佸啀 writeback鈥?
鍓嶈€呮槸闈欐€佽〃锛屽悗鑰呮槸 frame 妗ャ€?
## 涓轰粈涔堢幇鍦ㄥ€煎緱鍔犺繖涓€灞?
褰撳墠涓婂崐灞傚凡缁忔湁锛?
- `TaskSyscallApi`
- `TaskSyscallCatalog`
- `TaskSyscallDispatch`
- `TaskSyscallTable`
- `RuntimeTrapIngress`

杩欏凡缁忚冻澶熻〃杈撅細

- task 渚?syscall-facing 鍛藉悕
- syscall 鍙峰拰 trap service 鐨勫叧绯?- request 鎬庢牱钀藉埌 surface / handler
- trap frame ingress 鎬庢牱璧?`TrapFrameView`

浣嗗湪 `TaskSyscallTable` 鍜岀湡瀹?arch frame 涔嬮棿锛岃繕缂轰竴灞傚緢鍏抽敭鐨勨€滀腑闂存ˉ鈥濓細

- 瀹冧笉鐩存帴鐞嗚В ARMv7-A 淇濆瓨甯冨眬
- 瀹冧篃涓嶉噸鏂板彂鏄庣浜屽 request/result 鍗忚
- 瀹冨彧璐熻矗鎶婁竴涓渶灏?numbered syscall frame 绋冲畾鍦版敹鎴?`TaskSyscallRequest`

濡傛灉杩欎竴灞備笉鍗曠嫭鏀跺嚭鏉ワ紝鍚庨潰涓嶅悓 host verifier銆乫uture arch ingress銆乫uture user boundary 寰堝鏄撳張鍚勮嚜缁存姢涓€浠?鈥渟yscall 鍙峰湪鍝釜妲介噷銆佸弬鏁版€庝箞閲囥€佺粨鏋滄€庝箞鍥炲啓鈥?鐨勯噸澶嶉€昏緫銆?
## 妯″潡浣嶇疆涓庡叧绯?
妯″潡浣嶇疆锛?
- `Modules/system/kernel/task_syscall_frame.cppm`

褰撳墠寤鸿鍏崇郴鏄細

1. `kernel.task_syscall_api`
   - task-facing syscall 鍛藉悕闈?2. `kernel.task_syscall_catalog`
   - syscall 鍙?/ trap service / 璇箟鐩綍
3. `kernel.task_syscall_dispatch`
   - request -> transport / handler surface
4. `kernel.task_syscall_table`
   - syscall 鍙?-> 闈欐€?handler 琛?5. `kernel.task_syscall_frame`
   - numbered frame -> request -> table -> writeback
6. `kernel.runtime_trap_ingress`
   - 鐪熷疄 trap frame / arch ingress -> `TrapFrameView`

杩欐剰鍛崇潃锛?
- `TaskSyscallFrame` 涓嶅彇浠?`RuntimeTrapIngress`
- `TaskSyscallFrame` 涔熶笉鍥炲啓 `TaskSyscallTable`
- 瀹冨彧鏄妸鈥滄渶灏?syscall 鍙?frame鈥濇敹鎴愪竴鏉＄嫭绔嬫ˉ闈?
## 褰撳墠鏍稿績绫诲瀷

褰撳墠鏂板鐨勬牳蹇冪被鍨嬩笌鍑芥暟鏄細

- `TaskSyscallFrameStage`
- `task_syscall_frame_stage_name(...)`
- `TaskSyscallFrameView`
- `task_syscall_frame_view_ready(...)`
- `task_syscall_frame_view_from_request(...)`
- `task_syscall_request_from_frame_view(...)`
- `task_syscall_frame_view_decode(TrapRequest, ...)`
- `task_syscall_frame_view_decode(TrapFrameView, ...)`
- `TaskSyscallFrameAdapter<Frame>`
- `task_syscall_frame_adapter_ready(...)`
- `TaskSyscallFrameIngressAdapter<Frame>`
- `task_syscall_frame_ingress_adapter_ready(...)`
- `TaskSyscallFrameTraceEvent`
- `TaskSyscallFrameTraceBuffer<Capacity>`
- `TaskSyscallFrameBridge<Table, Frame, TraceBuffer>`
- `TaskSyscallFramePort<Frame>`
- `TaskSyscallCallFrameAdapter<Frame>`
- `TaskSyscallTrapCallFrameAdapter<Frame>`
- `TaskSyscallFrameCaller<Frame, Tick>`

- `make_task_syscall_frame_ingress_adapter(...)`
- `make_task_syscall_frame_adapter(...)`
- `make_task_syscall_call_frame_adapter(...)`
- `make_task_syscall_frame_bridge(...)`
- `make_task_syscall_frame_port(...)`
- `make_task_syscall_frame_caller(...)`

## 褰撳墠鏈€灏?frame 瑙嗗浘

`TaskSyscallFrameView` 褰撳墠鍙〃杈炬渶灏?numbered syscall frame 鎵€闇€瀛楁锛?
- `syscall`
- `arg0`
- `arg1`
- `arg2`
- `arg3`

瀹冩湁鎰忎笉鍦ㄨ繖涓€灞傚紩鍏ワ細

- 鐪熷疄瀵勫瓨鍣ㄤ繚瀛樺竷灞€
- `TrapOrigin`
- `return_pc / status / stack_pointer`
- 鐢ㄦ埛鎬佸湴鍧€绌洪棿鎴栨寚閽堣涔?
杩欎簺浠嶇劧搴旇缁х画鐣欑粰 arch ingress 鎴?future user ABI銆?
鍚屾椂杩欏眰鐜板湪涔熸彁渚涗簡涓ゆ潯涓撻棬缁?future ingress adapter 鐢ㄧ殑鍏叡杞崲锛?
- `task_syscall_frame_view_decode(const TrapRequest&, ...)`
- `task_syscall_frame_view_decode(const TrapFrameView&, ...)`

瀹冧滑鐨勬剰涔夋槸鎶娾€滃凡缁忚 lower-half 鏀舵垚 generic trap request / trap frame view 鐨勪笢瑗库€濊繘涓€姝ョǔ瀹氬湴瑙ｆ垚 `TaskSyscallFrameView`锛岃€屼笉鏄姣忎釜 leaf adapter 鍐嶅悇鑷淮鎶や竴浠?syscall 鍙锋槧灏勩€?
鍐嶅線鍓嶄竴姝ワ紝杩欏眰鐜板湪杩樻彁渚涗簡涓€涓洿鐩存帴鐨勫叕鍏辨嫾鎺ヤ欢锛?
- `TaskSyscallFrameIngressAdapter<Frame>`
- `make_task_syscall_frame_ingress_adapter(RuntimeTrapFrameAdapter<Frame>)`
- `make_task_syscall_frame_adapter(ingress_adapter)`
- `make_task_syscall_frame_adapter(RuntimeTrapFrameAdapter<Frame>&)`
- `make_task_syscall_frame_bridge(table, RuntimeTrapFrameAdapter<Frame>&, trace)`
- `make_task_syscall_frame_bridge(table, TaskSyscallFrameIngressAdapter<Frame>&, trace)`
- `make_task_syscall_call_frame_adapter(TaskSyscallTrapCallFrameAdapter<Frame>&)`
- `make_task_syscall_frame_caller(port, TaskSyscallTrapCallFrameAdapter<Frame>&)`

这次额外补的三条 helper，目标是把“leaf 已经持有稳定的 trap adapter / ingress adapter”这条接法进一步压薄：

- `make_task_syscall_frame_adapter(RuntimeTrapFrameAdapter<Frame>&)`
  - 把 leaf 已稳定持有的 trap adapter 直接封成 `TaskSyscallFrameAdapter<Frame>`。
- `make_task_syscall_frame_bridge(table, RuntimeTrapFrameAdapter<Frame>&, trace)`
  - 基于稳定 trap adapter 直接拼出 `TaskSyscallFrameBridge`。
- `make_task_syscall_frame_bridge(table, TaskSyscallFrameIngressAdapter<Frame>&, trace)`
  - 当 leaf 已经先显式持有 ingress adapter 时，避免再手写一层 `make_task_syscall_frame_adapter(...)` glue。

杩欓噷鏁呮剰閮藉彧鏀?lvalue reference锛岃€屼笉鏄复鏃跺璞°€傚師鍥犲緢绠€鍗曪細

- `TaskSyscallFrameAdapter<Frame>` 褰撳墠浠嶇劧閫氳繃 `ctx` 鎸囬拡鍥炴寚澶栭儴 adapter 瀛樺偍
- 濡傛灉 helper 鍋峰伔鎺ュ彈涓存椂 `RuntimeTrapFrameAdapter<Frame>` 鎴栦复鏃?ingress adapter锛屽氨浼氭妸 `ctx` 鎸囧埌鐢熷懡鍛ㄦ湡宸茬粡缁撴潫鐨勫璞?- 鎵€浠ュ叕鍏?glue 杩欓噷鏄庣‘瑕佹眰锛歵rap adapter / ingress adapter 鐨勫瓨鍌ㄥ繀椤荤敱 leaf 鎴?verifier 鑷繁鎸佹湁锛屽苟涓旇嚦灏戞椿鍒?bridge 鐢ㄥ畬涓烘

瀹冪殑浣滅敤鏄妸鐜版湁 `RuntimeTrapFrameAdapter<Frame>` 鐩存帴鍖呮垚 `TaskSyscallFrameAdapter<Frame>`锛岃繖鏍?leaf 鍙宸茬粡鏈夛細

- 鐪熷疄 frame -> `TrapFrameView`
- `TrapResult` -> 鐪熷疄 frame writeback

灏变笉闇€瑕佸啀鎵嬪啓绗簩浠?syscall-frame capture/apply glue銆?
## 褰撳墠 adapter 璐ｄ换

`TaskSyscallFrameAdapter<Frame>` 褰撳墠鍙仛涓や欢浜嬶細

- `capture(ctx, frame, out_view)`
- `apply_result(ctx, frame, result)`

涔熷氨鏄锛岃繖灞傛ˉ涓嶇洿鎺ョ悊瑙ｅ叿浣?frame 缁撴瀯锛岃€屾槸鎶娾€滃浣曚粠鏌愪釜 frame 鎶藉彇 syscall 缂栧彿涓庡弬鏁扳€濃€滃浣曟妸缁撴灉鍐欏洖鏌愪釜 frame鈥濋兘浜ょ粰 adapter銆?
杩欒瀹冨ぉ鐒跺彲浠ュ鎺ワ細

- host verifier 閲岀殑 fake frame
- future arch-neutral stub frame
- 浠ュ悗鐪熷疄 ARMv7-A / AArch64 / 鍏跺畠 leaf target 鐨?syscall frame adapter

## 褰撳墠 task-side caller

`TaskSyscallFrameCaller` 表示 task-side 的最小 caller 闭环，负责把 task-facing syscall facade 变成可走 `TaskSyscallFramePort` 的 numbered frame 调用。

它当前覆盖两类输入：

- `TaskSyscallRequest`
- `sys_yield / sys_sleep_until / sys_debug_write / sys_capability_call`

最小调用路径保持为：

- `request -> frame builder -> TaskSyscallFramePort -> TrapResult`

caller 侧新增的 helper 放在这里理解更自然：

- `make_task_syscall_call_frame_adapter(TaskSyscallTrapCallFrameAdapter<Frame>&)`
  - 把 leaf 已稳定提供的 `TaskSyscallRequest -> TrapRequest` glue 和 `TrapRequest -> Frame` builder 组合成 `TaskSyscallCallFrameAdapter<Frame>`。
- `make_task_syscall_frame_caller(port, TaskSyscallTrapCallFrameAdapter<Frame>&)`
  - 在已有 trap-call builder 的前提下，直接生成可绑定到 `bind_runtime(...)` 或 task syscall facade 的 `TaskSyscallFrameCaller`。

这样我们同时覆盖两条证据路径：

- `frame -> request -> table -> writeback`
- `task-side syscall surface -> request -> frame`

## 褰撳墠 bridge 璐ｄ换

`TaskSyscallFrameBridge<Table, Frame, ...>` 褰撳墠鎸夊浐瀹氫笁娈佃蛋锛?
1. `decode`
   - 閫氳繃 adapter 鎶?`Frame` 閲囨垚 `TaskSyscallFrameView`
2. `dispatch`
   - 鎶?`TaskSyscallFrameView` 杞垚 `TaskSyscallRequest`
   - 鍐嶄氦缁?`TaskSyscallTable`
3. `writeback`
   - 鎶?`TrapResult` 鍥炲啓鍒板師濮?`Frame`

瀹冨綋鍓嶄笉璐熻矗锛?
- 鍔ㄦ€?syscall registry
- 鐪熷疄 trap frame decode
- trap origin / privilege 瑙ｉ噴
- 绗簩濂?errno / negative-return ABI

## 褰撳墠閿欒璇箟

杩欏眰浠嶇劧鐩存帴澶嶇敤锛?
- `TrapDisposition`
- `TrapError`
- `TrapResult`

褰撳墠鏈€灏忚鍒欐槸锛?
- adapter 鏈粦瀹?  - `TrapDisposition::rejected`
  - `TrapError::unbound_adapter`
- frame capture 澶辫触
  - `TrapDisposition::rejected`
  - `TrapError::decode_failed`
- table 鏈粦瀹?  - `TrapDisposition::rejected`
  - `TrapError::unbound_bridge`
- result writeback 澶辫触
  - `TrapDisposition::rejected`
  - `TrapError::writeback_failed`

杩欐剰鍛崇潃 frame bridge 渚濈劧娌℃湁寮曞叆绗簩濂?frame-specific result 鍗忚銆?
## 褰撳墠 observability

褰撳墠 frame bridge 鑷甫鐙珛 trace锛?
- `TaskSyscallFrameTraceBuffer`

姣忔潯 trace 鑷冲皯璁板綍锛?
- `sequence`
- `stage`
- `syscall`
- `trap_service`
- `disposition`
- `error`
- `arg0..arg3`
- `value`
- `ok`

骞朵笖褰撳墠宸茬粡鏀寔锛?
- `task_syscall_frame_view_from_trace_event(event)`
- `task_syscall_request_from_trace_event(event)`
- `task_syscall_semantic_projection(event)`

涔熷氨鏄锛宖rame trace 鍜?dispatch/table trace 涓€鏍凤紝閮借兘琚噸鏂版姇褰卞洖鍚屼竴濂?task syscall 璇箟瀛楁銆?
## 涓庣幇鏈夊眰鐨勫垎宸?
褰撳墠寤鸿杩欐牱鍒嗭細

- `TaskSyscallDispatch`
  - 璐熻矗鈥滀竴涓?request 鎬庢牱钀藉埌涓€涓?surface鈥?- `TaskSyscallTable`
  - 璐熻矗鈥滀竴涓?syscall 鍙峰簲璇ヨ惤鍒板摢涓€涓?handler鈥?- `TaskSyscallFrame`
  - 璐熻矗鈥滀竴涓?numbered frame 鎬庢牱 decode銆乨ispatch銆佸啀 writeback鈥?- `RuntimeTrapIngress`
  - 璐熻矗鈥滅湡瀹?trap frame 鎬庢牱琚В閲婃垚鏋舵瀯鏃犲叧 trap 璇锋眰鈥?
杩欏嚑灞傛媶寮€浠ュ悗锛?
- request 璇箟
- handler table
- numbered frame writeback
- 鐪熷疄 trap ingress

灏变笉鍐嶈鎻夊洖鍚屼竴涓枃浠躲€?
## 褰撳墠璇佹嵁璺緞

褰撳墠涓庤繖灞傜洿鎺ョ浉鍏崇殑鐙珛璇佹嵁璺緞鏄細

- `Examples/kernel/runtime_task_syscall_frame_host`
- `Examples/kernel/runtime_task_syscall_frame_caller_host`

鏂板杩欎袱鏉¤瘉鎹悗锛屽彲浠ユ妸褰撳墠 verifier 鍏虫敞鐐规€荤粨鎴愶細

- `runtime_task_syscall_frame_host`
  - `RuntimeTrapFrameAdapter<Frame> -> TaskSyscallFrameIngressAdapter<Frame> -> TaskSyscallFrameAdapter<Frame>` 鐨勫叕鍏辨嫾鎺?  - `RuntimeTrapFrameAdapter<Frame>& -> TaskSyscallFrameAdapter<Frame>` 鐨勭洿鎺?helper
  - `RuntimeTrapFrameAdapter<Frame>& / TaskSyscallFrameIngressAdapter<Frame>& -> TaskSyscallFrameBridge` 鐨勭洿鎺?helper
- `runtime_task_syscall_frame_caller_host`
  - `TaskSyscallApi -> TaskSyscallFrameCaller -> TaskSyscallFramePort -> TaskSyscallFrameBridge -> TaskSyscallTable`
  - `TaskSyscallApi -> TaskSyscallFrameCaller -> generic trap frame -> TaskSyscallFrameBridge -> TaskSyscallTable`
  - `TaskSyscallTrapCallFrameAdapter<Frame>& -> TaskSyscallCallFrameAdapter<Frame>`
  - `TaskSyscallTrapCallFrameAdapter<Frame>& -> TaskSyscallFrameCaller<Frame, Tick>`
  - `bind_runtime(...)` 涓?builder/result-ready 璐熷悜璺緞

瀹冨綋鍓嶉獙璇侊細

- `TaskSyscallFrameView <-> TaskSyscallRequest` 鐨勬渶灏忚浆鎹?- frame -> table -> handler 鐨勬甯搁棴鐜?- table 娣峰悎鎸傛帴 dispatch bridge 涓庣洿杩?handler
- `RuntimeTrapFrameAdapter<Frame> -> TaskSyscallFrameIngressAdapter<Frame> -> TaskSyscallFrameAdapter<Frame>` 鐨勫叕鍏辨嫾鎺?- `unbound_adapter`
- `decode_failed`
- `writeback_failed`
- frame trace 鐨?stage / semantic projection

浠ュ強锛?
- `TaskSyscallApi -> TaskSyscallFrameCaller -> TaskSyscallFramePort -> TaskSyscallFrameBridge -> TaskSyscallTable`
- `bind_runtime(...)` 鎹㈡帴涓嶅悓 caller
- caller builder 澶辫触涓?caller result-ready 澶辫触鐨勮礋鍚戣矾寰?
## 褰撳墠闈炵洰鏍?
褰撳墠杩欏眰浠嶇劧涓嶅鐞嗭細

- 鐪熷疄 ARMv7-A SVC frame 褰㈢姸
- 鐪熷疄 trap origin / privilege 瑙ｉ噴
- 鐢ㄦ埛鎬佸湴鍧€绌洪棿涓庢寚閽堟牎楠?- 瀹屾暣 syscall ABI
- 鍔ㄦ€?syscall handler registry

瀹冨彧鏄厛鎶娾€滄渶灏?numbered syscall frame bridge鈥濈珛浣忥紝涓?future arch ingress 鎴?user ABI 鎻愪緵涓€涓洿骞插噣鐨勮惤鐐广€?
