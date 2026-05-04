import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json"
SESSION_SCHEMA_PATH = "schemas/minimal_kernel.kernel_runtime_session.v0.schema.json"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def ensure_exists(path_value: str, label: str, errors: list[str]):
    if not isinstance(path_value, str) or not path_value.strip():
        errors.append(f"{label}: missing path")
        return

    path = Path(path_value)
    if not path.exists():
        errors.append(f"{label}: not found -> {path}")


def validate_host_view(view: dict | None, label: str, errors: list[str]):
    if not isinstance(view, dict):
        return

    ensure_exists(view.get("summary_path"), f"{label}.summary_path", errors)
    ensure_exists(view.get("inspect_json_path"), f"{label}.inspect_json_path", errors)
    ensure_exists(view.get("report_markdown_path"), f"{label}.report_markdown_path", errors)
    ensure_exists(view.get("check_text_path"), f"{label}.check_text_path", errors)

    comparison = view.get("comparison")
    if isinstance(comparison, dict):
        ensure_exists(comparison.get("baseline_summary_path"), f"{label}.comparison.baseline_summary_path", errors)


def validate_qemu_view(view: dict | None, label: str, errors: list[str]):
    if not isinstance(view, dict):
        return

    ensure_exists(view.get("summary_path"), f"{label}.summary_path", errors)
    ensure_exists(view.get("report_markdown_path"), f"{label}.report_markdown_path", errors)
    ensure_exists(view.get("check_text_path"), f"{label}.check_text_path", errors)

    for index, result in enumerate(view.get("results", [])):
        if not isinstance(result, dict):
            errors.append(f"{label}.results[{index}]: invalid result entry")
            continue

        case_label = result.get("case") or f"case_{index}"
        ensure_exists(result.get("stdout_log_path"), f"{label}.results[{case_label}].stdout_log_path", errors)
        ensure_exists(result.get("stderr_log_path"), f"{label}.results[{case_label}].stderr_log_path", errors)


def validate_witness_bundle_view(view: dict | None, label: str, errors: list[str]):
    if not isinstance(view, dict):
        return

    ensure_exists(view.get("canonical_world_path"), f"{label}.canonical_world_path", errors)
    ensure_exists(view.get("output_root"), f"{label}.output_root", errors)
    ensure_exists(view.get("bundle_log_path"), f"{label}.bundle_log_path", errors)
    ensure_exists(view.get("summary_path"), f"{label}.summary_path", errors)
    ensure_exists(view.get("report_markdown_path"), f"{label}.report_markdown_path", errors)
    ensure_exists(view.get("check_text_path"), f"{label}.check_text_path", errors)


def validate_session_summary_view(view: dict | None, label: str, errors: list[str]):
    if not isinstance(view, dict):
        return

    ensure_exists(view.get("summary_path"), f"{label}.summary_path", errors)
    ensure_exists(view.get("runtime_ledger_path"), f"{label}.runtime_ledger_path", errors)
    ensure_exists(view.get("check_text_path"), f"{label}.check_text_path", errors)


def validate_runtime_ledger(ledger: dict, session_id: str, label: str, errors: list[str]):
    if ledger.get("schema") != "minimal_kernel.kernel_runtime_session.runtime_ledger/v0":
        errors.append(f"{label}.schema: unsupported schema -> {ledger.get('schema')}")
    if ledger.get("kind") != "minimal_kernel.kernel_runtime_session.runtime_ledger":
        errors.append(f"{label}.kind: unsupported kind -> {ledger.get('kind')}")
    if ledger.get("session_id") != session_id:
        errors.append(f"{label}.session_id: expected {session_id!r} but got {ledger.get('session_id')!r}")

    events = ledger.get("events")
    if not isinstance(events, list) or not events:
        errors.append(f"{label}.events: expected non-empty event list")
        return

    for index, event in enumerate(events):
        if not isinstance(event, dict):
            errors.append(f"{label}.events[{index}]: invalid event")
            continue
        if not isinstance(event.get("phase"), str) or not event.get("phase", "").strip():
            errors.append(f"{label}.events[{index}].phase: missing phase")
        if not isinstance(event.get("fact"), str) or not event.get("fact", "").strip():
            errors.append(f"{label}.events[{index}].fact: missing fact")
        if not isinstance(event.get("observed"), bool):
            errors.append(f"{label}.events[{index}].observed: expected boolean")


