from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from datetime import datetime
from pathlib import Path
from typing import Any

from system_compiler_front_page_route_lib import (
    choose_text,
    get_mapping,
    load_json,
    normalize_optional_path,
    normalize_path,
    resolve_output_path,
    write_text,
)


PLAN_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan/v0"
PLAN_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan"
ACTION_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0"
ACTION_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_action"
OPENER_SCHEMA = "system_compiler.front_page_entry_opener/v0"


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def string_list(value: Any) -> list[str]:
    return [choose_text(item) for item in get_list(value) if choose_text(item)]


def clone_opening_reason(reason: Any) -> OrderedDict[str, Any]:
    reason_map = get_mapping(reason)
    return OrderedDict(
        [
            ("kind", choose_text(reason_map.get("kind"))),
            ("summary", choose_text(reason_map.get("summary"))),
            ("source_summary_path", normalize_optional_path(reason_map.get("source_summary_path"))),
            ("drift_changed", bool(reason_map.get("drift_changed"))),
            ("drift_verdict", choose_text(reason_map.get("drift_verdict"))),
        ]
    )


def load_plan_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != PLAN_SCHEMA:
        raise ValueError(f"unsupported opening-flow consumer plan schema: {path}")
    if choose_text(summary.get("kind")) != PLAN_KIND:
        raise ValueError(f"unsupported opening-flow consumer plan kind: {path}")
    return summary


def make_surface(
    surface_id: str,
    label: str,
    role: str,
    summary_schema: str,
    summary_path: str,
    report_markdown_path: str,
    check_text_path: str,
) -> OrderedDict[str, str]:
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", label),
            ("role", role),
            ("summary_schema", summary_schema),
            ("summary_path", summary_path),
            ("report_markdown_path", report_markdown_path),
            ("check_text_path", check_text_path),
        ]
    )


def normalize_plan_action(action: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("action_id", choose_text(action.get("action_id"))),
            ("rank", int(action.get("rank", 0))),
            ("source_rank", int(action.get("source_rank", 0))),
            ("action_kind", choose_text(action.get("action_kind"))),
            ("entry_name", choose_text(action.get("entry_name"))),
            ("display_group", choose_text(action.get("display_group"))),
            ("selected_tab_id", choose_text(action.get("selected_tab_id"))),
            ("selected_role", choose_text(action.get("selected_role"))),
            ("query_kind", choose_text(action.get("query_kind"))),
            ("query_scope", choose_text(action.get("query_scope"))),
            ("target_summary_schema", choose_text(action.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(action.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(action.get("target_summary_path"))),
            ("projection_kind", choose_text(action.get("projection_kind"))),
            ("opening_reason", clone_opening_reason(action.get("opening_reason"))),
            ("projection_headline", choose_text(action.get("projection_headline"))),
            ("compare_context_available", bool(action.get("compare_context_available"))),
            ("landing_verdict", choose_text(action.get("landing_verdict"))),
            ("inspector_ready", bool(action.get("inspector_ready"))),
            ("inspector_mode", choose_text(action.get("inspector_mode"))),
            ("inspector_blockers", string_list(action.get("inspector_blockers"))),
            ("opener_summary_path", normalize_optional_path(action.get("opener_summary_path"))),
            ("opener_report_markdown_path", normalize_optional_path(action.get("opener_report_markdown_path"))),
            ("opener_check_text_path", normalize_optional_path(action.get("opener_check_text_path"))),
            ("expected_consumer_operation", choose_text(action.get("expected_consumer_operation"))),
            ("reason", choose_text(action.get("reason"))),
        ]
    )


def get_action_entries(plan_summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    return [normalize_plan_action(get_mapping(action)) for action in get_list(execution_plan.get("action_entries"))]


def get_default_action_id(plan_summary: dict[str, Any]) -> str:
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    return choose_text(get_mapping(execution_plan.get("default_action")).get("action_id"))


def select_action(
    plan_summary: dict[str, Any],
    requested_action_id: str,
    requested_action_kind: str,
    requested_entry_name: str,
) -> tuple[OrderedDict[str, Any], str, int]:
    actions = get_action_entries(plan_summary)
    action_id = choose_text(requested_action_id)
    action_kind = choose_text(requested_action_kind)
    entry_name = choose_text(requested_entry_name)

    if action_id:
        matches = [action for action in actions if choose_text(action.get("action_id")) == action_id]
        selector = f"action_id:{action_id}"
    elif entry_name:
        matches = [action for action in actions if choose_text(action.get("entry_name")) == entry_name]
        selector = f"entry_name:{entry_name}"
    elif action_kind:
        matches = [action for action in actions if choose_text(action.get("action_kind")) == action_kind]
        selector = f"action_kind:{action_kind}"
    else:
        default_action_id = get_default_action_id(plan_summary)
        matches = [action for action in actions if choose_text(action.get("action_id")) == default_action_id]
        selector = "default_action"

    if not matches:
        raise ValueError(f"no consumer plan action matched selector: {selector}")
    return matches[0], selector, len(matches)


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    plan_summary_path: Path,
    plan_summary: dict[str, Any],
    action: dict[str, Any],
) -> OrderedDict[str, Any]:
    plan_front_page = get_mapping(plan_summary.get("front_page"))
    plan_artifact_context = get_mapping(plan_summary.get("artifact_context"))
    surfaces = [
        make_surface(
            "source_consumer_plan",
            "source opening flow consumer plan",
            "source_consumer_plan",
            PLAN_SCHEMA,
            normalize_path(plan_summary_path),
            normalize_optional_path(plan_front_page.get("report_markdown_path"))
            or normalize_optional_path(plan_artifact_context.get("report_markdown_path")),
            normalize_optional_path(plan_front_page.get("check_text_path"))
            or normalize_optional_path(plan_artifact_context.get("check_text_path")),
        ),
        make_surface(
            "selected_opener",
            f"selected opener: {choose_text(action.get('entry_name'))}",
            "selected_opener",
            OPENER_SCHEMA,
            normalize_optional_path(action.get("opener_summary_path")),
            normalize_optional_path(action.get("opener_report_markdown_path")),
            normalize_optional_path(action.get("opener_check_text_path")),
        ),
    ]
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", surfaces),
        ]
    )


