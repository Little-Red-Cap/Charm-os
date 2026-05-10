from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_flow_consumer_plan_action import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


ACTION_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action.v0.schema.json"
PLAN_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan.v0.schema.json"
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


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    front_page = summary.get("front_page", {})
    selected_action = summary.get("selected_action", {})
    open_action = summary.get("open_action", {})
    opening_preview = summary.get("opening_preview", {})
    opener_surface = summary.get("opener_surface", {})

    ensure_exists(artifact_context.get("source_plan_summary_path"), "artifact_context.source_plan_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("action_summary_path"), "artifact_context.action_summary_path", errors)
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

    ensure_exists(selected_action.get("target_summary_path"), "selected_action.target_summary_path", errors)
    ensure_exists(selected_action.get("opener_summary_path"), "selected_action.opener_summary_path", errors)
    ensure_exists(selected_action.get("opener_report_markdown_path"), "selected_action.opener_report_markdown_path", errors)
    ensure_exists(selected_action.get("opener_check_text_path"), "selected_action.opener_check_text_path", errors)
    ensure_exists(open_action.get("target_summary_path"), "open_action.target_summary_path", errors)
    ensure_exists(open_action.get("opener_summary_path"), "open_action.opener_summary_path", errors)
    ensure_exists(opening_preview.get("opener_summary_path"), "opening_preview.opener_summary_path", errors)
    ensure_exists(opener_surface.get("summary_path"), "opener_surface.summary_path", errors)
    ensure_exists(opener_surface.get("report_markdown_path"), "opener_surface.report_markdown_path", errors)
    ensure_exists(opener_surface.get("check_text_path"), "opener_surface.check_text_path", errors)


def validate_action_consistency(summary: dict, errors: list[str]) -> None:
    selected_action = summary.get("selected_action", {})
    open_action = summary.get("open_action", {})
    opening_preview = summary.get("opening_preview", {})
    opener_surface = summary.get("opener_surface", {})
    receipt = summary.get("execution_receipt", {})
    source_plan = summary.get("source_plan", {})

    for field in (
        "action_id",
        "action_kind",
        "entry_name",
        "display_group",
        "selected_tab_id",
        "selected_role",
        "query_kind",
        "query_scope",
        "target_summary_schema",
        "target_summary_kind",
        "target_summary_path",
        "projection_kind",
        "opening_reason",
        "projection_headline",
        "projection_summary_lines",
        "projection_question_lines",
        "compare_context_available",
        "landing_verdict",
        "opener_summary_path",
        "opener_report_markdown_path",
        "opener_check_text_path",
        "expected_consumer_operation",
        "reason",
    ):
        expect_equal(open_action.get(field), selected_action.get(field), f"open_action.{field}", errors)

    expect_equal(receipt.get("selected_rank"), selected_action.get("rank"), "execution_receipt.selected_rank", errors)
    expect_equal(receipt.get("source_rank"), selected_action.get("source_rank"), "execution_receipt.source_rank", errors)
    expect_equal(
        receipt.get("planned_action_count"),
        source_plan.get("planned_action_count"),
        "execution_receipt.planned_action_count",
        errors,
    )
    expect_equal(summary.get("result"), "ok" if open_action.get("status") == "ready" else "fail", "result", errors)
    if open_action.get("status") == "ready":
        expect_equal(summary.get("violations"), [], "violations", errors)
    expect_equal(opening_preview.get("entry_name"), open_action.get("entry_name"), "opening_preview.entry_name", errors)
    expect_equal(
        opening_preview.get("opening_reason"),
        open_action.get("opening_reason"),
        "opening_preview.opening_reason",
        errors,
    )
    expect_equal(
        opening_preview.get("projection_kind"),
        open_action.get("projection_kind"),
        "opening_preview.projection_kind",
        errors,
    )
    expect_equal(
        opening_preview.get("headline"),
        open_action.get("projection_headline"),
        "opening_preview.headline",
        errors,
    )
    expect_equal(
        opening_preview.get("summary_lines"),
        open_action.get("projection_summary_lines"),
        "opening_preview.summary_lines",
        errors,
    )
    expect_equal(
        opening_preview.get("question_lines"),
        open_action.get("projection_question_lines"),
        "opening_preview.question_lines",
        errors,
    )
    expect_equal(
        opening_preview.get("opener_summary_path"),
        opener_surface.get("summary_path"),
        "opening_preview.opener_summary_path",
        errors,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening-flow consumer plan action summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to consumer plan action summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.consumer.plan-action.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.consumer.plan-action.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan-action").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.consumer.plan-action.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / ACTION_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_action_consistency(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        plan_summary_path = Path(artifact_context.get("source_plan_summary_path", "")).resolve()
        plan_summary = load_json(plan_summary_path)
        jsonschema.validate(plan_summary, load_json((repo_root / PLAN_SCHEMA_PATH).resolve()))

        opener_summary_path = Path(summary.get("opener_surface", {}).get("summary_path", "")).resolve()
        opener_summary = load_json(opener_summary_path)
        jsonschema.validate(opener_summary, load_json((repo_root / OPENER_SCHEMA_PATH).resolve()))

        request = summary.get("selection_request", {})
        expected_summary = build_summary_model(
            plan_summary_path=plan_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("action_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
            requested_action_id=request.get("requested_action_id", ""),
            requested_action_kind=request.get("requested_action_kind", ""),
            requested_entry_name=request.get("requested_entry_name", ""),
        )

        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_consumer_plan_action",
            "front_page",
            "artifact_context",
            "selection_request",
            "source_plan",
            "selected_action",
            "open_action",
            "opening_preview",
            "opener_surface",
            "execution_receipt",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("action_summary_path", "")),
            "artifact_context.action_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    open_action = summary.get("open_action", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] action -> {open_action.get('action_id', '')}")
    print(f"[OK] entry -> {open_action.get('entry_name', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
