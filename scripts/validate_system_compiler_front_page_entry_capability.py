from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_entry_capability_lib import (
    build_summary_model,
    load_json,
    normalize_path,
)


CAPABILITY_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_capability.v0.schema.json"
ROUTE_SCHEMA_PATH = "schemas/system_compiler.front_page_route.v0.schema.json"


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


def validate_source_schema(path_value: str | None, expected_schema: str, label: str, errors: list[str]) -> None:
    if expected_schema != "system_compiler.artifact_report_index/v0":
        return

    text = str(path_value or "").strip()
    if not text:
        return

    try:
        source = load_json(Path(text).resolve())
    except Exception as exc:
        errors.append(f"{label}: invalid json -> {exc}")
        return

    if source.get("schema") != expected_schema:
        errors.append(f"{label}: expected schema {expected_schema!r} but got {source.get('schema')!r}")


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    front_page = summary.get("front_page", {})
    root_surface = summary.get("root_surface", {})

    ensure_exists(artifact_context.get("input_route_summary_path"), "artifact_context.input_route_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("capability_summary_path"), "artifact_context.capability_summary_path", errors)
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
        validate_source_schema(
            route.get("source_summary_path"),
            str(route.get("source_summary_schema") or ""),
            f"route_provenance[{index}].source_summary_path",
            errors,
        )
        if route.get("route_kind") != "artifact_report_index":
            ensure_exists(route.get("source_input_summary_path"), f"route_provenance[{index}].source_input_summary_path", errors)
            ensure_exists(route.get("source_root_summary_path"), f"route_provenance[{index}].source_root_summary_path", errors)
            ensure_exists(route.get("source_report_markdown_path"), f"route_provenance[{index}].source_report_markdown_path", errors)
            ensure_exists(route.get("source_check_text_path"), f"route_provenance[{index}].source_check_text_path", errors)

    for index, hint in enumerate(summary.get("provenance_hints", [])):
        if not isinstance(hint, dict):
            errors.append(f"provenance_hints[{index}]: invalid hint")
            continue
        ensure_exists(hint.get("source_summary_path"), f"provenance_hints[{index}].source_summary_path", errors)
        validate_source_schema(
            hint.get("source_summary_path"),
            str(hint.get("source_summary_schema") or ""),
            f"provenance_hints[{index}].source_summary_path",
            errors,
        )

    preferred_entries = summary.get("capability_summary", {}).get("preferred_entries", {})
    if isinstance(preferred_entries, dict):
        for capability_id, entry_ref in preferred_entries.items():
            if entry_ref is None:
                continue
            if not isinstance(entry_ref, dict):
                errors.append(f"capability_summary.preferred_entries.{capability_id}: invalid entry")
                continue
            ensure_exists(
                entry_ref.get("summary_path"),
                f"capability_summary.preferred_entries.{capability_id}.summary_path",
                errors,
            )
            ensure_exists(
                entry_ref.get("report_markdown_path"),
                f"capability_summary.preferred_entries.{capability_id}.report_markdown_path",
                errors,
            )
            ensure_exists(
                entry_ref.get("check_text_path"),
                f"capability_summary.preferred_entries.{capability_id}.check_text_path",
                errors,
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry capability summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry capability summary JSON. If omitted, --bundle-root/front-page.entry-capability.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-capability.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-capability").resolve()
        summary_path = bundle_root / "front-page.entry-capability.summary.json"

    capability_schema_path = (repo_root / CAPABILITY_SCHEMA_PATH).resolve()
    route_schema_path = (repo_root / ROUTE_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        capability_schema = load_json(capability_schema_path)
        route_schema = load_json(route_schema_path)
        import jsonschema

        jsonschema.validate(summary, capability_schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        input_route_summary_path = Path(artifact_context.get("input_route_summary_path", "")).resolve()
        output_root = Path(artifact_context.get("output_root", "")).resolve()
        capability_summary_path = Path(artifact_context.get("capability_summary_path", "")).resolve()
        report_path = Path(artifact_context.get("report_markdown_path", "")).resolve()
        check_path = Path(artifact_context.get("check_text_path", "")).resolve()

        route_summary = load_json(input_route_summary_path)
        jsonschema.validate(route_summary, route_schema)

        expected_summary = build_summary_model(
            route_summary_path=input_route_summary_path,
            output_root=output_root,
            summary_path=capability_summary_path,
            report_path=report_path,
            check_path=check_path,
        )

        expect_equal(summary.get("schema"), expected_summary.get("schema"), "schema", errors)
        expect_equal(summary.get("kind"), expected_summary.get("kind"), "kind", errors)
        expect_equal(summary.get("generator"), expected_summary.get("generator"), "generator", errors)
        expect_equal(summary.get("result"), expected_summary.get("result"), "result", errors)
        expect_equal(summary.get("entry_capability"), expected_summary.get("entry_capability"), "entry_capability", errors)
        expect_equal(summary.get("front_page"), expected_summary.get("front_page"), "front_page", errors)
        expect_equal(summary.get("route_provenance"), expected_summary.get("route_provenance"), "route_provenance", errors)
        expect_equal(summary.get("artifact_context"), expected_summary.get("artifact_context"), "artifact_context", errors)
        expect_equal(summary.get("root_surface"), expected_summary.get("root_surface"), "root_surface", errors)
        expect_equal(summary.get("entry_status"), expected_summary.get("entry_status"), "entry_status", errors)
        expect_equal(summary.get("capability_summary"), expected_summary.get("capability_summary"), "capability_summary", errors)
        expect_equal(summary.get("provenance_hints"), expected_summary.get("provenance_hints"), "provenance_hints", errors)
        expect_equal(summary.get("questions"), expected_summary.get("questions"), "questions", errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(summary.get("artifact_context", {}).get("capability_summary_path", "")),
            "artifact_context.capability_summary_path",
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
    print(f"[OK] recommended mode -> {summary.get('entry_status', {}).get('recommended_entry_mode', '')}")
    print(f"[OK] entry tier -> {summary.get('entry_status', {}).get('entry_tier', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

