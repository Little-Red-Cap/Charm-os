from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_entry_landing_lib import (
    build_summary_model,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    landing_status = summary["landing_status"]
    primary_landing = summary["primary_landing"]
    landing_tabs = summary["landing_tabs"]
    provenance_roots = summary["provenance_roots"]
    query_hints = summary["query_hints"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Landing",
        "",
        f"- Result: `{summary['result']}`",
        f"- Input capability summary: `{summary['artifact_context']['input_capability_summary_path']}`",
        f"- Landing summary JSON: `{summary['artifact_context']['landing_summary_path']}`",
        f"- Recommended mode: `{landing_status['recommended_entry_mode']}`",
        f"- Entry tier: `{landing_status['entry_tier']}`",
        f"- Primary tab: `{landing_status['primary_tab_id'] or 'none'}`",
        f"- Tabs: `{', '.join(landing_status['available_tab_ids']) or 'none'}`",
        f"- Provenance roots: `{landing_status['provenance_root_count']}`",
        "",
        "## Primary Landing",
    ]

    if isinstance(primary_landing, dict) and primary_landing:
        entry = primary_landing["entry"]
        lines.append(
            "- `{0}` role=`{1}` schema=`{2}` depth=`{3}`".format(
                primary_landing["tab_id"],
                entry["role"],
                entry["summary_schema"],
                entry["depth"],
            )
        )
        lines.append(f"  path: `{entry['summary_path']}`")
    else:
        lines.append("- none")

    lines.extend(["", "## Explain Query Hints"])
    primary_query = query_hints["primary_query"]
    if isinstance(primary_query, dict) and primary_query:
        lines.append(
            "- primary `{0}` -> query=`{1}` scope=`{2}` selection=`{3}` compare_expected=`{4}`".format(
                primary_query["tab_id"],
                primary_query["query_kind"],
                primary_query["scope"],
                primary_query["selection_rule"],
                "yes" if primary_query["compare_expected"] else "no",
            )
        )
        lines.append(
            "  follow-up: `{0}`".format(
                "`, `".join(primary_query["followup_query_kinds"])
            )
        )
    else:
        lines.append("- none")

    lines.extend(["", "## Landing Tabs"])
    for tab in landing_tabs:
        entry = tab["entry"]
        lines.append(
            "- `{0}` capabilities=`{1}` role=`{2}` schema=`{3}` depth=`{4}`".format(
                tab["tab_id"],
                "`, `".join(tab["capability_ids"]),
                entry["role"],
                entry["summary_schema"],
                entry["depth"],
            )
        )
        lines.append(f"  path: `{entry['summary_path']}`")

    lines.extend(["", "## Tab Query Plan"])
    for query_hint in query_hints["tab_queries"]:
        lines.append(
            "- `{0}` query=`{1}` scope=`{2}` selection=`{3}` compare_expected=`{4}`".format(
                query_hint["tab_id"],
                query_hint["query_kind"],
                query_hint["scope"],
                query_hint["selection_rule"],
                "yes" if query_hint["compare_expected"] else "no",
            )
        )
        lines.append(
            "  follow-up: `{0}`".format(
                "`, `".join(query_hint["followup_query_kinds"])
            )
        )
        lines.append(f"  rationale: {query_hint['rationale']}")

    lines.extend(["", "## Provenance Roots"])
    if provenance_roots:
        for root in provenance_roots:
            lines.append(
                "- `{0}` schema=`{1}` owners=`{2}`".format(
                    root["root_id"],
                    root["source_summary_schema"],
                    "`, `".join(root["owner_route_ids"]),
                )
            )
            lines.append(f"  path: `{root['source_summary_path']}`")
            if root["available_supporting_surface_ids"]:
                lines.append(
                    "  supporting surfaces: `{0}`".format(
                        "`, `".join(root["available_supporting_surface_ids"])
                    )
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
    landing_status = summary["landing_status"]
    primary_query = summary["query_hints"]["primary_query"]
    return "\n".join(
        [
            f"input_capability_summary_path: {summary['artifact_context']['input_capability_summary_path']}",
            f"recommended_entry_mode: {landing_status['recommended_entry_mode']}",
            f"entry_tier: {landing_status['entry_tier']}",
            f"primary_tab_id: {landing_status['primary_tab_id'] or ''}",
            f"available_tab_ids: {','.join(landing_status['available_tab_ids'])}",
            f"fallback_tab_ids: {','.join(landing_status['fallback_tab_ids'])}",
            f"provenance_root_count: {landing_status['provenance_root_count']}",
            f"route_provenance_entry_count: {landing_status['route_provenance_entry_count']}",
            f"primary_query_kind: {primary_query['query_kind'] if isinstance(primary_query, dict) else ''}",
            f"primary_query_scope: {primary_query['scope'] if isinstance(primary_query, dict) else ''}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a concrete landing/open plan from one system compiler front_page entry capability summary."
    )
    parser.add_argument("--summary", required=True, help="Input front-page entry capability summary JSON.")
    parser.add_argument("--output-root", default="", help="Output directory for entry landing artifacts.")
    parser.add_argument("--landing-summary", default="", help="Explicit output path for landing summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for landing markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for landing check text.")
    args = parser.parse_args()

    input_summary_path = Path(args.summary).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-landing").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    landing_summary_path = resolve_output_path(
        args.landing_summary,
        output_root,
        "front-page.entry-landing.summary.json",
    )
    report_markdown_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-landing.report.md",
    )
    check_text_path = resolve_output_path(
        args.check_text,
        output_root,
        "front-page.entry-landing.check.txt",
    )

    try:
        summary = build_summary_model(
            capability_summary_path=input_summary_path,
            output_root=output_root,
            summary_path=landing_summary_path,
            report_path=report_markdown_path,
            check_path=check_text_path,
        )
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(landing_summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_markdown_path, build_report(summary))
        write_text(check_text_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-LANDING] summary={landing_summary_path}")
    print(f"[FRONT-PAGE-ENTRY-LANDING] mode={summary['landing_status']['recommended_entry_mode']}")
    print(f"[FRONT-PAGE-ENTRY-LANDING] primary={summary['landing_status']['primary_tab_id']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
