from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_entry_opening_flow_compare_lib import (
    build_compare_summary_model,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    flow_status = summary["flow_status"]
    flow_changes = summary["flow_changes"]
    case_summary = summary["opener_case_summary"]
    regression_surface = summary["flow_regression_surface"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Flow verdict: `{summary['flow_verdict']}`",
        f"- Baseline flow: `{summary['artifact_context']['baseline_flow_summary_path']}`",
        f"- Candidate flow: `{summary['artifact_context']['candidate_flow_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Flow Status",
        "- Baseline: `result={0} openers={1}/{2} projections={3} compare_context={4} inspector_ready={5}`".format(
            flow_status["baseline_result"],
            flow_status["baseline_actual_opener_count"],
            flow_status["baseline_expected_opener_count"],
            flow_status["baseline_available_projection_count"],
            flow_status["baseline_compare_context_count"],
            flow_status["baseline_inspector_ready_count"],
        ),
        "- Candidate: `result={0} openers={1}/{2} projections={3} compare_context={4} inspector_ready={5}`".format(
            flow_status["candidate_result"],
            flow_status["candidate_actual_opener_count"],
            flow_status["candidate_expected_opener_count"],
            flow_status["candidate_available_projection_count"],
            flow_status["candidate_compare_context_count"],
            flow_status["candidate_inspector_ready_count"],
        ),
        "",
        "## Flow Changes",
        "- opener_count_delta=`{0}` projection_delta=`{1}` compare_context_delta=`{2}` inspector_ready_delta=`{3}`".format(
            flow_changes["actual_opener_count_change"]["delta"],
            flow_changes["available_projection_count_change"]["delta"],
            flow_changes["compare_context_count_change"]["delta"],
            flow_changes["inspector_ready_count_change"]["delta"],
        ),
        "- step ids: `+[{0}] -[{1}]`".format(
            ", ".join(flow_changes["step_id_changes"]["added"]),
            ", ".join(flow_changes["step_id_changes"]["removed"]),
        ),
        "- opener cases: `+[{0}] -[{1}]`".format(
            ", ".join(flow_changes["opener_case_changes"]["added"]),
            ", ".join(flow_changes["opener_case_changes"]["removed"]),
        ),
        "",
        "## Opener Case Summary",
        "- cases baseline=`{0}` candidate=`{1}` changed=`{2}` added=`{3}` removed=`{4}` unchanged=`{5}`".format(
            case_summary["baseline_case_count"],
            case_summary["candidate_case_count"],
            case_summary["changed_case_count"],
            case_summary["added_case_count"],
            case_summary["removed_case_count"],
            case_summary["unchanged_case_count"],
        ),
        "- impacts regression=`{0}` improvement=`{1}` neutral=`{2}` projection_regression=`{3}` compare_context_lost=`{4}`".format(
            case_summary["regression_count"],
            case_summary["improvement_count"],
            case_summary["neutral_change_count"],
            case_summary["projection_regression_count"],
            case_summary["compare_context_lost_count"],
        ),
    ]

    if summary["opener_case_changes"]:
        lines.extend(["", "## Opener Case Changes"])
        for change in summary["opener_case_changes"]:
            lines.append(
                "- `{0}` kind=`{1}` impact=`{2}` projection=`{3}->{4}` compare_context=`{5}->{6}` inspector_ready=`{7}->{8}`".format(
                    change["name"],
                    change["change_kind"],
                    change["impact"],
                    change["baseline_projection_kind"] or "none",
                    change["candidate_projection_kind"] or "none",
                    change["baseline_compare_context_available"],
                    change["candidate_compare_context_available"],
                    change["baseline_inspector_ready"],
                    change["candidate_inspector_ready"],
                )
            )
            for note in change["change_notes"]:
                lines.append(f"  - {note}")

    lines.extend(["", "## Regression Surface"])
    if regression_surface["changed"]:
        for narrative in regression_surface["narratives"]:
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
    flow_status = summary["flow_status"]
    case_summary = summary["opener_case_summary"]
    return "\n".join(
        [
            f"baseline_flow_summary_path: {summary['artifact_context']['baseline_flow_summary_path']}",
            f"candidate_flow_summary_path: {summary['artifact_context']['candidate_flow_summary_path']}",
            f"flow_verdict: {summary['flow_verdict']}",
            f"baseline_result: {flow_status['baseline_result']}",
            f"candidate_result: {flow_status['candidate_result']}",
            f"baseline_actual_opener_count: {flow_status['baseline_actual_opener_count']}",
            f"candidate_actual_opener_count: {flow_status['candidate_actual_opener_count']}",
            f"baseline_available_projection_count: {flow_status['baseline_available_projection_count']}",
            f"candidate_available_projection_count: {flow_status['candidate_available_projection_count']}",
            f"baseline_compare_context_count: {flow_status['baseline_compare_context_count']}",
            f"candidate_compare_context_count: {flow_status['candidate_compare_context_count']}",
            f"changed_case_count: {case_summary['changed_case_count']}",
            f"added_case_count: {case_summary['added_case_count']}",
            f"removed_case_count: {case_summary['removed_case_count']}",
            f"regression_count: {case_summary['regression_count']}",
            f"improvement_count: {case_summary['improvement_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page entry opening flow summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline front-page entry opening flow summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate front-page entry opening flow summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opening flow compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.compare.check.txt")

    try:
        summary = build_compare_summary_model(
            baseline_flow_path=baseline_path,
            candidate_flow_path=candidate_path,
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

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE] verdict={summary['flow_verdict']}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE] changed_cases={summary['opener_case_summary']['changed_case_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
