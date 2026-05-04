import argparse
import sys
from pathlib import Path

from export_minimal_kernel_runtime_session_witness_inspect_compare_consumer import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


CONSUMER_SCHEMA_PATH = "schemas/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.schema.json"
COMPARE_SCHEMA_PATH = "schemas/minimal_kernel.runtime_session_witness_inspect_compare.v0.schema.json"


def ensure_exists(path_value: str | None, label: str, errors: list[str], required: bool = True) -> None:
    text = str(path_value or "").strip()
    if not text:
        if required:
            errors.append(f"{label}: missing path")
        return
    resolved = Path(text).resolve()
    if not resolved.exists():
        errors.append(f"{label}: not found -> {resolved}")


def ensure_array(value, label: str, errors: list[str]) -> None:
    if not isinstance(value, list):
        errors.append(f"{label}: expected array")


def expect_equal(actual, expected, label: str, errors: list[str]) -> None:
    if actual != expected:
        errors.append(f"{label}: expected {expected!r} but got {actual!r}")


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    ensure_exists(artifact_context.get("source_compare_summary_path"), "artifact_context.source_compare_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("consumer_summary_path"), "artifact_context.consumer_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    for index, ref in enumerate(summary.get("supporting_artifacts", [])):
        if not isinstance(ref, dict):
            errors.append(f"supporting_artifacts[{index}]: invalid artifact ref")
            continue
        ensure_exists(ref.get("path"), f"supporting_artifacts[{index}].path", errors)

    default_focus = summary.get("default_focus", {})
    if isinstance(default_focus, dict):
        for index, ref in enumerate(default_focus.get("artifact_refs", [])):
            if not isinstance(ref, dict):
                errors.append(f"default_focus.artifact_refs[{index}]: invalid artifact ref")
                continue
            ensure_exists(ref.get("path"), f"default_focus.artifact_refs[{index}].path", errors)

    for entry_index, entry in enumerate(summary.get("focus_entries", [])):
        if not isinstance(entry, dict):
            errors.append(f"focus_entries[{entry_index}]: invalid entry")
            continue
        ensure_array(entry.get("runtime_regressions"), f"focus_entries[{entry_index}].runtime_regressions", errors)
        ensure_array(entry.get("runtime_improvements"), f"focus_entries[{entry_index}].runtime_improvements", errors)
        ensure_array(entry.get("added_failure_codes"), f"focus_entries[{entry_index}].added_failure_codes", errors)
        ensure_array(entry.get("removed_failure_codes"), f"focus_entries[{entry_index}].removed_failure_codes", errors)
        ensure_array(entry.get("added_missing_runtime_facts"), f"focus_entries[{entry_index}].added_missing_runtime_facts", errors)
        ensure_array(entry.get("removed_missing_runtime_facts"), f"focus_entries[{entry_index}].removed_missing_runtime_facts", errors)
        ensure_array(entry.get("added_affected_focus"), f"focus_entries[{entry_index}].added_affected_focus", errors)
        ensure_array(entry.get("removed_affected_focus"), f"focus_entries[{entry_index}].removed_affected_focus", errors)
        ensure_array(entry.get("added_violations"), f"focus_entries[{entry_index}].added_violations", errors)
        ensure_array(entry.get("removed_violations"), f"focus_entries[{entry_index}].removed_violations", errors)
        ensure_array(entry.get("summary_lines"), f"focus_entries[{entry_index}].summary_lines", errors)
        ensure_array(entry.get("question_lines"), f"focus_entries[{entry_index}].question_lines", errors)
        for ref_index, ref in enumerate(entry.get("artifact_refs", [])):
            if not isinstance(ref, dict):
                errors.append(f"focus_entries[{entry_index}].artifact_refs[{ref_index}]: invalid artifact ref")
                continue
            ensure_exists(ref.get("path"), f"focus_entries[{entry_index}].artifact_refs[{ref_index}].path", errors)


def validate_counts(summary: dict, errors: list[str]) -> None:
    status = summary.get("consumer_status", {})
    entries = summary.get("focus_entries", [])
    changed_entries = [entry for entry in entries if entry.get("changed")]
    actionable_entries = [entry for entry in entries if entry.get("focus_kind") != "steady_state"]

    expect_equal(status.get("total_focus_count"), len(entries), "consumer_status.total_focus_count", errors)
    expect_equal(status.get("changed_focus_count"), len(changed_entries), "consumer_status.changed_focus_count", errors)
    expect_equal(status.get("actionable_focus_count"), len(actionable_entries), "consumer_status.actionable_focus_count", errors)
    if entries:
        expect_equal(status.get("default_focus_id"), entries[0].get("focus_id"), "consumer_status.default_focus_id", errors)
        expect_equal(summary.get("default_focus"), entries[0], "default_focus", errors)

    readiness = summary.get("readiness_surface", {})
    expect_equal(
        readiness.get("changed_focus_ids"),
        [entry.get("focus_id") for entry in changed_entries],
        "readiness_surface.changed_focus_ids",
        errors,
    )
    expect_equal(
        readiness.get("actionable_focus_ids"),
        [entry.get("focus_id") for entry in actionable_entries],
        "readiness_surface.actionable_focus_ids",
        errors,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate minimal-kernel runtime session witness inspect compare consumer summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to inspect-compare consumer summary JSON. If omitted, "
            "--bundle-root/session-witness.inspect.compare.consumer.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing session-witness.inspect.compare.consumer.summary.json.",
    )
    args = parser.parse_args()

    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    if args.summary:
        summary_path = Path(args.summary).resolve()
    else:
        bundle_root = Path(args.bundle_root or "out/minimal-kernel-runtime-session-witness-inspect-compare-consumer").resolve()
        summary_path = bundle_root / "session-witness.inspect.compare.consumer.summary.json"

    consumer_schema_path = (repo_root / CONSUMER_SCHEMA_PATH).resolve()
    compare_schema_path = (repo_root / COMPARE_SCHEMA_PATH).resolve()

    try:
        import jsonschema

        summary = load_json(summary_path)
        consumer_schema = load_json(consumer_schema_path)
        compare_schema = load_json(compare_schema_path)
        jsonschema.validate(summary, consumer_schema)

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        compare_summary_path = Path(artifact_context.get("source_compare_summary_path", "")).resolve()
        compare_summary = load_json(compare_summary_path)
        jsonschema.validate(compare_summary, compare_schema)

        expected_summary = build_summary_model(
            compare_summary_path=compare_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("consumer_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )

        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "compare_consumer",
            "artifact_context",
            "source_compare",
            "consumer_status",
            "default_focus",
            "focus_entries",
            "readiness_surface",
            "supporting_artifacts",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("consumer_summary_path", "")),
            "artifact_context.consumer_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    status = summary.get("consumer_status", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] default focus -> {status.get('default_focus_id', '')}")
    print(f"[OK] highest severity -> {status.get('highest_severity', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
