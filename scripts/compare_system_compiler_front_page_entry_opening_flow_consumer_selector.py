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


SELECTOR_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_selector/v0"
SELECTOR_KIND = "system_compiler.front_page_entry_opening_flow_consumer_selector"
COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_selector_compare/v0"
COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_consumer_selector_compare"


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


def string_array_changes(left: list[str], right: list[str]) -> OrderedDict[str, list[str]]:
    left_values = ordered_unique(left)
    right_values = ordered_unique(right)
    left_set = set(left_values)
    right_set = set(right_values)
    return OrderedDict(
        [
            ("added", [value for value in right_values if value not in left_set]),
            ("removed", [value for value in left_values if value not in right_set]),
        ]
    )


def has_array_changes(change: dict[str, list[str]]) -> bool:
    return bool(change.get("added") or change.get("removed"))


def count_change(baseline: int, candidate: int) -> OrderedDict[str, int]:
    return OrderedDict(
        [
            ("baseline", baseline),
            ("candidate", candidate),
            ("delta", candidate - baseline),
        ]
    )


def normalize_path_for_compare(path_value: str, root_value: str) -> str:
    path_text = choose_text(path_value)
    root_text = choose_text(root_value)
    if not path_text:
        return ""
    if not root_text:
        return normalize_path(path_text)

    try:
        path = Path(path_text).resolve()
        root = Path(root_text).resolve()
        return normalize_path(path.relative_to(root))
    except Exception:
        return normalize_path(path_text)


def load_selector_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != SELECTOR_SCHEMA:
        raise ValueError(f"unsupported front page entry opening-flow consumer selector schema: {path}")
    if choose_text(summary.get("kind")) != SELECTOR_KIND:
        raise ValueError(f"unsupported front page entry opening-flow consumer selector kind: {path}")
    return summary


def get_selector_workspace_root(selector_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(selector_summary.get("artifact_context"))
    source_consumer_summary_path = choose_text(artifact_context.get("source_consumer_summary_path"))
    if not source_consumer_summary_path:
        return choose_text(artifact_context.get("output_root"))

    try:
        source_path = Path(source_consumer_summary_path).resolve()
        if source_path.parent.name.lower() == "consumer":
            return normalize_path(source_path.parent.parent)
        return normalize_path(source_path.parent)
    except Exception:
        return choose_text(artifact_context.get("output_root"))


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


def build_selector_surface(
    selector_summary: dict[str, Any],
    selector_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    front_page = get_mapping(selector_summary.get("front_page"))
    artifact_context = get_mapping(selector_summary.get("artifact_context"))
    return make_surface(
        surface_id=surface_id,
        label=f"{role.replace('_', ' ')}: opening-flow consumer selector",
        role=role,
        summary_schema=SELECTOR_SCHEMA,
        summary_path=normalize_path(selector_summary_path),
        report_markdown_path=normalize_path(
            choose_text(front_page.get("report_markdown_path"))
            or choose_text(artifact_context.get("report_markdown_path"))
        ),
        check_text_path=normalize_path(
            choose_text(front_page.get("check_text_path")) or choose_text(artifact_context.get("check_text_path"))
        ),
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    supporting_surfaces: list[OrderedDict[str, str]],
) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", supporting_surfaces),
        ]
    )


def build_selector_provenance_entry(
    selector_summary: dict[str, Any],
    selector_summary_path: Path,
    provenance_id: str,
    selector_role: str,
) -> OrderedDict[str, Any]:
    front_page = get_mapping(selector_summary.get("front_page"))
    artifact_context = get_mapping(selector_summary.get("artifact_context"))
    status = get_mapping(selector_summary.get("selector_status"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("selector_role", selector_role),
            ("source_summary_schema", SELECTOR_SCHEMA),
            ("source_summary_path", normalize_path(selector_summary_path)),
            (
                "source_report_markdown_path",
                normalize_path(
                    choose_text(front_page.get("report_markdown_path"))
                    or choose_text(artifact_context.get("report_markdown_path"))
                ),
            ),
            (
                "source_check_text_path",
                normalize_path(
                    choose_text(front_page.get("check_text_path"))
                    or choose_text(artifact_context.get("check_text_path"))
                ),
            ),
            ("result", choose_text(selector_summary.get("result"))),
            ("open_plan_status", choose_text(status.get("open_plan_status"))),
            ("selected_entry_count", int(status.get("selected_entry_count", 0))),
            ("fallback_entry_count", int(status.get("fallback_entry_count", 0))),
            ("default_entry_name", choose_text(status.get("default_entry_name"))),
            ("compare_entry_name", choose_text(status.get("compare_entry_name"))),
        ]
    )


