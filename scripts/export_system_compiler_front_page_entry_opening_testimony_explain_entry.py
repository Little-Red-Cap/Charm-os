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


ROUTE_SCHEMA = "system_compiler.front_page_route/v0"
ROUTE_KIND = "system_compiler.front_page_route"
ROUTE_COMPARE_SCHEMA = "system_compiler.front_page_route_compare/v0"
ROUTE_COMPARE_KIND = "system_compiler.front_page_route_compare"
OPENING_TESTIMONY_LANDING_SCHEMA = "system_compiler.front_page_entry_opening_testimony_landing/v0"
OPENING_TESTIMONY_LANDING_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_testimony_landing_compare/v0"
OPENING_TESTIMONY_EXPLAIN_ENTRY_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry_compare/v0"
EXPLAIN_ENTRY_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry/v0"
EXPLAIN_ENTRY_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry"


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


def load_source_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    schema = choose_text(summary.get("schema"))
    kind = choose_text(summary.get("kind"))
    if (schema, kind) not in {
        (ROUTE_SCHEMA, ROUTE_KIND),
        (ROUTE_COMPARE_SCHEMA, ROUTE_COMPARE_KIND),
    }:
        raise ValueError(f"unsupported opening testimony explain-entry source: {path}")
    return summary


def ensure_route_summary(summary: dict[str, Any], path: Path) -> dict[str, Any]:
    if choose_text(summary.get("schema")) != ROUTE_SCHEMA:
        raise ValueError(f"unsupported front page route schema: {path}")
    if choose_text(summary.get("kind")) != ROUTE_KIND:
        raise ValueError(f"unsupported front page route kind: {path}")
    return summary


def load_route_summary(path_value: str) -> dict[str, Any]:
    return ensure_route_summary(load_json(Path(path_value).resolve()), Path(path_value).resolve())


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


