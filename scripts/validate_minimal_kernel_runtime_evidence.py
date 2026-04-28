import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json"


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

    try:
        summary = load_json(summary_path)
        schema = load_json(schema_path)
        jsonschema.validate(summary, schema)
        errors = validate_references(summary)
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