def normalize_selector_entries(selector_summary: dict[str, Any]) -> OrderedDict[str, OrderedDict[str, Any]]:
    selector_root = get_selector_workspace_root(selector_summary)
    entries: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    open_plan = get_mapping(selector_summary.get("open_plan"))
    for value in get_list(open_plan.get("ordered_entries")):
        entry = get_mapping(value)
        name = choose_text(entry.get("name"))
        if not name:
            continue
        target_summary_path = choose_text(entry.get("target_summary_path"))
        opener_summary_path = choose_text(entry.get("opener_summary_path"))
        entries[name] = OrderedDict(
            [
                ("name", name),
                ("rank", int(entry.get("rank", 0))),
                ("selected_tab_id", choose_text(entry.get("selected_tab_id"))),
                ("selected_role", choose_text(entry.get("selected_role"))),
                ("query_kind", choose_text(entry.get("query_kind"))),
                ("query_scope", choose_text(entry.get("query_scope"))),
                ("target_summary_schema", choose_text(entry.get("target_summary_schema"))),
                ("target_summary_kind", choose_text(entry.get("target_summary_kind"))),
                ("target_summary_path", target_summary_path),
                ("target_summary_compare_path", normalize_path_for_compare(target_summary_path, selector_root)),
                ("projection_kind", choose_text(entry.get("projection_kind"))),
                ("compare_context_available", bool(entry.get("compare_context_available"))),
                ("landing_verdict", choose_text(entry.get("landing_verdict"))),
                ("inspector_ready", bool(entry.get("inspector_ready"))),
                ("inspector_mode", choose_text(entry.get("inspector_mode"))),
                (
                    "inspector_blockers",
                    ordered_unique([choose_text(item) for item in get_list(entry.get("inspector_blockers"))]),
                ),
                ("opener_summary_path", opener_summary_path),
                ("opener_summary_compare_path", normalize_path_for_compare(opener_summary_path, selector_root)),
                ("opener_report_markdown_path", choose_text(entry.get("opener_report_markdown_path"))),
                ("opener_check_text_path", choose_text(entry.get("opener_check_text_path"))),
            ]
        )
    return entries


def build_selector_status(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
) -> OrderedDict[str, Any]:
    baseline_status = get_mapping(baseline_summary.get("selector_status"))
    candidate_status = get_mapping(candidate_summary.get("selector_status"))
    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_summary.get("result"))),
            ("candidate_result", choose_text(candidate_summary.get("result"))),
            ("baseline_open_plan_status", choose_text(baseline_status.get("open_plan_status"))),
            ("candidate_open_plan_status", choose_text(candidate_status.get("open_plan_status"))),
            ("baseline_selected_entry_count", int(baseline_status.get("selected_entry_count", 0))),
            ("candidate_selected_entry_count", int(candidate_status.get("selected_entry_count", 0))),
            ("baseline_fallback_entry_count", int(baseline_status.get("fallback_entry_count", 0))),
            ("candidate_fallback_entry_count", int(candidate_status.get("fallback_entry_count", 0))),
            ("baseline_default_entry_name", choose_text(baseline_status.get("default_entry_name"))),
            ("candidate_default_entry_name", choose_text(candidate_status.get("default_entry_name"))),
            ("baseline_compare_entry_name", choose_text(baseline_status.get("compare_entry_name"))),
            ("candidate_compare_entry_name", choose_text(candidate_status.get("compare_entry_name"))),
        ]
    )


