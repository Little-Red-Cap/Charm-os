import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SESSION_SCHEMA = "minimal_kernel.kernel_runtime_session/v0"
SESSION_KIND = "minimal_kernel.kernel_runtime_session"
SESSION_SCHEMA_PATH = "schemas/minimal_kernel.kernel_runtime_session.v0.schema.json"

DEFAULT_CONTRACTS = [
    "minimal_kernel_runtime_bridge",
    "minimal_kernel_trap_ingress",
    "minimal_kernel_task_syscall_frame",
    "minimal_kernel_task_message_session",
]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def as_int(value: Any, default: int = 0) -> int:
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def status_count_ok(view: dict[str, Any]) -> bool:
    status = as_dict(view.get("status"))
    return (
        as_int(status.get("ok"), 0) > 0
        and as_int(status.get("fail"), 0) == 0
        and as_int(status.get("other"), 0) == 0
    )


def normalize_token(value: Any) -> str:
    return str(value or "").strip().lower().replace("-", "_")


def result_name_set(results: list[Any], status: str = "ok") -> set[str]:
    names: set[str] = set()
    for result in results:
        entry = as_dict(result)
        if str(entry.get("status", "")).lower() != status:
            continue
        names.add(normalize_token(entry.get("case")))
        names.add(normalize_token(entry.get("label")))
        canonical = as_dict(entry.get("canonical_case"))
        names.add(normalize_token(canonical.get("seam")))
    return {name for name in names if name}


def standing_cases(results: list[Any]) -> list[str]:
    cases: list[str] = []
    for result in results:
        entry = as_dict(result)
        if str(entry.get("status", "")).lower() == "ok":
            cases.append(str(entry.get("case") or entry.get("label") or "unknown"))
    return cases


def regressed_cases(results: list[Any]) -> list[str]:
    cases: list[str] = []
    for result in results:
        entry = as_dict(result)
        if str(entry.get("status", "")).lower() != "ok":
            cases.append(str(entry.get("case") or entry.get("label") or "unknown"))
    return cases


def add_failure(
    failures: list[dict[str, Any]],
    code: str,
    domain: str,
    layer: str,
    focus: list[str],
    phase: str | None,
    message: str,
    required: bool = True,
) -> None:
    failures.append(
        {
            "code": code,
            "domain": domain,
            "layer": layer,
            "focus": focus,
            "required": required,
            "phase": phase,
            "message": message,
        }
    )


def derive_subject(evidence: dict[str, Any], canonical_world: dict[str, Any]) -> dict[str, str]:
    qemu = as_dict(evidence.get("qemu"))
    lower = as_dict(qemu.get("lower_half"))
    qemu_world = as_dict(lower.get("canonical_world"))
    canonical_subject = as_dict(canonical_world.get("subject"))

    board = str(canonical_subject.get("board") or "").strip()
    if not board:
        environment = str(qemu_world.get("environment") or "").strip()
        board = "armv7a_qemu" if "qemu" in environment else "armv7a_qemu"

    profile = str(canonical_subject.get("profile") or qemu_world.get("profile") or "debug").strip()
    if profile == "runtime-lower-half":
        profile = "debug"

    return {
        "board": board,
        "profile": profile,
        "leaf": "Examples/kernel/armv7a/qemu",
    }


def build_runtime_ledger(
    session_id: str,
    source_summary: Path,
    host: dict[str, Any],
    lower: dict[str, Any],
    semantic_ok: bool,
    machine_ok: bool,
    runtime_facts: dict[str, bool],
    failures: list[dict[str, Any]],
) -> dict[str, Any]:
    events: list[dict[str, Any]] = []

    def push(phase: str, domain: str, status: str, source: str | None, focus: list[str]) -> None:
        events.append(
            {
                "index": len(events),
                "phase": phase,
                "domain": domain,
                "status": status,
                "source": source,
                "focus": focus,
            }
        )

    cold = as_dict(host.get("cold"))
    warm = as_dict(host.get("warm"))
    push(
        "semantic.host.cold",
        "semantic",
        "standing" if status_count_ok(cold) else "missing",
        cold.get("summary_path"),
        ["host", "cold"],
    )
    push(
        "semantic.host.warm",
        "semantic",
        "standing" if status_count_ok(warm) else "missing",
        warm.get("summary_path"),
        ["host", "warm"],
    )
    push(
        "machine.qemu.lower_half",
        "machine",
        "standing" if machine_ok else "missing",
        lower.get("summary_path"),
        ["qemu", "lower-half"],
    )

    for key, focus in [
        ("tick", ["timer", "tick"]),
        ("trap", ["trap", "svc"]),
        ("thread", ["thread", "context"]),
        ("task_syscall", ["task", "syscall"]),
        ("handoff_continuity", ["handoff", "continuity"]),
    ]:
        push(
            f"runtime.{key}",
            "runtime",
            "standing" if runtime_facts.get(key, False) else "missing",
            lower.get("summary_path"),
            focus,
        )

    push(
        "session.verdict",
        "bundle",
        "standing" if semantic_ok and machine_ok and not failures else "collapsed",
        str(source_summary),
        ["session"],
    )

    return {
        "schema": "minimal_kernel.kernel_runtime_session.runtime_ledger/v0",
        "generated_at": utc_now(),
        "session_id": session_id,
        "source_summary": str(source_summary),
        "events": events,
    }


