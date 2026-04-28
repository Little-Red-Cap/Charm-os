from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_route_lib import (
    build_route_model,
    normalize_path,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    root_surface = summary["root_surface"]
    route_summary = summary["route_summary"]
    schema_counts = summary["schema_counts"]
    role_counts = summary["role_counts"]
    route_entries = summary["route_entries"]

    lines: list[str] = [
        "# System Compiler Front Page Route",
        "",
        f"- Result: `{summary['result']}`",
        f"- Input summary: `{summary['artifact_context']['input_summary_path']}`",
        f"- Route summary JSON: `{summary['artifact_context']['route_summary_path']}`",
        f"- Entry count: `{route_summary['entry_count']}`",
        f"- Unique summaries: `{route_summary['unique_summary_count']}`",
        f"- Repeated entries: `{route_summary['repeated_entry_count']}`",
        f"- Cycle entries: `{route_summary['cycle_entry_count']}`",
        f"- Max depth: `{route_summary['max_depth']}`",
        "",
        "## Route Tree",
    ]

    for entry in route_entries:
        indent = "  " * int(entry["depth"])
        flags: list[str] = []
        if entry["expanded"]:
            flags.append("expanded")
        else:
            flags.append("leaf")
        if entry["revisit"]:
            flags.append(f"revisit->{entry['first_route_id']}")
        if entry["cycle"]:
            flags.append("cycle")

        lines.append(
            "{0}- `{1}` {2} role=`{3}` schema=`{4}` flags=`{5}`".format(
                indent,
                entry["route_id"],
                entry["label"],
                entry["role"],
                entry["summary_schema"],
                ",".join(flags),
            )
        )
        lines.append(f"{indent}  path: `{entry['summary_path']}`")

    lines.extend(["", "## Schema Counts"])
    for schema_name, count in schema_counts.items():
        lines.append(f"- `{schema_name}`: `{count}`")

    lines.extend(["", "## Role Counts"])
    for role_name, count in role_counts.items():
        lines.append(f"- `{role_name}`: `{count}`")

    return "\n".join(lines) + "\n"


def build_check(summary: dict) -> str:
    root_surface = summary["root_surface"]
    route_summary = summary["route_summary"]
    route_entries = summary["route_entries"]
    level1_surface_ids = ",".join(entry["surface_id"] for entry in route_entries if int(entry["depth"]) == 1)
    return "\n".join(
        [
            f"input_summary_path: {summary['artifact_context']['input_summary_path']}",
            f"root_summary_path: {root_surface['summary_path']}",
            f"entry_count: {route_summary['entry_count']}",
            f"unique_summary_count: {route_summary['unique_summary_count']}",
            f"repeated_entry_count: {route_summary['repeated_entry_count']}",
            f"cycle_entry_count: {route_summary['cycle_entry_count']}",
            f"leaf_entry_count: {route_summary['leaf_entry_count']}",
            f"expanded_entry_count: {route_summary['expanded_entry_count']}",
            f"max_depth: {route_summary['max_depth']}",
            f"level1_surface_ids: {level1_surface_ids}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a machine-readable route trace by walking system compiler front_page surfaces."
    )
    parser.add_argument("--summary", required=True, help="Root summary JSON to consume.")
    parser.add_argument("--output-root", default="", help="Output directory for route artifacts.")
    parser.add_argument("--route-summary", default="", help="Explicit output path for route summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for route markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for route check text.")
    args = parser.parse_args()

    input_summary_path = Path(args.summary).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-route").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    route_summary_path = resolve_output_path(args.route_summary, output_root, "front-page.route.summary.json")
    report_markdown_path = resolve_output_path(args.report_markdown, output_root, "front-page.route.report.md")
    check_text_path = resolve_output_path(args.check_text, output_root, "front-page.route.check.txt")

    route_summary, schema_counts, role_counts, root_surface, route_entries = build_route_model(input_summary_path)
    summary = OrderedDict(
        [
            ("schema", "system_compiler.front_page_route/v0"),
            ("kind", "system_compiler.front_page_route"),
            ("generated_at_utc", datetime.utcnow().replace(microsecond=0).isoformat() + "Z"),
            ("generator", "scripts/export_system_compiler_front_page_route.py"),
            ("result", "ok"),
            (
                "route",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Route"),
                        (
                            "summary",
                            "A consumer-side traversal that starts from one root summary and follows machine front_page surfaces without peeking behind the artifact boundary.",
                        ),
                    ]
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("input_summary_path", normalize_path(input_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("route_summary_path", normalize_path(route_summary_path)),
                        ("report_markdown_path", normalize_path(report_markdown_path)),
                        ("check_text_path", normalize_path(check_text_path)),
                    ]
                ),
            ),
            ("root_surface", root_surface),
            ("route_summary", route_summary),
            ("schema_counts", schema_counts),
            ("role_counts", role_counts),
            ("route_entries", route_entries),
            ("violations", []),
        ]
    )

    write_text(route_summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    write_text(report_markdown_path, build_report(summary))
    write_text(check_text_path, build_check(summary))

    print(f"[FRONT-PAGE-ROUTE] summary={route_summary_path}")
    print(f"[FRONT-PAGE-ROUTE] entries={route_summary['entry_count']}")
    print(f"[FRONT-PAGE-ROUTE] max_depth={route_summary['max_depth']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
