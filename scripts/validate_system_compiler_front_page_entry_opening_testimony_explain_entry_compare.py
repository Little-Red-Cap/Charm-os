from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

from compare_system_compiler_front_page_entry_opening_testimony_explain_entry import build_compare_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


EXPLAIN_ENTRY_COMPARE_SCHEMA_PATH = (
    "schemas/system_compiler.front_page_entry_opening_testimony_explain_entry_compare.v0.schema.json"
)
EXPLAIN_ENTRY_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_testimony_explain_entry.v0.schema.json"


def ensure_exists(path_value: Any, label: str, errors: list[str]) -> None:
    text = str(path_value or "").strip()
    if not text:
        errors.append(f"{label}: missing path")
        return
    resolved = Path(text).resolve()
    if not resolved.exists():
        errors.append(f"{label}: not found -> {resolved}")


def expect_equal(actual, expected, label: str, errors: list[str]) -> None:
    if actual != expected:
        errors.append(f"{label}: expected {expected!r} but got {actual!r}")


def validate_references(summary: dict[str, Any], errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    front_page = summary.get("front_page", {})

    ensure_exists(
        artifact_context.get("baseline_opening_testimony_explain_entry_summary_path"),
        "artifact_context.baseline_opening_testimony_explain_entry_summary_path",
        errors,
    )
    ensure_exists(
        artifact_context.get("candidate_opening_testimony_explain_entry_summary_path"),
        "artifact_context.candidate_opening_testimony_explain_entry_summary_path",
        errors,
    )
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("compare_summary_path"), "artifact_context.compare_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    ensure_exists(front_page.get("summary_path"), "front_page.summary_path", errors)
    ensure_exists(front_page.get("report_markdown_path"), "front_page.report_markdown_path", errors)
    ensure_exists(front_page.get("check_text_path"), "front_page.check_text_path", errors)
    for index, surface in enumerate(front_page.get("supporting_surfaces", [])):
        if not isinstance(surface, dict):
            errors.append(f"front_page.supporting_surfaces[{index}]: invalid surface")
            continue
        ensure_exists(surface.get("summary_path"), f"front_page.supporting_surfaces[{index}].summary_path", errors)
        ensure_exists(surface.get("report_markdown_path"), f"front_page.supporting_surfaces[{index}].report_markdown_path", errors)
        ensure_exists(surface.get("check_text_path"), f"front_page.supporting_surfaces[{index}].check_text_path", errors)

    for index, provenance in enumerate(summary.get("explain_entry_provenance", [])):
        if not isinstance(provenance, dict):
            errors.append(f"explain_entry_provenance[{index}]: invalid provenance")
            continue
        ensure_exists(provenance.get("source_summary_path"), f"explain_entry_provenance[{index}].source_summary_path", errors)
        ensure_exists(provenance.get("source_report_markdown_path"), f"explain_entry_provenance[{index}].source_report_markdown_path", errors)
        ensure_exists(provenance.get("source_check_text_path"), f"explain_entry_provenance[{index}].source_check_text_path", errors)
        if provenance.get("decision_status") == "ready":
            ensure_exists(provenance.get("selected_summary_path"), f"explain_entry_provenance[{index}].selected_summary_path", errors)
        ensure_exists(provenance.get("source_route_summary_path"), f"explain_entry_provenance[{index}].source_route_summary_path", errors)


def validate_counts(summary: dict[str, Any], errors: list[str]) -> None:
    source_route_changes = summary.get("source_route_ref_changes", {})
    decision_changes = summary.get("explain_entry_decision_changes", {})
    selected_surface_changes = summary.get("selected_surface_changes", {})
    supporting_surface_changes = summary.get("supporting_surface_changes", {})
    next_question_changes = summary.get("next_question_changes", {})
    change_summary = summary.get("change_summary", {})

    expected_total = (
        int(source_route_changes.get("changed_field_count", 0))
        + int(decision_changes.get("changed_field_count", 0))
        + int(selected_surface_changes.get("changed_field_count", 0))
        + int(supporting_surface_changes.get("changed_field_count", 0))
        + int(next_question_changes.get("changed_field_count", 0))
    )
    expect_equal(change_summary.get("changed_field_count"), expected_total, "change_summary.changed_field_count", errors)
    expect_equal(
        change_summary.get("source_route_ref_changed_field_count"),
        source_route_changes.get("changed_field_count"),
        "change_summary.source_route_ref_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("explain_entry_decision_changed_field_count"),
        decision_changes.get("changed_field_count"),
        "change_summary.explain_entry_decision_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("selected_surface_changed_field_count"),
        selected_surface_changes.get("changed_field_count"),
        "change_summary.selected_surface_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("supporting_surface_changed_field_count"),
        supporting_surface_changes.get("changed_field_count"),
        "change_summary.supporting_surface_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("next_question_changed_field_count"),
        next_question_changes.get("changed_field_count"),
        "change_summary.next_question_changed_field_count",
        errors,
    )


def validate_no_raw_runtime_session_fields(summary: dict[str, Any], errors: list[str]) -> None:
    forbidden_names = {
        "runtime_session_summary",
        "world_compare_summary",
        "session_witness_inspect_compare_consumer",
        "runtime_evidence_bundle",
        "open_event_witness",
        "opening_testimony_landing",
    }

    def walk(value: Any, path: str) -> None:
        if isinstance(value, dict):
            for key, item in value.items():
                key_text = str(key)
                if key_text in forbidden_names:
                    errors.append(f"{path}.{key_text}: forbidden raw evidence or lower testimony field")
                walk(item, f"{path}.{key_text}")
        elif isinstance(value, list):
            for index, item in enumerate(value):
                walk(item, f"{path}[{index}]")

    walk(summary, "summary")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening testimony explain-entry compare summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to opening testimony explain-entry compare summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-testimony.explain-entry.compare.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-testimony.explain-entry.compare.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry-compare").resolve()
        summary_path = bundle_root / "front-page.entry-opening-testimony.explain-entry.compare.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / EXPLAIN_ENTRY_COMPARE_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)
        validate_no_raw_runtime_session_fields(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        baseline_path = Path(artifact_context.get("baseline_opening_testimony_explain_entry_summary_path", "")).resolve()
        candidate_path = Path(artifact_context.get("candidate_opening_testimony_explain_entry_summary_path", "")).resolve()
        explain_entry_schema = load_json((repo_root / EXPLAIN_ENTRY_SCHEMA_PATH).resolve())
        baseline_explain_entry = load_json(baseline_path)
        candidate_explain_entry = load_json(candidate_path)
        jsonschema.validate(baseline_explain_entry, explain_entry_schema)
        jsonschema.validate(candidate_explain_entry, explain_entry_schema)

        expected_summary = build_compare_summary_model(
            baseline_explain_entry_path=baseline_path,
            candidate_explain_entry_path=candidate_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("compare_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_testimony_explain_entry_compare",
            "front_page",
            "explain_entry_provenance",
            "artifact_context",
            "explain_entry_verdict",
            "explain_entry_status",
            "source_route_ref_changes",
            "explain_entry_decision_changes",
            "selected_surface_changes",
            "supporting_surface_changes",
            "next_question_changes",
            "change_summary",
            "explain_entry_regression_surface",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("compare_summary_path", "")),
            "artifact_context.compare_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] explain entry verdict -> {summary.get('explain_entry_verdict', '')}")
    print(f"[OK] changed fields -> {summary.get('change_summary', {}).get('changed_field_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
