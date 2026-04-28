from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_entry_capability_lib import (
    build_summary_model,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    entry_status = summary["entry_status"]
    capability_summary = summary["capability_summary"]
    provenance_hints = summary["provenance_hints"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Capability",
        "",
        f"- Result: `{summary['result']}`",
        f"- Input route summary: `{summary['artifact_context']['input_route_summary_path']}`",
        f"- Entry capability summary JSON: `{summary['artifact_context']['capability_summary_path']}`",
        f"- Recommended mode: `{entry_status['recommended_entry_mode']}`",
        f"- Entry tier: `{entry_status['entry_tier']}`",
        f"- Available capabilities: `{', '.join(capability_summary['available_capability_ids']) or 'none'}`",
        f"- Missing capabilities: `{', '.join(capability_summary['missing_capability_ids']) or 'none'}`",
        "",
        "## Route Status",
        "- Root: `schema={0} kind={1}`".format(
            entry_status["root_summary_schema"],
            entry_status["root_summary_kind"],
        ),
        "- Level-1 surfaces: `{0}`".format(", ".join(entry_status["level1_surface_ids"]) or "none"),
        "- Level-1 roles: `{0}`".format(", ".join(entry_status["level1_roles"]) or "none"),
        "- Route stats: `entries={0} repeated={1} cycles={2} expanded={3} max_depth={4}`".format(
            entry_status["route_entry_count"],
            entry_status["repeated_entry_count"],
            entry_status["cycle_entry_count"],
            entry_status["expanded_entry_count"],
            entry_status["max_depth"],
        ),
        "- Route provenance: `entries={0} owners={1}`".format(
            entry_status["route_provenance_entry_count"],
            entry_status["route_provenance_owner_count"],
        ),
        "",
        "## Capability Counts",
    ]

    for capability_id, count in summary["capability_summary"]["capability_counts"].items():
        lines.append(f"- `{capability_id}`: `{count}`")

    lines.extend(["", "## Preferred Entries"])
    for capability_id, entry_ref in summary["capability_summary"]["preferred_entries"].items():
        if entry_ref is None:
            lines.append(f"- `{capability_id}`: none")
            continue
        lines.append(
            "- `{0}` -> route=`{1}` role=`{2}` schema=`{3}` depth=`{4}`".format(
                capability_id,
                entry_ref["route_id"],
                entry_ref["role"],
                entry_ref["summary_schema"],
                entry_ref["depth"],
            )
        )
        lines.append(f"  path: `{entry_ref['summary_path']}`")

    lines.extend(["", "## Provenance Hints"])
    if provenance_hints:
        for hint in provenance_hints:
            lines.append(
                "- owner=`{0}` provenance=`{1}` kind=`{2}`".format(
                    hint["owner_route_id"],
                    hint["provenance_id"],
                    hint["provenance_route_kind"],
                )
            )
            lines.append(f"  source_summary: `{hint['source_summary_path']}`")
            lines.append(
                "  available_supporting_surfaces: `{0}`".format(
                    "`, `".join(hint["available_supporting_surface_ids"])
                )
                if hint["available_supporting_surface_ids"]
                else "  available_supporting_surfaces: none"
            )
    else:
        lines.append("- none")

    lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict) -> str:
    entry_status = summary["entry_status"]
    capability_summary = summary["capability_summary"]
    return "\n".join(
        [
            f"input_route_summary_path: {summary['artifact_context']['input_route_summary_path']}",
            f"recommended_entry_mode: {entry_status['recommended_entry_mode']}",
            f"entry_tier: {entry_status['entry_tier']}",
            f"available_capability_count: {entry_status['available_capability_count']}",
            f"missing_capability_count: {entry_status['missing_capability_count']}",
            f"level1_surface_ids: {','.join(entry_status['level1_surface_ids'])}",
            f"route_provenance_entry_count: {entry_status['route_provenance_entry_count']}",
            f"available_capability_ids: {','.join(capability_summary['available_capability_ids'])}",
            f"missing_capability_ids: {','.join(capability_summary['missing_capability_ids'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a machine-readable entry capability map from one system compiler front_page route summary."
    )
    parser.add_argument("--summary", required=True, help="Input front-page route summary JSON.")
    parser.add_argument("--output-root", default="", help="Output directory for entry capability artifacts.")
    parser.add_argument("--capability-summary", default="", help="Explicit output path for capability summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for capability markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for capability check text.")
    args = parser.parse_args()

    input_summary_path = Path(args.summary).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-capability").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    capability_summary_path = resolve_output_path(
        args.capability_summary,
        output_root,
        "front-page.entry-capability.summary.json",
    )
    report_markdown_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-capability.report.md",
    )
    check_text_path = resolve_output_path(
        args.check_text,
        output_root,
        "front-page.entry-capability.check.txt",
    )

    try:
        summary = build_summary_model(
            route_summary_path=input_summary_path,
            output_root=output_root,
            summary_path=capability_summary_path,
            report_path=report_markdown_path,
            check_path=check_text_path,
        )
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(capability_summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_markdown_path, build_report(summary))
        write_text(check_text_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-CAPABILITY] summary={capability_summary_path}")
    print(f"[FRONT-PAGE-ENTRY-CAPABILITY] mode={summary['entry_status']['recommended_entry_mode']}")
    print(f"[FRONT-PAGE-ENTRY-CAPABILITY] tier={summary['entry_status']['entry_tier']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