def validate_schema(repo_root: Path, summary: dict[str, Any]) -> None:
    try:
        import jsonschema
    except ImportError as exc:
        raise RuntimeError("jsonschema is required. Install it with: python -m pip install jsonschema") from exc

    schema = load_json(repo_root / SESSION_SCHEMA_PATH)
    jsonschema.validate(summary, schema)


def render_report(summary: dict[str, Any]) -> str:
    failures = as_list(summary.get("failures"))
    machine = as_dict(summary.get("machine_witness"))
    semantic = as_dict(summary.get("semantic_witness"))
    runtime = as_dict(summary.get("runtime"))
    ledger = as_dict(summary.get("ledger"))

    lines = [
        "# Kernel Runtime Session Witness Report",
        "",
        f"- Session: `{summary['session_id']}`",
        f"- World: `{summary['world']}`",
        f"- Status: `{as_dict(summary['verdict']).get('session_status')}`",
        f"- Failure domain: `{as_dict(summary['verdict']).get('failure_domain')}`",
        f"- Summary JSON: `{as_dict(summary['artifact_paths']).get('summary')}`",
        f"- Runtime ledger: `{ledger.get('runtime_ledger')}`",
        "",
        "## Semantic Witness",
        "",
        f"- Host: `{semantic.get('host')}`",
        f"- Status: `{semantic.get('status')}`",
        f"- Contracts: `{', '.join(str(item) for item in as_list(semantic.get('contracts')))}`",
        "",
        "## Machine Witness",
        "",
        f"- QEMU: `{machine.get('qemu')}`",
        f"- Status: `{machine.get('status')}`",
        f"- Standing cases: `{', '.join(str(item) for item in as_list(machine.get('standing_cases')))}`",
        f"- Regressed cases: `{', '.join(str(item) for item in as_list(machine.get('regressed_cases')))}`",
        "",
        "## Runtime Facts",
        "",
    ]

    for key in ["tick", "trap", "thread", "task_syscall", "handoff_continuity"]:
        lines.append(f"- `{key}`: `{runtime.get(key)}`")

    lines += ["", "## Failures", ""]
    if not failures:
        lines.append("- none")
    else:
        for failure in failures:
            entry = as_dict(failure)
            lines.append(
                f"- `{entry.get('code')}` domain=`{entry.get('domain')}` "
                f"phase=`{entry.get('phase')}` focus=`{','.join(str(item) for item in as_list(entry.get('focus')))}`"
            )
            lines.append(f"  message: `{entry.get('message')}`")

    return "\n".join(lines) + "\n"


