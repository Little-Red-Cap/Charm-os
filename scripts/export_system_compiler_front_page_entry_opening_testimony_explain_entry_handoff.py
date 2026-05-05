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


EXPLAIN_ENTRY_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry/v0"
EXPLAIN_ENTRY_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry"
HANDOFF_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0"
HANDOFF_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff"


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def ordered_unique_surfaces(surfaces: list[OrderedDict[str, Any]]) -> list[OrderedDict[str, Any]]:
    seen: set[tuple[str, str]] = set()
    result: list[OrderedDict[str, Any]] = []
    for surface in surfaces:
        key = (choose_text(surface.get("surface_id")), choose_text(surface.get("summary_path")))
        if key in seen:
            continue
        seen.add(key)
        result.append(surface)
    return result


def load_explain_entry_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != EXPLAIN_ENTRY_SCHEMA:
        raise ValueError(f"unsupported opening testimony explain-entry schema: {path}")
    if choose_text(summary.get("kind")) != EXPLAIN_ENTRY_KIND:
        raise ValueError(f"unsupported opening testimony explain-entry kind: {path}")
    return summary


def make_empty_surface() -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("surface_id", ""),
            ("label", ""),
            ("role", ""),
            ("summary_schema", ""),
            ("summary_kind", ""),
            ("summary_path", ""),
            ("report_markdown_path", ""),
            ("check_text_path", ""),
            ("route_id", ""),
            ("depth", None),
            ("source", ""),
        ]
    )


