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
    normalize_path,
    resolve_output_path,
    write_text,
)


CONSUMER_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer/v0"
CONSUMER_KIND = "system_compiler.front_page_entry_opening_flow_consumer"
SELECTOR_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_selector/v0"
SELECTOR_KIND = "system_compiler.front_page_entry_opening_flow_consumer_selector"


def get_list(value: Any) -> list[Any]:
    if isinstance(value, list):
        return value
    return []


def normalize_optional_path(value: Any) -> str:
    text = choose_text(value)
    if not text:
        return ""
    return normalize_path(text)


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


def load_consumer_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != CONSUMER_SCHEMA:
        raise ValueError(f"unsupported opening flow consumer schema: {path}")
    if choose_text(summary.get("kind")) != CONSUMER_KIND:
        raise ValueError(f"unsupported opening flow consumer kind: {path}")
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


def build_entry_view(entry: dict[str, Any], rank: int) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("rank", rank),
            ("name", choose_text(entry.get("name"))),
            ("selected_tab_id", choose_text(entry.get("selected_tab_id"))),
            ("selected_role", choose_text(entry.get("selected_role"))),
            ("query_kind", choose_text(entry.get("query_kind"))),
            ("query_scope", choose_text(entry.get("query_scope"))),
            ("target_summary_schema", choose_text(entry.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(entry.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(entry.get("target_summary_path"))),
            ("projection_kind", choose_text(entry.get("projection_kind"))),
            ("opening_reason", clone_opening_reason(entry.get("opening_reason"))),
            ("projection_headline", choose_text(entry.get("projection_headline"))),
            ("projection_summary_lines", string_list(entry.get("projection_summary_lines"))),
            ("compare_context_available", bool(entry.get("compare_context_available"))),
            ("landing_verdict", choose_text(entry.get("landing_verdict"))),
            ("inspector_ready", bool(entry.get("inspector_ready"))),
            ("inspector_mode", choose_text(entry.get("inspector_mode"))),
            (
                "inspector_blockers",
                [choose_text(item) for item in get_list(entry.get("inspector_blockers")) if choose_text(item)],
            ),
            ("opener_summary_path", normalize_optional_path(entry.get("summary_path"))),
            ("opener_report_markdown_path", normalize_optional_path(entry.get("report_markdown_path"))),
            ("opener_check_text_path", normalize_optional_path(entry.get("check_text_path"))),
        ]
    )


def find_entry_by_name(entries: list[dict[str, Any]], name: str) -> dict[str, Any] | None:
    for entry in entries:
        if choose_text(entry.get("name")) == name:
            return entry
    return None


def choose_fallback_entries(entries: list[dict[str, Any]], selected_names: set[str]) -> list[dict[str, Any]]:
    candidates = [
        entry
        for entry in entries
        if bool(entry.get("renderable")) and choose_text(entry.get("name")) not in selected_names
    ]
    return sorted(candidates, key=lambda entry: (int(entry.get("priority", 0)), choose_text(entry.get("name"))))


