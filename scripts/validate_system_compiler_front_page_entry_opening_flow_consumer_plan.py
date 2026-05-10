from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_flow_consumer_plan import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


PLAN_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan.v0.schema.json"
SELECTOR_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_selector.v0.schema.json"


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


def validate_front_page(summary: dict, errors: list[str]) -> None:
    front_page = summary.get("front_page", {})
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


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    ensure_exists(artifact_context.get("source_selector_summary_path"), "artifact_context.source_selector_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("consumer_plan_summary_path"), "artifact_context.consumer_plan_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    for index, action in enumerate(summary.get("execution_plan", {}).get("action_entries", [])):
        if not isinstance(action, dict):
            errors.append(f"execution_plan.action_entries[{index}]: invalid action")
            continue
        ensure_exists(action.get("target_summary_path"), f"execution_plan.action_entries[{index}].target_summary_path", errors)
        ensure_exists(action.get("opener_summary_path"), f"execution_plan.action_entries[{index}].opener_summary_path", errors)
        ensure_exists(
            action.get("opener_report_markdown_path"),
            f"execution_plan.action_entries[{index}].opener_report_markdown_path",
            errors,
        )
        ensure_exists(action.get("opener_check_text_path"), f"execution_plan.action_entries[{index}].opener_check_text_path", errors)


def validate_counts(summary: dict, errors: list[str]) -> None:
    status = summary.get("planner_status", {})
    execution_plan = summary.get("execution_plan", {})
    planning_surface = summary.get("planning_surface", {})
    actions = execution_plan.get("action_entries", [])
    next_actions = execution_plan.get("next_actions", [])
    default_action = execution_plan.get("default_action", {})
    compare_action = execution_plan.get("compare_action", {})

    expect_equal(status.get("planned_action_count"), len(actions), "planner_status.planned_action_count", errors)
    expect_equal(status.get("next_action_count"), len(next_actions), "planner_status.next_action_count", errors)
    expect_equal(
        status.get("omitted_entry_count"),
        len(planning_surface.get("omitted_entry_names", [])),
        "planner_status.omitted_entry_count",
        errors,
    )
    expect_equal(status.get("default_action_name"), default_action.get("entry_name", ""), "planner_status.default_action_name", errors)
    expect_equal(status.get("compare_action_name"), compare_action.get("entry_name", ""), "planner_status.compare_action_name", errors)
    expect_equal(summary.get("result"), status.get("result"), "result", errors)
    for index, action in enumerate(actions):
        expect_equal(action.get("rank"), index, f"execution_plan.action_entries[{index}].rank", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening flow consumer plan summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to entry opening flow consumer plan summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.consumer.plan.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.consumer.plan.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.consumer.plan.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / PLAN_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_front_page(summary, errors)
        validate_references(summary, errors)
        validate_counts(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        selector_summary_path = Path(artifact_context.get("source_selector_summary_path", "")).resolve()
        selector_summary = load_json(selector_summary_path)
        jsonschema.validate(selector_summary, load_json((repo_root / SELECTOR_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            selector_summary_path=selector_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("consumer_plan_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )

        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_consumer_plan",
            "front_page",
            "artifact_context",
            "source_selector",
            "planner_status",
            "execution_plan",
            "planning_surface",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("consumer_plan_summary_path", "")),
            "artifact_context.consumer_plan_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    status = summary.get("planner_status", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] default action -> {status.get('default_action_name', '')}")
    print(f"[OK] planned actions -> {status.get('planned_action_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
