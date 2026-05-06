from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


OPEN_EVENT_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json"
BRIDGE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_runtime_session_opening_flow_plan_action.v0.schema.json"


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
    event = summary.get("open_event", {})
    source_artifact = event.get("source_artifact", {})
    workspace = summary.get("workspace_facade", {})
    opening_input_refs = event.get("opening_input_refs", {})
    event_status = event.get("status")

    ensure_exists(artifact_context.get("source_action_summary_path"), "artifact_context.source_action_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("open_event_summary_path"), "artifact_context.open_event_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)
    ensure_exists(artifact_context.get("source_action_compare_summary_path"), "artifact_context.source_action_compare_summary_path", errors, required=False)

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

    ensure_exists(
        source_artifact.get("summary_path"),
        "open_event.source_artifact.summary_path",
        errors,
        required=event_status != "blocked",
    )
    ensure_exists(source_artifact.get("opener_summary_path"), "open_event.source_artifact.opener_summary_path", errors)
    ensure_exists(source_artifact.get("opener_report_markdown_path"), "open_event.source_artifact.opener_report_markdown_path", errors)
    ensure_exists(source_artifact.get("opener_check_text_path"), "open_event.source_artifact.opener_check_text_path", errors)
    ensure_exists(workspace.get("primary_summary_path"), "workspace_facade.primary_summary_path", errors)
    ensure_exists(workspace.get("primary_report_markdown_path"), "workspace_facade.primary_report_markdown_path", errors)
    ensure_exists(workspace.get("primary_check_text_path"), "workspace_facade.primary_check_text_path", errors)
    ensure_exists(opening_input_refs.get("consumer_summary_ref", {}).get("path"), "open_event.opening_input_refs.consumer_summary_ref.path", errors)
    ensure_exists(opening_input_refs.get("selected_focus_ref", {}).get("path"), "open_event.opening_input_refs.selected_focus_ref.path", errors)
    ensure_exists(opening_input_refs.get("selected_explain_hop_ref", {}).get("path"), "open_event.opening_input_refs.selected_explain_hop_ref.path", errors)
    ensure_exists(opening_input_refs.get("selected_artifact_ref", {}).get("path"), "open_event.opening_input_refs.selected_artifact_ref.path", errors, required=False)
    for index, ref in enumerate(opening_input_refs.get("fallback_artifact_refs", [])):
        if not isinstance(ref, dict):
            errors.append(f"open_event.opening_input_refs.fallback_artifact_refs[{index}]: invalid ref")
            continue
        ensure_exists(ref.get("path"), f"open_event.opening_input_refs.fallback_artifact_refs[{index}].path", errors, required=False)


def validate_counts(summary: dict, errors: list[str]) -> None:
    decision = summary.get("consumer_decision", {})
    diagnostic = summary.get("diagnostic_preview", {})
    explanation = summary.get("explanation_view", {})
    candidates = decision.get("candidate_consumers", [])
    rejected = decision.get("rejected_consumers", [])
    selected = [candidate for candidate in candidates if isinstance(candidate, dict) and bool(candidate.get("selected"))]
    opening_input_refs = summary.get("open_event", {}).get("opening_input_refs", {})

    expect_equal(decision.get("candidate_consumer_count"), len(candidates), "consumer_decision.candidate_consumer_count", errors)
    expect_equal(decision.get("rejected_consumer_count"), len(rejected), "consumer_decision.rejected_consumer_count", errors)
    expect_equal(len(selected), 1, "consumer_decision.selected_candidate_count", errors)
    expect_equal(len(candidates), 1, "consumer_decision.candidate_consumers length", errors)
    expect_equal(len(rejected), 0, "consumer_decision.rejected_consumers length", errors)
    expect_equal(diagnostic.get("line_count"), len(diagnostic.get("summary_lines", [])), "diagnostic_preview.line_count", errors)
    expect_equal(
        diagnostic.get("question_count"),
        len(diagnostic.get("question_lines", [])),
        "diagnostic_preview.question_count",
        errors,
    )
    expect_equal(
        explanation.get("diagnostic_headline"),
        diagnostic.get("headline"),
        "explanation_view.diagnostic_headline",
        errors,
    )
    expect_equal(len(opening_input_refs.get("fallback_artifact_refs", [])) >= 0, True, "opening_input_refs.fallback presence", errors)


def validate_status(summary: dict, errors: list[str]) -> None:
    event_status = summary.get("open_event", {}).get("status")
    result = summary.get("result")
    workspace_status = summary.get("workspace_facade", {}).get("status")
    compare = summary.get("compare_summary", {})
    judgment = summary.get("judgment", {})

    expect_equal(result, "fail" if event_status == "blocked" else "ok", "result", errors)
    expect_equal(workspace_status, "blocked" if event_status == "blocked" else "projected", "workspace_facade.status", errors)
    expect_equal(judgment.get("semantic_role"), "opening_judgment_carrier", "judgment.semantic_role", errors)
    expect_equal(judgment.get("status"), event_status, "judgment.status", errors)
    expect_equal(judgment.get("accepted"), event_status != "blocked", "judgment.accepted", errors)
    expect_equal(judgment.get("grade"), "compared" if bool(compare.get("available")) else "described", "judgment.grade", errors)
    expected_verdict = "not_attached"
    if bool(compare.get("available")):
        expected_verdict = "drifted" if bool(compare.get("reason_changed")) else "standing"
    expect_equal(compare.get("action_verdict"), expected_verdict, "compare_summary.action_verdict", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate runtime-session wrapper open-event summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to runtime-session wrapper open-event summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.open-event.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.open-event.summary.json.",
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
            args.bundle_root or "out/system-compiler-front-page-entry-runtime-session-opening-flow-open-event"
        ).resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.open-event.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / OPEN_EVENT_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)
        validate_status(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        bridge_summary_path = Path(artifact_context.get("source_action_summary_path", "")).resolve()
        bridge_summary = load_json(bridge_summary_path)
        jsonschema.validate(bridge_summary, load_json((repo_root / BRIDGE_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            bridge_summary_path=bridge_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("open_event_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_open_event",
            "front_page",
            "artifact_context",
            "open_event",
            "consumer_decision",
            "plan",
            "action_records",
            "compare_summary",
            "workspace_facade",
            "diagnostic_preview",
            "witness_refs",
            "judgment",
            "explanation_view",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("open_event_summary_path", "")),
            "artifact_context.open_event_summary_path",
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
    print(f"[OK] event -> {summary.get('open_event', {}).get('open_event_id', '')}")
    print(f"[OK] status -> {summary.get('open_event', {}).get('status', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
