from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_entry_landing_lib import (
    build_summary_model,
    load_json,
    normalize_path,
)


LANDING_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_landing.v0.schema.json"
CAPABILITY_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_capability.v0.schema.json"


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
    root_surface = summary.get("root_surface", {})

    ensure_exists(artifact_context.get("input_capability_summary_path"), "artifact_context.input_capability_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("landing_summary_path"), "artifact_context.landing_summary_path", errors)
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

    ensure_exists(root_surface.get("summary_path"), "root_surface.summary_path", errors)

    for index, route in enumerate(summary.get("route_provenance", [])):
        if not isinstance(route, dict):
            errors.append(f"route_provenance[{index}]: invalid route entry")
            continue
        ensure_exists(route.get("source_summary_path"), f"route_provenance[{index}].source_summary_path", errors)
        ensure_exists(route.get("source_input_summary_path"), f"route_provenance[{index}].source_input_summary_path", errors)
        ensure_exists(route.get("source_root_summary_path"), f"route_provenance[{index}].source_root_summary_path", errors)
        ensure_exists(route.get("source_report_markdown_path"), f"route_provenance[{index}].source_report_markdown_path", errors)
        ensure_exists(route.get("source_check_text_path"), f"route_provenance[{index}].source_check_text_path", errors)

    if isinstance(summary.get("primary_landing"), dict):
        ensure_exists(summary["primary_landing"].get("entry", {}).get("summary_path"), "primary_landing.entry.summary_path", errors)
        ensure_exists(summary["primary_landing"].get("entry", {}).get("report_markdown_path"), "primary_landing.entry.report_markdown_path", errors)
        ensure_exists(summary["primary_landing"].get("entry", {}).get("check_text_path"), "primary_landing.entry.check_text_path", errors)

    for index, tab in enumerate(summary.get("landing_tabs", [])):
        if not isinstance(tab, dict):
            errors.append(f"landing_tabs[{index}]: invalid tab")
            continue
        entry = tab.get("entry", {})
        ensure_exists(entry.get("summary_path"), f"landing_tabs[{index}].entry.summary_path", errors)
        ensure_exists(entry.get("report_markdown_path"), f"landing_tabs[{index}].entry.report_markdown_path", errors)
        ensure_exists(entry.get("check_text_path"), f"landing_tabs[{index}].entry.check_text_path", errors)

    for index, root in enumerate(summary.get("provenance_roots", [])):
        if not isinstance(root, dict):
            errors.append(f"provenance_roots[{index}]: invalid root")
            continue
        ensure_exists(root.get("source_summary_path"), f"provenance_roots[{index}].source_summary_path", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry landing summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry landing summary JSON. If omitted, --bundle-root/front-page.entry-landing.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-landing.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-landing").resolve()
        summary_path = bundle_root / "front-page.entry-landing.summary.json"

    landing_schema_path = (repo_root / LANDING_SCHEMA_PATH).resolve()
    capability_schema_path = (repo_root / CAPABILITY_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        landing_schema = load_json(landing_schema_path)
        capability_schema = load_json(capability_schema_path)
        import jsonschema

        jsonschema.validate(summary, landing_schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        input_capability_summary_path = Path(artifact_context.get("input_capability_summary_path", "")).resolve()
        output_root = Path(artifact_context.get("output_root", "")).resolve()
        landing_summary_path = Path(artifact_context.get("landing_summary_path", "")).resolve()
        report_path = Path(artifact_context.get("report_markdown_path", "")).resolve()
        check_path = Path(artifact_context.get("check_text_path", "")).resolve()

        capability_summary = load_json(input_capability_summary_path)
        jsonschema.validate(capability_summary, capability_schema)

        expected_summary = build_summary_model(
            capability_summary_path=input_capability_summary_path,
            output_root=output_root,
            summary_path=landing_summary_path,
            report_path=report_path,
            check_path=check_path,
        )

        expect_equal(summary.get("schema"), expected_summary.get("schema"), "schema", errors)
        expect_equal(summary.get("kind"), expected_summary.get("kind"), "kind", errors)
        expect_equal(summary.get("generator"), expected_summary.get("generator"), "generator", errors)
        expect_equal(summary.get("result"), expected_summary.get("result"), "result", errors)
        expect_equal(summary.get("entry_landing"), expected_summary.get("entry_landing"), "entry_landing", errors)
        expect_equal(summary.get("front_page"), expected_summary.get("front_page"), "front_page", errors)
        expect_equal(summary.get("route_provenance"), expected_summary.get("route_provenance"), "route_provenance", errors)
        expect_equal(summary.get("artifact_context"), expected_summary.get("artifact_context"), "artifact_context", errors)
        expect_equal(summary.get("root_surface"), expected_summary.get("root_surface"), "root_surface", errors)
        expect_equal(summary.get("landing_status"), expected_summary.get("landing_status"), "landing_status", errors)
        expect_equal(summary.get("fallback_mode_order"), expected_summary.get("fallback_mode_order"), "fallback_mode_order", errors)
        expect_equal(summary.get("primary_landing"), expected_summary.get("primary_landing"), "primary_landing", errors)
        expect_equal(summary.get("secondary_landings"), expected_summary.get("secondary_landings"), "secondary_landings", errors)
        expect_equal(summary.get("landing_tabs"), expected_summary.get("landing_tabs"), "landing_tabs", errors)
        expect_equal(summary.get("provenance_roots"), expected_summary.get("provenance_roots"), "provenance_roots", errors)
        expect_equal(summary.get("questions"), expected_summary.get("questions"), "questions", errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(summary.get("artifact_context", {}).get("landing_summary_path", "")),
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

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] recommended mode -> {summary.get('landing_status', {}).get('recommended_entry_mode', '')}")
    print(f"[OK] primary tab -> {summary.get('landing_status', {}).get('primary_tab_id', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