def build_selector_changes(
    selector_status: dict[str, Any],
    baseline_entries: OrderedDict[str, OrderedDict[str, Any]],
    candidate_entries: OrderedDict[str, OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    baseline_order = list(baseline_entries.keys())
    candidate_order = list(candidate_entries.keys())
    entry_changes = string_array_changes(baseline_order, candidate_order)
    order_changed = baseline_order != candidate_order
    changed = (
        selector_status.get("baseline_result") != selector_status.get("candidate_result")
        or selector_status.get("baseline_open_plan_status") != selector_status.get("candidate_open_plan_status")
        or selector_status.get("baseline_default_entry_name") != selector_status.get("candidate_default_entry_name")
        or selector_status.get("baseline_compare_entry_name") != selector_status.get("candidate_compare_entry_name")
        or selector_status.get("baseline_selected_entry_count") != selector_status.get("candidate_selected_entry_count")
        or selector_status.get("baseline_fallback_entry_count") != selector_status.get("candidate_fallback_entry_count")
        or has_array_changes(entry_changes)
        or order_changed
    )
    return OrderedDict(
        [
            ("result_changed", bool(selector_status.get("baseline_result") != selector_status.get("candidate_result"))),
            (
                "open_plan_status_changed",
                bool(
                    selector_status.get("baseline_open_plan_status")
                    != selector_status.get("candidate_open_plan_status")
                ),
            ),
            (
                "selected_entry_count_change",
                count_change(
                    int(selector_status.get("baseline_selected_entry_count", 0)),
                    int(selector_status.get("candidate_selected_entry_count", 0)),
                ),
            ),
            (
                "fallback_entry_count_change",
                count_change(
                    int(selector_status.get("baseline_fallback_entry_count", 0)),
                    int(selector_status.get("candidate_fallback_entry_count", 0)),
                ),
            ),
            (
                "default_entry_change",
                OrderedDict(
                    [
                        ("baseline", choose_text(selector_status.get("baseline_default_entry_name"))),
                        ("candidate", choose_text(selector_status.get("candidate_default_entry_name"))),
                        (
                            "changed",
                            bool(
                                selector_status.get("baseline_default_entry_name")
                                != selector_status.get("candidate_default_entry_name")
                            ),
                        ),
                    ]
                ),
            ),
            (
                "compare_entry_change",
                OrderedDict(
                    [
                        ("baseline", choose_text(selector_status.get("baseline_compare_entry_name"))),
                        ("candidate", choose_text(selector_status.get("candidate_compare_entry_name"))),
                        (
                            "changed",
                            bool(
                                selector_status.get("baseline_compare_entry_name")
                                != selector_status.get("candidate_compare_entry_name")
                            ),
                        ),
                    ]
                ),
            ),
            ("ordered_entry_name_changes", entry_changes),
            ("ordered_entry_order_changed", order_changed),
            ("changed", changed),
        ]
    )


def compare_entry_field(
    notes: list[str],
    field_name: str,
    baseline_entry: dict[str, Any],
    candidate_entry: dict[str, Any],
) -> None:
    if baseline_entry.get(field_name) != candidate_entry.get(field_name):
        notes.append(
            "{0}: {1} -> {2}".format(
                field_name,
                baseline_entry.get(field_name),
                candidate_entry.get(field_name),
            )
        )


def classify_changed_entry(
    baseline_entry: dict[str, Any],
    candidate_entry: dict[str, Any],
) -> tuple[str, list[str]]:
    notes: list[str] = []
    for field_name in (
        "rank",
        "selected_tab_id",
        "selected_role",
        "query_kind",
        "query_scope",
        "target_summary_schema",
        "target_summary_kind",
        "target_summary_compare_path",
        "projection_kind",
        "compare_context_available",
        "landing_verdict",
        "inspector_ready",
        "inspector_mode",
        "inspector_blockers",
        "opener_summary_compare_path",
    ):
        compare_entry_field(notes, field_name, baseline_entry, candidate_entry)

    impact = "neutral"
    if bool(baseline_entry.get("compare_context_available")) and not bool(
        candidate_entry.get("compare_context_available")
    ):
        impact = "regression"
    if bool(baseline_entry.get("inspector_ready")) and not bool(candidate_entry.get("inspector_ready")):
        impact = "regression"
    if choose_text(baseline_entry.get("projection_kind")) and not choose_text(candidate_entry.get("projection_kind")):
        impact = "regression"

    if impact == "neutral":
        if not bool(baseline_entry.get("compare_context_available")) and bool(
            candidate_entry.get("compare_context_available")
        ):
            impact = "improvement"
        if not bool(baseline_entry.get("inspector_ready")) and bool(candidate_entry.get("inspector_ready")):
            impact = "improvement"
        if not choose_text(baseline_entry.get("projection_kind")) and choose_text(candidate_entry.get("projection_kind")):
            impact = "improvement"

    return impact, notes


def build_entry_change_record(
    name: str,
    change_kind: str,
    impact: str,
    baseline_entry: dict[str, Any] | None,
    candidate_entry: dict[str, Any] | None,
    change_notes: list[str],
) -> OrderedDict[str, Any]:
    baseline = baseline_entry or {}
    candidate = candidate_entry or {}
    return OrderedDict(
        [
            ("name", name),
            ("change_kind", change_kind),
            ("impact", impact),
            ("baseline_rank", int(baseline.get("rank", -1))),
            ("candidate_rank", int(candidate.get("rank", -1))),
            ("baseline_selected_tab_id", choose_text(baseline.get("selected_tab_id"))),
            ("candidate_selected_tab_id", choose_text(candidate.get("selected_tab_id"))),
            ("baseline_query_kind", choose_text(baseline.get("query_kind"))),
            ("candidate_query_kind", choose_text(candidate.get("query_kind"))),
            ("baseline_query_scope", choose_text(baseline.get("query_scope"))),
            ("candidate_query_scope", choose_text(candidate.get("query_scope"))),
            ("baseline_target_summary_schema", choose_text(baseline.get("target_summary_schema"))),
            ("candidate_target_summary_schema", choose_text(candidate.get("target_summary_schema"))),
            ("baseline_target_summary_kind", choose_text(baseline.get("target_summary_kind"))),
            ("candidate_target_summary_kind", choose_text(candidate.get("target_summary_kind"))),
            ("baseline_target_summary_path", choose_text(baseline.get("target_summary_path"))),
            ("candidate_target_summary_path", choose_text(candidate.get("target_summary_path"))),
            ("baseline_projection_kind", choose_text(baseline.get("projection_kind"))),
            ("candidate_projection_kind", choose_text(candidate.get("projection_kind"))),
            ("baseline_compare_context_available", bool(baseline.get("compare_context_available"))),
            ("candidate_compare_context_available", bool(candidate.get("compare_context_available"))),
            ("baseline_landing_verdict", choose_text(baseline.get("landing_verdict"))),
            ("candidate_landing_verdict", choose_text(candidate.get("landing_verdict"))),
            ("baseline_inspector_ready", bool(baseline.get("inspector_ready"))),
            ("candidate_inspector_ready", bool(candidate.get("inspector_ready"))),
            ("baseline_opener_summary_path", choose_text(baseline.get("opener_summary_path"))),
            ("candidate_opener_summary_path", choose_text(candidate.get("opener_summary_path"))),
            ("change_notes", ordered_unique(change_notes)),
        ]
    )


def build_entry_changes(
    baseline_entries: OrderedDict[str, OrderedDict[str, Any]],
    candidate_entries: OrderedDict[str, OrderedDict[str, Any]],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    changes: list[OrderedDict[str, Any]] = []
    summary = OrderedDict(
        [
            ("baseline_entry_count", len(baseline_entries)),
            ("candidate_entry_count", len(candidate_entries)),
            ("changed_entry_count", 0),
            ("added_entry_count", 0),
            ("removed_entry_count", 0),
            ("unchanged_entry_count", 0),
            ("regression_count", 0),
            ("improvement_count", 0),
            ("neutral_change_count", 0),
            ("order_changed_count", 0),
            ("projection_changed_count", 0),
            ("compare_context_lost_count", 0),
            ("compare_context_gained_count", 0),
            ("inspector_readiness_changed_count", 0),
            ("query_changed_count", 0),
            ("target_changed_count", 0),
        ]
    )

    ordered_names = ordered_unique(list(baseline_entries.keys()) + list(candidate_entries.keys()))
    for name in ordered_names:
        baseline_entry = baseline_entries.get(name)
        candidate_entry = candidate_entries.get(name)
        if baseline_entry is None and candidate_entry is not None:
            impact = "improvement" if choose_text(candidate_entry.get("projection_kind")) else "neutral"
            changes.append(build_entry_change_record(name, "added", impact, None, candidate_entry, ["entry added"]))
            summary["added_entry_count"] += 1
        elif baseline_entry is not None and candidate_entry is None:
            impact = "regression" if choose_text(baseline_entry.get("projection_kind")) else "neutral"
            changes.append(build_entry_change_record(name, "removed", impact, baseline_entry, None, ["entry removed"]))
            summary["removed_entry_count"] += 1
        elif baseline_entry is not None and candidate_entry is not None:
            impact, notes = classify_changed_entry(baseline_entry, candidate_entry)
            if notes:
                changes.append(build_entry_change_record(name, "changed", impact, baseline_entry, candidate_entry, notes))
                summary["changed_entry_count"] += 1
            else:
                summary["unchanged_entry_count"] += 1

            if int(baseline_entry.get("rank", -1)) != int(candidate_entry.get("rank", -1)):
                summary["order_changed_count"] += 1
            if baseline_entry.get("projection_kind") != candidate_entry.get("projection_kind"):
                summary["projection_changed_count"] += 1
            if bool(baseline_entry.get("compare_context_available")) and not bool(
                candidate_entry.get("compare_context_available")
            ):
                summary["compare_context_lost_count"] += 1
            if not bool(baseline_entry.get("compare_context_available")) and bool(
                candidate_entry.get("compare_context_available")
            ):
                summary["compare_context_gained_count"] += 1
            if bool(baseline_entry.get("inspector_ready")) != bool(candidate_entry.get("inspector_ready")):
                summary["inspector_readiness_changed_count"] += 1
            if (
                baseline_entry.get("query_kind") != candidate_entry.get("query_kind")
                or baseline_entry.get("query_scope") != candidate_entry.get("query_scope")
            ):
                summary["query_changed_count"] += 1
            if (
                baseline_entry.get("target_summary_schema") != candidate_entry.get("target_summary_schema")
                or baseline_entry.get("target_summary_kind") != candidate_entry.get("target_summary_kind")
                or baseline_entry.get("target_summary_compare_path") != candidate_entry.get("target_summary_compare_path")
            ):
                summary["target_changed_count"] += 1

        if changes and changes[-1].get("name") == name:
            impact_value = changes[-1].get("impact")
            if impact_value == "regression":
                summary["regression_count"] += 1
            elif impact_value == "improvement":
                summary["improvement_count"] += 1
            else:
                summary["neutral_change_count"] += 1

    return changes, summary


def build_regression_surface(
    selector_status: dict[str, Any],
    selector_changes: dict[str, Any],
    entry_changes: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    removed_entries: list[str] = []
    lost_compare_context_entries: list[str] = []
    lost_inspector_ready_entries: list[str] = []
    narratives: list[str] = []

    for change in entry_changes:
        name = choose_text(change.get("name"))
        if change.get("change_kind") == "removed":
            removed_entries.append(name)
        if bool(change.get("baseline_compare_context_available")) and not bool(
            change.get("candidate_compare_context_available")
        ):
            lost_compare_context_entries.append(name)
        if bool(change.get("baseline_inspector_ready")) and not bool(change.get("candidate_inspector_ready")):
            lost_inspector_ready_entries.append(name)

    failed_result_transition = (
        choose_text(selector_status.get("baseline_result")) == "ok"
        and choose_text(selector_status.get("candidate_result")) != "ok"
    )
    blocked_plan_transition = (
        choose_text(selector_status.get("baseline_open_plan_status")) == "ready"
        and choose_text(selector_status.get("candidate_open_plan_status")) != "ready"
    )
    default_entry_changed = bool(get_mapping(selector_changes.get("default_entry_change")).get("changed"))
    compare_entry_changed = bool(get_mapping(selector_changes.get("compare_entry_change")).get("changed"))

    if failed_result_transition:
        narratives.append("candidate selector no longer reports ok")
    if blocked_plan_transition:
        narratives.append("candidate selector open plan is no longer ready")
    if default_entry_changed:
        narratives.append(
            "default explain entry changed: {0} -> {1}".format(
                selector_status.get("baseline_default_entry_name"),
                selector_status.get("candidate_default_entry_name"),
            )
        )
    if compare_entry_changed:
        narratives.append(
            "compare explain entry changed: {0} -> {1}".format(
                selector_status.get("baseline_compare_entry_name"),
                selector_status.get("candidate_compare_entry_name"),
            )
        )
    if removed_entries:
        narratives.append("candidate removed selected entries: {0}".format(", ".join(removed_entries)))
    if lost_compare_context_entries:
        narratives.append("candidate lost compare context: {0}".format(", ".join(lost_compare_context_entries)))
    if lost_inspector_ready_entries:
        narratives.append("candidate lost inspector readiness: {0}".format(", ".join(lost_inspector_ready_entries)))

    changed = bool(
        failed_result_transition
        or blocked_plan_transition
        or default_entry_changed
        or compare_entry_changed
        or removed_entries
        or lost_compare_context_entries
        or lost_inspector_ready_entries
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("default_entry_changed", default_entry_changed),
            ("compare_entry_changed", compare_entry_changed),
            ("removed_entry_names", ordered_unique(removed_entries)),
            ("lost_compare_context_entry_names", ordered_unique(lost_compare_context_entries)),
            ("lost_inspector_ready_entry_names", ordered_unique(lost_inspector_ready_entries)),
            ("failed_result_transition", failed_result_transition),
            ("blocked_plan_transition", blocked_plan_transition),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_selector_verdict(
    selector_changes: dict[str, Any],
    entry_summary: dict[str, int],
    regression_surface: dict[str, Any],
    selector_status: dict[str, Any],
) -> str:
    if choose_text(selector_status.get("candidate_result")) != "ok":
        return "collapsed"
    if choose_text(selector_status.get("candidate_open_plan_status")) != "ready":
        return "collapsed"
    if bool(regression_surface.get("changed")):
        return "drifted"
    if not bool(selector_changes.get("changed")) and int(entry_summary.get("changed_entry_count", 0)) == 0:
        return "standing"
    if (
        int(entry_summary.get("improvement_count", 0)) > 0
        or int(entry_summary.get("added_entry_count", 0)) > 0
        or int(entry_summary.get("compare_context_gained_count", 0)) > 0
        or int(selector_changes.get("selected_entry_count_change", {}).get("delta", 0)) > 0
    ):
        return "improved"
    return "drifted"


def build_questions(
    selector_verdict: str,
    regression_surface: dict[str, Any],
    entry_summary: dict[str, int],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if selector_verdict == "standing":
        compare_questions.append("Should this selector order become the canonical explain-open handoff?")
    elif selector_verdict == "improved":
        compare_questions.append("Which added selector entry should be promoted into the first explain surface?")
    elif selector_verdict == "drifted":
        compare_questions.append("Did selector drift change the first thing a reader sees?")
    else:
        compare_questions.append("Which selector dependency collapsed before a default explain entry could be trusted?")

    if bool(regression_surface.get("default_entry_changed")):
        next_questions.append("Should default-entry drift block publishing this front-page opening flow?")
    if bool(regression_surface.get("compare_entry_changed")):
        next_questions.append("Should compare-neighbor drift require a lower landing compare review?")
    if int(entry_summary.get("query_changed_count", 0)) > 0:
        next_questions.append("Should query drift be surfaced as a selector-level explain warning?")
    if int(entry_summary.get("target_changed_count", 0)) > 0:
        next_questions.append("Should target summary drift trigger a deeper opener compare?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this selector compare instead of diffing selector JSON?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_compare_summary_model(
    baseline_selector_path: Path,
    candidate_selector_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_selector_summary(baseline_selector_path)
    candidate_summary = load_selector_summary(candidate_selector_path)
    baseline_entries = normalize_selector_entries(baseline_summary)
    candidate_entries = normalize_selector_entries(candidate_summary)

    selector_status = build_selector_status(baseline_summary, candidate_summary)
    selector_changes = build_selector_changes(selector_status, baseline_entries, candidate_entries)
    entry_changes, entry_summary = build_entry_changes(baseline_entries, candidate_entries)
    regression_surface = build_regression_surface(selector_status, selector_changes, entry_changes)
    selector_verdict = build_selector_verdict(selector_changes, entry_summary, regression_surface, selector_status)

    supporting_surfaces = [
        build_selector_surface(
            baseline_summary,
            baseline_selector_path,
            "baseline_consumer_selector",
            "baseline_consumer_selector",
        ),
        build_selector_surface(
            candidate_summary,
            candidate_selector_path,
            "candidate_consumer_selector",
            "candidate_consumer_selector",
        ),
    ]

    return OrderedDict(
        [
            ("schema", COMPARE_SCHEMA),
            ("kind", COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py"),
            ("result", "ok"),
            (
                "opening_flow_consumer_selector_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Selector Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two opening-flow consumer selector witnesses preserve the same explain-open order.",
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
                    supporting_surfaces=supporting_surfaces,
                ),
            ),
            (
                "selector_provenance",
                [
                    build_selector_provenance_entry(
                        baseline_summary,
                        baseline_selector_path,
                        "baseline_consumer_selector",
                        "baseline_selector",
                    ),
                    build_selector_provenance_entry(
                        candidate_summary,
                        candidate_selector_path,
                        "candidate_consumer_selector",
                        "candidate_selector",
                    ),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_selector_summary_path", normalize_path(baseline_selector_path)),
                        ("candidate_selector_summary_path", normalize_path(candidate_selector_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("selector_verdict", selector_verdict),
            ("selector_status", selector_status),
            ("selector_changes", selector_changes),
            ("selector_entry_summary", entry_summary),
            ("selector_entry_changes", entry_changes),
            ("selector_regression_surface", regression_surface),
            ("questions", build_questions(selector_verdict, regression_surface, entry_summary)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    selector_status = summary["selector_status"]
    selector_changes = summary["selector_changes"]
    entry_summary = summary["selector_entry_summary"]
    regression_surface = summary["selector_regression_surface"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Selector Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Selector verdict: `{summary['selector_verdict']}`",
        f"- Baseline selector: `{summary['artifact_context']['baseline_selector_summary_path']}`",
        f"- Candidate selector: `{summary['artifact_context']['candidate_selector_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Selector Status",
        "- Baseline: `result={0} plan={1} selected={2} fallback={3} default={4} compare={5}`".format(
            selector_status["baseline_result"],
            selector_status["baseline_open_plan_status"],
            selector_status["baseline_selected_entry_count"],
            selector_status["baseline_fallback_entry_count"],
            selector_status["baseline_default_entry_name"],
            selector_status["baseline_compare_entry_name"],
        ),
        "- Candidate: `result={0} plan={1} selected={2} fallback={3} default={4} compare={5}`".format(
            selector_status["candidate_result"],
            selector_status["candidate_open_plan_status"],
            selector_status["candidate_selected_entry_count"],
            selector_status["candidate_fallback_entry_count"],
            selector_status["candidate_default_entry_name"],
            selector_status["candidate_compare_entry_name"],
        ),
        "",
        "## Selector Changes",
        "- selected_delta=`{0}` fallback_delta=`{1}` order_changed=`{2}`".format(
            selector_changes["selected_entry_count_change"]["delta"],
            selector_changes["fallback_entry_count_change"]["delta"],
            selector_changes["ordered_entry_order_changed"],
        ),
        "- default: `{0}` -> `{1}`".format(
            selector_changes["default_entry_change"]["baseline"],
            selector_changes["default_entry_change"]["candidate"],
        ),
        "- compare: `{0}` -> `{1}`".format(
            selector_changes["compare_entry_change"]["baseline"],
            selector_changes["compare_entry_change"]["candidate"],
        ),
        "",
        "## Entry Summary",
        "- entries baseline=`{0}` candidate=`{1}` changed=`{2}` added=`{3}` removed=`{4}` unchanged=`{5}`".format(
            entry_summary["baseline_entry_count"],
            entry_summary["candidate_entry_count"],
            entry_summary["changed_entry_count"],
            entry_summary["added_entry_count"],
            entry_summary["removed_entry_count"],
            entry_summary["unchanged_entry_count"],
        ),
        "- impacts regression=`{0}` improvement=`{1}` neutral=`{2}` order_changed=`{3}` query_changed=`{4}` target_changed=`{5}`".format(
            entry_summary["regression_count"],
            entry_summary["improvement_count"],
            entry_summary["neutral_change_count"],
            entry_summary["order_changed_count"],
            entry_summary["query_changed_count"],
            entry_summary["target_changed_count"],
        ),
    ]

    if summary["selector_entry_changes"]:
        lines.extend(["", "## Entry Changes"])
        for change in summary["selector_entry_changes"]:
            lines.append(
                "- `{0}` kind=`{1}` impact=`{2}` rank=`{3}->{4}` projection=`{5}->{6}` compare_context=`{7}->{8}` inspector_ready=`{9}->{10}`".format(
                    change["name"],
                    change["change_kind"],
                    change["impact"],
                    change["baseline_rank"],
                    change["candidate_rank"],
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


def build_check(summary: dict[str, Any]) -> str:
    selector_status = summary["selector_status"]
    entry_summary = summary["selector_entry_summary"]
    return "\n".join(
        [
            f"baseline_selector_summary_path: {summary['artifact_context']['baseline_selector_summary_path']}",
            f"candidate_selector_summary_path: {summary['artifact_context']['candidate_selector_summary_path']}",
            f"selector_verdict: {summary['selector_verdict']}",
            f"baseline_result: {selector_status['baseline_result']}",
            f"candidate_result: {selector_status['candidate_result']}",
            f"baseline_default_entry_name: {selector_status['baseline_default_entry_name']}",
            f"candidate_default_entry_name: {selector_status['candidate_default_entry_name']}",
            f"baseline_compare_entry_name: {selector_status['baseline_compare_entry_name']}",
            f"candidate_compare_entry_name: {selector_status['candidate_compare_entry_name']}",
            f"baseline_selected_entry_count: {selector_status['baseline_selected_entry_count']}",
            f"candidate_selected_entry_count: {selector_status['candidate_selected_entry_count']}",
            f"changed_entry_count: {entry_summary['changed_entry_count']}",
            f"added_entry_count: {entry_summary['added_entry_count']}",
            f"removed_entry_count: {entry_summary['removed_entry_count']}",
            f"regression_count: {entry_summary['regression_count']}",
            f"improvement_count: {entry_summary['improvement_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page entry opening-flow consumer selector summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline opening-flow consumer selector summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening-flow consumer selector summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for selector compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for selector compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for selector compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for selector compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(
        args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-selector-compare"
    ).resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(
        args.summary,
        output_root,
        "front-page.entry-opening-flow.consumer.selector.compare.summary.json",
    )
    report_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-opening-flow.consumer.selector.compare.report.md",
    )
    check_path = resolve_output_path(
        args.check_text,
        output_root,
        "front-page.entry-opening-flow.consumer.selector.compare.check.txt",
    )

    try:
        summary = build_compare_summary_model(
            baseline_selector_path=baseline_path,
            candidate_selector_path=candidate_path,
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

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE] verdict={summary['selector_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE] changed_entries={0}".format(
            summary["selector_entry_summary"]["changed_entry_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
