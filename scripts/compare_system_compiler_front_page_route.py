from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from datetime import datetime
from pathlib import Path

from system_compiler_front_page_route_compare_lib import (
    build_compare_summary_model,
    has_array_changes,
    resolve_output_path,
    write_text,
)


def build_report(summary: dict) -> str:
    route_status = summary["route_status"]
    route_changes = summary["route_changes"]
    entry_summary = summary["entry_summary"]
    route_regression_surface = summary["route_regression_surface"]
    entry_changes = summary["entry_changes"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Route Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Route verdict: `{summary['route_verdict']}`",
        f"- Baseline route: `{summary['artifact_context']['baseline_route_summary_path']}`",
        f"- Candidate route: `{summary['artifact_context']['candidate_route_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Route Status",
        "- Baseline: `result={0} root_schema={1} entries={2} unique={3} repeated={4} cycles={5} max_depth={6}`".format(
            route_status["baseline_result"],
            route_status["baseline_root_summary_schema"],
            route_status["baseline_entry_count"],
            route_status["baseline_unique_summary_count"],
            route_status["baseline_repeated_entry_count"],
            route_status["baseline_cycle_entry_count"],
            route_status["baseline_max_depth"],
        ),
        "- Candidate: `result={0} root_schema={1} entries={2} unique={3} repeated={4} cycles={5} max_depth={6}`".format(
            route_status["candidate_result"],
            route_status["candidate_root_summary_schema"],
            route_status["candidate_entry_count"],
            route_status["candidate_unique_summary_count"],
            route_status["candidate_repeated_entry_count"],
            route_status["candidate_cycle_entry_count"],
            route_status["candidate_max_depth"],
        ),
        "- Level-1 baseline: `{0}`".format(", ".join(route_status["baseline_level1_surface_ids"]) or "none"),
        "- Level-1 candidate: `{0}`".format(", ".join(route_status["candidate_level1_surface_ids"]) or "none"),
        "",
        "## Route Drift",
        "- Entry changes: `changed={0} added={1} removed={2} regressions={3} improvements={4} neutral={5}`".format(
            entry_summary["changed_entry_count"],
            entry_summary["added_entry_count"],
            entry_summary["removed_entry_count"],
            entry_summary["regression_count"],
            entry_summary["improvement_count"],
            entry_summary["neutral_change_count"],
        ),
        "- Shape changes: `depth={0} revisit={1} cycle={2} expanded={3}`".format(
            entry_summary["depth_changed_count"],
            entry_summary["revisit_changed_count"],
            entry_summary["cycle_changed_count"],
            entry_summary["expanded_changed_count"],
        ),
    ]

    if route_changes["root_label_changed"] or route_changes["root_schema_changed"] or route_changes["root_kind_changed"]:
        lines.extend(["", "## Root Drift"])
        if route_changes["root_label_changed"]:
            lines.append(
                "- Root label: `{0}` -> `{1}`".format(
                    route_status["baseline_root_label"],
                    route_status["candidate_root_label"],
                )
            )
        if route_changes["root_schema_changed"]:
            lines.append(
                "- Root schema: `{0}` -> `{1}`".format(
                    route_status["baseline_root_summary_schema"],
                    route_status["candidate_root_summary_schema"],
                )
            )
        if route_changes["root_kind_changed"]:
            lines.append(
                "- Root kind: `{0}` -> `{1}`".format(
                    route_status["baseline_root_summary_kind"],
                    route_status["candidate_root_summary_kind"],
                )
            )

    lines.extend(["", "## Route Provenance"])
    for route in summary["route_provenance"]:
        lines.append(
            "- `{0}` via `{1}` -> `{2}`".format(
                route["id"],
                route["route_kind"],
                route["source_input_summary_path"],
            )
        )
        lines.append(
            "  - level-1 surfaces: `{0}`".format(", ".join(route["level1_surface_ids"]) or "none")
        )

    lines.extend(["", "## Route Change Surface"])
    if has_array_changes(route_changes["level1_surface_changes"]):
        lines.append(
            "- Level-1 surfaces: `+[{0}] -[{1}]`".format(
                ", ".join(route_changes["level1_surface_changes"]["added"]),
                ", ".join(route_changes["level1_surface_changes"]["removed"]),
            )
        )
    if route_changes["level1_order_changed"]:
        lines.append("- Level-1 surface order changed")
    if has_array_changes(route_changes["key_surface_changes"]):
        lines.append(
            "- Key surfaces: `+[{0}] -[{1}]`".format(
                ", ".join(route_changes["key_surface_changes"]["added"]),
                ", ".join(route_changes["key_surface_changes"]["removed"]),
            )
        )
    if route_changes["schema_count_changes"]:
        lines.append("- Schema count changes:")
        for schema_name, change in route_changes["schema_count_changes"].items():
            lines.append(
                "  - `{0}`: `{1} -> {2}`".format(
                    schema_name,
                    change["baseline"],
                    change["candidate"],
                )
            )
    if route_changes["role_count_changes"]:
        lines.append("- Role count changes:")
        for role_name, change in route_changes["role_count_changes"].items():
            lines.append(
                "  - `{0}`: `{1} -> {2}`".format(
                    role_name,
                    change["baseline"],
                    change["candidate"],
                )
            )
    if route_changes["route_provenance_detail_changes"]:
        lines.append("- Route provenance detail changes:")
        for change in route_changes["route_provenance_detail_changes"]:
            lines.append(
                "  - `{0}`: source `{1}` -> `{2}`".format(
                    change["provenance_id"],
                    change["baseline_source_summary_path"],
                    change["candidate_source_summary_path"],
                )
            )

    lines.extend(["", "## Regression Surface"])
    if route_regression_surface["changed"]:
        if route_regression_surface["removed_level1_surfaces"]:
            lines.append(
                "- Removed level-1 surfaces: `{0}`".format(
                    "`, `".join(route_regression_surface["removed_level1_surfaces"])
                )
            )
        if route_regression_surface["missing_key_surface_ids"]:
            lines.append(
                "- Missing key surfaces: `{0}`".format(
                    "`, `".join(route_regression_surface["missing_key_surface_ids"])
                )
            )
        if route_regression_surface["regressed_entries"]:
            lines.append(
                "- Regressed entries: `{0}`".format(
                    "`, `".join(route_regression_surface["regressed_entries"])
                )
            )
        if route_regression_surface["route_provenance_detail_changed_ids"]:
            lines.append(
                "- Route provenance detail changed: `{0}`".format(
                    "`, `".join(route_regression_surface["route_provenance_detail_changed_ids"])
                )
            )
        for narrative in route_regression_surface["narratives"]:
            lines.append(f"- {narrative}")
    else:
        lines.append("- No route regression surface detected")

    lines.extend(
        [
            "",
            "## Entry Changes",
            "Anchor | Change | Impact | Surface | Role | Depth | Expanded",
            "--- | --- | --- | --- | --- | --- | ---",
        ]
    )
    if entry_changes:
        for change in entry_changes:
            lines.append(
                "{0} | {1} | {2} | {3} | {4} | {5}->{6} | {7}->{8}".format(
                    change["anchor_id"],
                    change["change_kind"],
                    change["impact"],
                    change["surface_id"],
                    change["role"],
                    change["baseline_depth"],
                    change["candidate_depth"],
                    change["baseline_expanded"],
                    change["candidate_expanded"],
                )
            )
    else:
        lines.append("none | unchanged | none | none | none | none->none | none->none")

    lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict) -> str:
    route_changes = summary["route_changes"]
    route_regression_surface = summary["route_regression_surface"]
    entry_summary = summary["entry_summary"]
    return "\n".join(
        [
            f"baseline_route_summary_path: {summary['artifact_context']['baseline_route_summary_path']}",
            f"candidate_route_summary_path: {summary['artifact_context']['candidate_route_summary_path']}",
            f"route_verdict: {summary['route_verdict']}",
            "entry_changes: changed={0} added={1} removed={2}".format(
                entry_summary["changed_entry_count"],
                entry_summary["added_entry_count"],
                entry_summary["removed_entry_count"],
            ),
            "entry_impact: regressions={0} improvements={1} neutral={2}".format(
                entry_summary["regression_count"],
                entry_summary["improvement_count"],
                entry_summary["neutral_change_count"],
            ),
            "level1_surface_changes: +[{0}] -[{1}]".format(
                ", ".join(route_changes["level1_surface_changes"]["added"]),
                ", ".join(route_changes["level1_surface_changes"]["removed"]),
            ),
            "key_surface_changes: +[{0}] -[{1}]".format(
                ", ".join(route_changes["key_surface_changes"]["added"]),
                ", ".join(route_changes["key_surface_changes"]["removed"]),
            ),
            "route_provenance_detail_changes: [{0}]".format(
                ", ".join(
                    change["provenance_id"]
                    for change in route_changes["route_provenance_detail_changes"]
                )
            ),
            "route_regression_surface: changed={0} affected={1}".format(
                route_regression_surface["changed"],
                len(route_regression_surface["affected_surface_ids"]),
            ),
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page route summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline front-page route summary path.")
    parser.add_argument("--candidate", required=True, help="Candidate front-page route summary path.")
    parser.add_argument("--output-root", default="", help="Output root for route compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-route-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.route.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.route.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.route.compare.check.txt")

    try:
        summary = build_compare_summary_model(
            baseline_route_path=baseline_path,
            candidate_route_path=candidate_path,
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

    print(f"[FRONT-PAGE-ROUTE-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ROUTE-COMPARE] verdict={summary['route_verdict']}")
    print(f"[FRONT-PAGE-ROUTE-COMPARE] changed_entries={summary['entry_summary']['changed_entry_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

