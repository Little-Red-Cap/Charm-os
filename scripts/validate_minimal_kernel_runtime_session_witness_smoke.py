import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/minimal_kernel.runtime_session_witness_smoke.v0.schema.json"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def ensure_exists(path_value: str | None, label: str, errors: list[str]):
    if not isinstance(path_value, str) or not path_value.strip():
        errors.append(f"{label}: missing path")
        return

    if not Path(path_value).exists():
        errors.append(f"{label}: not found -> {path_value}")


def validate_artifact_paths(summary: dict):
    errors: list[str] = []

    ensure_exists(summary.get("output_root"), "output_root", errors)

    artifacts = summary.get("artifacts", {})
    if not isinstance(artifacts, dict):
        return ["artifacts: invalid object"]

    ensure_exists(artifacts.get("summary"), "artifacts.summary", errors)
    ensure_exists(artifacts.get("report_markdown"), "artifacts.report_markdown", errors)
    ensure_exists(artifacts.get("check_text"), "artifacts.check_text", errors)

    session = artifacts.get("session", {})
    if isinstance(session, dict):
        ensure_exists(session.get("output_root"), "artifacts.session.output_root", errors)
        ensure_exists(session.get("summary"), "artifacts.session.summary", errors)
        ensure_exists(session.get("runtime_ledger"), "artifacts.session.runtime_ledger", errors)
        ensure_exists(session.get("report_markdown"), "artifacts.session.report_markdown", errors)
        ensure_exists(session.get("check_text"), "artifacts.session.check_text", errors)

    world_compare = artifacts.get("world_compare_session_drift", {})
    if isinstance(world_compare, dict):
        ensure_exists(world_compare.get("output_root"), "artifacts.world_compare_session_drift.output_root", errors)
        ensure_exists(world_compare.get("summary"), "artifacts.world_compare_session_drift.summary", errors)
        ensure_exists(world_compare.get("report_markdown"), "artifacts.world_compare_session_drift.report_markdown", errors)
        ensure_exists(world_compare.get("check_text"), "artifacts.world_compare_session_drift.check_text", errors)

    witness_export = artifacts.get("witness_session_failure_export", {})
    if isinstance(witness_export, dict):
        ensure_exists(witness_export.get("output_root"), "artifacts.witness_session_failure_export.output_root", errors)
        ensure_exists(witness_export.get("baseline_summary"), "artifacts.witness_session_failure_export.baseline_summary", errors)
        ensure_exists(witness_export.get("candidate_summary"), "artifacts.witness_session_failure_export.candidate_summary", errors)
        ensure_exists(
            witness_export.get("world_compare_summary"),
            "artifacts.witness_session_failure_export.world_compare_summary",
            errors,
        )
        ensure_exists(
            witness_export.get("world_compare_report_markdown"),
            "artifacts.witness_session_failure_export.world_compare_report_markdown",
            errors,
        )
        ensure_exists(
            witness_export.get("world_compare_check_text"),
            "artifacts.witness_session_failure_export.world_compare_check_text",
            errors,
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate minimal kernel runtime session witness smoke summary and referenced artifacts."
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
        bundle_root = Path(args.bundle_root or "cmake-build-minimal-kernel-runtime-session-witness-smoke").resolve()
        summary_path = bundle_root / "summary.json"

    schema_path = (repo_root / SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        schema = load_json(schema_path)
        jsonschema.validate(summary, schema)
        errors = validate_artifact_paths(summary)
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
