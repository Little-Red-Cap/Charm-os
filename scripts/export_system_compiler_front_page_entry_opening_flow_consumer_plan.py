from __future__ import annotations

import argparse
import json
from collections import Counter, OrderedDict
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


SELECTOR_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_selector/v0"
SELECTOR_KIND = "system_compiler.front_page_entry_opening_flow_consumer_selector"
PLAN_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan/v0"
PLAN_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan"
MAX_NEXT_ACTIONS = 3


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def string_list(value: Any) -> list[str]:
    return [choose_text(item) for item in get_list(value) if choose_text(item)]


def load_selector_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != SELECTOR_SCHEMA:
        raise ValueError(f"unsupported opening flow consumer selector schema: {path}")
    if choose_text(summary.get("kind")) != SELECTOR_KIND:
        raise ValueError(f"unsupported opening flow consumer selector kind: {path}")
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


def build_action(
    entry: dict[str, Any],
    action_id: str,
    rank: int,
    action_kind: str,
    display_group: str,
    reason: str,
) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("action_id", action_id),
            ("rank", rank),
            ("source_rank", int(entry.get("rank", 0))),
            ("action_kind", action_kind),
            ("entry_name", choose_text(entry.get("name"))),
            ("display_group", display_group),
            ("selected_tab_id", choose_text(entry.get("selected_tab_id"))),
            ("selected_role", choose_text(entry.get("selected_role"))),
            ("query_kind", choose_text(entry.get("query_kind"))),
            ("query_scope", choose_text(entry.get("query_scope"))),
            ("target_summary_schema", choose_text(entry.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(entry.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(entry.get("target_summary_path"))),
            ("projection_kind", choose_text(entry.get("projection_kind"))),
            ("opening_reason", get_mapping(entry.get("opening_reason"))),
            ("projection_headline", choose_text(entry.get("projection_headline"))),
            ("projection_summary_lines", string_list(entry.get("projection_summary_lines"))),
            ("projection_question_lines", string_list(entry.get("projection_question_lines"))),
            ("compare_context_available", bool(entry.get("compare_context_available"))),
            ("landing_verdict", choose_text(entry.get("landing_verdict"))),
            ("inspector_ready", bool(entry.get("inspector_ready"))),
            ("inspector_mode", choose_text(entry.get("inspector_mode"))),
            (
                "inspector_blockers",
                [choose_text(item) for item in get_list(entry.get("inspector_blockers")) if choose_text(item)],
            ),
            ("opener_summary_path", normalize_optional_path(entry.get("opener_summary_path"))),
            ("opener_report_markdown_path", normalize_optional_path(entry.get("opener_report_markdown_path"))),
            ("opener_check_text_path", normalize_optional_path(entry.get("opener_check_text_path"))),
            ("expected_consumer_operation", "open-opener-summary"),
            ("reason", reason),
        ]
    )