def render_check(summary: dict[str, Any]) -> str:
    verdict = as_dict(summary.get("verdict"))
    failures = as_list(summary.get("failures"))
    ledger = as_dict(summary.get("ledger"))
    return "\n".join(
        [
            f"summary: {as_dict(summary.get('artifact_paths')).get('summary')}",
            f"session_id: {summary.get('session_id')}",
            f"session_status: {verdict.get('session_status')}",
            f"failure_domain: {verdict.get('failure_domain')}",
            f"failure_count: {len(failures)}",
            f"runtime_ledger: {ledger.get('runtime_ledger')}",
            f"runtime_ledger_events: {ledger.get('event_count')}",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Export a minimal kernel runtime session witness object.")
    parser.add_argument(
        "--runtime-evidence-summary",
        default="out/minimal-kernel-runtime-evidence/summary.json",
        help="Runtime evidence bundle summary.json used as the source fact object.",
    )
    parser.add_argument(
        "--canonical-world",
        default="Examples/kernel/canonical_worlds/minimal_kernel_runtime.world.json",
        help="Canonical world JSON used for world/subject provenance. Pass an empty string to skip.",
    )
    parser.add_argument(
        "--output-root",
        default="out/minimal-kernel-runtime-evidence/session",
        help="Output directory for session artifacts.",
    )
    parser.add_argument("--summary-path", default="", help="Explicit kernel_runtime_session.summary.json path.")
    parser.add_argument("--runtime-ledger-path", default="", help="Explicit runtime_ledger.json path.")
    parser.add_argument("--report-path", default="", help="Explicit report.md path.")
    parser.add_argument("--check-path", default="", help="Explicit check.txt path.")
    parser.add_argument("--session-id", default="minimal_kernel_runtime.armv7a_qemu.debug")
    parser.add_argument("--validate", action="store_true", help="Validate the exported session summary against schema.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    source_summary_path = Path(args.runtime_evidence_summary).resolve()
    output_root = Path(args.output_root).resolve()
    summary_path = Path(args.summary_path).resolve() if args.summary_path else output_root / "kernel_runtime_session.summary.json"
    runtime_ledger_path = (
        Path(args.runtime_ledger_path).resolve() if args.runtime_ledger_path else output_root / "runtime_ledger.json"
    )
    report_path = Path(args.report_path).resolve() if args.report_path else output_root / "report.md"
    check_path = Path(args.check_path).resolve() if args.check_path else output_root / "check.txt"

    canonical_world_path: Path | None = None
    canonical_world: dict[str, Any] = {}
    if args.canonical_world.strip():
        canonical_world_path = Path(args.canonical_world).resolve()

    failures: list[dict[str, Any]] = []

    try:
        evidence = load_json(source_summary_path)
    except Exception as exc:
        print(f"[ERROR] failed to read runtime evidence summary: {exc}", file=sys.stderr)
        return 1

    if canonical_world_path is not None:
        try:
            canonical_world = load_json(canonical_world_path)
        except Exception as exc:
            add_failure(
                failures,
                "world_contract_missing",
                "world",
                "bundle",
                ["world", "contract"],
                "world.load",
                f"canonical world could not be loaded: {exc}",
            )

    host = as_dict(evidence.get("host"))
    cold = as_dict(host.get("cold"))
    warm = as_dict(host.get("warm"))
    host_exit_ok = as_int(host.get("bundle_exit_code"), -1) == 0
    semantic_ok = bool(host) and host_exit_ok and status_count_ok(cold) and status_count_ok(warm)
    if not semantic_ok:
        add_failure(
            failures,
            "host_semantic_mismatch",
            "semantic",
            "upper_half",
            ["host", "semantic"],
            "semantic.host",
            "host cold/warm semantic witness is missing or not standing",
        )

    qemu = as_dict(evidence.get("qemu"))
    lower = as_dict(qemu.get("lower_half"))
    results = as_list(lower.get("results"))
    qemu_exit_ok = as_int(qemu.get("bundle_exit_code"), -1) == 0
    lower_complete = as_int(lower.get("completed_case_count"), -1) == as_int(lower.get("case_count"), -2)
    lower_status_ok = status_count_ok(lower)
    machine_ok = bool(qemu) and bool(lower) and qemu_exit_ok and lower_complete and lower_status_ok
    if not machine_ok:
        add_failure(
            failures,
            "machine_witness_missing",
            "machine",
            "lower_half",
            ["qemu", "lower-half"],
            "machine.qemu",
            "QEMU lower-half witness is missing, incomplete, or not standing",
        )

    ok_names = result_name_set(results, status="ok")
    regressed = regressed_cases(results)

    trap_ingress = bool(ok_names & {"runtime_trap", "trap_ingress", "task_syscall"})
    runtime_loop = bool(ok_names & {"runtime_live", "live_runtime"})
    timer_ingress = runtime_loop
    interrupt_ingress = runtime_loop or bool(ok_names & {"runtime_leaf_ports", "leaf_ports"})
    context_ingress = bool(ok_names & {"runtime_thread", "thread_egress", "task_syscall", "handoff_live"})
    thread_fact = bool(ok_names & {"runtime_thread", "thread_egress"})
    task_syscall_fact = bool(ok_names & {"task_syscall"})
    handoff_seen = "handoff_live" in ok_names or any(normalize_token(item) == "handoff_live" for item in regressed)
    handoff_ok = "handoff_live" in ok_names or "handoff_landing" in ok_names

    runtime_facts = {
        "tick": timer_ingress,
        "trap": trap_ingress,
        "thread": thread_fact,
        "task_syscall": task_syscall_fact,
        "handoff_continuity": handoff_ok,
    }

    if not trap_ingress:
        add_failure(
            failures,
            "trap_not_observed",
            "machine",
            "lower_half",
            ["trap", "svc"],
            "runtime.trap",
            "trap ingress was not observed in the standing lower-half cases",
        )
    if not timer_ingress:
        add_failure(
            failures,
            "tick_not_observed",
            "machine",
            "lower_half",
            ["timer", "tick"],
            "runtime.tick",
            "timer tick was not observed in the standing lower-half cases",
        )
    if not thread_fact:
        add_failure(
            failures,
            "thread_not_resumed",
            "runtime",
            "lower_half",
            ["thread", "context"],
            "runtime.thread",
            "runtime thread witness was not observed in the standing lower-half cases",
        )
    if not task_syscall_fact:
        add_failure(
            failures,
            "decode_failed",
            "runtime",
            "lower_half",
            ["task", "syscall"],
            "runtime.task_syscall",
            "task syscall witness was not decoded from the standing lower-half cases",
        )
    if not handoff_ok:
        add_failure(
            failures,
            "handoff_continuity_broken" if handoff_seen else "handoff_not_landed",
            "runtime",
            "lower_half",
            ["handoff", "session"],
            "handoff.live",
            "handoff launch/landing continuity was not proven by the standing lower-half cases",
        )

    world_name = str(canonical_world.get("name") or "").strip()
    if not world_name:
        qemu_world = as_dict(lower.get("canonical_world"))
        world_name = str(qemu_world.get("id") or "minimal_kernel_runtime")

    runtime_ledger = build_runtime_ledger(
        session_id=args.session_id,
        source_summary=source_summary_path,
        host=host,
        lower=lower,
        semantic_ok=semantic_ok,
        machine_ok=machine_ok,
        runtime_facts=runtime_facts,
        failures=failures,
    )

    session_status = "standing" if not failures else "collapsed"
    failure_domain = None if not failures else failures[0]["domain"]

    summary = {
        "schema": SESSION_SCHEMA,
        "kind": SESSION_KIND,
        "generated_at": utc_now(),
        "session_id": args.session_id,
        "world": world_name,
        "subject": derive_subject(evidence, canonical_world),
        "semantic_witness": {
            "host": bool(host),
            "status": "standing" if semantic_ok else ("degraded" if host else "missing"),
            "contracts": DEFAULT_CONTRACTS,
            "source_summary": str(Path(str(host.get("output_root"))).resolve()) if host.get("output_root") else None,
            "cold_summary": str(cold.get("summary_path")) if cold.get("summary_path") else None,
            "warm_summary": str(warm.get("summary_path")) if warm.get("summary_path") else None,
        },
        "machine_witness": {
            "qemu": bool(qemu),
            "status": "standing" if machine_ok else ("degraded" if qemu else "missing"),
            "source_summary": str(lower.get("summary_path")) if lower.get("summary_path") else None,
            "exception_ingress": trap_ingress,
            "interrupt_ingress": interrupt_ingress,
            "timer_ingress": timer_ingress,
            "trap_ingress": trap_ingress,
            "context_ingress": context_ingress,
            "runtime_loop": runtime_loop,
            "standing_cases": standing_cases(results),
            "regressed_cases": regressed,
        },
        "runtime": runtime_facts,
        "ledger": {
            "phase_ledger": None,
            "runtime_ledger": str(runtime_ledger_path),
            "event_count": len(runtime_ledger["events"]),
        },
        "verdict": {
            "session_status": session_status,
            "failure_domain": failure_domain,
        },
        "failures": failures,
        "artifact_paths": {
            "summary": str(summary_path),
            "runtime_ledger": str(runtime_ledger_path),
            "report": str(report_path),
            "check": str(check_path),
            "source_runtime_evidence": str(source_summary_path),
        },
        "provenance": {
            "runtime_evidence_summary": str(source_summary_path),
            "canonical_world": str(canonical_world_path) if canonical_world_path else None,
        },
    }

    try:
        if args.validate:
            validate_schema(repo_root, summary)
        write_json(runtime_ledger_path, runtime_ledger)
        write_json(summary_path, summary)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(render_report(summary), encoding="utf-8")
        check_path.parent.mkdir(parents=True, exist_ok=True)
        check_path.write_text(render_check(summary), encoding="utf-8")
    except Exception as exc:
        print(f"[ERROR] failed to export runtime session: {exc}", file=sys.stderr)
        return 1

    print(f"[OK] session summary -> {summary_path}")
    print(f"[OK] runtime ledger -> {runtime_ledger_path}")
    print(f"[OK] verdict -> {session_status}")
    return 0 if session_status == "standing" else 1


if __name__ == "__main__":
    raise SystemExit(main())
