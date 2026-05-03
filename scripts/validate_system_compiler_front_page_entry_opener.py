from __future__ import annotations

import argparse
import sys
from pathlib import Path

from system_compiler_front_page_entry_opener_lib import (
    build_summary_model,
    load_json,
    normalize_path,
)


OPENER_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opener.v0.schema.json"
LANDING_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_landing.v0.schema.json"
LANDING_COMPARE_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_landing_compare.v0.schema.json"


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
    open_action = summary.get("open_action", {})
    inspector_invocation = summary.get("inspector_invocation", {})
    opened_projection = summary.get("opened_projection", {})

    ensure_exists(artifact_context.get("source_landing_summary_path"), "artifact_context.source_landing_summary_path", errors)
    ensure_exists(
        artifact_context.get("source_landing_compare_summary_path"),
        "artifact_context.source_landing_compare_summary_path",
        errors,
        required=False,
    )
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("opener_summary_path"), "artifact_context.opener_summary_path", errors)
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

    ensure_exists(open_action.get("target_summary_path"), "open_action.target_summary_path", errors)
    ensure_exists(
        open_action.get("target_report_markdown_path"),
        "open_action.target_report_markdown_path",
        errors,
        required=False,
    )
    ensure_exists(
        open_action.get("target_check_text_path"),
        "open_action.target_check_text_path",
        errors,
        required=False,
    )

    if inspector_invocation.get("ready"):
        if not inspector_invocation.get("arguments"):
            errors.append("inspector_invocation.arguments: ready invocation must carry arguments")
        if not inspector_invocation.get("powershell_command"):
            errors.append("inspector_invocation.powershell_command: ready invocation must carry command")
    else:
        if inspector_invocation.get("arguments"):
            errors.append("inspector_invocation.arguments: blocked invocation must not carry arguments")
        if inspector_invocation.get("powershell_command"):
            errors.append("inspector_invocation.powershell_command: blocked invocation must not carry command")

    ensure_exists(
        opened_projection.get("source_summary_path"),
        "opened_projection.source_summary_path",
        errors,
        required=False,
    )
    for index, path_value in enumerate(opened_projection.get("supporting_summary_paths", [])):
        ensure_exists(path_value, f"opened_projection.supporting_summary_paths[{index}]", errors, required=False)
    for index, path_value in enumerate(opened_projection.get("evidence_paths", [])):
        ensure_exists(path_value, f"opened_projection.evidence_paths[{index}]", errors, required=False)
    for index, path_value in enumerate(opened_projection.get("compare_paths", [])):
        ensure_exists(path_value, f"opened_projection.compare_paths[{index}]", errors, required=False)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opener summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to entry opener summary JSON. If omitted, --bundle-root/front-page.entry-opener.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opener.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opener").resolve()
        summary_path = bundle_root / "front-page.entry-opener.summary.json"

    opener_schema_path = (repo_root / OPENER_SCHEMA_PATH).resolve()
    landing_schema_path = (repo_root / LANDING_SCHEMA_PATH).resolve()
    landing_compare_schema_path = (repo_root / LANDING_COMPARE_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        opener_schema = load_json(opener_schema_path)
        landing_schema = load_json(landing_schema_path)
        landing_compare_schema = load_json(landing_compare_schema_path)
        import jsonschema

        jsonschema.validate(summary, opener_schema)
        errors: list[str] = []
        validate_references(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        landing_summary_path = Path(artifact_context.get("source_landing_summary_path", "")).resolve()
        landing_compare_text = str(artifact_context.get("source_landing_compare_summary_path", "")).strip()
        landing_compare_summary_path = Path(landing_compare_text).resolve() if landing_compare_text else None
        output_root = Path(artifact_context.get("output_root", "")).resolve()
        opener_summary_path = Path(artifact_context.get("opener_summary_path", "")).resolve()
        report_path = Path(artifact_context.get("report_markdown_path", "")).resolve()
        check_path = Path(artifact_context.get("check_text_path", "")).resolve()

        landing_summary = load_json(landing_summary_path)
        jsonschema.validate(landing_summary, landing_schema)
        if landing_compare_summary_path is not None:
            landing_compare_summary = load_json(landing_compare_summary_path)
            jsonschema.validate(landing_compare_summary, landing_compare_schema)

        expected_summary = build_summary_model(
            landing_summary_path=landing_summary_path,
            landing_compare_summary_path=landing_compare_summary_path,
            output_root=output_root,
            summary_path=opener_summary_path,
            report_path=report_path,
            check_path=check_path,
        )

        expect_equal(summary.get("schema"), expected_summary.get("schema"), "schema", errors)
        expect_equal(summary.get("kind"), expected_summary.get("kind"), "kind", errors)
        expect_equal(summary.get("generator"), expected_summary.get("generator"), "generator", errors)
        expect_equal(summary.get("result"), expected_summary.get("result"), "result", errors)
        expect_equal(summary.get("entry_opener"), expected_summary.get("entry_opener"), "entry_opener", errors)
        expect_equal(summary.get("front_page"), expected_summary.get("front_page"), "front_page", errors)
        expect_equal(summary.get("artifact_context"), expected_summary.get("artifact_context"), "artifact_context", errors)
        expect_equal(summary.get("source_landing"), expected_summary.get("source_landing"), "source_landing", errors)
        expect_equal(
            summary.get("source_landing_compare"),
            expected_summary.get("source_landing_compare"),
            "source_landing_compare",
            errors,
        )
        expect_equal(summary.get("compare_context"), expected_summary.get("compare_context"), "compare_context", errors)
        expect_equal(summary.get("open_action"), expected_summary.get("open_action"), "open_action", errors)
        expect_equal(
            summary.get("inspector_invocation"),
            expected_summary.get("inspector_invocation"),
            "inspector_invocation",
            errors,
        )
        expect_equal(summary.get("opened_projection"), expected_summary.get("opened_projection"), "opened_projection", errors)
        expect_equal(summary.get("questions"), expected_summary.get("questions"), "questions", errors)
        expect_equal(summary.get("violations"), [], "violations", errors)
        expect_equal(
            normalize_path(summary_path),
            normalize_path(summary.get("artifact_context", {}).get("opener_summary_path", "")),
            "artifact_context.opener_summary_path",
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
    print(f"[OK] open action -> {summary.get('open_action', {}).get('status', '')}")
    print(f"[OK] inspector ready -> {summary.get('inspector_invocation', {}).get('ready', False)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
