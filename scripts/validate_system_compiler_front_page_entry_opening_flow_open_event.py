from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_flow_open_event import build_summary_model
from system_compiler_front_page_route_lib import choose_text, load_json, normalize_path


OPEN_EVENT_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json"
ACTION_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action.v0.schema.json"
ACTION_COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare.v0.schema.json"


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

    ensure_exists(artifact_context.get("source_action_summary_path"), "artifact_context.source_action_summary_path", errors)
    ensure_exists(
        artifact_context.get("source_action_compare_summary_path"),
        "artifact_context.source_action_compare_summary_path",
        errors,
        required=False,
    )
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("open_event_summary_path"), "artifact_context.open_event_summary_path", errors)
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

    ensure_exists(source_artifact.get("summary_path"), "open_event.source_artifact.summary_path", errors)
    ensure_exists(source_artifact.get("opener_summary_path"), "open_event.source_artifact.opener_summary_path", errors)
    ensure_exists(source_artifact.get("opener_report_markdown_path"), "open_event.source_artifact.opener_report_markdown_path", errors)
    ensure_exists(source_artifact.get("opener_check_text_path"), "open_event.source_artifact.opener_check_text_path", errors)
    ensure_exists(workspace.get("primary_summary_path"), "workspace_facade.primary_summary_path", errors)
    ensure_exists(workspace.get("primary_report_markdown_path"), "workspace_facade.primary_report_markdown_path", errors)
    ensure_exists(workspace.get("primary_check_text_path"), "workspace_facade.primary_check_text_path", errors)

    for index, ref in enumerate(summary.get("witness_refs", [])):
        if not isinstance(ref, dict):
            errors.append(f"witness_refs[{index}]: invalid witness ref")
            continue
        ensure_exists(ref.get("summary_path"), f"witness_refs[{index}].summary_path", errors)
        ensure_exists(ref.get("report_markdown_path"), f"witness_refs[{index}].report_markdown_path", errors, required=False)
        ensure_exists(ref.get("check_text_path"), f"witness_refs[{index}].check_text_path", errors, required=False)


def validate_counts(summary: dict, errors: list[str]) -> None:
    decision = summary.get("consumer_decision", {})
    diagnostic = summary.get("diagnostic_preview", {})
    explanation = summary.get("explanation_view", {})
    candidates = decision.get("candidate_consumers", [])
    rejected = decision.get("rejected_consumers", [])
    selected = [candidate for candidate in candidates if isinstance(candidate, dict) and bool(candidate.get("selected"))]
    expect_equal(decision.get("candidate_consumer_count"), len(candidates), "consumer_decision.candidate_consumer_count", errors)
    expect_equal(decision.get("rejected_consumer_count"), len(rejected), "consumer_decision.rejected_consumer_count", errors)
    expect_equal(len(selected), 1, "consumer_decision.selected_candidate_count", errors)
    if selected:
        expect_equal(
            decision.get("selected_consumer", {}).get("selected_action_id"),
            selected[0].get("action_id"),
            "consumer_decision.selected_consumer.selected_action_id",
            errors,
        )

    witness_refs = summary.get("witness_refs", [])
    explanation_refs = explanation.get("witness_refs", [])
    expect_equal(len(explanation_refs), len(witness_refs), "explanation_view.witness_refs count", errors)
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
    expect_equal(
        explanation.get("diagnostic_summary_lines"),
        diagnostic.get("summary_lines"),
        "explanation_view.diagnostic_summary_lines",
        errors,
    )
    expect_equal(
        explanation.get("diagnostic_question_lines"),
        diagnostic.get("question_lines"),
        "explanation_view.diagnostic_question_lines",
        errors,
    )


def validate_status(summary: dict, errors: list[str]) -> None:
    event_status = summary.get("open_event", {}).get("status")
    result = summary.get("result")
    workspace_status = summary.get("workspace_facade", {}).get("status")
    compare = summary.get("compare_summary", {})
    judgment = summary.get("judgment", {})
    questions = summary.get("questions", {})

    expect_equal(result, "fail" if event_status == "blocked" else "ok", "result", errors)
    expect_equal(workspace_status, "blocked" if event_status == "blocked" else "projected", "workspace_facade.status", errors)
    expect_equal(judgment.get("semantic_role"), "opening_judgment_carrier", "judgment.semantic_role", errors)
    expect_equal(judgment.get("status"), event_status, "judgment.status", errors)
    expect_equal(judgment.get("accepted"), event_status != "blocked", "judgment.accepted", errors)
    expect_equal(judgment.get("grade"), "compared" if bool(compare.get("available")) else "described", "judgment.grade", errors)
    expected_basis = ["source_plan_action", "selected_opener", "open_event"]
    if bool(compare.get("available")):
        ensure_exists(compare.get("summary_path"), "compare_summary.summary_path", errors)
        expected_basis.append("source_action_compare")
    else:
        expect_equal(compare.get("action_verdict"), "not_attached", "compare_summary.action_verdict", errors)
    expect_equal(judgment.get("basis"), expected_basis, "judgment.basis", errors)
    typed_questions = questions.get("typed_next_questions", [])
    if not isinstance(typed_questions, list) or len(typed_questions) != 2:
        errors.append("questions.typed_next_questions must contain exactly two typed hints")
    else:
        expected_primary_kind = "inspect_action_compare" if bool(compare.get("available")) else "attach_action_compare"
        expect_equal(typed_questions[0].get("kind"), expected_primary_kind, "questions.typed_next_questions[0].kind", errors)
        expect_equal(
            typed_questions[1].get("kind"),
            "inspect_rejected_consumers",
            "questions.typed_next_questions[1].kind",
            errors,
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening-flow open-event summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to open-event summary JSON. If omitted, "
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-open-event").resolve()
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
        action_summary_path = Path(artifact_context.get("source_action_summary_path", "")).resolve()
        action_summary = load_json(action_summary_path)
        jsonschema.validate(action_summary, load_json((repo_root / ACTION_SCHEMA_PATH).resolve()))

        action_compare_summary_path_text = choose_text(artifact_context.get("source_action_compare_summary_path"))
        action_compare_summary_path = Path(action_compare_summary_path_text).resolve() if action_compare_summary_path_text else None
        if action_compare_summary_path is not None:
            action_compare_summary = load_json(action_compare_summary_path)
            jsonschema.validate(action_compare_summary, load_json((repo_root / ACTION_COMPARE_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            action_summary_path=action_summary_path,
            action_compare_summary_path=action_compare_summary_path,
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

    event = summary.get("open_event", {})
    selected = summary.get("consumer_decision", {}).get("selected_consumer", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] event -> {event.get('open_event_id', '')}")
    print(f"[OK] selected -> {selected.get('consumer_id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
