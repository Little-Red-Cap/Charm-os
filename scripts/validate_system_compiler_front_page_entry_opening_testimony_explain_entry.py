from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

from export_system_compiler_front_page_entry_opening_testimony_explain_entry import (
    EXPLAIN_ENTRY_SCHEMA,
    build_summary_model,
)
from system_compiler_front_page_route_lib import load_json, normalize_path


EXPLAIN_ENTRY_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_testimony_explain_entry.v0.schema.json"
ROUTE_SCHEMA_PATH = "schemas/system_compiler.front_page_route.v0.schema.json"
ROUTE_COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_route_compare.v0.schema.json"


def ensure_exists(path_value: Any, label: str, errors: list[str], required: bool = True) -> None:
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


def validate_references(summary: dict[str, Any], errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    front_page = summary.get("front_page", {})
    source_ref = summary.get("source_route_ref", {})
    selected = summary.get("selected_surface", {})

    ensure_exists(artifact_context.get("source_summary_path"), "artifact_context.source_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("explain_entry_summary_path"), "artifact_context.explain_entry_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    ensure_exists(source_ref.get("summary_path"), "source_route_ref.summary_path", errors)
    ensure_exists(source_ref.get("report_markdown_path"), "source_route_ref.report_markdown_path", errors)
    ensure_exists(source_ref.get("check_text_path"), "source_route_ref.check_text_path", errors)
    ensure_exists(source_ref.get("baseline_route_summary_path"), "source_route_ref.baseline_route_summary_path", errors, required=False)
    ensure_exists(source_ref.get("candidate_route_summary_path"), "source_route_ref.candidate_route_summary_path", errors, required=False)

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

    if summary.get("result") == "ok":
        ensure_exists(selected.get("summary_path"), "selected_surface.summary_path", errors)
        ensure_exists(selected.get("report_markdown_path"), "selected_surface.report_markdown_path", errors)
        ensure_exists(selected.get("check_text_path"), "selected_surface.check_text_path", errors)

    for index, surface in enumerate(summary.get("supporting_surfaces", [])):
        if not isinstance(surface, dict):
            errors.append(f"supporting_surfaces[{index}]: invalid surface")
            continue
        ensure_exists(surface.get("summary_path"), f"supporting_surfaces[{index}].summary_path", errors, required=False)
        ensure_exists(surface.get("report_markdown_path"), f"supporting_surfaces[{index}].report_markdown_path", errors, required=False)
        ensure_exists(surface.get("check_text_path"), f"supporting_surfaces[{index}].check_text_path", errors, required=False)


def validate_status(summary: dict[str, Any], errors: list[str]) -> None:
    decision = summary.get("explain_entry_decision", {})
    expected_result = "ok" if decision.get("status") == "ready" else "fail"
    expect_equal(summary.get("result"), expected_result, "result", errors)
    if decision.get("status") == "ready" and summary.get("violations"):
        errors.append("violations must be empty when explain_entry_decision.status is ready")
    if decision.get("status") == "blocked" and not summary.get("violations"):
        errors.append("violations must explain blocked explain_entry_decision.status")
    expect_equal(decision.get("selected_tab_id"), "opening_testimony_explain", "selected_tab_id", errors)
    expect_equal(decision.get("selected_role"), "opening_testimony_explain_entry", "selected_role", errors)

    question_kinds = [question.get("kind") for question in summary.get("next_questions", []) if isinstance(question, dict)]
    expect_equal(
        question_kinds,
        ["inspect_selected_surface", "inspect_source_route", "inspect_supporting_surfaces"],
        "next_questions.kind",
        errors,
    )


def validate_no_raw_runtime_session_fields(summary: dict[str, Any], errors: list[str]) -> None:
    forbidden_names = {
        "runtime_session_summary",
        "world_compare_summary",
        "session_witness_inspect_compare_consumer",
        "runtime_evidence_bundle",
    }

    def walk(value: Any, path: str) -> None:
        if isinstance(value, dict):
            for key, item in value.items():
                key_text = str(key)
                if key_text in forbidden_names:
                    errors.append(f"{path}.{key_text}: forbidden raw runtime/session field")
                walk(item, f"{path}.{key_text}")
        elif isinstance(value, list):
            for index, item in enumerate(value):
                walk(item, f"{path}[{index}]")

    walk(summary, "summary")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler opening testimony explain-entry summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to opening testimony explain-entry summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-testimony.explain-entry.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-testimony.explain-entry.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry").resolve()
        summary_path = bundle_root / "front-page.entry-opening-testimony.explain-entry.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / EXPLAIN_ENTRY_SCHEMA_PATH).resolve()))
        if summary.get("schema") != EXPLAIN_ENTRY_SCHEMA:
            raise ValueError(f"unsupported opening testimony explain-entry schema: {summary_path}")

        errors: list[str] = []
        validate_references(summary, errors)
        validate_status(summary, errors)
        validate_no_raw_runtime_session_fields(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        source_summary_path = Path(artifact_context.get("source_summary_path", "")).resolve()
        source_summary = load_json(source_summary_path)
        source_schema = source_summary.get("schema")
        if source_schema == "system_compiler.front_page_route/v0":
            jsonschema.validate(source_summary, load_json((repo_root / ROUTE_SCHEMA_PATH).resolve()))
        elif source_schema == "system_compiler.front_page_route_compare/v0":
            jsonschema.validate(source_summary, load_json((repo_root / ROUTE_COMPARE_SCHEMA_PATH).resolve()))
        else:
            errors.append(f"source summary schema is unsupported: {source_schema!r}")

        expected_summary = build_summary_model(
            source_summary_path=source_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("explain_entry_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_testimony_explain_entry",
            "front_page",
            "artifact_context",
            "source_route_ref",
            "explain_entry_decision",
            "selected_surface",
            "supporting_surfaces",
            "opening_reason",
            "next_questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("explain_entry_summary_path", "")),
            "artifact_context.explain_entry_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    decision = summary.get("explain_entry_decision", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] explain entry -> {decision.get('status', '')}")
    print(f"[OK] selected surface -> {summary.get('selected_surface', {}).get('surface_id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
