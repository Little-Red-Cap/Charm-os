from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_entry_landing_compare_lib import (
    build_compare_summary_model,
    has_array_changes,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    landing_status = summary["landing_status"]
    landing_changes = summary["landing_changes"]
    tab_summary = summary["tab_summary"]
    regression_surface = summary["landing_regression_surface"]
    tab_changes = summary["tab_changes"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Landing Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Landing verdict: `{summary['landing_verdict']}`",
        f"- Baseline landing: `{summary['artifact_context']['baseline_landing_summary_path']}`",
        f"- Candidate landing: `{summary['artifact_context']['candidate_landing_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Landing Status",
        "- Baseline: `mode={0} tier={1} primary={2} tabs={3} provenance_roots={4}`".format(
            landing_status["baseline_recommended_entry_mode"],
            landing_status["baseline_entry_tier"],
            landing_status["baseline_primary_tab_id"] or "none",
            landing_status["baseline_tab_count"],
            landing_status["baseline_provenance_root_count"],
        ),
        "- Candidate: `mode={0} tier={1} primary={2} tabs={3} provenance_roots={4}`".format(
            landing_status["candidate_recommended_entry_mode"],
            landing_status["candidate_entry_tier"],
            landing_status["candidate_primary_tab_id"] or "none",
            landing_status["candidate_tab_count"],
            landing_status["candidate_provenance_root_count"],
        ),
        "- Baseline tabs: `{0}`".format(", ".join(landing_status["baseline_available_tab_ids"]) or "none"),
        "- Candidate tabs: `{0}`".format(", ".join(landing_status["candidate_available_tab_ids"]) or "none"),
        "",
        "## Landing Drift",
        "- Tab changes: `changed={0} added={1} removed={2} regressions={3} improvements={4} neutral={5}`".format(
            tab_summary["changed_tab_count"],
            tab_summary["added_tab_count"],
            tab_summary["removed_tab_count"],
            tab_summary["regression_count"],
            tab_summary["improvement_count"],
            tab_summary["neutral_change_count"],
        ),
        "- Structural changes: `order={0} capability_alias={1} primary={2} path={3}`".format(
            tab_summary["order_changed_count"],
            tab_summary["capability_alias_changed_count"],
            tab_summary["primary_flag_changed_count"],
            tab_summary["path_changed_count"],
        ),
    ]

    if (
        landing_changes["root_label_changed"]
        or landing_changes["root_schema_changed"]
        or landing_changes["root_kind_changed"]
    ):
        lines.extend(["", "## Root Drift"])
        if landing_changes["root_label_changed"]:
            lines.append(
                "- Root label: `{0}` -> `{1}`".format(
                    landing_status["baseline_root_label"],
                    landing_status["candidate_root_label"],
                )
            )
        if landing_changes["root_schema_changed"]:
            lines.append(
                "- Root schema: `{0}` -> `{1}`".format(
                    landing_status["baseline_root_summary_schema"],
                    landing_status["candidate_root_summary_schema"],
                )
            )
        if landing_changes["root_kind_changed"]:
            lines.append(
                "- Root kind: `{0}` -> `{1}`".format(
                    landing_status["baseline_root_summary_kind"],
                    landing_status["candidate_root_summary_kind"],
                )
            )

    lines.extend(["", "## Landing Provenance"])
    for landing in summary["landing_provenance"]:
        lines.append(
            "- `{0}` primary=`{1}` tabs=`{2}` provenance_roots=`{3}`".format(
                landing["id"],
                landing["primary_tab_id"] or "none",
                ", ".join(landing["available_tab_ids"]) or "none",
                landing["provenance_root_count"],
            )
        )

    lines.extend(["", "## Change Surface"])
    if landing_changes["recommended_mode_changed"]:
        lines.append(
            "- Recommended mode: `{0}` -> `{1}`".format(
                landing_status["baseline_recommended_entry_mode"],
                landing_status["candidate_recommended_entry_mode"],
            )
        )
    if landing_changes["entry_tier_changed"]:
        lines.append(
            "- Entry tier: `{0}` -> `{1}`".format(
                landing_status["baseline_entry_tier"],
                landing_status["candidate_entry_tier"],
            )
        )
    if landing_changes["primary_tab_changed"]:
        lines.append(
            "- Primary tab: `{0}` -> `{1}`".format(
                landing_status["baseline_primary_tab_id"] or "none",
                landing_status["candidate_primary_tab_id"] or "none",
            )
        )
    if has_array_changes(landing_changes["available_tab_changes"]):
        lines.append(
            "- Available tabs: `+[{0}] -[{1}]`".format(
                ", ".join(landing_changes["available_tab_changes"]["added"]),
                ", ".join(landing_changes["available_tab_changes"]["removed"]),
            )
        )
    if has_array_changes(landing_changes["direct_capability_changes"]):
        lines.append(
            "- Direct modes: `+[{0}] -[{1}]`".format(
                ", ".join(landing_changes["direct_capability_changes"]["added"]),
                ", ".join(landing_changes["direct_capability_changes"]["removed"]),
            )
        )
    if has_array_changes(landing_changes["provenance_root_changes"]):
        lines.append(
            "- Provenance roots: `+[{0}] -[{1}]`".format(
                ", ".join(landing_changes["provenance_root_changes"]["added"]),
                ", ".join(landing_changes["provenance_root_changes"]["removed"]),
            )
        )

    lines.extend(["", "## Regression Surface"])
    if regression_surface["changed"]:
        if regression_surface["removed_tab_ids"]:
            lines.append(
                "- Removed tabs: `{0}`".format("`, `".join(regression_surface["removed_tab_ids"]))
            )
        if regression_surface["lost_direct_modes"]:
            lines.append(
                "- Lost direct modes: `{0}`".format("`, `".join(regression_surface["lost_direct_modes"]))
            )
        if regression_surface["missing_primary_tab_id"]:
            lines.append(f"- Missing baseline primary tab: `{regression_surface['missing_primary_tab_id']}`")
        if regression_surface["downgraded_tier"]:
            lines.append("- Candidate entry tier regressed")
        for narrative in regression_surface["narratives"]:
            lines.append(f"- {narrative}")
    else:
        lines.append("- No landing regression surface detected")

    lines.extend(
        [
            "",
            "## Tab Changes",
            "Anchor | Change | Impact | Baseline tab | Candidate tab | Order | Capabilities",
            "--- | --- | --- | --- | --- | --- | ---",
        ]
    )
    if tab_changes:
        for change in tab_changes:
            lines.append(
                "{0} | {1} | {2} | {3} | {4} | {5}->{6} | {7} -> {8}".format(
                    change["anchor_id"],
                    change["change_kind"],
                    change["impact"],
                    change["baseline_tab_id"],
                    change["candidate_tab_id"],
                    change["baseline_order_index"],
                    change["candidate_order_index"],
                    ", ".join(change["baseline_capability_ids"]),
                    ", ".join(change["candidate_capability_ids"]),
                )
            )
    else:
        lines.append("none | unchanged | none | none | none | none->none | none -> none")

    lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict) -> str:
    landing_changes = summary["landing_changes"]
    tab_summary = summary["tab_summary"]
    regression_surface = summary["landing_regression_surface"]
    return "\n".join(
        [
            f"baseline_landing_summary_path: {summary['artifact_context']['baseline_landing_summary_path']}",
            f"candidate_landing_summary_path: {summary['artifact_context']['candidate_landing_summary_path']}",
            f"landing_verdict: {summary['landing_verdict']}",
            "tab_changes: changed={0} added={1} removed={2}".format(
                tab_summary["changed_tab_count"],
                tab_summary["added_tab_count"],
                tab_summary["removed_tab_count"],
            ),
            "tab_impact: regressions={0} improvements={1} neutral={2}".format(
                tab_summary["regression_count"],
                tab_summary["improvement_count"],
                tab_summary["neutral_change_count"],
            ),
            "available_tab_changes: +[{0}] -[{1}]".format(
                ", ".join(landing_changes["available_tab_changes"]["added"]),
                ", ".join(landing_changes["available_tab_changes"]["removed"]),
            ),
            "direct_capability_changes: +[{0}] -[{1}]".format(
                ", ".join(landing_changes["direct_capability_changes"]["added"]),
                ", ".join(landing_changes["direct_capability_changes"]["removed"]),
            ),
            "provenance_root_changes: +[{0}] -[{1}]".format(
                ", ".join(landing_changes["provenance_root_changes"]["added"]),
                ", ".join(landing_changes["provenance_root_changes"]["removed"]),
            ),
            "landing_regression_surface: changed={0} affected={1}".format(
                regression_surface["changed"],
                len(regression_surface["affected_tab_ids"]),
            ),
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page entry landing summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline entry landing summary path.")
    parser.add_argument("--candidate", required=True, help="Candidate entry landing summary path.")
    parser.add_argument("--output-root", default="", help="Output root for entry landing compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-landing-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-landing.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-landing.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-landing.compare.check.txt")

    try:
        summary = build_compare_summary_model(
            baseline_landing_path=baseline_path,
            candidate_landing_path=candidate_path,
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

    print(f"[FRONT-PAGE-ENTRY-LANDING-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-LANDING-COMPARE] verdict={summary['landing_verdict']}")
    print(f"[FRONT-PAGE-ENTRY-LANDING-COMPARE] changed_tabs={summary['tab_summary']['changed_tab_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