def build_execution_plan(selector_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    open_plan = get_mapping(selector_summary.get("open_plan"))
    default_entry = get_mapping(open_plan.get("default_entry"))
    compare_entry = get_mapping(open_plan.get("compare_entry"))
    ordered_entries = [get_mapping(entry) for entry in get_list(open_plan.get("ordered_entries"))]

    actions: list[OrderedDict[str, Any]] = []
    selected_names: set[str] = set()

    if default_entry:
        actions.append(
            build_action(
                default_entry,
                "open-default",
                len(actions),
                "default",
                "primary",
                "open consumer-selected default explain entry",
            )
        )
        selected_names.add(actions[-1]["entry_name"])

    compare_name = choose_text(compare_entry.get("name"))
    if compare_entry and compare_name and compare_name not in selected_names:
        actions.append(
            build_action(
                compare_entry,
                "open-compare-neighbor",
                len(actions),
                "compare-neighbor",
                "compare",
                "open compare-aware neighbor selected by the consumer selector",
            )
        )
        selected_names.add(actions[-1]["entry_name"])

    next_entries: list[dict[str, Any]] = []
    for entry in ordered_entries:
        name = choose_text(entry.get("name"))
        if not name or name in selected_names:
            continue
        next_entries.append(entry)
        selected_names.add(name)
        if len(next_entries) >= MAX_NEXT_ACTIONS:
            break

    for index, entry in enumerate(next_entries, start=1):
        actions.append(
            build_action(
                entry,
                f"open-next-{index}",
                len(actions),
                "next",
                "fallback",
                "open fallback explain entry from selector order",
            )
        )

    return OrderedDict(
        [
            ("status", "ready" if actions else "blocked"),
            ("default_action", actions[0] if actions else OrderedDict()),
            (
                "compare_action",
                next((action for action in actions if action["action_kind"] == "compare-neighbor"), OrderedDict()),
            ),
            ("next_actions", [action for action in actions if action["action_kind"] == "next"]),
            ("action_entries", actions),
            (
                "planning_notes",
                [
                    "default action follows consumer selector default entry",
                    "compare action follows consumer selector compare entry when distinct",
                    f"next actions are capped at {MAX_NEXT_ACTIONS} fallback entries",
                ],
            ),
        ]
    )


def build_planning_surface(selector_summary: dict[str, Any], execution_plan: dict[str, Any]) -> OrderedDict[str, Any]:
    ordered_entries = [get_mapping(entry) for entry in get_list(get_mapping(selector_summary.get("open_plan")).get("ordered_entries"))]
    actions = [get_mapping(action) for action in get_list(execution_plan.get("action_entries"))]
    planned_names = [choose_text(action.get("entry_name")) for action in actions if choose_text(action.get("entry_name"))]
    planned_name_set = set(planned_names)
    omitted_names = [
        choose_text(entry.get("name"))
        for entry in ordered_entries
        if choose_text(entry.get("name")) and choose_text(entry.get("name")) not in planned_name_set
    ]
    projection_kinds = Counter(choose_text(action.get("projection_kind")) for action in actions if choose_text(action.get("projection_kind")))
    target_schemas = Counter(
        choose_text(action.get("target_summary_schema")) for action in actions if choose_text(action.get("target_summary_schema"))
    )
    default_action = get_mapping(execution_plan.get("default_action"))
    compare_action = get_mapping(execution_plan.get("compare_action"))

    return OrderedDict(
        [
            ("planned_action_ids", [choose_text(action.get("action_id")) for action in actions]),
            ("planned_entry_names", planned_names),
            ("omitted_entry_names", omitted_names),
            ("projection_kinds", OrderedDict(sorted(projection_kinds.items()))),
            ("target_summary_schemas", OrderedDict(sorted(target_schemas.items()))),
            ("compare_context_action_count", sum(1 for action in actions if bool(action.get("compare_context_available")))),
            ("inspector_ready_action_count", sum(1 for action in actions if bool(action.get("inspector_ready")))),
            ("default_action_id", choose_text(default_action.get("action_id"))),
            ("compare_action_id", choose_text(compare_action.get("action_id"))),
            ("max_next_action_count", MAX_NEXT_ACTIONS),
        ]
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    selector_summary_path: Path,
    selector_summary: dict[str, Any],
    execution_plan: dict[str, Any],
) -> OrderedDict[str, Any]:
    selector_front_page = get_mapping(selector_summary.get("front_page"))
    surfaces: list[OrderedDict[str, str]] = [
        make_surface(
            "source_consumer_selector",
            "source opening flow consumer selector",
            "source_consumer_selector",
            SELECTOR_SCHEMA,
            normalize_path(selector_summary_path),
            normalize_optional_path(selector_front_page.get("report_markdown_path")),
            normalize_optional_path(selector_front_page.get("check_text_path")),
        )
    ]

    for action in get_list(execution_plan.get("action_entries")):
        action_map = get_mapping(action)
        action_id = choose_text(action_map.get("action_id"))
        surfaces.append(
            make_surface(
                f"{action_id}_opener",
                f"{action_id}: {choose_text(action_map.get('entry_name'))}",
                "planned_opener",
                "system_compiler.front_page_entry_opener/v0",
                normalize_optional_path(action_map.get("opener_summary_path")),
                normalize_optional_path(action_map.get("opener_report_markdown_path")),
                normalize_optional_path(action_map.get("opener_check_text_path")),
            )
        )

    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", surfaces),
        ]
    )


