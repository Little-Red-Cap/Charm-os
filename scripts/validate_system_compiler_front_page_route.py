from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from system_compiler_front_page_route_lib import build_route_model, load_json, normalize_path


SCHEMA_PATH = "schemas/system_compiler.front_page_route.v0.schema.json"


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
    root_surface = summary.get("root_surface", {})

    ensure_exists(artifact_context.get("input_summary_path"), "artifact_context.input_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("route_summary_path"), "artifact_context.route_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    ensure_exists(root_surface.get("summary_path"), "root_surface.summary_path", errors)
    ensure_exists(root_surface.get("report_markdown_path"), "root_surface.report_markdown_path", errors)
    ensure_exists(root_surface.get("check_text_path"), "root_surface.check_text_path", errors)

    for index, entry in enumerate(summary.get("route_entries", [])):
        if not isinstance(entry, dict):
            errors.append(f"route_entries[{index}]: invalid entry")
            continue

        ensure_exists(entry.get("summary_path"), f"route_entries[{index}].summary_path", errors)
        ensure_exists(entry.get("report_markdown_path"), f"route_entries[{index}].report_markdown_path", errors)
        ensure_exists(entry.get("check_text_path"), f"route_entries[{index}].check_text_path", errors)

    for index, entry in enumerate(summary.get("route_provenance_entries", [])):
        if not isinstance(entry, dict):
            errors.append(f"route_provenance_entries[{index}]: invalid entry")
            continue

        ensure_exists(entry.get("owner_summary_path"), f"route_provenance_entries[{index}].owner_summary_path", errors)
        ensure_exists(entry.get("source_summary_path"), f"route_provenance_entries[{index}].source_summary_path", errors)
        validate_source_schema(
            entry.get("source_summary_path"),
            str(entry.get("source_summary_schema") or ""),
            f"route_provenance_entries[{index}].source_summary_path",
            errors,
        )

        if entry.get("provenance_route_kind") != "artifact_report_index":
            ensure_exists(
                entry.get("source_front_page_summary_path"),
                f"route_provenance_entries[{index}].source_front_page_summary_path",
                errors,
            )
            ensure_exists(
                entry.get("source_front_page_report_markdown_path"),
                f"route_provenance_entries[{index}].source_front_page_report_markdown_path",
                errors,
            )
            ensure_exists(
                entry.get("source_front_page_check_text_path"),
                f"route_provenance_entries[{index}].source_front_page_check_text_path",
                errors,
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page route summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to route summary JSON. If omitted, --bundle-root/front-page.route.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Output root containing front-page.route.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-route").resolve()
        summary_path = bundle_root / "front-page.route.summary.json"

    schema_path = (repo_root / SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        schema = load_json(schema_path)
        import jsonschema

        jsonschema.validate(summary, schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        input_summary_path = Path(artifact_context.get("input_summary_path", "")).resolve()
        (
            route_summary,
            route_provenance_summary,
            schema_counts,
            role_counts,
            root_surface,
            route_entries,
            route_provenance_entries,
        ) = build_route_model(input_summary_path)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("route_summary_path", "")),
            "artifact_context.route_summary_path",
            errors,
        )
        expect_equal(summary.get("root_surface"), root_surface, "root_surface", errors)
        expect_equal(summary.get("route_summary"), route_summary, "route_summary", errors)
        expect_equal(
            summary.get("route_provenance_summary"),
            route_provenance_summary,
            "route_provenance_summary",
            errors,
        )
        expect_equal(summary.get("schema_counts"), schema_counts, "schema_counts", errors)
        expect_equal(summary.get("role_counts"), role_counts, "role_counts", errors)
        expect_equal(summary.get("route_entries"), route_entries, "route_entries", errors)
        expect_equal(
            summary.get("route_provenance_entries"),
            route_provenance_entries,
            "route_provenance_entries",
            errors,
        )
        expect_equal(summary.get("result"), "ok", "result", errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] input summary -> {artifact_context.get('input_summary_path', '')}")
    print(f"[OK] route entries -> {route_summary['entry_count']}")
    print(f"[OK] route provenance entries -> {route_provenance_summary['entry_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