def normalize_surface(surface_value: Any) -> OrderedDict[str, Any]:
    surface = get_mapping(surface_value)
    depth_value: int | None
    try:
        depth_value = int(surface.get("depth"))
    except (TypeError, ValueError):
        depth_value = None
    return OrderedDict(
        [
            ("surface_id", choose_text(surface.get("surface_id"))),
            ("label", choose_text(surface.get("label"))),
            ("role", choose_text(surface.get("role"))),
            ("summary_schema", choose_text(surface.get("summary_schema"))),
            ("summary_kind", choose_text(surface.get("summary_kind"))),
            ("summary_path", normalize_optional_path(surface.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(surface.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(surface.get("check_text_path"))),
            ("route_id", choose_text(surface.get("route_id"))),
            ("depth", depth_value),
            ("source", choose_text(surface.get("source"))),
        ]
    )


def make_front_page_surface(
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


def front_page_surface_from_target(
    surface: dict[str, Any],
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    label = choose_text(surface.get("label")) or choose_text(surface.get("surface_id")) or surface_id
    return make_front_page_surface(
        surface_id=surface_id,
        label=label,
        role=role,
        summary_schema=choose_text(surface.get("summary_schema")),
        summary_path=normalize_optional_path(surface.get("summary_path")),
        report_markdown_path=normalize_optional_path(surface.get("report_markdown_path")),
        check_text_path=normalize_optional_path(surface.get("check_text_path")),
    )


def source_report_path(explain_entry_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(explain_entry_summary.get("artifact_context"))
    front_page = get_mapping(explain_entry_summary.get("front_page"))
    return normalize_optional_path(artifact_context.get("report_markdown_path") or front_page.get("report_markdown_path"))


def source_check_path(explain_entry_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(explain_entry_summary.get("artifact_context"))
    front_page = get_mapping(explain_entry_summary.get("front_page"))
    return normalize_optional_path(artifact_context.get("check_text_path") or front_page.get("check_text_path"))


def build_source_explain_entry_ref(
    explain_entry_summary: dict[str, Any],
    explain_entry_summary_path: Path,
) -> OrderedDict[str, str]:
    decision = get_mapping(explain_entry_summary.get("explain_entry_decision"))
    selected = get_mapping(explain_entry_summary.get("selected_surface"))
    return OrderedDict(
        [
            ("summary_schema", EXPLAIN_ENTRY_SCHEMA),
            ("summary_kind", EXPLAIN_ENTRY_KIND),
            ("summary_path", normalize_path(explain_entry_summary_path)),
            ("report_markdown_path", source_report_path(explain_entry_summary)),
            ("check_text_path", source_check_path(explain_entry_summary)),
            ("result", choose_text(explain_entry_summary.get("result"))),
            ("decision_status", choose_text(decision.get("status"))),
            ("selection_kind", choose_text(decision.get("selection_kind"))),
            ("selected_surface_id", choose_text(selected.get("surface_id"))),
        ]
    )


def build_violations(explain_entry_summary: dict[str, Any], open_target: dict[str, Any]) -> list[str]:
    violations: list[str] = []
    decision = get_mapping(explain_entry_summary.get("explain_entry_decision"))
    target_summary_path = normalize_optional_path(open_target.get("summary_path"))

    if choose_text(explain_entry_summary.get("result")) != "ok":
        violations.append("source explain entry result is not ok")
    if choose_text(decision.get("status")) != "ready":
        violations.append("source explain_entry_decision.status is not ready")
    if not choose_text(open_target.get("surface_id")):
        violations.append("selected surface is missing")
    if not target_summary_path:
        violations.append("selected surface summary_path is missing")
    elif not Path(target_summary_path).exists():
        violations.append(f"selected surface summary_path is not found: {target_summary_path}")

    return list(dict.fromkeys(violations))


def build_opening_reason(explain_entry_summary: dict[str, Any]) -> OrderedDict[str, str]:
    source_reason = get_mapping(explain_entry_summary.get("opening_reason"))
    source_summary = choose_text(source_reason.get("summary"))
    handoff_summary = "Handoff is determined by the explain-entry default selection policy."
    summary = f"{source_summary} {handoff_summary}".strip() if source_summary else handoff_summary
    return OrderedDict(
        [
            ("kind", choose_text(source_reason.get("kind"))),
            ("source_reason_summary", source_summary),
            ("summary", summary),
            ("source_summary_path", normalize_optional_path(source_reason.get("source_summary_path"))),
        ]
    )


def build_handoff_action(status: str, open_target: dict[str, Any], violations: list[str]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("status", status),
            ("action_kind", "open_explain_surface"),
            ("query_kind", "default_explain_surface"),
            ("query_scope", "selected_surface"),
            ("expected_consumer_operation", "open-selected-summary"),
            ("target_summary_schema", choose_text(open_target.get("summary_schema"))),
            ("target_summary_kind", choose_text(open_target.get("summary_kind"))),
            ("target_summary_path", normalize_optional_path(open_target.get("summary_path"))),
            ("target_report_markdown_path", normalize_optional_path(open_target.get("report_markdown_path"))),
            ("target_check_text_path", normalize_optional_path(open_target.get("check_text_path"))),
            ("blockers", violations),
        ]
    )


def build_next_questions() -> list[OrderedDict[str, str]]:
    return [
        OrderedDict(
            [
                ("kind", "inspect_open_target"),
                ("summary", "Open the selected explain surface carried by this handoff."),
                ("target_ref", "open_target.summary_path"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_source_explain_entry"),
                ("summary", "Inspect the explain-entry decision that produced this handoff."),
                ("target_ref", "source_explain_entry_ref.summary_path"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_supporting_targets"),
                ("summary", "Inspect supporting explain surfaces preserved alongside the handoff target."),
                ("target_ref", "supporting_targets"),
            ]
        ),
    ]


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    source_ref: dict[str, Any],
    open_target: dict[str, Any],
    supporting_targets: list[OrderedDict[str, Any]],
    ready: bool,
) -> OrderedDict[str, Any]:
    surfaces: list[OrderedDict[str, str]] = [
        make_front_page_surface(
            "source_opening_testimony_explain_entry",
            "source opening testimony explain entry",
            "source_opening_testimony_explain_entry",
            choose_text(source_ref.get("summary_schema")),
            choose_text(source_ref.get("summary_path")),
            choose_text(source_ref.get("report_markdown_path")),
            choose_text(source_ref.get("check_text_path")),
        )
    ]

    if ready:
        surfaces.append(front_page_surface_from_target(open_target, "selected_explain_surface", "selected_explain_surface"))

    for index, surface in enumerate(supporting_targets):
        if not choose_text(surface.get("summary_path")):
            continue
        surfaces.append(front_page_surface_from_target(surface, f"supporting_explain_surface_{index}", "supporting_explain_surface"))

    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", surfaces),
        ]
    )


def build_summary_model(
    source_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    source_summary = load_explain_entry_summary(source_summary_path)
    source_ref = build_source_explain_entry_ref(source_summary, source_summary_path)
    open_target = normalize_surface(source_summary.get("selected_surface"))
    supporting_targets = ordered_unique_surfaces(
        [normalize_surface(surface) for surface in get_list(source_summary.get("supporting_surfaces"))]
    )
    violations = build_violations(source_summary, open_target)
    handoff_status = "blocked" if violations else "ready"
    result = "ok" if handoff_status == "ready" else "fail"

    return OrderedDict(
        [
            ("schema", HANDOFF_SCHEMA),
            ("kind", HANDOFF_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"),
            ("result", result),
            (
                "opening_testimony_explain_entry_handoff",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Explain Entry Handoff"),
                        (
                            "summary",
                            "A deterministic handoff action projected from one opening testimony explain-entry decision.",
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
                    source_ref=source_ref,
                    open_target=open_target,
                    supporting_targets=supporting_targets,
                    ready=handoff_status == "ready",
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_summary_path", normalize_path(source_summary_path)),
                        ("source_report_markdown_path", source_ref["report_markdown_path"]),
                        ("source_check_text_path", source_ref["check_text_path"]),
                        ("output_root", normalize_path(output_root)),
                        ("handoff_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("source_explain_entry_ref", source_ref),
            (
                "handoff_decision",
                OrderedDict(
                    [
                        ("status", handoff_status),
                        ("handoff_id", "open-selected-explain-surface"),
                        ("selected_tab_id", "opening_testimony_explain"),
                        ("selected_role", "opening_testimony_explain_handoff"),
                    ]
                ),
            ),
            ("open_target", open_target),
            ("supporting_targets", supporting_targets),
            ("opening_reason", build_opening_reason(source_summary)),
            ("handoff_action", build_handoff_action(handoff_status, open_target, violations)),
            ("next_questions", build_next_questions()),
            ("violations", violations),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    decision = summary["handoff_decision"]
    source_ref = summary["source_explain_entry_ref"]
    target = summary["open_target"]
    action = summary["handoff_action"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Explain Entry Handoff",
        "",
        f"- Result: `{summary['result']}`",
        f"- Handoff status: `{decision['status']}`",
        f"- Source explain entry: `{source_ref['summary_path']}`",
        f"- Source decision: status=`{source_ref['decision_status']}` selection=`{source_ref['selection_kind']}` selected=`{source_ref['selected_surface_id']}`",
        "",
        "## Open Target",
        f"- surface: `{target['surface_id']}` role=`{target['role']}` source=`{target['source']}` depth=`{target['depth']}`",
        f"- schema: `{target['summary_schema']}` kind=`{target['summary_kind']}`",
        f"- summary: `{target['summary_path'] or 'none'}`",
        f"- report: `{target['report_markdown_path'] or 'none'}`",
        f"- check: `{target['check_text_path'] or 'none'}`",
        "",
        "## Handoff Action",
        "- status=`{0}` action=`{1}` query=`{2}` scope=`{3}` operation=`{4}`".format(
            action["status"],
            action["action_kind"],
            action["query_kind"],
            action["query_scope"],
            action["expected_consumer_operation"],
        ),
        f"- reason: {summary['opening_reason']['summary']}",
        "",
        "## Supporting Targets",
    ]
    if summary["supporting_targets"]:
        for surface in summary["supporting_targets"]:
            lines.append(f"- `{surface['surface_id']}`: `{surface['summary_path']}`")
    else:
        lines.append("- none")

    lines.extend(["", "## Next Questions"])
    for question in summary["next_questions"]:
        lines.append(f"- `{question['kind']}`: {question['summary']} ({question['target_ref']})")

    if summary["violations"]:
        lines.extend(["", "## Violations"])
        for violation in summary["violations"]:
            lines.append(f"- {violation}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    source_ref = summary["source_explain_entry_ref"]
    decision = summary["handoff_decision"]
    target = summary["open_target"]
    action = summary["handoff_action"]
    return "\n".join(
        [
            f"source_summary_path: {summary['artifact_context']['source_summary_path']}",
            f"result: {summary['result']}",
            f"handoff_status: {decision['status']}",
            f"handoff_id: {decision['handoff_id']}",
            f"selected_tab_id: {decision['selected_tab_id']}",
            f"selected_role: {decision['selected_role']}",
            f"source_decision_status: {source_ref['decision_status']}",
            f"source_selection_kind: {source_ref['selection_kind']}",
            f"open_target_surface_id: {target['surface_id']}",
            f"open_target_summary_schema: {target['summary_schema']}",
            f"open_target_summary_path: {target['summary_path']}",
            f"handoff_action_kind: {action['action_kind']}",
            f"handoff_query_kind: {action['query_kind']}",
            f"handoff_query_scope: {action['query_scope']}",
            f"handoff_expected_consumer_operation: {action['expected_consumer_operation']}",
            f"violations: {'|'.join(summary['violations'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a deterministic handoff action from one opening testimony explain-entry summary."
    )
    parser.add_argument("--source-summary", required=True, help="Input opening testimony explain-entry summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for explain-entry handoff artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for handoff summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for handoff markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for handoff check text.")
    args = parser.parse_args()

    source_summary_path = Path(args.source_summary).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry-handoff").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(
        args.summary,
        output_root,
        "front-page.entry-opening-testimony.explain-entry.handoff.summary.json",
    )
    report_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-opening-testimony.explain-entry.handoff.report.md",
    )
    check_path = resolve_output_path(
        args.check_text,
        output_root,
        "front-page.entry-opening-testimony.explain-entry.handoff.check.txt",
    )

    try:
        summary = build_summary_model(source_summary_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    decision = summary["handoff_decision"]
    target = summary["open_target"]
    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF] summary={summary_path}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF] status={0} target={1}".format(
            decision["status"],
            target["surface_id"] or "none",
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