def surface_from_route_entry(entry_value: Any, source: str) -> OrderedDict[str, Any]:
    entry = get_mapping(entry_value)
    depth_value: int | None
    try:
        depth_value = int(entry.get("depth"))
    except (TypeError, ValueError):
        depth_value = None
    return OrderedDict(
        [
            ("surface_id", choose_text(entry.get("surface_id"))),
            ("label", choose_text(entry.get("label"))),
            ("role", choose_text(entry.get("role"))),
            ("summary_schema", choose_text(entry.get("summary_schema"))),
            ("summary_kind", choose_text(entry.get("summary_kind"))),
            ("summary_path", normalize_optional_path(entry.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(entry.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(entry.get("check_text_path"))),
            ("route_id", choose_text(entry.get("route_id"))),
            ("depth", depth_value),
            ("source", source),
        ]
    )


def surface_from_front_page_surface(surface_value: Any, source: str) -> OrderedDict[str, Any]:
    surface = get_mapping(surface_value)
    return OrderedDict(
        [
            ("surface_id", choose_text(surface.get("id"))),
            ("label", choose_text(surface.get("label"))),
            ("role", choose_text(surface.get("role"))),
            ("summary_schema", choose_text(surface.get("summary_schema"))),
            ("summary_kind", ""),
            ("summary_path", normalize_optional_path(surface.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(surface.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(surface.get("check_text_path"))),
            ("route_id", ""),
            ("depth", None),
            ("source", source),
        ]
    )


def front_page_surface_from_ref(surface: dict[str, Any], surface_id: str, role: str) -> OrderedDict[str, str]:
    label = choose_text(surface.get("label")) or choose_text(surface.get("surface_id")) or surface_id
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", label),
            ("role", role),
            ("summary_schema", choose_text(surface.get("summary_schema"))),
            ("summary_path", normalize_optional_path(surface.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(surface.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(surface.get("check_text_path"))),
        ]
    )


def find_route_entry(route_summary: dict[str, Any], surface_id: str, prefer_depth: int | None = None) -> OrderedDict[str, Any] | None:
    candidates = [
        surface_from_route_entry(entry, "route_entry")
        for entry in get_list(route_summary.get("route_entries"))
        if choose_text(get_mapping(entry).get("surface_id")) == surface_id
    ]
    if prefer_depth is not None:
        for candidate in candidates:
            if candidate["depth"] == prefer_depth:
                return candidate
    return candidates[0] if candidates else None


def route_level1_surfaces(route_summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    return [
        surface_from_route_entry(entry, "route_level1")
        for entry in get_list(route_summary.get("route_entries"))
        if int(get_mapping(entry).get("depth", -1)) == 1
    ]


def find_front_page_surface(summary: dict[str, Any], surface_id: str) -> OrderedDict[str, Any] | None:
    front_page = get_mapping(summary.get("front_page"))
    for surface_value in get_list(front_page.get("supporting_surfaces")):
        if choose_text(get_mapping(surface_value).get("id")) == surface_id:
            return surface_from_front_page_surface(surface_value, "route_compare_front_page")
    return None


def choose_route_explain_surface(route_summary: dict[str, Any]) -> tuple[OrderedDict[str, Any], str, str, list[OrderedDict[str, Any]]]:
    root_surface = get_mapping(route_summary.get("root_surface"))
    root_schema = choose_text(root_surface.get("summary_schema"))
    level1_surfaces = route_level1_surfaces(route_summary)

    if root_schema == OPENING_TESTIMONY_LANDING_SCHEMA:
        selected = find_route_entry(route_summary, "source_open_event_witness", prefer_depth=1) or make_empty_surface()
        return (
            selected,
            "route_landing_default",
            "Open the source open-event witness declared by this opening testimony landing route.",
            [surface for surface in level1_surfaces if surface["surface_id"] != selected["surface_id"]],
        )

    if root_schema == OPENING_TESTIMONY_LANDING_COMPARE_SCHEMA:
        selected = find_route_entry(route_summary, "candidate_opening_testimony_landing", prefer_depth=1) or make_empty_surface()
        supporting = [
            surface
            for surface in level1_surfaces
            if surface["surface_id"] in {"baseline_opening_testimony_landing", "candidate_opening_testimony_landing"}
            and surface["surface_id"] != selected["surface_id"]
        ]
        return (
            selected,
            "route_landing_compare_default",
            "Open the candidate opening testimony landing from this landing-compare route.",
            supporting,
        )

    if root_schema == OPENING_TESTIMONY_EXPLAIN_ENTRY_COMPARE_SCHEMA:
        selected = find_route_entry(route_summary, "candidate_opening_testimony_explain_entry", prefer_depth=1) or make_empty_surface()
        supporting = [
            surface
            for surface in level1_surfaces
            if surface["surface_id"] in {
                "baseline_opening_testimony_explain_entry",
                "candidate_opening_testimony_explain_entry",
            }
            and surface["surface_id"] != selected["surface_id"]
        ]
        return (
            selected,
            "route_explain_entry_compare_default",
            "Open the candidate opening testimony explain entry from this explain-entry-compare route.",
            supporting,
        )

    return (
        make_empty_surface(),
        "blocked",
        f"Unsupported route root for opening testimony explain entry: {root_schema}",
        level1_surfaces,
    )


def candidate_route_path(route_compare_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(route_compare_summary.get("artifact_context"))
    return normalize_optional_path(artifact_context.get("candidate_route_summary_path"))


def choose_added_or_changed_surface_id(route_compare_summary: dict[str, Any]) -> str:
    route_changes = get_mapping(route_compare_summary.get("route_changes"))
    level1_changes = get_mapping(route_changes.get("level1_surface_changes"))
    added = [choose_text(item) for item in get_list(level1_changes.get("added")) if choose_text(item)]
    if "candidate_opening_testimony_landing" in added:
        return "candidate_opening_testimony_landing"
    if added:
        return added[0]

    for change_value in get_list(route_compare_summary.get("entry_changes")):
        change = get_mapping(change_value)
        if choose_text(change.get("change_kind")) not in {"added", "changed"}:
            continue
        surface_id = choose_text(change.get("surface_id"))
        if surface_id and surface_id != "root" and choose_text(change.get("candidate_summary_path")):
            return surface_id
    return ""


def choose_route_compare_explain_surface(
    route_compare_summary: dict[str, Any],
) -> tuple[OrderedDict[str, Any], str, str, list[OrderedDict[str, Any]]]:
    verdict = choose_text(route_compare_summary.get("route_verdict"))
    baseline_route = find_front_page_surface(route_compare_summary, "baseline_route")
    candidate_route = find_front_page_surface(route_compare_summary, "candidate_route")
    route_surfaces = [surface for surface in [baseline_route, candidate_route] if surface is not None]

    if verdict == "collapsed":
        return (
            make_empty_surface(),
            "blocked",
            "Route compare collapsed, so no default opening testimony explain surface is trusted.",
            route_surfaces,
        )

    if verdict == "standing":
        selected = candidate_route or make_empty_surface()
        supporting = [surface for surface in route_surfaces if surface["surface_id"] != selected["surface_id"]]
        return (
            selected,
            "route_compare_candidate_root",
            "Open the candidate route root because the opening testimony route still stands.",
            supporting,
        )

    if verdict in {"improved", "drifted"}:
        candidate_path = candidate_route_path(route_compare_summary)
        selected_surface_id = choose_added_or_changed_surface_id(route_compare_summary)
        selected = make_empty_surface()
        if candidate_path and selected_surface_id:
            candidate_summary = load_route_summary(candidate_path)
            selected = find_route_entry(candidate_summary, selected_surface_id, prefer_depth=1) or make_empty_surface()
            if selected["surface_id"]:
                selected["source"] = "route_compare_candidate_change"
        supporting = [surface for surface in route_surfaces if surface["surface_id"] != selected["surface_id"]]
        return (
            selected,
            "route_compare_candidate_change",
            "Open the candidate changed or newly added surface selected by the route compare.",
            supporting,
        )

    return (
        make_empty_surface(),
        "blocked",
        f"Unsupported route compare verdict for opening testimony explain entry: {verdict}",
        route_surfaces,
    )


def build_source_route_ref(source_summary: dict[str, Any], source_summary_path: Path) -> OrderedDict[str, str]:
    schema = choose_text(source_summary.get("schema"))
    artifact_context = get_mapping(source_summary.get("artifact_context"))
    if schema == ROUTE_SCHEMA:
        root_surface = get_mapping(source_summary.get("root_surface"))
        return OrderedDict(
            [
                ("summary_schema", ROUTE_SCHEMA),
                ("summary_kind", ROUTE_KIND),
                ("summary_path", normalize_path(source_summary_path)),
                ("report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
                ("check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
                ("root_summary_schema", choose_text(root_surface.get("summary_schema"))),
                ("route_verdict", ""),
                ("baseline_route_summary_path", ""),
                ("candidate_route_summary_path", ""),
            ]
        )

    route_status = get_mapping(source_summary.get("route_status"))
    return OrderedDict(
        [
            ("summary_schema", ROUTE_COMPARE_SCHEMA),
            ("summary_kind", ROUTE_COMPARE_KIND),
            ("summary_path", normalize_path(source_summary_path)),
            ("report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("root_summary_schema", choose_text(route_status.get("candidate_root_summary_schema"))),
            ("route_verdict", choose_text(source_summary.get("route_verdict"))),
            ("baseline_route_summary_path", normalize_optional_path(artifact_context.get("baseline_route_summary_path"))),
            ("candidate_route_summary_path", normalize_optional_path(artifact_context.get("candidate_route_summary_path"))),
        ]
    )


def surface_is_ready(surface: dict[str, Any]) -> bool:
    summary_path = choose_text(surface.get("summary_path"))
    return bool(summary_path and Path(summary_path).exists())


def build_violations(source_summary: dict[str, Any], selected_surface: dict[str, Any], selection_kind: str) -> list[str]:
    violations: list[str] = []
    if choose_text(source_summary.get("result")) != "ok":
        violations.append("source route result is not ok")
    if selection_kind == "blocked":
        schema = choose_text(source_summary.get("schema"))
        if schema == ROUTE_COMPARE_SCHEMA and choose_text(source_summary.get("route_verdict")) == "collapsed":
            violations.append("source route compare verdict is collapsed")
        else:
            violations.append("no supported opening testimony explain surface could be selected")
    if not choose_text(selected_surface.get("surface_id")):
        violations.append("selected explain surface is missing")
    if not choose_text(selected_surface.get("summary_path")):
        violations.append("selected explain surface summary_path is missing")
    elif not Path(choose_text(selected_surface.get("summary_path"))).exists():
        violations.append(f"selected explain surface summary_path is not found: {selected_surface['summary_path']}")
    return list(dict.fromkeys(violations))


def build_next_questions(source_schema: str, selected_surface: dict[str, Any]) -> list[OrderedDict[str, str]]:
    selected_ref = choose_text(selected_surface.get("surface_id")) or "selected_surface"
    route_question = (
        "Inspect the route compare that selected this explain surface."
        if source_schema == ROUTE_COMPARE_SCHEMA
        else "Inspect the route that selected this explain surface."
    )
    return [
        OrderedDict(
            [
                ("kind", "inspect_selected_surface"),
                ("summary", "Open the selected explain surface for this opening testimony route."),
                ("target_ref", f"selected_surface.{selected_ref}"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_source_route"),
                ("summary", route_question),
                ("target_ref", "source_route_ref.summary_path"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_supporting_surfaces"),
                ("summary", "Inspect the supporting route surfaces preserved by this explain entry decision."),
                ("target_ref", "supporting_surfaces"),
            ]
        ),
    ]


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    source_ref: dict[str, Any],
    selected_surface: dict[str, Any],
    supporting_surfaces: list[OrderedDict[str, Any]],
    ready: bool,
) -> OrderedDict[str, Any]:
    front_page_surfaces: list[OrderedDict[str, str]] = [
        OrderedDict(
            [
                ("id", "source_route"),
                ("label", "source front-page route"),
                ("role", "source_route"),
                ("summary_schema", choose_text(source_ref.get("summary_schema"))),
                ("summary_path", choose_text(source_ref.get("summary_path"))),
                ("report_markdown_path", choose_text(source_ref.get("report_markdown_path"))),
                ("check_text_path", choose_text(source_ref.get("check_text_path"))),
            ]
        )
    ]
    if ready:
        front_page_surfaces.append(front_page_surface_from_ref(selected_surface, "selected_explain_surface", "selected_explain_surface"))
    for index, surface in enumerate(supporting_surfaces):
        if not choose_text(surface.get("summary_path")):
            continue
        front_page_surfaces.append(
            front_page_surface_from_ref(surface, f"supporting_explain_surface_{index}", "supporting_explain_surface")
        )

    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", front_page_surfaces),
        ]
    )


def build_summary_model(
    source_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    source_summary = load_source_summary(source_summary_path)
    source_schema = choose_text(source_summary.get("schema"))
    if source_schema == ROUTE_SCHEMA:
        selected_surface, selection_kind, reason_summary, supporting_surfaces = choose_route_explain_surface(source_summary)
    else:
        selected_surface, selection_kind, reason_summary, supporting_surfaces = choose_route_compare_explain_surface(source_summary)
    supporting_surfaces = ordered_unique_surfaces(supporting_surfaces)
    source_ref = build_source_route_ref(source_summary, source_summary_path)
    violations = build_violations(source_summary, selected_surface, selection_kind)
    decision_status = "blocked" if violations else "ready"
    result = "ok" if decision_status == "ready" else "fail"

    return OrderedDict(
        [
            ("schema", EXPLAIN_ENTRY_SCHEMA),
            ("kind", EXPLAIN_ENTRY_KIND),
            ("generated_at_utc", datetime.utcnow().replace(microsecond=0).isoformat() + "Z"),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_testimony_explain_entry.py"),
            ("result", result),
            (
                "opening_testimony_explain_entry",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Explain Entry"),
                        (
                            "summary",
                            "A thin default explain-entry decision projected from an opening testimony route or route compare.",
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
                    selected_surface=selected_surface,
                    supporting_surfaces=supporting_surfaces,
                    ready=decision_status == "ready",
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_summary_path", normalize_path(source_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("explain_entry_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("source_route_ref", source_ref),
            (
                "explain_entry_decision",
                OrderedDict(
                    [
                        ("status", decision_status),
                        ("selection_kind", selection_kind),
                        ("selected_entry_id", choose_text(selected_surface.get("surface_id"))),
                        ("selected_tab_id", "opening_testimony_explain"),
                        ("selected_role", "opening_testimony_explain_entry"),
                        ("selected_source", choose_text(selected_surface.get("source"))),
                    ]
                ),
            ),
            ("selected_surface", selected_surface),
            ("supporting_surfaces", supporting_surfaces),
            (
                "opening_reason",
                OrderedDict(
                    [
                        (
                            "kind",
                            "route_compare_default_explain_surface"
                            if source_schema == ROUTE_COMPARE_SCHEMA
                            else "route_default_explain_surface",
                        ),
                        ("summary", reason_summary),
                        ("source_summary_path", normalize_path(source_summary_path)),
                    ]
                ),
            ),
            ("next_questions", build_next_questions(source_schema, selected_surface)),
            ("violations", violations),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    decision = summary["explain_entry_decision"]
    selected = summary["selected_surface"]
    source_ref = summary["source_route_ref"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Explain Entry",
        "",
        f"- Result: `{summary['result']}`",
        f"- Decision: `{decision['status']}`",
        f"- Selection kind: `{decision['selection_kind']}`",
        f"- Source route: `{source_ref['summary_path']}`",
        f"- Selected surface: `{selected['surface_id']}` role=`{selected['role']}`",
        f"- Selected summary: `{selected['summary_path']}`",
        "",
        "## Opening Reason",
        summary["opening_reason"]["summary"],
        "",
        "## Supporting Surfaces",
    ]
    for surface in summary["supporting_surfaces"]:
        lines.append(
            "- `{0}` role=`{1}` schema=`{2}` path=`{3}`".format(
                surface["surface_id"],
                surface["role"],
                surface["summary_schema"],
                surface["summary_path"],
            )
        )
    if not summary["supporting_surfaces"]:
        lines.append("- none")

    lines.extend(["", "## Next Questions"])
    for question in summary["next_questions"]:
        lines.append(f"- `{question['kind']}`: {question['summary']}")

    if summary["violations"]:
        lines.extend(["", "## Violations"])
        for violation in summary["violations"]:
            lines.append(f"- {violation}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    decision = summary["explain_entry_decision"]
    selected = summary["selected_surface"]
    return "\n".join(
        [
            f"source_summary_path: {summary['artifact_context']['source_summary_path']}",
            f"explain_entry_summary_path: {summary['artifact_context']['explain_entry_summary_path']}",
            f"result: {summary['result']}",
            f"decision_status: {decision['status']}",
            f"selection_kind: {decision['selection_kind']}",
            f"selected_surface_id: {selected['surface_id']}",
            f"selected_summary_schema: {selected['summary_schema']}",
            f"selected_summary_path: {selected['summary_path']}",
            f"supporting_surface_count: {len(summary['supporting_surfaces'])}",
            f"violation_count: {len(summary['violations'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export an opening testimony explain-entry decision from a front-page route or route compare."
    )
    parser.add_argument("--source-summary", required=True, help="Source front-page route or route compare summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for explain-entry artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for explain-entry summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for explain-entry markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for explain-entry check text.")
    args = parser.parse_args()

    source_summary_path = Path(args.source_summary).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-testimony.explain-entry.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-testimony.explain-entry.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-testimony.explain-entry.check.txt")

    summary = build_summary_model(
        source_summary_path=source_summary_path,
        output_root=output_root,
        summary_path=summary_path,
        report_path=report_path,
        check_path=check_path,
    )
    write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    write_text(report_path, build_report(summary))
    write_text(check_path, build_check(summary))

    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY] summary={summary_path}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY] status={0} selected={1} kind={2}".format(
            summary["explain_entry_decision"]["status"],
            summary["selected_surface"]["surface_id"],
            summary["explain_entry_decision"]["selection_kind"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
