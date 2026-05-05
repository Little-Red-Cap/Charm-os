from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_flow_open_event_witness import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


OPEN_EVENT_WITNESS_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event_witness.v0.schema.json"
OPEN_EVENT_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json"


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
    witness_entry = summary.get("witness_entry", {})

    ensure_exists(artifact_context.get("source_open_event_summary_path"), "artifact_context.source_open_event_summary_path", errors)
    ensure_exists(artifact_context.get("source_open_event_report_markdown_path"), "artifact_context.source_open_event_report_markdown_path", errors)
    ensure_exists(artifact_context.get("source_open_event_check_text_path"), "artifact_context.source_open_event_check_text_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("open_event_witness_summary_path"), "artifact_context.open_event_witness_summary_path", errors)
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
        ensure_exists(surface.get("report_markdown_path"), f"front_page.supporting_surfaces[{index}].report_markdown_path", errors, required=False)
        ensure_exists(surface.get("check_text_path"), f"front_page.supporting_surfaces[{index}].check_text_path", errors, required=False)

    ensure_exists(witness_entry.get("source_path"), "witness_entry.source_path", errors)
    for index, path in enumerate(witness_entry.get("artifact_refs", [])):
        ensure_exists(path, f"witness_entry.artifact_refs[{index}]", errors, required=False)
    for index, ref in enumerate(summary.get("evidence_refs", [])):
        if not isinstance(ref, dict):
            errors.append(f"evidence_refs[{index}]: invalid evidence ref")
            continue
        ensure_exists(ref.get("summary_path"), f"evidence_refs[{index}].summary_path", errors)
        ensure_exists(ref.get("report_markdown_path"), f"evidence_refs[{index}].report_markdown_path", errors, required=False)
        ensure_exists(ref.get("check_text_path"), f"evidence_refs[{index}].check_text_path", errors, required=False)
    explanation = summary.get("explanation", {})
    for index, observation in enumerate(explanation.get("opening_input_observations", [])):
        if not isinstance(observation, dict):
            errors.append(f"explanation.opening_input_observations[{index}]: invalid observation")
            continue
        ensure_exists(observation.get("path"), f"explanation.opening_input_observations[{index}].path", errors, required=False)


def validate_counts(summary: dict, errors: list[str]) -> None:
    judgment = summary.get("judgment", {})
    witness_entry = summary.get("witness_entry", {})
    evidence_refs = summary.get("evidence_refs", [])
    artifact_refs = witness_entry.get("artifact_refs", [])
    expect_equal(judgment.get("evidence_ref_count"), len(evidence_refs), "judgment.evidence_ref_count", errors)
    expect_equal(judgment.get("artifact_ref_count"), len(artifact_refs), "judgment.artifact_ref_count", errors)
    expect_equal(witness_entry.get("id"), judgment.get("witness_id"), "witness_entry.id", errors)
    expect_equal(witness_entry.get("status"), judgment.get("witness_status"), "witness_entry.status", errors)
    expect_equal(judgment.get("accepted"), judgment.get("witness_status") == "ok", "judgment.accepted", errors)
    opening_inputs = summary.get("explanation", {}).get("opening_input_observations", [])
    if opening_inputs and len(opening_inputs) < 4:
        errors.append("explanation.opening_input_observations must contain at least four primary opening refs")


def validate_status(summary: dict, errors: list[str]) -> None:
    result = summary.get("result")
    judgment = summary.get("judgment", {})
    identity = summary.get("open_event_identity", {})
    violations = summary.get("violations", [])
    expected_result = "ok" if judgment.get("witness_status") == "ok" else "fail"
    expect_equal(result, expected_result, "result", errors)
    expect_equal(judgment.get("source_judgment_status"), identity.get("open_event_status"), "judgment.source_judgment_status", errors)
    expected_grade = "compared" if bool(judgment.get("compare_available")) else "described"
    expect_equal(judgment.get("source_judgment_grade"), expected_grade, "judgment.source_judgment_grade", errors)
    source_basis = judgment.get("source_judgment_basis", [])
    if not isinstance(source_basis, list) or not source_basis:
        errors.append("judgment.source_judgment_basis must be a non-empty list")
    if "open_event" not in source_basis:
        errors.append("judgment.source_judgment_basis must include open_event")
    if judgment.get("witness_status") == "ok" and violations:
        errors.append("violations must be empty when witness_status is ok")
    if judgment.get("witness_status") == "fail" and not violations:
        errors.append("violations must explain fail witness_status")
    has_runtime_session_inputs = bool(summary.get("explanation", {}).get("opening_input_observations"))
    required_focus = (
        ["front_page", "opening_flow", "runtime_session", "session_witness", "artifact_target"]
        if has_runtime_session_inputs
        else ["front_page", "opening_flow", "consumer", "plan", "compare", "workspace"]
    )
    expect_equal(summary.get("witness_entry", {}).get("witness_focus"), required_focus, "witness_entry.witness_focus", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening-flow open-event witness summary."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to open-event witness summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.open-event.witness.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.open-event.witness.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-open-event-witness").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.open-event.witness.summary.json"

    try:
        import jsonschema

        summary = load_json(summary_path)
        jsonschema.validate(summary, load_json((repo_root / OPEN_EVENT_WITNESS_SCHEMA_PATH).resolve()))

        errors: list[str] = []
        validate_references(summary, errors)
        validate_counts(summary, errors)
        validate_status(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        open_event_summary_path = Path(artifact_context.get("source_open_event_summary_path", "")).resolve()
        open_event_summary = load_json(open_event_summary_path)
        jsonschema.validate(open_event_summary, load_json((repo_root / OPEN_EVENT_SCHEMA_PATH).resolve()))

        expected_summary = build_summary_model(
            open_event_summary_path=open_event_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("open_event_witness_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )
        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_open_event_witness",
            "front_page",
            "artifact_context",
            "open_event_identity",
            "judgment",
            "witness_entry",
            "evidence_refs",
            "explanation",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("open_event_witness_summary_path", "")),
            "artifact_context.open_event_witness_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    judgment = summary.get("judgment", {})
    identity = summary.get("open_event_identity", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] witness -> {judgment.get('witness_id', '')}")
    print(f"[OK] event -> {identity.get('open_event_id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
