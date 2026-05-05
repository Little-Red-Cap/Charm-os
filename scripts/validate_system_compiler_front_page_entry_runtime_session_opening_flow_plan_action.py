from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


BRIDGE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_runtime_session_opening_flow_plan_action.v0.schema.json"
CONSUMER_SCHEMA_PATH = "schemas/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.schema.json"


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
    source_consumer = summary.get("source_consumer", {})
    front_page = summary.get("front_page", {})
    facade_surface = summary.get("facade_surface", {})
    artifact_target = summary.get("artifact_target", {})
    judgment_inputs = summary.get("judgment_inputs", {})

    ensure_exists(artifact_context.get("source_consumer_summary_path"), "artifact_context.source_consumer_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("bridge_summary_path"), "artifact_context.bridge_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)
    ensure_exists(source_consumer.get("source_compare", {}).get("summary_path"), "source_consumer.source_compare.summary_path", errors, required=False)

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

    ensure_exists(facade_surface.get("summary_path"), "facade_surface.summary_path", errors)
    ensure_exists(facade_surface.get("report_markdown_path"), "facade_surface.report_markdown_path", errors)
    ensure_exists(facade_surface.get("check_text_path"), "facade_surface.check_text_path", errors)
    ensure_exists(
        artifact_target.get("selected_artifact_ref", {}).get("path"),
        "artifact_target.selected_artifact_ref.path",
        errors,
        required=False,
    )
    ensure_exists(
        judgment_inputs.get("consumer_summary_ref", {}).get("path"),
        "judgment_inputs.consumer_summary_ref.path",
        errors,
    )
    ensure_exists(
        judgment_inputs.get("default_focus_ref", {}).get("path"),
        "judgment_inputs.default_focus_ref.path",
        errors,
    )
    ensure_exists(
        judgment_inputs.get("selected_explain_hop_ref", {}).get("path"),
        "judgment_inputs.selected_explain_hop_ref.path",
        errors,
    )
    ensure_exists(
        judgment_inputs.get("selected_artifact_ref", {}).get("path"),
        "judgment_inputs.selected_artifact_ref.path",
        errors,
        required=False,
    )
    for index, ref in enumerate(judgment_inputs.get("fallback_artifact_refs", [])):
        if not isinstance(ref, dict):
            errors.append(f"judgment_inputs.fallback_artifact_refs[{index}]: invalid ref")
            continue
        ensure_exists(ref.get("path"), f"judgment_inputs.fallback_artifact_refs[{index}].path", errors, required=False)


def validate_counts(summary: dict, errors: list[str]) -> None:
    open_action = summary.get("open_action", {})
    preview = summary.get("opening_preview", {})
    execution_receipt = summary.get("execution_receipt", {})
    artifact_target = summary.get("artifact_target", {})
    judgment_inputs = summary.get("judgment_inputs", {})

    expect_equal(preview.get("line_count"), len(preview.get("summary_lines", [])), "opening_preview.line_count", errors)
    expect_equal(
        preview.get("question_count"),
        len(preview.get("question_lines", [])),
        "opening_preview.question_count",
        errors,
    )
    expect_equal(execution_receipt.get("planned_action_count"), 1, "execution_receipt.planned_action_count", errors)
    expect_equal(open_action.get("action_id"), "open-default", "open_action.action_id", errors)
    expect_equal(open_action.get("entry_name"), "runtime-session-inspect-consumer", "open_action.entry_name", errors)
    expect_equal(
        open_action.get("expected_consumer_operation"),
        "open-consumer-summary",
        "open_action.expected_consumer_operation",
        errors,
    )
    expect_equal(
        artifact_target.get("selected_artifact_ref"),
        judgment_inputs.get("selected_artifact_ref"),
        "artifact_target.selected_artifact_ref",
        errors,
    )
    expect_equal(
        artifact_target.get("fallback_artifact_refs"),
        judgment_inputs.get("fallback_artifact_refs"),
        "artifact_target.fallback_artifact_refs",
        errors,
    )


def validate_status(summary: dict, errors: list[str]) -> None:
    source_consumer = summary.get("source_consumer", {})
    open_action = summary.get("open_action", {})
    blockers = open_action.get("blockers", [])
    expected_status = "blocked" if blockers else "ready"
    expect_equal(open_action.get("status"), expected_status, "open_action.status", errors)
    if source_consumer.get("result") != "ok":
        expect_equal(summary.get("result"), "fail", "result", errors)
    else:
        expect_equal(summary.get("result"), "ok", "result", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate runtime-session opening-flow plan-action bridge summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to runtime-session opening-flow bridge summary JSON. If omitted, "
            "--bundle-root/front-page.entry-runtime-session-opening-flow.plan-action.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-runtime-session-opening-flow.plan-action.summary.json.",
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
        bundle_root = Path(
            args.bundle_root or "out/system-compiler-front-page-entry-runtime-session-opening-flow-plan-action"
        ).resolve()
        summary_path = bundle_root / "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / BRIDGE_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)
        validate_status(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        consumer_summary_path = Path(artifact_context.get("source_consumer_summary_path", "")).resolve()
        consumer_summary = load_json(consumer_summary_path)
        jsonschema.validate(consumer_summary, load_json((repo_root / CONSUMER_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            consumer_summary_path=consumer_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("bridge_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "runtime_session_opening_flow_plan_action",
            "front_page",
            "artifact_context",
            "source_consumer",
            "judgment_inputs",
            "facade_surface",
            "artifact_target",
            "open_action",
            "opening_preview",
            "execution_receipt",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("bridge_summary_path", "")),
            "artifact_context.bridge_summary_path",
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
    print(f"[OK] status -> {summary.get('open_action', {}).get('status', '')}")
    print(f"[OK] target -> {summary.get('artifact_target', {}).get('selected_artifact_ref', {}).get('id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
