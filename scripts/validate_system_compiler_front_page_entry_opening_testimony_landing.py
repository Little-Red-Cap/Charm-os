from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_testimony_landing import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


OPENING_TESTIMONY_LANDING_SCHEMA_PATH = (
    "schemas/system_compiler.front_page_entry_opening_testimony_landing.v0.schema.json"
)
OPEN_EVENT_WITNESS_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event_witness.v0.schema.json"


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
    source_ref = summary.get("source_witness_ref", {})
    identity = summary.get("opening_identity", {})
    targets = summary.get("artifact_targets", {})

    ensure_exists(artifact_context.get("source_witness_summary_path"), "artifact_context.source_witness_summary_path", errors)
    ensure_exists(
        artifact_context.get("source_witness_report_markdown_path"),
        "artifact_context.source_witness_report_markdown_path",
        errors,
    )
    ensure_exists(
        artifact_context.get("source_witness_check_text_path"),
        "artifact_context.source_witness_check_text_path",
        errors,
    )
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("landing_summary_path"), "artifact_context.landing_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    ensure_exists(source_ref.get("summary_path"), "source_witness_ref.summary_path", errors)
    ensure_exists(source_ref.get("report_markdown_path"), "source_witness_ref.report_markdown_path", errors)
    ensure_exists(source_ref.get("check_text_path"), "source_witness_ref.check_text_path", errors)
    ensure_exists(identity.get("source_open_event_summary_path"), "opening_identity.source_open_event_summary_path", errors)

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
            required=False,
        )
        ensure_exists(
            surface.get("check_text_path"),
            f"front_page.supporting_surfaces[{index}].check_text_path",
            errors,
            required=False,
        )

    for index, ref in enumerate(targets.get("evidence_refs", [])):
        if not isinstance(ref, dict):
            errors.append(f"artifact_targets.evidence_refs[{index}]: invalid evidence ref")
            continue
        ensure_exists(ref.get("summary_path"), f"artifact_targets.evidence_refs[{index}].summary_path", errors)
        ensure_exists(
            ref.get("report_markdown_path"),
            f"artifact_targets.evidence_refs[{index}].report_markdown_path",
            errors,
            required=False,
        )
        ensure_exists(
            ref.get("check_text_path"),
            f"artifact_targets.evidence_refs[{index}].check_text_path",
            errors,
            required=False,
        )
    for index, path in enumerate(targets.get("witness_artifact_refs", [])):
        ensure_exists(path, f"artifact_targets.witness_artifact_refs[{index}]", errors, required=False)


def validate_counts(summary: dict, errors: list[str]) -> None:
    targets = summary.get("artifact_targets", {})
    preview = summary.get("testimony_preview", {})
    expect_equal(
        targets.get("evidence_ref_count"),
        len(targets.get("evidence_refs", [])),
        "artifact_targets.evidence_ref_count",
        errors,
    )
    expect_equal(
        targets.get("witness_artifact_ref_count"),
        len(targets.get("witness_artifact_refs", [])),
        "artifact_targets.witness_artifact_ref_count",
        errors,
    )
    expect_equal(
        preview.get("summary_line_count"),
        len(preview.get("summary_lines", [])),
        "testimony_preview.summary_line_count",
        errors,
    )
    expect_equal(
        preview.get("explanation_line_count"),
        len(preview.get("explanation_text_lines", [])),
        "testimony_preview.explanation_line_count",
        errors,
    )
    expect_equal(
        preview.get("observation_count"),
        len(preview.get("observation_lines", [])),
        "testimony_preview.observation_count",
        errors,
    )


def validate_status(summary: dict, errors: list[str]) -> None:
    decision = summary.get("landing_decision", {})
    violations = summary.get("violations", [])
    expected_result = "ok" if decision.get("status") == "ready" else "fail"
    expect_equal(summary.get("result"), expected_result, "result", errors)
    if decision.get("status") == "ready" and violations:
        errors.append("violations must be empty when landing_decision.status is ready")
    if decision.get("status") == "blocked" and not violations:
        errors.append("violations must explain blocked landing_decision.status")
    expect_equal(decision.get("selected_entry_id"), "open-event-witness", "landing_decision.selected_entry_id", errors)
    expect_equal(decision.get("selected_tab_id"), "opening_testimony", "landing_decision.selected_tab_id", errors)
    expect_equal(decision.get("selected_role"), "opening_testimony", "landing_decision.selected_role", errors)

    question_kinds = [question.get("kind") for question in summary.get("next_questions", []) if isinstance(question, dict)]
    expect_equal(
        question_kinds,
        ["inspect_open_event", "inspect_evidence_refs", "compare_open_event_witness"],
        "next_questions.kind",
        errors,
    )


def validate_no_runtime_session_raw_surface(summary: dict, errors: list[str]) -> None:
    forbidden_names = {
        "runtime_session_summary",
        "world_compare_summary",
        "session_witness_inspect_compare_consumer",
        "baseline_summary",
        "candidate_summary",
    }

    def walk(value, path: str) -> None:
        if isinstance(value, dict):
            for key, item in value.items():
                key_text = str(key)
                if key_text in forbidden_names:
                    errors.append(f"{path}.{key_text}: forbidden raw runtime/session compare field")
                walk(item, f"{path}.{key_text}")
        elif isinstance(value, list):
            for index, item in enumerate(value):
                walk(item, f"{path}[{index}]")

    walk(summary, "summary")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening testimony landing summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to opening testimony landing summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-testimony.landing.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-testimony.landing.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-testimony-landing").resolve()
        summary_path = bundle_root / "front-page.entry-opening-testimony.landing.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / OPENING_TESTIMONY_LANDING_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)
        validate_status(summary, errors)
        validate_no_runtime_session_raw_surface(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        witness_summary_path = Path(artifact_context.get("source_witness_summary_path", "")).resolve()
        witness_summary = load_json(witness_summary_path)
        jsonschema.validate(witness_summary, load_json((repo_root / OPEN_EVENT_WITNESS_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            witness_summary_path=witness_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("landing_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_testimony_landing",
            "front_page",
            "artifact_context",
            "source_witness_ref",
            "opening_identity",
            "landing_decision",
            "testimony_preview",
            "artifact_targets",
            "next_questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("landing_summary_path", "")),
            "artifact_context.landing_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    decision = summary.get("landing_decision", {})
    identity = summary.get("opening_identity", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] landing -> {decision.get('status', '')}")
    print(f"[OK] open_event -> {identity.get('open_event_id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