def build_open_plan(
    consumer_summary: dict[str, Any],
    entries: list[dict[str, Any]],
) -> OrderedDict[str, Any]:
    status = get_mapping(consumer_summary.get("consumer_status"))
    default_name = choose_text(status.get("default_opening_name"))
    compare_name = choose_text(status.get("compare_opening_name"))
    default_entry = find_entry_by_name(entries, default_name)
    compare_entry = find_entry_by_name(entries, compare_name)
    selected_names = {name for name in (default_name, compare_name) if name}
    fallbacks = choose_fallback_entries(entries, selected_names)

    default_view = build_entry_view(default_entry, 0) if default_entry is not None else None
    compare_view = build_entry_view(compare_entry, 1) if compare_entry is not None else None
    fallback_views = [build_entry_view(entry, index + 2) for index, entry in enumerate(fallbacks)]
    ordered_views = [view for view in [default_view, compare_view] if view is not None] + fallback_views

    return OrderedDict(
        [
            ("status", "ready" if default_view is not None else "blocked"),
            ("default_entry", default_view or OrderedDict()),
            ("compare_entry", compare_view or OrderedDict()),
            ("fallback_entries", fallback_views),
            ("ordered_entries", ordered_views),
            (
                "selection_notes",
                [
                    "default entry follows consumer_status.default_opening_name",
                    "compare entry follows consumer_status.compare_opening_name when present",
                    "fallback entries preserve consumer handoff priority order",
                ],
            ),
        ]
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    consumer_summary_path: Path,
    consumer_summary: dict[str, Any],
    open_plan: dict[str, Any],
) -> OrderedDict[str, Any]:
    consumer_front_page = get_mapping(consumer_summary.get("front_page"))
    surfaces: list[OrderedDict[str, str]] = [
        make_surface(
            "source_consumer_handoff",
            "source opening flow consumer handoff",
            "source_consumer_handoff",
            CONSUMER_SCHEMA,
            normalize_path(consumer_summary_path),
            normalize_optional_path(consumer_front_page.get("report_markdown_path")),
            normalize_optional_path(consumer_front_page.get("check_text_path")),
        )
    ]

    default_entry = get_mapping(open_plan.get("default_entry"))
    if default_entry:
        surfaces.append(
            make_surface(
                "selected_default_opener",
                f"selected default opener: {default_entry['name']}",
                "selected_default_opener",
                "system_compiler.front_page_entry_opener/v0",
                default_entry["opener_summary_path"],
                default_entry["opener_report_markdown_path"],
                default_entry["opener_check_text_path"],
            )
        )

    compare_entry = get_mapping(open_plan.get("compare_entry"))
    if compare_entry and compare_entry.get("name") != default_entry.get("name"):
        surfaces.append(
            make_surface(
                "selected_compare_opener",
                f"selected compare opener: {compare_entry['name']}",
                "selected_compare_opener",
                "system_compiler.front_page_entry_opener/v0",
                compare_entry["opener_summary_path"],
                compare_entry["opener_report_markdown_path"],
                compare_entry["opener_check_text_path"],
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
    consumer_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    consumer_summary = load_consumer_summary(consumer_summary_path)
    entries = [get_mapping(entry) for entry in get_list(consumer_summary.get("opening_handoff_entries"))]
    open_plan = build_open_plan(consumer_summary, entries)
    ordered_entries = get_list(open_plan.get("ordered_entries"))
    source_status = get_mapping(consumer_summary.get("consumer_status"))
    result = "ok" if open_plan.get("status") == "ready" and choose_text(consumer_summary.get("result")) == "ok" else "fail"

    return OrderedDict(
        [
            ("schema", SELECTOR_SCHEMA),
            ("kind", SELECTOR_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_consumer_selector.py"),
            ("result", result),
            (
                "opening_flow_consumer_selector",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Selector"),
                        (
                            "summary",
                            "A deterministic reader that turns an opening-flow consumer handoff into a selected explain entry order.",
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
                    consumer_summary_path=consumer_summary_path,
                    consumer_summary=consumer_summary,
                    open_plan=open_plan,
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_consumer_summary_path", normalize_path(consumer_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("selector_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "source_consumer",
                OrderedDict(
                    [
                        ("result", choose_text(consumer_summary.get("result"))),
                        ("total_opening_count", int(source_status.get("total_opening_count", 0))),
                        ("renderable_opening_count", int(source_status.get("renderable_opening_count", 0))),
                        ("compare_aware_opening_count", int(source_status.get("compare_aware_opening_count", 0))),
                        ("default_opening_name", choose_text(source_status.get("default_opening_name"))),
                        ("compare_opening_name", choose_text(source_status.get("compare_opening_name"))),
                    ]
                ),
            ),
            (
                "selector_status",
                OrderedDict(
                    [
                        ("result", result),
                        ("open_plan_status", choose_text(open_plan.get("status"))),
                        ("selected_entry_count", len(ordered_entries)),
                        (
                            "default_entry_name",
                            choose_text(get_mapping(open_plan.get("default_entry")).get("name")),
                        ),
                        (
                            "compare_entry_name",
                            choose_text(get_mapping(open_plan.get("compare_entry")).get("name")),
                        ),
                        ("fallback_entry_count", len(get_list(open_plan.get("fallback_entries")))),
                    ]
                ),
            ),
            ("open_plan", open_plan),
            (
                "questions",
                OrderedDict(
                    [
                        (
                            "selector_questions",
                            [
                                "Should the default entry be rendered as the first explain panel?",
                                "Should the compare entry be rendered beside the default entry or behind a compare affordance?",
                            ],
                        ),
                        (
                            "next_questions",
                            [
                                "Which projection kind should become the first rich explain adapter?",
                                "Should blocked inspector modes be mapped to summary-native readers?",
                            ],
                        ),
                    ]
                ),
            ),
            ("violations", [] if result == "ok" else ["no ready default entry was selected"]),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["selector_status"]
    open_plan = summary["open_plan"]
    default_entry = get_mapping(open_plan.get("default_entry"))
    compare_entry = get_mapping(open_plan.get("compare_entry"))

    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Selector",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source consumer: `{summary['artifact_context']['source_consumer_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['selector_summary_path']}`",
        "",
        "## Selector Status",
        "- plan=`{0}` selected=`{1}` fallback=`{2}`".format(
            status["open_plan_status"],
            status["selected_entry_count"],
            status["fallback_entry_count"],
        ),
        f"- default entry: `{status['default_entry_name']}`",
        f"- compare entry: `{status['compare_entry_name']}`",
        "",
        "## Default Entry",
        "- `{0}` tab=`{1}` query=`{2}/{3}` projection=`{4}` target=`{5}`".format(
            default_entry.get("name", ""),
            default_entry.get("selected_tab_id", ""),
            default_entry.get("query_kind", ""),
            default_entry.get("query_scope", ""),
            default_entry.get("projection_kind", ""),
            default_entry.get("target_summary_path", ""),
        ),
        "- reason=`{0}` headline={1}".format(
            get_mapping(default_entry.get("opening_reason")).get("kind", ""),
            default_entry.get("projection_headline", "") or "none",
        ),
        "",
        "## Compare Entry",
        "- `{0}` tab=`{1}` query=`{2}/{3}` projection=`{4}` target=`{5}`".format(
            compare_entry.get("name", ""),
            compare_entry.get("selected_tab_id", ""),
            compare_entry.get("query_kind", ""),
            compare_entry.get("query_scope", ""),
            compare_entry.get("projection_kind", ""),
            compare_entry.get("target_summary_path", ""),
        ),
        "- reason=`{0}` headline={1}".format(
            get_mapping(compare_entry.get("opening_reason")).get("kind", ""),
            compare_entry.get("projection_headline", "") or "none",
        ),
        "",
        "## Ordered Entries",
    ]
    for entry in get_list(open_plan.get("ordered_entries")):
        lines.append(
            "- rank=`{0}` name=`{1}` schema=`{2}` reason=`{3}` projection=`{4}` compare=`{5}` inspector_ready=`{6}`".format(
                entry["rank"],
                entry["name"],
                entry["target_summary_schema"],
                entry["opening_reason"]["kind"],
                entry["projection_kind"],
                entry["compare_context_available"],
                entry["inspector_ready"],
            )
        )

    lines.extend(["", "## Questions"])
    for question in summary["questions"]["selector_questions"]:
        lines.append(f"- selector: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    status = summary["selector_status"]
    return "\n".join(
        [
            f"result: {summary['result']}",
            f"source_consumer_summary_path: {summary['artifact_context']['source_consumer_summary_path']}",
            f"open_plan_status: {status['open_plan_status']}",
            f"selected_entry_count: {status['selected_entry_count']}",
            f"default_entry_name: {status['default_entry_name']}",
            f"compare_entry_name: {status['compare_entry_name']}",
            f"fallback_entry_count: {status['fallback_entry_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a selected explain entry order from an opening flow consumer handoff."
    )
    parser.add_argument("--consumer", required=True, help="Input front-page entry opening flow consumer summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for selector artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for selector summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for selector markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for selector check text.")
    args = parser.parse_args()

    consumer_path = Path(args.consumer).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-selector").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.selector.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.consumer.selector.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.selector.check.txt")

    try:
        summary = build_summary_model(
            consumer_summary_path=consumer_path,
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

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR] default={summary['selector_status']['default_entry_name']}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR] selected={summary['selector_status']['selected_entry_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