def build_source_plan(plan_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    status = get_mapping(plan_summary.get("planner_status"))
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    default_action = get_mapping(execution_plan.get("default_action"))
    compare_action = get_mapping(execution_plan.get("compare_action"))
    return OrderedDict(
        [
            ("result", choose_text(plan_summary.get("result"))),
            ("execution_plan_status", choose_text(status.get("execution_plan_status"))),
            ("planned_action_count", int(status.get("planned_action_count", 0))),
            ("default_action_id", choose_text(default_action.get("action_id"))),
            ("default_action_name", choose_text(status.get("default_action_name"))),
            ("compare_action_id", choose_text(compare_action.get("action_id"))),
            ("compare_action_name", choose_text(status.get("compare_action_name"))),
        ]
    )


def build_open_action(action: dict[str, Any]) -> OrderedDict[str, Any]:
    blockers: list[str] = []
    if not choose_text(action.get("opener_summary_path")):
        blockers.append("selected plan action is missing opener summary path")
    if not choose_text(action.get("target_summary_path")):
        blockers.append("selected plan action is missing target summary path")
    if choose_text(action.get("expected_consumer_operation")) != "open-opener-summary":
        blockers.append("selected plan action does not request open-opener-summary")

    return OrderedDict(
        [
            ("status", "blocked" if blockers else "ready"),
            ("action_id", choose_text(action.get("action_id"))),
            ("action_kind", choose_text(action.get("action_kind"))),
            ("entry_name", choose_text(action.get("entry_name"))),
            ("display_group", choose_text(action.get("display_group"))),
            ("selected_tab_id", choose_text(action.get("selected_tab_id"))),
            ("selected_role", choose_text(action.get("selected_role"))),
            ("query_kind", choose_text(action.get("query_kind"))),
            ("query_scope", choose_text(action.get("query_scope"))),
            ("target_summary_schema", choose_text(action.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(action.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(action.get("target_summary_path"))),
            ("projection_kind", choose_text(action.get("projection_kind"))),
            ("opening_reason", clone_opening_reason(action.get("opening_reason"))),
            ("projection_headline", choose_text(action.get("projection_headline"))),
            ("compare_context_available", bool(action.get("compare_context_available"))),
            ("landing_verdict", choose_text(action.get("landing_verdict"))),
            ("opener_summary_path", normalize_optional_path(action.get("opener_summary_path"))),
            ("opener_report_markdown_path", normalize_optional_path(action.get("opener_report_markdown_path"))),
            ("opener_check_text_path", normalize_optional_path(action.get("opener_check_text_path"))),
            ("expected_consumer_operation", choose_text(action.get("expected_consumer_operation"))),
            ("reason", choose_text(action.get("reason"))),
            ("blockers", blockers),
        ]
    )


def build_opening_preview(action: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("available", bool(choose_text(action.get("projection_headline")))),
            ("entry_name", choose_text(action.get("entry_name"))),
            ("opening_reason", clone_opening_reason(action.get("opening_reason"))),
            ("projection_kind", choose_text(action.get("projection_kind"))),
            ("headline", choose_text(action.get("projection_headline"))),
            ("summary_lines", []),
            ("question_lines", []),
            ("opener_summary_path", normalize_optional_path(action.get("opener_summary_path"))),
            ("opener_report_markdown_path", normalize_optional_path(action.get("opener_report_markdown_path"))),
            ("opener_check_text_path", normalize_optional_path(action.get("opener_check_text_path"))),
            ("blockers", [] if choose_text(action.get("projection_headline")) else ["selected action has no projection headline"]),
        ]
    )


def build_opener_surface(action: dict[str, Any]) -> OrderedDict[str, Any]:
    opener_summary_path = normalize_optional_path(action.get("opener_summary_path"))
    return OrderedDict(
        [
            ("available", bool(opener_summary_path)),
            ("summary_schema", OPENER_SCHEMA),
            ("summary_path", opener_summary_path),
            ("report_markdown_path", normalize_optional_path(action.get("opener_report_markdown_path"))),
            ("check_text_path", normalize_optional_path(action.get("opener_check_text_path"))),
        ]
    )


def build_execution_receipt(
    plan_summary: dict[str, Any],
    action: dict[str, Any],
    effective_selector: str,
) -> OrderedDict[str, Any]:
    status = get_mapping(plan_summary.get("planner_status"))
    return OrderedDict(
        [
            ("consumer_operation", "open-opener-summary"),
            ("selected_rank", int(action.get("rank", 0))),
            ("source_rank", int(action.get("source_rank", 0))),
            ("chosen_by", effective_selector),
            ("planned_action_count", int(status.get("planned_action_count", 0))),
            ("inspector_ready", bool(action.get("inspector_ready"))),
            ("inspector_mode", choose_text(action.get("inspector_mode"))),
            (
                "inspector_blockers",
                string_list(action.get("inspector_blockers")),
            ),
        ]
    )


def build_questions(action: dict[str, Any]) -> OrderedDict[str, list[str]]:
    action_kind = choose_text(action.get("action_kind"))
    action_questions = ["Should this plan action become the explain consumer's initial open event?"]
    if action_kind == "compare-neighbor" or bool(action.get("compare_context_available")):
        action_questions.append("Should compare context render beside this selected opener?")
    next_questions = [
        "Should later explain tools consume this action facade instead of inspecting the full plan?",
        "Should a workspace action wrapper be added once the first real consumer shell lands?",
    ]
    return OrderedDict([("action_questions", action_questions), ("next_questions", next_questions)])


def build_summary_model(
    plan_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    requested_action_id: str = "",
    requested_action_kind: str = "",
    requested_entry_name: str = "",
) -> OrderedDict[str, Any]:
    plan_summary = load_plan_summary(plan_summary_path)
    action, effective_selector, matched_action_count = select_action(
        plan_summary,
        requested_action_id=requested_action_id,
        requested_action_kind=requested_action_kind,
        requested_entry_name=requested_entry_name,
    )
    open_action = build_open_action(action)
    source_plan = build_source_plan(plan_summary)
    result = (
        "ok"
        if source_plan["result"] == "ok"
        and source_plan["execution_plan_status"] == "ready"
        and open_action["status"] == "ready"
        else "fail"
    )

    return OrderedDict(
        [
            ("schema", ACTION_SCHEMA),
            ("kind", ACTION_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"),
            ("result", result),
            (
                "opening_flow_consumer_plan_action",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Plan Action"),
                        ("summary", "A single deterministic explain-open action selected from a consumer plan witness."),
                    ]
                ),
            ),
            ("front_page", build_front_page(summary_path, report_path, check_path, plan_summary_path, plan_summary, action)),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_plan_summary_path", normalize_path(plan_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("action_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "selection_request",
                OrderedDict(
                    [
                        ("requested_action_id", choose_text(requested_action_id)),
                        ("requested_action_kind", choose_text(requested_action_kind)),
                        ("requested_entry_name", choose_text(requested_entry_name)),
                        ("effective_selector", effective_selector),
                        ("matched_action_count", matched_action_count),
                    ]
                ),
            ),
            ("source_plan", source_plan),
            ("selected_action", action),
            ("open_action", open_action),
            ("opening_preview", build_opening_preview(action)),
            ("opener_surface", build_opener_surface(action)),
            ("execution_receipt", build_execution_receipt(plan_summary, action, effective_selector)),
            ("questions", build_questions(action)),
            ("violations", [] if result == "ok" else list(open_action["blockers"])),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    selection = summary["selection_request"]
    open_action = summary["open_action"]
    opening_preview = summary["opening_preview"]
    source_plan = summary["source_plan"]
    receipt = summary["execution_receipt"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Plan Action",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source plan: `{summary['artifact_context']['source_plan_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['action_summary_path']}`",
        "",
        "## Selection",
        "- selector=`{0}` requested_id=`{1}` requested_kind=`{2}` requested_entry=`{3}` matches=`{4}`".format(
            selection["effective_selector"],
            selection["requested_action_id"] or "none",
            selection["requested_action_kind"] or "none",
            selection["requested_entry_name"] or "none",
            selection["matched_action_count"],
        ),
        "- source plan=`{0}` actions=`{1}` default=`{2}` compare=`{3}`".format(
            source_plan["execution_plan_status"],
            source_plan["planned_action_count"],
            source_plan["default_action_name"] or "none",
            source_plan["compare_action_name"] or "none",
        ),
        "",
        "## Open Action",
        "- status=`{0}` id=`{1}` kind=`{2}` entry=`{3}` group=`{4}`".format(
            open_action["status"],
            open_action["action_id"],
            open_action["action_kind"],
            open_action["entry_name"],
            open_action["display_group"],
        ),
        "- tab=`{0}` role=`{1}` query=`{2}/{3}` projection=`{4}` opening_reason=`{5}`".format(
            open_action["selected_tab_id"],
            open_action["selected_role"],
            open_action["query_kind"],
            open_action["query_scope"],
            open_action["projection_kind"],
            open_action["opening_reason"]["kind"],
        ),
        f"- target summary: `{open_action['target_summary_path']}`",
        f"- opener summary: `{open_action['opener_summary_path']}`",
        f"- reason: {open_action['reason']}",
        "",
        "## Opening Preview",
        "- available=`{0}` kind=`{1}` reason=`{2}`".format(
            opening_preview["available"],
            opening_preview["projection_kind"],
            opening_preview["opening_reason"]["kind"],
        ),
        f"- headline: {opening_preview['headline'] or 'none'}",
        "",
        "## Execution Receipt",
        "- operation=`{0}` selected_rank=`{1}` source_rank=`{2}` inspector_ready=`{3}` inspector_mode=`{4}`".format(
            receipt["consumer_operation"],
            receipt["selected_rank"],
            receipt["source_rank"],
            receipt["inspector_ready"],
            receipt["inspector_mode"] or "none",
        ),
    ]
    if open_action["blockers"]:
        lines.extend(["", "## Blockers"])
        for blocker in open_action["blockers"]:
            lines.append(f"- {blocker}")
    lines.extend(["", "## Questions"])
    for question in questions["action_questions"]:
        lines.append(f"- action: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    selection = summary["selection_request"]
    open_action = summary["open_action"]
    receipt = summary["execution_receipt"]
    return "\n".join(
        [
            f"source_plan_summary_path: {summary['artifact_context']['source_plan_summary_path']}",
            f"result: {summary['result']}",
            f"effective_selector: {selection['effective_selector']}",
            f"matched_action_count: {selection['matched_action_count']}",
            f"open_status: {open_action['status']}",
            f"action_id: {open_action['action_id']}",
            f"action_kind: {open_action['action_kind']}",
            f"entry_name: {open_action['entry_name']}",
            f"display_group: {open_action['display_group']}",
            f"query_kind: {open_action['query_kind']}",
            f"query_scope: {open_action['query_scope']}",
            f"projection_kind: {open_action['projection_kind']}",
            f"opening_reason_kind: {open_action['opening_reason']['kind']}",
            f"projection_headline: {open_action['projection_headline']}",
            f"opener_summary_path: {open_action['opener_summary_path']}",
            f"consumer_operation: {receipt['consumer_operation']}",
            f"inspector_ready: {receipt['inspector_ready']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export one deterministic opening action from a front-page entry opening-flow consumer plan summary."
    )
    parser.add_argument("--plan", required=True, help="Input opening-flow consumer plan summary JSON.")
    parser.add_argument("--action-id", default="", help="Select an action by action_id.")
    parser.add_argument(
        "--action-kind",
        default="",
        choices=["", "default", "compare-neighbor", "next"],
        help="Select the first action with this action_kind.",
    )
    parser.add_argument("--entry-name", default="", help="Select an action by entry_name.")
    parser.add_argument("--output-root", default="", help="Output root for consumer plan action artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for action summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for action markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for action check text.")
    args = parser.parse_args()

    selector_count = sum(1 for value in (args.action_id, args.action_kind, args.entry_name) if choose_text(value))
    if selector_count > 1:
        print("[ERROR] use only one of --action-id, --action-kind, or --entry-name")
        return 1

    plan_path = Path(args.plan).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan-action").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.plan-action.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.consumer.plan-action.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.plan-action.check.txt")

    try:
        summary = build_summary_model(
            plan_summary_path=plan_path,
            output_root=output_root,
            summary_path=summary_path,
            report_path=report_path,
            check_path=check_path,
            requested_action_id=args.action_id,
            requested_action_kind=args.action_kind,
            requested_entry_name=args.entry_name,
        )
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION] action={summary['open_action']['action_id']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION] open={0}/{1} entry={2}".format(
            summary["open_action"]["query_kind"],
            summary["open_action"]["query_scope"],
            summary["open_action"]["entry_name"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
