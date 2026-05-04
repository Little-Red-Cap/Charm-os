from __future__ import annotations

import argparse
import sys
from pathlib import Path

from export_system_compiler_front_page_entry_opening_flow_consumer import build_summary_model
from system_compiler_front_page_route_lib import load_json, normalize_path


CONSUMER_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow_consumer.v0.schema.json"
FLOW_SCHEMA_PATH = "schemas/system_compiler.front_page_entry_opening_flow.v0.schema.json"


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


def validate_front_page(summary: dict, errors: list[str]) -> None:
    front_page = summary.get("front_page", {})
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


def validate_references(summary: dict, errors: list[str]) -> None:
    artifact_context = summary.get("artifact_context", {})
    ensure_exists(artifact_context.get("source_flow_summary_path"), "artifact_context.source_flow_summary_path", errors)
    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("consumer_summary_path"), "artifact_context.consumer_summary_path", errors)
    ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
    ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    for label in ("default_opening", "compare_opening"):
        opening = summary.get(label, {})
        if opening.get("available"):
            ensure_exists(opening.get("target_summary_path"), f"{label}.target_summary_path", errors)

    for index, entry in enumerate(summary.get("opening_handoff_entries", [])):
        if not isinstance(entry, dict):
            errors.append(f"opening_handoff_entries[{index}]: invalid entry")
            continue
        ensure_exists(entry.get("summary_path"), f"opening_handoff_entries[{index}].summary_path", errors)
        ensure_exists(entry.get("report_markdown_path"), f"opening_handoff_entries[{index}].report_markdown_path", errors)
        ensure_exists(entry.get("check_text_path"), f"opening_handoff_entries[{index}].check_text_path", errors)
        ensure_exists(entry.get("target_summary_path"), f"opening_handoff_entries[{index}].target_summary_path", errors)


def validate_counts(summary: dict, errors: list[str]) -> None:
    entries = summary.get("opening_handoff_entries", [])
    status = summary.get("consumer_status", {})
    source_flow = summary.get("source_flow", {})
    readiness = summary.get("readiness_surface", {})

    total_count = len(entries)
    renderable_count = sum(1 for entry in entries if entry.get("renderable"))
    ready_open_action_count = sum(1 for entry in entries if entry.get("open_action_status") == "ready")
    compare_aware_count = sum(1 for entry in entries if entry.get("compare_context_available"))
    inspector_ready_count = sum(1 for entry in entries if entry.get("inspector_ready"))
    blocked_inspector_count = total_count - inspector_ready_count

    expect_equal(status.get("total_opening_count"), total_count, "consumer_status.total_opening_count", errors)
    expect_equal(status.get("renderable_opening_count"), renderable_count, "consumer_status.renderable_opening_count", errors)
    expect_equal(
        status.get("ready_open_action_count"),
        ready_open_action_count,
        "consumer_status.ready_open_action_count",
        errors,
    )
    expect_equal(
        status.get("compare_aware_opening_count"),
        compare_aware_count,
        "consumer_status.compare_aware_opening_count",
        errors,
    )
    expect_equal(status.get("inspector_ready_count"), inspector_ready_count, "consumer_status.inspector_ready_count", errors)
    expect_equal(
        status.get("blocked_inspector_count"),
        blocked_inspector_count,
        "consumer_status.blocked_inspector_count",
        errors,
    )
    expect_equal(source_flow.get("actual_opener_count"), total_count, "source_flow.actual_opener_count", errors)
    expect_equal(
        source_flow.get("available_projection_count"),
        sum(1 for entry in entries if entry.get("projection_status") == "available"),
        "source_flow.available_projection_count",
        errors,
    )
    expect_equal(
        readiness.get("renderable_openings"),
        [entry.get("name") for entry in entries if entry.get("renderable")],
        "readiness_surface.renderable_openings",
        errors,
    )
    expect_equal(
        readiness.get("blocked_openings"),
        [entry.get("name") for entry in entries if not entry.get("renderable")],
        "readiness_surface.blocked_openings",
        errors,
    )
    expect_equal(
        readiness.get("preview_ready_openings"),
        [entry.get("name") for entry in entries if entry.get("projection_headline") or entry.get("projection_summary_lines")],
        "readiness_surface.preview_ready_openings",
        errors,
    )
    expect_equal(
        readiness.get("drift_reason_openings"),
        [entry.get("name") for entry in entries if bool(entry.get("opening_reason", {}).get("drift_changed"))],
        "readiness_surface.drift_reason_openings",
        errors,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler front_page entry opening flow consumer summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help=(
            "Path to entry opening flow consumer summary JSON. If omitted, "
            "--bundle-root/front-page.entry-opening-flow.consumer.summary.json is used."
        ),
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing front-page.entry-opening-flow.consumer.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-front-page-entry-opening-flow-consumer").resolve()
        summary_path = bundle_root / "front-page.entry-opening-flow.consumer.summary.json"

    consumer_schema_path = (repo_root / CONSUMER_SCHEMA_PATH).resolve()
    flow_schema_path = (repo_root / FLOW_SCHEMA_PATH).resolve()

    try:
        import jsonschema

        summary = load_json(summary_path)
        consumer_schema = load_json(consumer_schema_path)
        flow_schema = load_json(flow_schema_path)
        jsonschema.validate(summary, consumer_schema)

        errors: list[str] = []
        validate_front_page(summary, errors)
        validate_references(summary, errors)
        validate_counts(summary, errors)

        artifact_context = summary.get("artifact_context", {})
        flow_summary_path = Path(artifact_context.get("source_flow_summary_path", "")).resolve()
        flow_summary = load_json(flow_summary_path)
        jsonschema.validate(flow_summary, flow_schema)

        expected_summary = build_summary_model(
            flow_summary_path=flow_summary_path,
            output_root=Path(artifact_context.get("output_root", "")).resolve(),
            summary_path=Path(artifact_context.get("consumer_summary_path", "")).resolve(),
            report_path=Path(artifact_context.get("report_markdown_path", "")).resolve(),
            check_path=Path(artifact_context.get("check_text_path", "")).resolve(),
        )

        for field in (
            "schema",
            "kind",
            "generator",
            "result",
            "opening_flow_consumer",
            "front_page",
            "artifact_context",
            "source_flow",
            "consumer_status",
            "default_opening",
            "compare_opening",
            "readiness_surface",
            "opening_handoff_entries",
            "questions",
            "violations",
        ):
            expect_equal(summary.get(field), expected_summary.get(field), field, errors)

        expect_equal(
            normalize_path(summary_path),
            normalize_path(artifact_context.get("consumer_summary_path", "")),
            "artifact_context.consumer_summary_path",
            errors,
        )
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    status = summary.get("consumer_status", {})
    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] default opening -> {status.get('default_opening_name', '')}")
    print(f"[OK] renderable -> {status.get('renderable_opening_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
