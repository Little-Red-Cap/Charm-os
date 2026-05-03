from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_entry_opening_flow_compare_lib import (
    build_compare_summary_model,
    load_json,
    normalize_path,
)


COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_compare.v0.schema.json"
FLOW_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow.v0.schema.json"


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

    for field in (
        "baseline_flow_summary_path",
        "candidate_flow_summary_path",
        "output_root",
        "compare_summary_path",
        "report_markdown_path",
        "check_text_path",
    ):
        ensure_exists(artifact_context.get(field), f"artifact_context.{field}", errors)

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

    for index, provenance in enumerate(summary.get("flow_provenance", [])):
        if not isinstance(provenance, dict):
            errors.append(f"flow_provenance[{index}]: invalid provenance entry")
            continue
        ensure_exists(provenance.get("source_summary_path"), f"flow_provenance[{index}].source_summary_path", errors)
        ensure_exists(
            provenance.get("source_report_markdown_path"),
            f"flow_provenance[{index}].source_report_markdown_path",
            errors,
        )
        ensure_exists(
            provenance.get("source_check_text_path"),
            f"flow_provenance[{index}].source_check_text_path",
            errors,
        )

    for index, change in enumerate(summary.get("opener_case_changes", [])):
        if not isinstance(change, dict):
            errors.append(f"opener_case_changes[{index}]: invalid case change")
            continue
        ensure_exists(
            change.get("baseline_summary_path"),
            f"opener_case_changes[{index}].baseline_summary_path",
            errors,
        ) if change.get("baseline_summary_path") else None
        ensure_exists(
            change.get("candidate_summary_path"),
            f"opener_case_changes[{index}].candidate_summary_path",
            errors,
        ) if change.get("candidate_summary_path") else None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening flow compare summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry opening flow compare summary JSON. If omitted, --bundle-root/front-page.entry-opening-flow.compare.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.compare.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-compare").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.compare.summary.json"

    compare_schema_path = (repo_root / COMPARE_SCHEMA_PATH).resolve()
    flow_schema_path = (repo_root / FLOW_SCHEMA_PATH).resolve()

    try:
        import jsonschema

        summary = load_json(summary_path)
        compare_schema = load_json(compare_schema_path)
        flow_schema = load_json(flow_schema_path)
        jsonschema.validate(summary, compare_schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        baseline_flow_path = Path(artifact_context.get("baseline_flow_summary_path", "")).resolve()
        candidate_flow_path = Path(artifact_context.get("candidate_flow_summary_path", "")).resolve()
        output_root = Path(artifact_context.get("output_root", "")).resolve()
        compare_summary_path = Path(artifact_context.get("compare_summary_path", "")).resolve()
        report_path = Path(artifact_context.get("report_markdown_path", "")).resolve()
        check_path = Path(artifact_context.get("check_text_path", "")).resolve()

        jsonschema.validate(load_json(baseline_flow_path), flow_schema)
        jsonschema.validate(load_json(candidate_flow_path), flow_schema)

        expected_summary = build_compare_summary_model(
            baseline_flow_path=baseline_flow_path,
            candidate_flow_path=candidate_flow_path,
            output_root=output_root,
            summary_path=compare_summary_path,
            report_path=report_path,
            check_path=check_path,
        )

        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_compare",
            "front_page",
            "flow_provenance",
            "artifact_context",
            "flow_verdict",
            "flow_status",
            "flow_changes",
            "opener_case_summary",
            "opener_case_changes",
            "flow_regression_surface",
            "questions",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(summary.get("artifact_context", {}).get("compare_summary_path", "")),
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
    print(f"[OK] flow verdict -> {summary.get('flow_verdict', '')}")
    print(f"[OK] changed cases -> {summary.get('opener_case_summary', {}).get('changed_case_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
