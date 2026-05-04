from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_route_lib import load_json, normalize_path


FLOW_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow.v0.schema.json"
OPENER_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opener.v0.schema.json"


def ensure_exists(path_value: str | None, label: str, errors: list[str], required: bool = True) -> None:
    text = str(path_value or "").strip()
    if not text:
        if required:
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
        ensure_exists(
            surface.get("report_markdown_path"),
            f"front_page.supporting_surfaces[{index}].report_markdown_path",
            errors,
        )
        ensure_exists(
            surface.get("check_text_path"),
            f"front_page.supporting_surfaces[{index}].check_text_path",
            errors,
        )


def validate_artifact_context(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    for field in (
        "route_root",
        "capability_root",
        "landing_root",
        "landing_compare_root",
        "opener_root",
        "output_root",
        "flow_summary_path",
        "report_markdown_path",
        "check_text_path",
    ):
        ensure_exists(artifact_context.get(field), f"artifact_context.{field}", errors)


def validate_flow_steps(summary: dict, errors: list[str]) -> None:
    for index, step in enumerate(summary.get("flow_steps", [])):
        if not isinstance(step, dict):
            errors.append(f"flow_steps[{index}]: invalid step")
            continue
        ensure_exists(step.get("script_path"), f"flow_steps[{index}].script_path", errors)
        ensure_exists(step.get("output_root"), f"flow_steps[{index}].output_root", errors)
        if step.get("status") != "completed":
            errors.append(f"flow_steps[{index}].status: expected 'completed' but got {step.get('status')!r}")


def validate_opener_cases(summary: dict, opener_schema: dict, errors: list[str]) -> None:
    try:
        import jsonschema
    except ImportError:
        errors.append("jsonschema is required. Install it with: python -m pip install jsonschema")
        return

    for index, case in enumerate(summary.get("opener_cases", [])):
        if not isinstance(case, dict):
            errors.append(f"opener_cases[{index}]: invalid case")
            continue

        summary_path_text = str(case.get("summary_path") or "").strip()
        report_path_text = str(case.get("report_markdown_path") or "").strip()
        check_path_text = str(case.get("check_text_path") or "").strip()
        target_summary_path_text = str(case.get("target_summary_path") or "").strip()

        ensure_exists(summary_path_text, f"opener_cases[{index}].summary_path", errors)
        ensure_exists(report_path_text, f"opener_cases[{index}].report_markdown_path", errors)
        ensure_exists(check_path_text, f"opener_cases[{index}].check_text_path", errors)
        ensure_exists(target_summary_path_text, f"opener_cases[{index}].target_summary_path", errors)

        if not summary_path_text:
            continue

        try:
            opener_summary_path = Path(summary_path_text).resolve()
            opener_summary = load_json(opener_summary_path)
            jsonschema.validate(opener_summary, opener_schema)
        except Exception as exc:
            errors.append(f"opener_cases[{index}].summary_path: failed to load/validate opener summary: {exc}")
            continue

        artifact_context = opener_summary.get("artifact_context", {})
        open_action = opener_summary.get("open_action", {})
        compare_context = opener_summary.get("compare_context", {})
        inspector_invocation = opener_summary.get("inspector_invocation", {})
        opened_projection = opener_summary.get("opened_projection", {})

        expect_equal(
            normalize_path(summary_path_text),
            normalize_path(artifact_context.get("opener_summary_path", "")),
            f"opener_cases[{index}].summary_path",
            errors,
        )
        expect_equal(
            normalize_path(report_path_text),
            normalize_path(artifact_context.get("report_markdown_path", "")),
            f"opener_cases[{index}].report_markdown_path",
            errors,
        )
        expect_equal(
            normalize_path(check_path_text),
            normalize_path(artifact_context.get("check_text_path", "")),
            f"opener_cases[{index}].check_text_path",
            errors,
        )
        expect_equal(case.get("open_action_status"), open_action.get("status"), f"opener_cases[{index}].open_action_status", errors)
        expect_equal(case.get("selected_tab_id"), open_action.get("selected_tab_id"), f"opener_cases[{index}].selected_tab_id", errors)
        expect_equal(case.get("selected_role"), open_action.get("selected_role"), f"opener_cases[{index}].selected_role", errors)
        expect_equal(case.get("query_kind"), open_action.get("query_kind"), f"opener_cases[{index}].query_kind", errors)
        expect_equal(case.get("query_scope"), open_action.get("query_scope"), f"opener_cases[{index}].query_scope", errors)
        expect_equal(case.get("selection_rule"), open_action.get("selection_rule"), f"opener_cases[{index}].selection_rule", errors)
        expect_equal(
            case.get("opening_reason"),
            open_action.get("opening_reason"),
            f"opener_cases[{index}].opening_reason",
            errors,
        )
        expect_equal(
            case.get("target_summary_schema"),
            open_action.get("target_summary_schema"),
            f"opener_cases[{index}].target_summary_schema",
            errors,
        )
        expect_equal(
            case.get("target_summary_kind"),
            open_action.get("target_summary_kind"),
            f"opener_cases[{index}].target_summary_kind",
            errors,
        )
        expect_equal(
            normalize_path(target_summary_path_text),
            normalize_path(open_action.get("target_summary_path", "")),
            f"opener_cases[{index}].target_summary_path",
            errors,
        )
        expect_equal(
            case.get("open_action_blockers"),
            open_action.get("blockers"),
            f"opener_cases[{index}].open_action_blockers",
            errors,
        )
        expect_equal(
            case.get("projection_status"),
            opened_projection.get("status"),
            f"opener_cases[{index}].projection_status",
            errors,
        )
        expect_equal(
            case.get("projection_kind"),
            opened_projection.get("projection_kind"),
            f"opener_cases[{index}].projection_kind",
            errors,
        )
        expect_equal(
            case.get("projection_headline"),
            opened_projection.get("headline"),
            f"opener_cases[{index}].projection_headline",
            errors,
        )
        expect_equal(
            case.get("projection_summary_lines"),
            opened_projection.get("summary_lines"),
            f"opener_cases[{index}].projection_summary_lines",
            errors,
        )
        expect_equal(
            case.get("projection_question_lines"),
            opened_projection.get("question_lines"),
            f"opener_cases[{index}].projection_question_lines",
            errors,
        )
        expect_equal(
            case.get("projection_blockers"),
            opened_projection.get("blockers"),
            f"opener_cases[{index}].projection_blockers",
            errors,
        )
        expect_equal(
            case.get("compare_context_available"),
            compare_context.get("available"),
            f"opener_cases[{index}].compare_context_available",
            errors,
        )
        expect_equal(
            case.get("landing_verdict"),
            compare_context.get("landing_verdict"),
            f"opener_cases[{index}].landing_verdict",
            errors,
        )
        expect_equal(
            case.get("inspector_ready"),
            inspector_invocation.get("ready"),
            f"opener_cases[{index}].inspector_ready",
            errors,
        )
        expect_equal(
            case.get("inspector_mode"),
            inspector_invocation.get("mode"),
            f"opener_cases[{index}].inspector_mode",
            errors,
        )
        expect_equal(
            case.get("inspector_blockers"),
            inspector_invocation.get("blockers"),
            f"opener_cases[{index}].inspector_blockers",
            errors,
        )
        expect_equal(
            case.get("opener_compare_questions"),
            opener_summary.get("questions", {}).get("compare_questions"),
            f"opener_cases[{index}].opener_compare_questions",
            errors,
        )
        expect_equal(
            case.get("opener_next_questions"),
            opener_summary.get("questions", {}).get("next_questions"),
            f"opener_cases[{index}].opener_next_questions",
            errors,
        )


def validate_counts(summary: dict, errors: list[str]) -> None:
    flow_status = summary.get("flow_status", {})
    flow_steps = summary.get("flow_steps", [])
    opener_cases = summary.get("opener_cases", [])

    actual_opener_count = len(opener_cases)
    available_projection_count = sum(1 for case in opener_cases if case.get("projection_status") == "available")
    compare_context_count = sum(1 for case in opener_cases if bool(case.get("compare_context_available")))
    inspector_ready_count = sum(1 for case in opener_cases if bool(case.get("inspector_ready")))
    blocked_inspector_count = actual_opener_count - inspector_ready_count
    completed_step_count = sum(1 for step in flow_steps if step.get("status") == "completed")

    expect_equal(flow_status.get("actual_opener_count"), actual_opener_count, "flow_status.actual_opener_count", errors)
    expect_equal(
        flow_status.get("available_projection_count"),
        available_projection_count,
        "flow_status.available_projection_count",
        errors,
    )
    expect_equal(flow_status.get("compare_context_count"), compare_context_count, "flow_status.compare_context_count", errors)
    expect_equal(flow_status.get("inspector_ready_count"), inspector_ready_count, "flow_status.inspector_ready_count", errors)
    expect_equal(
        flow_status.get("blocked_inspector_count"),
        blocked_inspector_count,
        "flow_status.blocked_inspector_count",
        errors,
    )
    expect_equal(flow_status.get("completed_step_count"), completed_step_count, "flow_status.completed_step_count", errors)
    expect_equal(summary.get("result"), flow_status.get("result"), "result", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening flow summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry opening flow summary JSON. If omitted, --bundle-root/front-page.entry-opening-flow.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.summary.json"

    schema_path = (repo_root / FLOW_SCHEMA_PATH).resolve()
    opener_schema_path = (repo_root / OPENER_SCHEMA_PATH).resolve()

    try:
        import jsonschema

        summary = load_json(summary_path)
        schema = load_json(schema_path)
        opener_schema = load_json(opener_schema_path)
        jsonschema.validate(summary, schema)

        errors: list[str] = []
        validate_front_page(summary, errors)
        validate_artifact_context(summary, errors)
        validate_flow_steps(summary, errors)
        validate_opener_cases(summary, opener_schema, errors)
        validate_counts(summary, errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(summary.get("artifact_context", {}).get("flow_summary_path", "")),
            "artifact_context.flow_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    flow_status = summary.get("flow_status", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] openers -> {flow_status.get('actual_opener_count', 0)}")
    print(f"[OK] projections -> {flow_status.get('available_projection_count', 0)}")
    print(f"[OK] compare context -> {flow_status.get('compare_context_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
