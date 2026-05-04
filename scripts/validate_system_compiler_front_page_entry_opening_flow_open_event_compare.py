from __future__ import annotations

import argparse
import sys
from pathlib import Path

from compare_system_compiler_front_page_entry_opening_flow_open_event import build_compare_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


OPEN_EVENT_COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event_compare.v0.schema.json"
OPEN_EVENT_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json"


def ensure_exists(path_value: str | None, label: str, errors: list[str]) -> None:
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


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    front_page = summary.get("front_page", {})

    ensure_exists(artifact_context.get("baseline_open_event_summary_path"), "artifact_context.baseline_open_event_summary_path", errors)
    ensure_exists(artifact_context.get("candidate_open_event_summary_path"), "artifact_context.candidate_open_event_summary_path", errors)
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

    for index, provenance in enumerate(summary.get("event_provenance", [])):
        if not isinstance(provenance, dict):
            errors.append(f"event_provenance[{index}]: invalid provenance")
            continue
        ensure_exists(provenance.get("source_summary_path"), f"event_provenance[{index}].source_summary_path", errors)
        ensure_exists(provenance.get("source_report_markdown_path"), f"event_provenance[{index}].source_report_markdown_path", errors)
        ensure_exists(provenance.get("source_check_text_path"), f"event_provenance[{index}].source_check_text_path", errors)


def validate_counts(summary: dict, errors: list[str]) -> None:
    event_changes = summary.get("event_changes", {})
    consumer_changes = summary.get("consumer_decision_changes", {})
    plan_changes = summary.get("plan_changes", {})
    action_changes = summary.get("action_record_changes", {})
    compare_changes = summary.get("compare_context_changes", {})
    workspace_changes = summary.get("workspace_facade_changes", {})
    witness_changes = summary.get("witness_ref_changes", {})
    diagnostic_changes = summary.get("diagnostic_preview_changes", {})
    explanation_changes = summary.get("explanation_view_changes", {})
    change_summary = summary.get("change_summary", {})

    expected_total = (
        int(event_changes.get("changed_field_count", 0))
        + int(consumer_changes.get("changed_field_count", 0))
        + int(plan_changes.get("changed_field_count", 0))
        + int(action_changes.get("changed_field_count", 0))
        + int(compare_changes.get("changed_field_count", 0))
        + int(workspace_changes.get("changed_field_count", 0))
        + int(witness_changes.get("changed_field_count", 0))
        + int(diagnostic_changes.get("changed_field_count", 0))
        + int(explanation_changes.get("changed_field_count", 0))
    )
    expect_equal(change_summary.get("changed_field_count"), expected_total, "change_summary.changed_field_count", errors)
    expect_equal(change_summary.get("event_changed_field_count"), event_changes.get("changed_field_count"), "change_summary.event_changed_field_count", errors)
    expect_equal(
        change_summary.get("consumer_changed_field_count"),
        consumer_changes.get("changed_field_count"),
        "change_summary.consumer_changed_field_count",
        errors,
    )
    expect_equal(change_summary.get("plan_changed_field_count"), plan_changes.get("changed_field_count"), "change_summary.plan_changed_field_count", errors)
    expect_equal(
        change_summary.get("action_changed_field_count"),
        action_changes.get("changed_field_count"),
        "change_summary.action_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("compare_changed_field_count"),
        compare_changes.get("changed_field_count"),
        "change_summary.compare_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("workspace_changed_field_count"),
        workspace_changes.get("changed_field_count"),
        "change_summary.workspace_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("witness_changed_field_count"),
        witness_changes.get("changed_field_count"),
        "change_summary.witness_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("diagnostic_changed_field_count"),
        diagnostic_changes.get("changed_field_count"),
        "change_summary.diagnostic_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("explanation_changed_field_count"),
        explanation_changes.get("changed_field_count"),
        "change_summary.explanation_changed_field_count",
        errors,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening-flow open-event compare summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to open-event compare summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.open-event.compare.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.open-event.compare.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-open-event-compare").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.open-event.compare.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / OPEN_EVENT_COMPARE_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        baseline_path = Path(artifact_context.get("baseline_open_event_summary_path", "")).resolve()
        candidate_path = Path(artifact_context.get("candidate_open_event_summary_path", "")).resolve()
        event_schema = load_json((repo_root / OPEN_EVENT_SCHEMA_PATH).resolve())
        baseline_event = load_json(baseline_path)
        candidate_event = load_json(candidate_path)
        jsonschema.validate(baseline_event, event_schema)
        jsonschema.validate(candidate_event, event_schema)

        expected_summary = build_compare_summary_model(
            baseline_open_event_path=baseline_path,
            candidate_open_event_path=candidate_path,
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
            "opening_flow_open_event_compare",
            "front_page",
            "event_provenance",
            "artifact_context",
            "event_verdict",
            "event_status",
            "event_changes",
            "consumer_decision_changes",
            "plan_changes",
            "action_record_changes",
            "compare_context_changes",
            "workspace_facade_changes",
            "witness_ref_changes",
            "diagnostic_preview_changes",
            "explanation_view_changes",
            "change_summary",
            "event_regression_surface",
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
    print(f"[OK] event verdict -> {summary.get('event_verdict', '')}")
    print(f"[OK] changed fields -> {summary.get('change_summary', {}).get('changed_field_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
