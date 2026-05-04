import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/minimal_kernel.runtime_session_witness_inspect_compare.v0.schema.json"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def ensure_exists(path_value: str | None, label: str, errors: list[str]):
    if not isinstance(path_value, str) or not path_value.strip():
        errors.append(f"{label}: missing path")
        return

    if not Path(path_value).exists():
        errors.append(f"{label}: not found -> {path_value}")


def ensure_array(value, label: str, errors: list[str]):
    if not isinstance(value, list):
        errors.append(f"{label}: expected array")


def validate_references(summary: dict):
    errors: list[str] = []

    artifact_context = summary.get("artifact_context", {})
    if not isinstance(artifact_context, dict):
        return ["artifact_context: invalid object"]

    ensure_exists(artifact_context.get("summary_path"), "artifact_context.summary_path", errors)
    ensure_exists(artifact_context.get("baseline_summary_path"), "artifact_context.baseline_summary_path", errors)
    ensure_exists(artifact_context.get("compare_summary_path"), "artifact_context.compare_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)

    current = summary.get("current", {})
    if isinstance(current, dict):
        ensure_exists(current.get("summary_path"), "current.summary_path", errors)
        ensure_exists(current.get("output_root"), "current.output_root", errors)
        ensure_array(current.get("violations"), "current.violations", errors)

        artifacts = current.get("artifacts", {})
        if isinstance(artifacts, dict):
            ensure_exists(artifacts.get("summary"), "current.artifacts.summary", errors)
            ensure_exists(artifacts.get("report_markdown"), "current.artifacts.report_markdown", errors)
            ensure_exists(artifacts.get("check_text"), "current.artifacts.check_text", errors)

            session = artifacts.get("session", {})
            if isinstance(session, dict):
                ensure_exists(session.get("output_root"), "current.artifacts.session.output_root", errors)
                ensure_exists(session.get("summary"), "current.artifacts.session.summary", errors)
                ensure_exists(session.get("runtime_ledger"), "current.artifacts.session.runtime_ledger", errors)
                ensure_exists(session.get("report_markdown"), "current.artifacts.session.report_markdown", errors)
                ensure_exists(session.get("check_text"), "current.artifacts.session.check_text", errors)

            world_compare = artifacts.get("world_compare_session_drift", {})
            if isinstance(world_compare, dict):
                ensure_exists(
                    world_compare.get("output_root"),
                    "current.artifacts.world_compare_session_drift.output_root",
                    errors,
                )
                ensure_exists(
                    world_compare.get("summary"),
                    "current.artifacts.world_compare_session_drift.summary",
                    errors,
                )
                ensure_exists(
                    world_compare.get("report_markdown"),
                    "current.artifacts.world_compare_session_drift.report_markdown",
                    errors,
                )
                ensure_exists(
                    world_compare.get("check_text"),
                    "current.artifacts.world_compare_session_drift.check_text",
                    errors,
                )

            witness_export = artifacts.get("witness_session_failure_export", {})
            if isinstance(witness_export, dict):
                ensure_exists(
                    witness_export.get("output_root"),
                    "current.artifacts.witness_session_failure_export.output_root",
                    errors,
                )
                ensure_exists(
                    witness_export.get("baseline_summary"),
                    "current.artifacts.witness_session_failure_export.baseline_summary",
                    errors,
                )
                ensure_exists(
                    witness_export.get("candidate_summary"),
                    "current.artifacts.witness_session_failure_export.candidate_summary",
                    errors,
                )
                ensure_exists(
                    witness_export.get("world_compare_summary"),
                    "current.artifacts.witness_session_failure_export.world_compare_summary",
                    errors,
                )
                ensure_exists(
                    witness_export.get("world_compare_report_markdown"),
                    "current.artifacts.witness_session_failure_export.world_compare_report_markdown",
                    errors,
                )
                ensure_exists(
                    witness_export.get("world_compare_check_text"),
                    "current.artifacts.witness_session_failure_export.world_compare_check_text",
                    errors,
                )

    comparison = summary.get("comparison", {})
    if isinstance(comparison, dict):
        ensure_exists(comparison.get("summary_path"), "comparison.summary_path", errors)
        ensure_exists(comparison.get("baseline_summary_path"), "comparison.baseline_summary_path", errors)

        session = comparison.get("session", {})
        if isinstance(session, dict):
            runtime = session.get("runtime", {})
            if isinstance(runtime, dict):
                ensure_array(runtime.get("regressed"), "comparison.session.runtime.regressed", errors)
                ensure_array(runtime.get("improved"), "comparison.session.runtime.improved", errors)

        world_compare = comparison.get("world_compare_session_drift", {})
        if isinstance(world_compare, dict):
            for key in ("failure_codes", "missing_runtime_facts", "affected_focus"):
                delta = world_compare.get(key, {})
                if isinstance(delta, dict):
                    ensure_array(delta.get("added"), f"comparison.world_compare_session_drift.{key}.added", errors)
                    ensure_array(delta.get("removed"), f"comparison.world_compare_session_drift.{key}.removed", errors)

        witness_compare = comparison.get("witness_session_failure_export", {})
        if isinstance(witness_compare, dict):
            for key in ("failure_codes", "missing_runtime_facts", "affected_focus"):
                delta = witness_compare.get(key, {})
                if isinstance(delta, dict):
                    ensure_array(delta.get("added"), f"comparison.witness_session_failure_export.{key}.added", errors)
                    ensure_array(delta.get("removed"), f"comparison.witness_session_failure_export.{key}.removed", errors)

        violations = comparison.get("violations", {})
        if isinstance(violations, dict):
            ensure_array(violations.get("added"), "comparison.violations.added", errors)
            ensure_array(violations.get("removed"), "comparison.violations.removed", errors)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate minimal kernel runtime session witness inspect compare summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        required=True,
        help="Path to inspect-compare summary JSON.",
    )
    args = parser.parse_args()

    try:
        import jsonschema
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    summary_path = Path(args.summary).resolve()
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
    print(f"[OK] references -> {summary.get('artifact_context', {}).get('output_root', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