def validate_session_summary_content(
    view: dict | None,
    session_schema: dict,
    jsonschema_module,
    errors: list[str],
):
    if not isinstance(view, dict):
        return

    summary_path_value = view.get("summary_path")
    runtime_ledger_path_value = view.get("runtime_ledger_path")
    if not isinstance(summary_path_value, str) or not summary_path_value.strip():
        return

    summary_path = Path(summary_path_value)
    if not summary_path.exists():
        return

    try:
        session_summary = load_json(summary_path)
        jsonschema_module.validate(session_summary, session_schema)
    except Exception as exc:
        errors.append(f"session_summary.summary_path: invalid session summary -> {exc}")
        return

    if session_summary.get("session_id") != view.get("session_id"):
        errors.append(
            "session_summary.session_id: expected "
            f"{view.get('session_id')!r} but got {session_summary.get('session_id')!r}"
        )
    if session_summary.get("world") != view.get("world"):
        errors.append(f"session_summary.world: expected {view.get('world')!r} but got {session_summary.get('world')!r}")
    verdict = session_summary.get("verdict", {})
    if isinstance(verdict, dict) and verdict.get("session_status") != view.get("session_status"):
        errors.append(
            "session_summary.session_status: expected "
            f"{view.get('session_status')!r} but got {verdict.get('session_status')!r}"
        )
    if isinstance(verdict, dict):
        session_status = verdict.get("session_status")
        view_result = view.get("result")
        failure = verdict.get("failure")
        if view_result == "ok" and session_status != "standing":
            errors.append(
                "session_summary.result: result 'ok' requires session_status 'standing' "
                f"but got {session_status!r}"
            )
        if view_result == "fail" and session_status == "standing":
            errors.append("session_summary.result: result 'fail' cannot point at a standing session")
        if session_status == "standing" and failure is not None:
            errors.append("session_summary.verdict.failure: standing session must not carry failure")
        if session_status != "standing" and failure is None:
            errors.append("session_summary.verdict.failure: non-standing session must carry failure")

    ledger = session_summary.get("ledger", {})
    if isinstance(ledger, dict):
        runtime_ledger = ledger.get("runtime_ledger")
        if isinstance(runtime_ledger_path_value, str) and runtime_ledger_path_value.strip():
            if Path(str(runtime_ledger)).resolve() != Path(runtime_ledger_path_value).resolve():
                errors.append(
                    "session_summary.runtime_ledger_path: expected "
                    f"{runtime_ledger_path_value!r} but got {runtime_ledger!r}"
                )

    if isinstance(runtime_ledger_path_value, str) and runtime_ledger_path_value.strip():
        runtime_ledger_path = Path(runtime_ledger_path_value)
        if runtime_ledger_path.exists():
            try:
                validate_runtime_ledger(
                    load_json(runtime_ledger_path),
                    str(session_summary.get("session_id", "")),
                    "session_summary.runtime_ledger",
                    errors,
                )
            except Exception as exc:
                errors.append(f"session_summary.runtime_ledger_path: invalid runtime ledger -> {exc}")


def validate_references(summary: dict):
    errors: list[str] = []

    ensure_exists(summary.get("output_root"), "output_root", errors)
    ensure_exists(summary.get("report_markdown_path"), "report_markdown_path", errors)
    ensure_exists(summary.get("check_text_path"), "check_text_path", errors)

    host = summary.get("host")
    if isinstance(host, dict):
        ensure_exists(host.get("output_root"), "host.output_root", errors)
        ensure_exists(host.get("bundle_log_path"), "host.bundle_log_path", errors)
        validate_host_view(host.get("cold"), "host.cold", errors)
        validate_host_view(host.get("warm"), "host.warm", errors)

    qemu = summary.get("qemu")
    if isinstance(qemu, dict):
        ensure_exists(qemu.get("output_root"), "qemu.output_root", errors)
        ensure_exists(qemu.get("bundle_log_path"), "qemu.bundle_log_path", errors)
        validate_qemu_view(qemu.get("lower_half"), "qemu.lower_half", errors)

    validate_session_summary_view(summary.get("session_summary"), "session_summary", errors)
    validate_witness_bundle_view(summary.get("witness_bundle"), "witness_bundle", errors)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate minimal kernel runtime evidence bundle summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to summary.json. If omitted, --bundle-root/summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing summary.json.",
    )
    args = parser.parse_args()

    try:
        import jsonschema
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    if args.summary:
        summary_path = Path(args.summary).resolve()
    else:
        bundle_root = Path(args.bundle_root or "out/minimal-kernel-runtime-evidence").resolve()
        summary_path = bundle_root / "summary.json"

    schema_path = (repo_root / SCHEMA_PATH).resolve()
    session_schema_path = (repo_root / SESSION_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        schema = load_json(schema_path)
        session_schema = load_json(session_schema_path)
        jsonschema.validate(summary, schema)
        errors = validate_references(summary)
        validate_session_summary_content(summary.get("session_summary"), session_schema, jsonschema, errors)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] references -> {summary.get('output_root', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
