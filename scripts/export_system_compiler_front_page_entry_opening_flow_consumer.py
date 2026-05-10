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
    normalize_path,
    resolve_output_path,
    write_text,
)


FLOW_SCHEMA = "system_compiler.front_page_entry_opening_flow/v0"
FLOW_KIND = "system_compiler.front_page_entry_opening_flow"
CONSUMER_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer/v0"
CONSUMER_KIND = "system_compiler.front_page_entry_opening_flow_consumer"


def get_list(value: Any) -> list[Any]:
    if isinstance(value, list):
        return value
    return []


def ordered_unique(values: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        text = choose_text(value)
        if not text or text in seen:
            continue
        seen.add(text)
        result.append(text)
    return result


def path_exists(path_value: Any) -> bool:
    text = choose_text(path_value)
    return bool(text and Path(text).exists())


def normalize_optional_path(path_value: Any) -> str:
    text = choose_text(path_value)
    if not text:
        return ""
    return normalize_path(text)


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


def string_list(value: Any) -> list[str]:
    return [choose_text(item) for item in get_list(value) if choose_text(item)]


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


def load_flow_summary(flow_path: Path) -> dict[str, Any]:
    summary = load_json(flow_path)
    if choose_text(summary.get("schema")) != FLOW_SCHEMA:
        raise ValueError(f"unsupported opening flow schema: {flow_path}")
    if choose_text(summary.get("kind")) != FLOW_KIND:
        raise ValueError(f"unsupported opening flow kind: {flow_path}")
    return summary


def build_opening_handoff_entry(index: int, case: dict[str, Any]) -> OrderedDict[str, Any]:
    open_action_ready = choose_text(case.get("open_action_status")) == "ready"
    projection_available = choose_text(case.get("projection_status")) == "available"
    target_exists = path_exists(case.get("target_summary_path"))
    summary_exists = path_exists(case.get("summary_path"))
    report_exists = path_exists(case.get("report_markdown_path"))
    check_exists = path_exists(case.get("check_text_path"))
    inspector_ready = bool(case.get("inspector_ready"))
    open_action_blockers = string_list(case.get("open_action_blockers"))
    projection_blockers = string_list(case.get("projection_blockers"))
    inspector_blockers = string_list(case.get("inspector_blockers"))

    blockers: list[str] = []
    if not open_action_ready:
        blockers.append("open action is not ready")
    if not projection_available:
        blockers.append("opened projection is not available")
    if not target_exists:
        blockers.append("target summary path is missing")
    if not summary_exists:
        blockers.append("opener summary path is missing")
    if not report_exists:
        blockers.append("opener report path is missing")
    if not check_exists:
        blockers.append("opener check path is missing")

    renderable = open_action_ready and projection_available and target_exists and summary_exists
    handoff_status = "ready" if renderable else "blocked"
    compare_context_available = bool(case.get("compare_context_available"))

    priority = index
    if compare_context_available:
        priority -= 100
    if choose_text(case.get("name")) == "root-witness":
        priority -= 50
    if not renderable:
        priority += 1000

    return OrderedDict(
        [
            ("name", choose_text(case.get("name"))),
            ("handoff_status", handoff_status),
            ("renderable", renderable),
            ("priority", priority),
            ("open_action_status", choose_text(case.get("open_action_status"))),
            ("selected_tab_id", choose_text(case.get("selected_tab_id"))),
            ("selected_role", choose_text(case.get("selected_role"))),
            ("query_kind", choose_text(case.get("query_kind"))),
            ("query_scope", choose_text(case.get("query_scope"))),
            ("selection_rule", choose_text(case.get("selection_rule"))),
            ("opening_reason", clone_opening_reason(case.get("opening_reason"))),
            ("target_summary_schema", choose_text(case.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(case.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(case.get("target_summary_path"))),
            ("open_action_blockers", open_action_blockers),
            ("projection_status", choose_text(case.get("projection_status"))),
            ("projection_kind", choose_text(case.get("projection_kind"))),
            ("projection_headline", choose_text(case.get("projection_headline"))),
            ("projection_summary_lines", string_list(case.get("projection_summary_lines"))),
            ("projection_question_lines", string_list(case.get("projection_question_lines"))),
            ("projection_blockers", projection_blockers),
            ("compare_context_available", compare_context_available),
            ("landing_verdict", choose_text(case.get("landing_verdict"))),
            ("inspector_ready", inspector_ready),
            ("inspector_mode", choose_text(case.get("inspector_mode"))),
            ("summary_path", normalize_optional_path(case.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(case.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(case.get("check_text_path"))),
            ("inspector_blockers", inspector_blockers),
            ("opener_compare_questions", string_list(case.get("opener_compare_questions"))),
            ("opener_next_questions", string_list(case.get("opener_next_questions"))),
            ("handoff_blockers", ordered_unique(blockers)),
        ]
    )


def choose_default_opening(entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any] | None:
    renderable = [entry for entry in entries if entry["renderable"]]
    if not renderable:
        return None
    plain_root = [entry for entry in renderable if entry["name"] == "root-witness"]
    if plain_root:
        return plain_root[0]
    return sorted(renderable, key=lambda entry: (entry["priority"], entry["name"]))[0]


def choose_compare_opening(entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any] | None:
    compare_entries = [entry for entry in entries if entry["renderable"] and entry["compare_context_available"]]
    if not compare_entries:
        return None
    return sorted(compare_entries, key=lambda entry: (entry["priority"], entry["name"]))[0]


def project_opening(entry: OrderedDict[str, Any] | None) -> OrderedDict[str, Any]:
    if entry is None:
        return OrderedDict(
            [
                ("available", False),
                ("name", ""),
                ("selected_tab_id", ""),
                ("query_kind", ""),
                ("target_summary_schema", ""),
                ("target_summary_path", ""),
                ("projection_kind", ""),
                ("opening_reason", clone_opening_reason({})),
                ("projection_headline", ""),
                ("projection_summary_lines", []),
                ("projection_question_lines", []),
                ("blockers", []),
                ("questions", OrderedDict([("compare_questions", []), ("next_questions", [])])),
                ("reason", "no renderable opening handoff entry exists"),
            ]
        )

    return OrderedDict(
        [
            ("available", True),
            ("name", entry["name"]),
            ("selected_tab_id", entry["selected_tab_id"]),
            ("query_kind", entry["query_kind"]),
            ("target_summary_schema", entry["target_summary_schema"]),
            ("target_summary_path", entry["target_summary_path"]),
            ("projection_kind", entry["projection_kind"]),
            ("opening_reason", entry["opening_reason"]),
            ("projection_headline", entry["projection_headline"]),
            ("projection_summary_lines", entry["projection_summary_lines"]),
            ("projection_question_lines", entry["projection_question_lines"]),
            (
                "blockers",
                ordered_unique(entry["open_action_blockers"] + entry["projection_blockers"] + entry["handoff_blockers"]),
            ),
            (
                "questions",
                OrderedDict(
                    [
                        ("compare_questions", entry["opener_compare_questions"]),
                        ("next_questions", entry["opener_next_questions"]),
                    ]
                ),
            ),
            ("reason", "first stable renderable opening for consumer preview"),
        ]
    )


def build_readiness_surface(entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    projection_kinds = Counter(entry["projection_kind"] for entry in entries if entry["projection_kind"])
    target_schemas = Counter(entry["target_summary_schema"] for entry in entries if entry["target_summary_schema"])
    opening_reason_kinds = Counter(entry["opening_reason"]["kind"] for entry in entries if entry["opening_reason"]["kind"])
    blocker_counter = Counter()
    for entry in entries:
        for blocker in (
            entry["open_action_blockers"]
            + entry["projection_blockers"]
            + entry["inspector_blockers"]
            + entry["handoff_blockers"]
        ):
            blocker_counter[blocker] += 1

    return OrderedDict(
        [
            ("projection_kinds", OrderedDict(sorted(projection_kinds.items()))),
            ("target_summary_schemas", OrderedDict(sorted(target_schemas.items()))),
            ("opening_reason_kinds", OrderedDict(sorted(opening_reason_kinds.items()))),
            ("blocked_reason_counts", OrderedDict(sorted(blocker_counter.items()))),
            (
                "compare_aware_openings",
                [entry["name"] for entry in entries if entry["compare_context_available"]],
            ),
            (
                "renderable_openings",
                [entry["name"] for entry in entries if entry["renderable"]],
            ),
            (
                "preview_ready_openings",
                [
                    entry["name"]
                    for entry in entries
                    if entry["projection_headline"] or entry["projection_summary_lines"]
                ],
            ),
            (
                "drift_reason_openings",
                [entry["name"] for entry in entries if bool(entry["opening_reason"]["drift_changed"])],
            ),
            (
                "blocked_openings",
                [entry["name"] for entry in entries if not entry["renderable"]],
            ),
        ]
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    flow_summary_path: Path,
    flow_summary: dict[str, Any],
    default_entry: OrderedDict[str, Any] | None,
    compare_entry: OrderedDict[str, Any] | None,
) -> OrderedDict[str, Any]:
    flow_front_page = get_mapping(flow_summary.get("front_page"))
    surfaces: list[OrderedDict[str, str]] = [
        make_surface(
            "source_opening_flow",
            "source opening flow",
            "source_opening_flow",
            FLOW_SCHEMA,
            normalize_path(flow_summary_path),
            normalize_optional_path(flow_front_page.get("report_markdown_path")),
            normalize_optional_path(flow_front_page.get("check_text_path")),
        )
    ]

    if default_entry is not None:
        surfaces.append(
            make_surface(
                "default_opening_opener",
                f"default opening: {default_entry['name']}",
                "default_opening_opener",
                "system_compiler.front_page_entry_opener/v0",
                default_entry["summary_path"],
                default_entry["report_markdown_path"],
                default_entry["check_text_path"],
            )
        )
    if compare_entry is not None and compare_entry is not default_entry:
        surfaces.append(
            make_surface(
                "compare_opening_opener",
                f"compare opening: {compare_entry['name']}",
                "compare_opening_opener",
                "system_compiler.front_page_entry_opener/v0",
                compare_entry["summary_path"],
                compare_entry["report_markdown_path"],
                compare_entry["check_text_path"],
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
    flow_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    flow_summary = load_flow_summary(flow_summary_path)
    flow_status = get_mapping(flow_summary.get("flow_status"))
    cases = [get_mapping(case) for case in get_list(flow_summary.get("opener_cases"))]
    entries = [build_opening_handoff_entry(index, case) for index, case in enumerate(cases)]
    default_entry = choose_default_opening(entries)
    compare_entry = choose_compare_opening(entries)
    readiness_surface = build_readiness_surface(entries)
    renderable_count = sum(1 for entry in entries if entry["renderable"])
    ready_open_action_count = sum(1 for entry in entries if entry["open_action_status"] == "ready")
    compare_aware_count = sum(1 for entry in entries if entry["compare_context_available"])
    inspector_ready_count = sum(1 for entry in entries if entry["inspector_ready"])
    blocked_inspector_count = len(entries) - inspector_ready_count
    result = (
        "ok"
        if choose_text(flow_summary.get("result")) == "ok" and len(entries) > 0 and renderable_count == len(entries)
        else "fail"
    )

    return OrderedDict(
        [
            ("schema", CONSUMER_SCHEMA),
            ("kind", CONSUMER_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_consumer.py"),
            ("result", result),
            (
                "opening_flow_consumer",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer"),
                        (
                            "summary",
                            "A thin consumer handoff that turns an opening-flow evidence object into ordered explain entries and readiness facts.",
                        ),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(
                    summary_path=summary_path,
                    report_path=report_path,
                    check_path=check_path,
                    flow_summary_path=flow_summary_path,
                    flow_summary=flow_summary,
                    default_entry=default_entry,
                    compare_entry=compare_entry,
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_flow_summary_path", normalize_path(flow_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("consumer_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "source_flow",
                OrderedDict(
                    [
                        ("result", choose_text(flow_summary.get("result"))),
                        ("expected_opener_count", int(flow_status.get("expected_opener_count", 0))),
                        ("actual_opener_count", int(flow_status.get("actual_opener_count", 0))),
                        ("available_projection_count", int(flow_status.get("available_projection_count", 0))),
                        ("compare_context_count", int(flow_status.get("compare_context_count", 0))),
                        ("inspector_ready_count", int(flow_status.get("inspector_ready_count", 0))),
                        ("blocked_inspector_count", int(flow_status.get("blocked_inspector_count", 0))),
                    ]
                ),
            ),
            (
                "consumer_status",
                OrderedDict(
                    [
                        ("result", result),
                        ("total_opening_count", len(entries)),
                        ("renderable_opening_count", renderable_count),
                        ("ready_open_action_count", ready_open_action_count),
                        ("compare_aware_opening_count", compare_aware_count),
                        ("inspector_ready_count", inspector_ready_count),
                        ("blocked_inspector_count", blocked_inspector_count),
                        ("default_opening_name", default_entry["name"] if default_entry is not None else ""),
                        ("compare_opening_name", compare_entry["name"] if compare_entry is not None else ""),
                    ]
                ),
            ),
            ("default_opening", project_opening(default_entry)),
            ("compare_opening", project_opening(compare_entry)),
            ("readiness_surface", readiness_surface),
            ("opening_handoff_entries", entries),
            (
                "questions",
                OrderedDict(
                    [
                        (
                            "consumer_questions",
                            [
                                "Which renderable opening should the explain surface show first?",
                                "Which compare-aware opening should be kept nearby for counterfactual review?",
                            ],
                        ),
                        (
                            "next_questions",
                            [
                                "Should a higher explain layer render the default opening projection directly?",
                                "Which blocked inspector reason should become the next supported consumer adapter?",
                            ],
                        ),
                    ]
                ),
            ),
            (
                "violations",
                []
                if result == "ok"
                else ["source flow failed, no opening handoff entries exist, or one or more entries are not renderable"],
            ),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["consumer_status"]
    default_opening = summary["default_opening"]
    compare_opening = summary["compare_opening"]
    readiness = summary["readiness_surface"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source flow: `{summary['artifact_context']['source_flow_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['consumer_summary_path']}`",
        "",
        "## Consumer Status",
        "- openings=`{0}` renderable=`{1}` open_action_ready=`{2}` compare_aware=`{3}` inspector_ready=`{4}`".format(
            status["total_opening_count"],
            status["renderable_opening_count"],
            status["ready_open_action_count"],
            status["compare_aware_opening_count"],
            status["inspector_ready_count"],
        ),
        f"- default opening: `{status['default_opening_name']}`",
        f"- compare opening: `{status['compare_opening_name']}`",
        "",
        "## Default Opening",
        "- `{0}` schema=`{1}` projection=`{2}` reason=`{3}` target=`{4}`".format(
            default_opening["name"],
            default_opening["target_summary_schema"],
            default_opening["projection_kind"],
            default_opening["opening_reason"]["kind"],
            default_opening["target_summary_path"],
        ),
        f"- headline: {default_opening['projection_headline'] or 'none'}",
        "",
        "## Compare Opening",
        "- `{0}` schema=`{1}` projection=`{2}` reason=`{3}` target=`{4}`".format(
            compare_opening["name"],
            compare_opening["target_summary_schema"],
            compare_opening["projection_kind"],
            compare_opening["opening_reason"]["kind"],
            compare_opening["target_summary_path"],
        ),
        f"- headline: {compare_opening['projection_headline'] or 'none'}",
        "",
        "## Opening Entries",
    ]
    for entry in summary["opening_handoff_entries"]:
        lines.append(
            "- `{0}` status=`{1}` renderable=`{2}` tab=`{3}` reason=`{4}` projection=`{5}` compare=`{6}` inspector_ready=`{7}`".format(
                entry["name"],
                entry["handoff_status"],
                entry["renderable"],
                entry["selected_tab_id"],
                entry["opening_reason"]["kind"],
                entry["projection_kind"],
                entry["compare_context_available"],
                entry["inspector_ready"],
            )
        )
        if entry["projection_headline"]:
            lines.append(f"  headline: {entry['projection_headline']}")
        for item in entry["projection_summary_lines"][:2]:
            lines.append(f"  summary: {item}")

    lines.extend(["", "## Readiness Surface"])
    for kind, count in readiness["opening_reason_kinds"].items():
        lines.append(f"- opening reason `{kind}` count=`{count}`")
    for reason, count in readiness["blocked_reason_counts"].items():
        lines.append(f"- blocker `{reason}` count=`{count}`")

    lines.extend(["", "## Questions"])
    for question in summary["questions"]["consumer_questions"]:
        lines.append(f"- consumer: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    status = summary["consumer_status"]
    return "\n".join(
        [
            f"result: {summary['result']}",
            f"source_flow_summary_path: {summary['artifact_context']['source_flow_summary_path']}",
            f"total_opening_count: {status['total_opening_count']}",
            f"renderable_opening_count: {status['renderable_opening_count']}",
            f"ready_open_action_count: {status['ready_open_action_count']}",
            f"compare_aware_opening_count: {status['compare_aware_opening_count']}",
            f"inspector_ready_count: {status['inspector_ready_count']}",
            f"blocked_inspector_count: {status['blocked_inspector_count']}",
            f"default_opening_name: {status['default_opening_name']}",
            f"compare_opening_name: {status['compare_opening_name']}",
            f"default_opening_reason_kind: {summary['default_opening']['opening_reason']['kind']}",
            f"default_projection_headline: {summary['default_opening']['projection_headline']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a consumer handoff for a system compiler front_page entry opening flow summary."
    )
    parser.add_argument("--flow", required=True, help="Input front-page entry opening flow summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for consumer artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for consumer summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for consumer markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for consumer check text.")
    args = parser.parse_args()

    flow_path = Path(args.flow).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.consumer.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.check.txt")

    try:
        summary = build_summary_model(
            flow_summary_path=flow_path,
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

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER] default={summary['consumer_status']['default_opening_name']}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER] renderable={summary['consumer_status']['renderable_opening_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
