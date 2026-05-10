from __future__ import annotations

import argparse
import sys
from pathlib import Path

from compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action import build_compare_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare.v0.schema.json"
ACTION_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action.v0.schema.json"


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

    ensure_exists(artifact_context.get("baseline_action_summary_path"), "artifact_context.baseline_action_summary_path", errors)
    ensure_exists(artifact_context.get("candidate_action_summary_path"), "artifact_context.candidate_action_summary_path", errors)
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

    for index, provenance in enumerate(summary.get("action_provenance", [])):
        if not isinstance(provenance, dict):
            errors.append(f"action_provenance[{index}]: invalid provenance")
            continue
        ensure_exists(provenance.get("source_summary_path"), f"action_provenance[{index}].source_summary_path", errors)
        ensure_exists(provenance.get("source_report_markdown_path"), f"action_provenance[{index}].source_report_markdown_path", errors)
        ensure_exists(provenance.get("source_check_text_path"), f"action_provenance[{index}].source_check_text_path", errors)


def validate_counts(summary: dict, errors: list[str]) -> None:
    selection = summary.get("selection_changes", {})
    open_action = summary.get("open_action_changes", {})
    opener_surface = summary.get("opener_surface_changes", {})
    receipt = summary.get("execution_receipt_changes", {})
    change_summary = summary.get("change_summary", {})

    expected_total = (
        int(selection.get("changed_field_count", 0))
        + int(open_action.get("changed_field_count", 0))
        + int(opener_surface.get("changed_field_count", 0))
        + int(receipt.get("changed_field_count", 0))
    )
    expect_equal(change_summary.get("changed_field_count"), expected_total, "change_summary.changed_field_count", errors)
    expect_equal(
        change_summary.get("selection_changed_field_count"),
        selection.get("changed_field_count"),
        "change_summary.selection_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("open_action_changed_field_count"),
        open_action.get("changed_field_count"),
        "change_summary.open_action_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("opener_surface_changed_field_count"),
        opener_surface.get("changed_field_count"),
        "change_summary.opener_surface_changed_field_count",
        errors,
    )
    expect_equal(
        change_summary.get("execution_receipt_changed_field_count"),
        receipt.get("changed_field_count"),
        "change_summary.execution_receipt_changed_field_count",
        errors,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening-flow consumer plan action compare summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to plan action compare summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.consumer.plan-action.compare.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.consumer.plan-action.compare.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / COMPARE_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        baseline_action_path = Path(artifact_context.get("baseline_action_summary_path", "")).resolve()
        candidate_action_path = Path(artifact_context.get("candidate_action_summary_path", "")).resolve()
        action_schema = load_json((repo_root / ACTION_SCHEMA_PATH).resolve())
        baseline_action = load_json(baseline_action_path)
        candidate_action = load_json(candidate_action_path)
        jsonschema.validate(baseline_action, action_schema)
        jsonschema.validate(candidate_action, action_schema)

        expected_summary = build_compare_summary_model(
            baseline_action_path=baseline_action_path,
            candidate_action_path=candidate_action_path,
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
            "opening_flow_consumer_plan_action_compare",
            "front_page",
            "action_provenance",
            "artifact_context",
            "action_verdict",
            "action_status",
            "selection_changes",
            "open_action_changes",
            "opener_surface_changes",
            "execution_receipt_changes",
            "change_summary",
            "action_regression_surface",
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
    print(f"[OK] action verdict -> {summary.get('action_verdict', '')}")
    print(f"[OK] changed fields -> {summary.get('change_summary', {}).get('changed_field_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
