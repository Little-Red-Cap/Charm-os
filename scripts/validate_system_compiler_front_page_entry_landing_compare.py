from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_entry_landing_compare_lib import (
    build_compare_summary_model,
    load_json,
    normalize_path,
)


COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_landing_compare.v0.schema.json"
LANDING_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_landing.v0.schema.json"


def ensure_exists(path_value: str | None, label: str, errors: list[str]) -> None:
    if path_value is None:
        errors.append(f"{label}: missing path")
        return
    text = str(path_value).strip()
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

    ensure_exists(
        artifact_context.get("baseline_landing_summary_path"),
        "artifact_context.baseline_landing_summary_path",
        errors,
    )
    ensure_exists(
        artifact_context.get("candidate_landing_summary_path"),
        "artifact_context.candidate_landing_summary_path",
        errors,
    )
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("compare_summary_path"), "artifact_context.compare_summary_path", errors)
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

    for index, landing in enumerate(summary.get("landing_provenance", [])):
        if not isinstance(landing, dict):
            errors.append(f"landing_provenance[{index}]: invalid landing entry")
            continue
        ensure_exists(landing.get("source_summary_path"), f"landing_provenance[{index}].source_summary_path", errors)
        ensure_exists(
            landing.get("source_input_capability_summary_path"),
            f"landing_provenance[{index}].source_input_capability_summary_path",
            errors,
        )
        ensure_exists(
            landing.get("source_root_summary_path"),
            f"landing_provenance[{index}].source_root_summary_path",
            errors,
        )
        ensure_exists(
            landing.get("source_report_markdown_path"),
            f"landing_provenance[{index}].source_report_markdown_path",
            errors,
        )
        ensure_exists(
            landing.get("source_check_text_path"),
            f"landing_provenance[{index}].source_check_text_path",
            errors,
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry landing compare summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry landing compare summary JSON. If omitted, --bundle-root/front-page.entry-landing.compare.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-landing.compare.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-landing-compare").resolve()
        summary_path = bundle_root / "front-page.entry-landing.compare.summary.json"

    compare_schema_path = (repo_root / COMPARE_SCHEMA_PATH).resolve()
    landing_schema_path = (repo_root / LANDING_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        compare_schema = load_json(compare_schema_path)
        landing_schema = load_json(landing_schema_path)
        import jsonschema

        jsonschema.validate(summary, compare_schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        baseline_landing_path = Path(artifact_context.get("baseline_landing_summary_path", "")).resolve()
        candidate_landing_path = Path(artifact_context.get("candidate_landing_summary_path", "")).resolve()
        output_root = Path(artifact_context.get("output_root", "")).resolve()
        compare_summary_path = Path(artifact_context.get("compare_summary_path", "")).resolve()
        report_path = Path(artifact_context.get("report_markdown_path", "")).resolve()
        check_path = Path(artifact_context.get("check_text_path", "")).resolve()

        baseline_landing = load_json(baseline_landing_path)
        candidate_landing = load_json(candidate_landing_path)
        jsonschema.validate(baseline_landing, landing_schema)
        jsonschema.validate(candidate_landing, landing_schema)

        expected_summary = build_compare_summary_model(
            baseline_landing_path=baseline_landing_path,
            candidate_landing_path=candidate_landing_path,
            output_root=output_root,
            summary_path=compare_summary_path,
            report_path=report_path,
            check_path=check_path,
        )

        expect_equal(summary.get("schema"), expected_summary.get("schema"), "schema", errors)
        expect_equal(summary.get("kind"), expected_summary.get("kind"), "kind", errors)
        expect_equal(summary.get("generator"), expected_summary.get("generator"), "generator", errors)
        expect_equal(summary.get("result"), expected_summary.get("result"), "result", errors)
        expect_equal(summary.get("landing_compare"), expected_summary.get("landing_compare"), "landing_compare", errors)
        expect_equal(summary.get("front_page"), expected_summary.get("front_page"), "front_page", errors)
        expect_equal(summary.get("landing_provenance"), expected_summary.get("landing_provenance"), "landing_provenance", errors)
        expect_equal(summary.get("artifact_context"), expected_summary.get("artifact_context"), "artifact_context", errors)
        expect_equal(summary.get("landing_verdict"), expected_summary.get("landing_verdict"), "landing_verdict", errors)
        expect_equal(summary.get("landing_status"), expected_summary.get("landing_status"), "landing_status", errors)
        expect_equal(summary.get("primary_query_status"), expected_summary.get("primary_query_status"), "primary_query_status", errors)
        expect_equal(summary.get("landing_changes"), expected_summary.get("landing_changes"), "landing_changes", errors)
        expect_equal(summary.get("query_plan_changes"), expected_summary.get("query_plan_changes"), "query_plan_changes", errors)
        expect_equal(summary.get("query_summary"), expected_summary.get("query_summary"), "query_summary", errors)
        expect_equal(summary.get("query_changes"), expected_summary.get("query_changes"), "query_changes", errors)
        expect_equal(summary.get("tab_summary"), expected_summary.get("tab_summary"), "tab_summary", errors)
        expect_equal(summary.get("tab_changes"), expected_summary.get("tab_changes"), "tab_changes", errors)
        expect_equal(
            summary.get("landing_regression_surface"),
            expected_summary.get("landing_regression_surface"),
            "landing_regression_surface",
            errors,
        )
        expect_equal(
            summary.get("query_regression_surface"),
            expected_summary.get("query_regression_surface"),
            "query_regression_surface",
            errors,
        )
        expect_equal(summary.get("questions"), expected_summary.get("questions"), "questions", errors)
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
    print(f"[OK] landing verdict -> {summary.get('landing_verdict', '')}")
    print(f"[OK] changed tabs -> {summary.get('tab_summary', {}).get('changed_tab_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
