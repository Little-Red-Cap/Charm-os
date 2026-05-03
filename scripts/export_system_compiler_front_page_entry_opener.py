from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_entry_opener_lib import (
    build_summary_model,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    source_landing = summary["source_landing"]
    compare_context = summary["compare_context"]
    open_action = summary["open_action"]
    inspector_invocation = summary["inspector_invocation"]
    opened_projection = summary["opened_projection"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opener",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source landing: `{summary['artifact_context']['source_landing_summary_path']}`",
        f"- Landing compare: `{summary['artifact_context']['source_landing_compare_summary_path'] or 'none'}`",
        f"- Summary JSON: `{summary['artifact_context']['opener_summary_path']}`",
        f"- Root: `{source_landing['root_label'] or 'unknown'}`",
        "",
        "## Open Action",
        "- status=`{0}` tab=`{1}` query=`{2}` scope=`{3}` selection=`{4}` compare_expected=`{5}`".format(
            open_action["status"],
            open_action["selected_tab_id"] or "none",
            open_action["query_kind"] or "none",
            open_action["query_scope"] or "none",
            open_action["selection_rule"] or "none",
            "yes" if open_action["compare_expected"] else "no",
        ),
        "- target schema=`{0}` kind=`{1}`".format(
            open_action["target_summary_schema"] or "none",
            open_action["target_summary_kind"] or "none",
        ),
        f"- target summary: `{open_action['target_summary_path'] or 'none'}`",
        f"- target report: `{open_action['target_report_markdown_path'] or 'none'}`",
        f"- target check: `{open_action['target_check_text_path'] or 'none'}`",
    ]

    if open_action["followup_query_kinds"]:
        lines.append("- follow-up queries: `{0}`".format("`, `".join(open_action["followup_query_kinds"])))
    if open_action["rationale"]:
        lines.append(f"- rationale: {open_action['rationale']}")
    if open_action["blockers"]:
        lines.append("- open blockers: `{0}`".format("`, `".join(open_action["blockers"])))

    lines.extend(["", "## Inspector Invocation"])
    lines.append(
        "- ready=`{0}` mode=`{1}` query=`{2}` format=`{3}`".format(
            "yes" if inspector_invocation["ready"] else "no",
            inspector_invocation["mode"],
            inspector_invocation["query_kind"] or "none",
            inspector_invocation["output_format"],
        )
    )
    lines.append(f"- script: `{inspector_invocation['script_path']}`")
    if inspector_invocation["arguments"]:
        lines.append("- arguments: `{0}`".format("` `".join(inspector_invocation["arguments"])))
    if inspector_invocation["powershell_command"]:
        lines.append("- command: `{0}`".format("` `".join(inspector_invocation["powershell_command"])))
    if inspector_invocation["blockers"]:
        lines.append("- blockers: `{0}`".format("`, `".join(inspector_invocation["blockers"])))

    lines.extend(["", "## Opened Projection"])
    lines.append(
        "- status=`{0}` kind=`{1}` source_schema=`{2}` source_kind=`{3}`".format(
            opened_projection["status"],
            opened_projection["projection_kind"],
            opened_projection["source_summary_schema"] or "none",
            opened_projection["source_summary_kind"] or "none",
        )
    )
    lines.append(f"- source summary: `{opened_projection['source_summary_path'] or 'none'}`")
    lines.append(f"- headline: {opened_projection['headline'] or 'none'}")
    if opened_projection["summary_lines"]:
        for item in opened_projection["summary_lines"]:
            lines.append(f"- summary: {item}")
    if opened_projection["question_lines"]:
        for item in opened_projection["question_lines"]:
            lines.append(f"- question: {item}")
    if opened_projection["supporting_summary_paths"]:
        lines.append(
            "- supporting summaries: `{0}`".format("`, `".join(opened_projection["supporting_summary_paths"]))
        )
    if opened_projection["evidence_paths"]:
        lines.append("- evidence paths: `{0}`".format("`, `".join(opened_projection["evidence_paths"])))
    if opened_projection["compare_paths"]:
        lines.append("- compare paths: `{0}`".format("`, `".join(opened_projection["compare_paths"])))
    if opened_projection["blockers"]:
        lines.append("- projection blockers: `{0}`".format("`, `".join(opened_projection["blockers"])))

    lines.extend(["", "## Compare Context"])
    if compare_context["available"]:
        lines.append(
            "- verdict=`{0}` relation=`{1}` primary_query_changed=`{2}` landing_regression=`{3}` query_regression=`{4}`".format(
                compare_context["landing_verdict"],
                compare_context["related_landing_role"],
                "yes" if compare_context["primary_query_changed"] else "no",
                "yes" if compare_context["landing_regression_changed"] else "no",
                "yes" if compare_context["query_regression_changed"] else "no",
            )
        )
        for narrative in compare_context["narratives"]:
            lines.append(f"- {narrative}")
    else:
        lines.append("- none")

    lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict) -> str:
    open_action = summary["open_action"]
    inspector_invocation = summary["inspector_invocation"]
    compare_context = summary["compare_context"]
    opened_projection = summary["opened_projection"]
    return "\n".join(
        [
            f"source_landing_summary_path: {summary['artifact_context']['source_landing_summary_path']}",
            f"source_landing_compare_summary_path: {summary['artifact_context']['source_landing_compare_summary_path']}",
            f"open_status: {open_action['status']}",
            f"selected_tab_id: {open_action['selected_tab_id']}",
            f"query_kind: {open_action['query_kind']}",
            f"query_scope: {open_action['query_scope']}",
            f"selection_rule: {open_action['selection_rule']}",
            f"compare_expected: {open_action['compare_expected']}",
            f"target_summary_schema: {open_action['target_summary_schema']}",
            f"target_summary_path: {open_action['target_summary_path']}",
            f"inspector_ready: {inspector_invocation['ready']}",
            f"inspector_arguments: {' '.join(inspector_invocation['arguments'])}",
            f"inspector_blockers: {' | '.join(inspector_invocation['blockers'])}",
            f"opened_projection_status: {opened_projection['status']}",
            f"opened_projection_kind: {opened_projection['projection_kind']}",
            f"opened_projection_source_summary_path: {opened_projection['source_summary_path']}",
            f"compare_context_available: {compare_context['available']}",
            f"landing_verdict: {compare_context['landing_verdict']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export one deterministic explain opening action from a front-page entry landing summary."
    )
    parser.add_argument("--landing", required=True, help="Source front-page entry landing summary JSON.")
    parser.add_argument(
        "--landing-compare",
        default="",
        help="Optional front-page entry landing compare summary JSON that references --landing.",
    )
    parser.add_argument("--output-root", default="", help="Output root for entry opener artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opener summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opener markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opener check text.")
    args = parser.parse_args()

    landing_path = Path(args.landing).resolve()
    landing_compare_path = Path(args.landing_compare).resolve() if args.landing_compare else None
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opener").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opener.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opener.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opener.check.txt")

    try:
        summary = build_summary_model(
            landing_summary_path=landing_path,
            landing_compare_summary_path=landing_compare_path,
            output_root=output_root,
            summary_path=summary_path,
            report_path=report_path,
            check_path=check_path,
        )
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENER] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENER] action={summary['open_action']['status']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENER] open={0}/{1} inspector_ready={2}".format(
            summary["open_action"]["query_kind"],
            summary["open_action"]["query_scope"],
            summary["inspector_invocation"]["ready"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