def build_summary_model(
    selector_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    selector_summary = load_selector_summary(selector_summary_path)
    execution_plan = build_execution_plan(selector_summary)
    planning_surface = build_planning_surface(selector_summary, execution_plan)
    selector_status = get_mapping(selector_summary.get("selector_status"))
    action_entries = get_list(execution_plan.get("action_entries"))
    next_actions = get_list(execution_plan.get("next_actions"))
    result = (
        "ok"
        if choose_text(selector_summary.get("result")) == "ok" and choose_text(execution_plan.get("status")) == "ready"
        else "fail"
    )

    return OrderedDict(
        [
            ("schema", PLAN_SCHEMA),
            ("kind", PLAN_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_consumer_plan.py"),
            ("result", result),
            (
                "opening_flow_consumer_plan",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Plan"),
                        ("summary", "A deterministic execution plan for opening selected consumer explain entries."),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(summary_path, report_path, check_path, selector_summary_path, selector_summary, execution_plan),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_selector_summary_path", normalize_path(selector_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("consumer_plan_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "source_selector",
                OrderedDict(
                    [
                        ("result", choose_text(selector_summary.get("result"))),
                        ("open_plan_status", choose_text(selector_status.get("open_plan_status"))),
                        ("selected_entry_count", int(selector_status.get("selected_entry_count", 0))),
                        ("default_entry_name", choose_text(selector_status.get("default_entry_name"))),
                        ("compare_entry_name", choose_text(selector_status.get("compare_entry_name"))),
                        ("fallback_entry_count", int(selector_status.get("fallback_entry_count", 0))),
                    ]
                ),
            ),
            (
                "planner_status",
                OrderedDict(
                    [
                        ("result", result),
                        ("execution_plan_status", choose_text(execution_plan.get("status"))),
                        ("planned_action_count", len(action_entries)),
                        ("default_action_name", choose_text(get_mapping(execution_plan.get("default_action")).get("entry_name"))),
                        ("compare_action_name", choose_text(get_mapping(execution_plan.get("compare_action")).get("entry_name"))),
                        ("next_action_count", len(next_actions)),
                        ("omitted_entry_count", len(get_list(planning_surface.get("omitted_entry_names")))),
                    ]
                ),
            ),
            ("execution_plan", execution_plan),
            ("planning_surface", planning_surface),
            (
                "questions",
                OrderedDict(
                    [
                        (
                            "planner_questions",
                            [
                                "Should the default action execute automatically when an explain consumer opens?",
                                "Should the compare action render beside the default action or stay behind an affordance?",
                            ],
                        ),
                        (
                            "next_questions",
                            [
                                "Which planned opener should become the first rich explain adapter?",
                                "Should omitted fallback entries be lazy-loaded from the selector artifact?",
                            ],
                        ),
                    ]
                ),
            ),
            ("violations", [] if result == "ok" else ["no ready consumer plan action was produced"]),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["planner_status"]
    execution_plan = summary["execution_plan"]
    planning_surface = summary["planning_surface"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Plan",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source selector: `{summary['artifact_context']['source_selector_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['consumer_plan_summary_path']}`",
        "",
        "## Planner Status",
        "- plan=`{0}` actions=`{1}` next=`{2}` omitted=`{3}`".format(
            status["execution_plan_status"],
            status["planned_action_count"],
            status["next_action_count"],
            status["omitted_entry_count"],
        ),
        f"- default action: `{status['default_action_name']}`",
        f"- compare action: `{status['compare_action_name']}`",
        "",
        "## Execution Plan",
    ]

    for action in get_list(execution_plan.get("action_entries")):
        lines.append(
            "- rank=`{0}` id=`{1}` kind=`{2}` entry=`{3}` tab=`{4}` projection=`{5}`".format(
                action["rank"],
                action["action_id"],
                action["action_kind"],
                action["entry_name"],
                action["selected_tab_id"],
                action["projection_kind"],
            )
        )
        lines.append(
            "  reason=`{0}` headline={1}".format(
                get_mapping(action.get("opening_reason")).get("kind", ""),
                action.get("projection_headline", "") or "none",
            )
        )
        lines.append(
            "  projection_lines: summary=`{0}` questions=`{1}`".format(
                len(get_list(action.get("projection_summary_lines"))),
                len(get_list(action.get("projection_question_lines"))),
            )
        )
        lines.append(f"  opener: `{action['opener_summary_path']}`")

    lines.extend(["", "## Planning Surface"])
    lines.append("- planned entries: `{0}`".format("`, `".join(planning_surface["planned_entry_names"])))
    lines.append("- omitted entries: `{0}`".format("`, `".join(planning_surface["omitted_entry_names"])))
    lines.extend(["", "## Questions"])
    for question in summary["questions"]["planner_questions"]:
        lines.append(f"- planner: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    status = summary["planner_status"]
    return "\n".join(
        [
            f"result: {summary['result']}",
            f"source_selector_summary_path: {summary['artifact_context']['source_selector_summary_path']}",
            f"execution_plan_status: {status['execution_plan_status']}",
            f"planned_action_count: {status['planned_action_count']}",
            f"default_action_name: {status['default_action_name']}",
            f"compare_action_name: {status['compare_action_name']}",
            f"next_action_count: {status['next_action_count']}",
            f"omitted_entry_count: {status['omitted_entry_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export an execution plan from a front-page entry opening-flow consumer selector summary."
    )
    parser.add_argument("--selector", required=True, help="Input front-page entry opening-flow consumer selector summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for consumer plan artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for consumer plan summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for consumer plan markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for consumer plan check text.")
    args = parser.parse_args()

    selector_path = Path(args.selector).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.plan.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.consumer.plan.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.plan.check.txt")

    try:
        summary = build_summary_model(selector_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN] actions={summary['planner_status']['planned_action_count']}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN] default={summary['planner_status']['default_action_name']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
