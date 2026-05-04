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


PLAN_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan/v0"
PLAN_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan"
COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_compare/v0"
COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_compare"


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


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
    return OrderedDict([("baseline", baseline), ("candidate", candidate), ("delta", candidate - baseline)])


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


def load_plan_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != PLAN_SCHEMA:
        raise ValueError(f"unsupported front page entry opening-flow consumer plan schema: {path}")
    if choose_text(summary.get("kind")) != PLAN_KIND:
        raise ValueError(f"unsupported front page entry opening-flow consumer plan kind: {path}")
    return summary


def get_plan_workspace_root(plan_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(plan_summary.get("artifact_context"))
    source_selector_summary_path = choose_text(artifact_context.get("source_selector_summary_path"))
    if not source_selector_summary_path:
        return choose_text(artifact_context.get("output_root"))

    try:
        source_path = Path(source_selector_summary_path).resolve()
        if source_path.parent.name.lower() == "selector":
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


def build_plan_surface(
    plan_summary: dict[str, Any],
    plan_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    front_page = get_mapping(plan_summary.get("front_page"))
    artifact_context = get_mapping(plan_summary.get("artifact_context"))
    return make_surface(
        surface_id=surface_id,
        label=f"{role.replace('_', ' ')}: opening-flow consumer plan",
        role=role,
        summary_schema=PLAN_SCHEMA,
        summary_path=normalize_path(plan_summary_path),
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


def build_plan_provenance_entry(
    plan_summary: dict[str, Any],
    plan_summary_path: Path,
    provenance_id: str,
    plan_role: str,
) -> OrderedDict[str, Any]:
    front_page = get_mapping(plan_summary.get("front_page"))
    artifact_context = get_mapping(plan_summary.get("artifact_context"))
    status = get_mapping(plan_summary.get("planner_status"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("plan_role", plan_role),
            ("source_summary_schema", PLAN_SCHEMA),
            ("source_summary_path", normalize_path(plan_summary_path)),
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
            ("result", choose_text(plan_summary.get("result"))),
            ("execution_plan_status", choose_text(status.get("execution_plan_status"))),
            ("planned_action_count", int(status.get("planned_action_count", 0))),
            ("next_action_count", int(status.get("next_action_count", 0))),
            ("omitted_entry_count", int(status.get("omitted_entry_count", 0))),
            ("default_action_name", choose_text(status.get("default_action_name"))),
            ("compare_action_name", choose_text(status.get("compare_action_name"))),
        ]
    )


def normalize_plan_actions(plan_summary: dict[str, Any]) -> OrderedDict[str, OrderedDict[str, Any]]:
    plan_root = get_plan_workspace_root(plan_summary)
    actions: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    for value in get_list(execution_plan.get("action_entries")):
        action = get_mapping(value)
        action_id = choose_text(action.get("action_id"))
        if not action_id:
            continue
        target_summary_path = choose_text(action.get("target_summary_path"))
        opener_summary_path = choose_text(action.get("opener_summary_path"))
        actions[action_id] = OrderedDict(
            [
                ("action_id", action_id),
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
                ("target_summary_path", target_summary_path),
                ("target_summary_compare_path", normalize_path_for_compare(target_summary_path, plan_root)),
                ("projection_kind", choose_text(action.get("projection_kind"))),
                ("compare_context_available", bool(action.get("compare_context_available"))),
                ("landing_verdict", choose_text(action.get("landing_verdict"))),
                ("inspector_ready", bool(action.get("inspector_ready"))),
                ("inspector_mode", choose_text(action.get("inspector_mode"))),
                (
                    "inspector_blockers",
                    ordered_unique([choose_text(item) for item in get_list(action.get("inspector_blockers"))]),
                ),
                ("opener_summary_path", opener_summary_path),
                ("opener_summary_compare_path", normalize_path_for_compare(opener_summary_path, plan_root)),
                ("opener_report_markdown_path", choose_text(action.get("opener_report_markdown_path"))),
                ("opener_check_text_path", choose_text(action.get("opener_check_text_path"))),
                ("expected_consumer_operation", choose_text(action.get("expected_consumer_operation"))),
            ]
        )
    return actions


def build_plan_status(baseline_summary: dict[str, Any], candidate_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    baseline_status = get_mapping(baseline_summary.get("planner_status"))
    candidate_status = get_mapping(candidate_summary.get("planner_status"))
    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_summary.get("result"))),
            ("candidate_result", choose_text(candidate_summary.get("result"))),
            ("baseline_execution_plan_status", choose_text(baseline_status.get("execution_plan_status"))),
            ("candidate_execution_plan_status", choose_text(candidate_status.get("execution_plan_status"))),
            ("baseline_planned_action_count", int(baseline_status.get("planned_action_count", 0))),
            ("candidate_planned_action_count", int(candidate_status.get("planned_action_count", 0))),
            ("baseline_next_action_count", int(baseline_status.get("next_action_count", 0))),
            ("candidate_next_action_count", int(candidate_status.get("next_action_count", 0))),
            ("baseline_omitted_entry_count", int(baseline_status.get("omitted_entry_count", 0))),
            ("candidate_omitted_entry_count", int(candidate_status.get("omitted_entry_count", 0))),
            ("baseline_default_action_name", choose_text(baseline_status.get("default_action_name"))),
            ("candidate_default_action_name", choose_text(candidate_status.get("default_action_name"))),
            ("baseline_compare_action_name", choose_text(baseline_status.get("compare_action_name"))),
            ("candidate_compare_action_name", choose_text(candidate_status.get("compare_action_name"))),
        ]
    )


def build_named_change(baseline: str, candidate: str) -> OrderedDict[str, Any]:
    return OrderedDict([("baseline", baseline), ("candidate", candidate), ("changed", baseline != candidate)])


def build_plan_changes(
    plan_status: dict[str, Any],
    baseline_actions: OrderedDict[str, OrderedDict[str, Any]],
    candidate_actions: OrderedDict[str, OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    baseline_order = list(baseline_actions.keys())
    candidate_order = list(candidate_actions.keys())
    action_id_changes = string_array_changes(baseline_order, candidate_order)
    order_changed = baseline_order != candidate_order
    changed = (
        plan_status.get("baseline_result") != plan_status.get("candidate_result")
        or plan_status.get("baseline_execution_plan_status") != plan_status.get("candidate_execution_plan_status")
        or plan_status.get("baseline_default_action_name") != plan_status.get("candidate_default_action_name")
        or plan_status.get("baseline_compare_action_name") != plan_status.get("candidate_compare_action_name")
        or plan_status.get("baseline_planned_action_count") != plan_status.get("candidate_planned_action_count")
        or plan_status.get("baseline_next_action_count") != plan_status.get("candidate_next_action_count")
        or plan_status.get("baseline_omitted_entry_count") != plan_status.get("candidate_omitted_entry_count")
        or has_array_changes(action_id_changes)
        or order_changed
    )
    return OrderedDict(
        [
            ("result_changed", plan_status.get("baseline_result") != plan_status.get("candidate_result")),
            (
                "execution_plan_status_changed",
                plan_status.get("baseline_execution_plan_status")
                != plan_status.get("candidate_execution_plan_status"),
            ),
            (
                "planned_action_count_change",
                count_change(
                    int(plan_status.get("baseline_planned_action_count", 0)),
                    int(plan_status.get("candidate_planned_action_count", 0)),
                ),
            ),
            (
                "next_action_count_change",
                count_change(
                    int(plan_status.get("baseline_next_action_count", 0)),
                    int(plan_status.get("candidate_next_action_count", 0)),
                ),
            ),
            (
                "omitted_entry_count_change",
                count_change(
                    int(plan_status.get("baseline_omitted_entry_count", 0)),
                    int(plan_status.get("candidate_omitted_entry_count", 0)),
                ),
            ),
            (
                "default_action_change",
                build_named_change(
                    choose_text(plan_status.get("baseline_default_action_name")),
                    choose_text(plan_status.get("candidate_default_action_name")),
                ),
            ),
            (
                "compare_action_change",
                build_named_change(
                    choose_text(plan_status.get("baseline_compare_action_name")),
                    choose_text(plan_status.get("candidate_compare_action_name")),
                ),
            ),
            ("ordered_action_id_changes", action_id_changes),
            ("ordered_action_order_changed", order_changed),
            ("changed", changed),
        ]
    )


def compare_action_field(
    notes: list[str],
    field_name: str,
    baseline_action: dict[str, Any],
    candidate_action: dict[str, Any],
) -> None:
    if baseline_action.get(field_name) != candidate_action.get(field_name):
        notes.append(f"{field_name}: {baseline_action.get(field_name)} -> {candidate_action.get(field_name)}")


def classify_changed_action(
    baseline_action: dict[str, Any],
    candidate_action: dict[str, Any],
) -> tuple[str, list[str]]:
    notes: list[str] = []
    for field_name in (
        "rank",
        "source_rank",
        "action_kind",
        "entry_name",
        "display_group",
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
        "expected_consumer_operation",
    ):
        compare_action_field(notes, field_name, baseline_action, candidate_action)

    impact = "neutral"
    if choose_text(baseline_action.get("action_kind")) in ("default", "compare-neighbor") and notes:
        impact = "regression"
    if bool(baseline_action.get("compare_context_available")) and not bool(
        candidate_action.get("compare_context_available")
    ):
        impact = "regression"
    if bool(baseline_action.get("inspector_ready")) and not bool(candidate_action.get("inspector_ready")):
        impact = "regression"
    if choose_text(baseline_action.get("projection_kind")) and not choose_text(candidate_action.get("projection_kind")):
        impact = "regression"

    if impact == "neutral":
        if not bool(baseline_action.get("compare_context_available")) and bool(
            candidate_action.get("compare_context_available")
        ):
            impact = "improvement"
        if not bool(baseline_action.get("inspector_ready")) and bool(candidate_action.get("inspector_ready")):
            impact = "improvement"
        if not choose_text(baseline_action.get("projection_kind")) and choose_text(candidate_action.get("projection_kind")):
            impact = "improvement"

    return impact, notes


def build_action_change_record(
    action_id: str,
    change_kind: str,
    impact: str,
    baseline_action: dict[str, Any] | None,
    candidate_action: dict[str, Any] | None,
    change_notes: list[str],
) -> OrderedDict[str, Any]:
    baseline = baseline_action or {}
    candidate = candidate_action or {}
    return OrderedDict(
        [
            ("action_id", action_id),
            ("change_kind", change_kind),
            ("impact", impact),
            ("baseline_rank", int(baseline.get("rank", -1))),
            ("candidate_rank", int(candidate.get("rank", -1))),
            ("baseline_action_kind", choose_text(baseline.get("action_kind"))),
            ("candidate_action_kind", choose_text(candidate.get("action_kind"))),
            ("baseline_entry_name", choose_text(baseline.get("entry_name"))),
            ("candidate_entry_name", choose_text(candidate.get("entry_name"))),
            ("baseline_display_group", choose_text(baseline.get("display_group"))),
            ("candidate_display_group", choose_text(candidate.get("display_group"))),
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
            ("baseline_inspector_ready", bool(baseline.get("inspector_ready"))),
            ("candidate_inspector_ready", bool(candidate.get("inspector_ready"))),
            ("baseline_opener_summary_path", choose_text(baseline.get("opener_summary_path"))),
            ("candidate_opener_summary_path", choose_text(candidate.get("opener_summary_path"))),
            ("change_notes", ordered_unique(change_notes)),
        ]
    )


def build_action_changes(
    baseline_actions: OrderedDict[str, OrderedDict[str, Any]],
    candidate_actions: OrderedDict[str, OrderedDict[str, Any]],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    changes: list[OrderedDict[str, Any]] = []
    summary = OrderedDict(
        [
            ("baseline_action_count", len(baseline_actions)),
            ("candidate_action_count", len(candidate_actions)),
            ("changed_action_count", 0),
            ("added_action_count", 0),
            ("removed_action_count", 0),
            ("unchanged_action_count", 0),
            ("regression_count", 0),
            ("improvement_count", 0),
            ("neutral_change_count", 0),
            ("order_changed_count", 0),
            ("projection_changed_count", 0),
            ("compare_context_lost_count", 0),
            ("compare_context_gained_count", 0),
            ("inspector_readiness_changed_count", 0),
            ("operation_changed_count", 0),
            ("target_changed_count", 0),
        ]
    )

    ordered_action_ids = ordered_unique(list(baseline_actions.keys()) + list(candidate_actions.keys()))
    for action_id in ordered_action_ids:
        baseline_action = baseline_actions.get(action_id)
        candidate_action = candidate_actions.get(action_id)
        if baseline_action is None and candidate_action is not None:
            impact = "improvement" if choose_text(candidate_action.get("projection_kind")) else "neutral"
            changes.append(build_action_change_record(action_id, "added", impact, None, candidate_action, ["action added"]))
            summary["added_action_count"] += 1
        elif baseline_action is not None and candidate_action is None:
            impact = (
                "regression"
                if choose_text(baseline_action.get("action_kind")) in ("default", "compare-neighbor")
                or choose_text(baseline_action.get("projection_kind"))
                else "neutral"
            )
            changes.append(build_action_change_record(action_id, "removed", impact, baseline_action, None, ["action removed"]))
            summary["removed_action_count"] += 1
        elif baseline_action is not None and candidate_action is not None:
            impact, notes = classify_changed_action(baseline_action, candidate_action)
            if notes:
                changes.append(build_action_change_record(action_id, "changed", impact, baseline_action, candidate_action, notes))
                summary["changed_action_count"] += 1
            else:
                summary["unchanged_action_count"] += 1

            if int(baseline_action.get("rank", -1)) != int(candidate_action.get("rank", -1)):
                summary["order_changed_count"] += 1
            if baseline_action.get("projection_kind") != candidate_action.get("projection_kind"):
                summary["projection_changed_count"] += 1
            if bool(baseline_action.get("compare_context_available")) and not bool(
                candidate_action.get("compare_context_available")
            ):
                summary["compare_context_lost_count"] += 1
            if not bool(baseline_action.get("compare_context_available")) and bool(
                candidate_action.get("compare_context_available")
            ):
                summary["compare_context_gained_count"] += 1
            if bool(baseline_action.get("inspector_ready")) != bool(candidate_action.get("inspector_ready")):
                summary["inspector_readiness_changed_count"] += 1
            if baseline_action.get("expected_consumer_operation") != candidate_action.get("expected_consumer_operation"):
                summary["operation_changed_count"] += 1
            if (
                baseline_action.get("target_summary_schema") != candidate_action.get("target_summary_schema")
                or baseline_action.get("target_summary_kind") != candidate_action.get("target_summary_kind")
                or baseline_action.get("target_summary_compare_path")
                != candidate_action.get("target_summary_compare_path")
            ):
                summary["target_changed_count"] += 1

        if changes and changes[-1].get("action_id") == action_id:
            impact_value = changes[-1].get("impact")
            if impact_value == "regression":
                summary["regression_count"] += 1
            elif impact_value == "improvement":
                summary["improvement_count"] += 1
            else:
                summary["neutral_change_count"] += 1

    return changes, summary


def build_regression_surface(
    plan_status: dict[str, Any],
    plan_changes: dict[str, Any],
    action_changes: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    removed_actions: list[str] = []
    removed_default_or_compare_actions: list[str] = []
    lost_compare_context_actions: list[str] = []
    lost_inspector_ready_actions: list[str] = []
    narratives: list[str] = []

    for change in action_changes:
        action_id = choose_text(change.get("action_id"))
        if change.get("change_kind") == "removed":
            removed_actions.append(action_id)
            if choose_text(change.get("baseline_action_kind")) in ("default", "compare-neighbor"):
                removed_default_or_compare_actions.append(action_id)
        if bool(change.get("baseline_compare_context_available")) and not bool(
            change.get("candidate_compare_context_available")
        ):
            lost_compare_context_actions.append(action_id)
        if bool(change.get("baseline_inspector_ready")) and not bool(change.get("candidate_inspector_ready")):
            lost_inspector_ready_actions.append(action_id)

    failed_result_transition = (
        choose_text(plan_status.get("baseline_result")) == "ok"
        and choose_text(plan_status.get("candidate_result")) != "ok"
    )
    blocked_plan_transition = (
        choose_text(plan_status.get("baseline_execution_plan_status")) == "ready"
        and choose_text(plan_status.get("candidate_execution_plan_status")) != "ready"
    )
    default_action_changed = bool(get_mapping(plan_changes.get("default_action_change")).get("changed"))
    compare_action_changed = bool(get_mapping(plan_changes.get("compare_action_change")).get("changed"))

    if failed_result_transition:
        narratives.append("candidate consumer plan no longer reports ok")
    if blocked_plan_transition:
        narratives.append("candidate consumer execution plan is no longer ready")
    if default_action_changed:
        narratives.append(
            "default action changed: {0} -> {1}".format(
                plan_status.get("baseline_default_action_name"),
                plan_status.get("candidate_default_action_name"),
            )
        )
    if compare_action_changed:
        narratives.append(
            "compare action changed: {0} -> {1}".format(
                plan_status.get("baseline_compare_action_name"),
                plan_status.get("candidate_compare_action_name"),
            )
        )
    if removed_default_or_compare_actions:
        narratives.append("candidate removed default/compare actions: {0}".format(", ".join(removed_default_or_compare_actions)))
    if lost_compare_context_actions:
        narratives.append("candidate lost compare context actions: {0}".format(", ".join(lost_compare_context_actions)))
    if lost_inspector_ready_actions:
        narratives.append("candidate lost inspector readiness actions: {0}".format(", ".join(lost_inspector_ready_actions)))

    changed = bool(
        failed_result_transition
        or blocked_plan_transition
        or default_action_changed
        or compare_action_changed
        or removed_default_or_compare_actions
        or lost_compare_context_actions
        or lost_inspector_ready_actions
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("default_action_changed", default_action_changed),
            ("compare_action_changed", compare_action_changed),
            ("removed_action_ids", ordered_unique(removed_actions)),
            ("removed_default_or_compare_action_ids", ordered_unique(removed_default_or_compare_actions)),
            ("lost_compare_context_action_ids", ordered_unique(lost_compare_context_actions)),
            ("lost_inspector_ready_action_ids", ordered_unique(lost_inspector_ready_actions)),
            ("failed_result_transition", failed_result_transition),
            ("blocked_plan_transition", blocked_plan_transition),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_plan_verdict(
    plan_changes: dict[str, Any],
    action_summary: dict[str, int],
    regression_surface: dict[str, Any],
    plan_status: dict[str, Any],
) -> str:
    if choose_text(plan_status.get("candidate_result")) != "ok":
        return "collapsed"
    if choose_text(plan_status.get("candidate_execution_plan_status")) != "ready":
        return "collapsed"
    if bool(regression_surface.get("changed")):
        return "drifted"
    if not bool(plan_changes.get("changed")) and int(action_summary.get("changed_action_count", 0)) == 0:
        return "standing"
    if (
        int(action_summary.get("improvement_count", 0)) > 0
        or int(action_summary.get("added_action_count", 0)) > 0
        or int(action_summary.get("compare_context_gained_count", 0)) > 0
        or int(plan_changes.get("planned_action_count_change", {}).get("delta", 0)) > 0
    ):
        return "improved"
    return "drifted"


def build_questions(
    plan_verdict: str,
    regression_surface: dict[str, Any],
    action_summary: dict[str, int],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if plan_verdict == "standing":
        compare_questions.append("Should this consumer action plan become the canonical explain-open execution witness?")
    elif plan_verdict == "improved":
        compare_questions.append("Which added or improved action should be promoted into the first explain surface?")
    elif plan_verdict == "drifted":
        compare_questions.append("Did action-plan drift change what a consumer executes first?")
    else:
        compare_questions.append("Which action-plan dependency collapsed before an explain consumer could open safely?")

    if bool(regression_surface.get("default_action_changed")):
        next_questions.append("Should default-action drift block publishing this front-page opening flow?")
    if bool(regression_surface.get("compare_action_changed")):
        next_questions.append("Should compare-action drift require a selector compare review?")
    if int(action_summary.get("operation_changed_count", 0)) > 0:
        next_questions.append("Should operation drift become a hard consumer-plan violation?")
    if int(action_summary.get("target_changed_count", 0)) > 0:
        next_questions.append("Should target drift trigger a deeper opener compare?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this plan compare instead of diffing plan JSON?")

    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_plan_path: Path,
    candidate_plan_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_plan_summary(baseline_plan_path)
    candidate_summary = load_plan_summary(candidate_plan_path)
    baseline_actions = normalize_plan_actions(baseline_summary)
    candidate_actions = normalize_plan_actions(candidate_summary)

    plan_status = build_plan_status(baseline_summary, candidate_summary)
    plan_changes = build_plan_changes(plan_status, baseline_actions, candidate_actions)
    action_changes, action_summary = build_action_changes(baseline_actions, candidate_actions)
    regression_surface = build_regression_surface(plan_status, plan_changes, action_changes)
    plan_verdict = build_plan_verdict(plan_changes, action_summary, regression_surface, plan_status)

    supporting_surfaces = [
        build_plan_surface(baseline_summary, baseline_plan_path, "baseline_consumer_plan", "baseline_consumer_plan"),
        build_plan_surface(candidate_summary, candidate_plan_path, "candidate_consumer_plan", "candidate_consumer_plan"),
    ]

    return OrderedDict(
        [
            ("schema", COMPARE_SCHEMA),
            ("kind", COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py"),
            ("result", "ok"),
            (
                "opening_flow_consumer_plan_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Plan Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two consumer plan witnesses preserve the same explain-open actions.",
                        ),
                    ]
                ),
            ),
            ("front_page", build_front_page(summary_path, report_path, check_path, supporting_surfaces)),
            (
                "plan_provenance",
                [
                    build_plan_provenance_entry(baseline_summary, baseline_plan_path, "baseline_consumer_plan", "baseline_plan"),
                    build_plan_provenance_entry(candidate_summary, candidate_plan_path, "candidate_consumer_plan", "candidate_plan"),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_plan_summary_path", normalize_path(baseline_plan_path)),
                        ("candidate_plan_summary_path", normalize_path(candidate_plan_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("plan_verdict", plan_verdict),
            ("plan_status", plan_status),
            ("plan_changes", plan_changes),
            ("plan_action_summary", action_summary),
            ("plan_action_changes", action_changes),
            ("plan_regression_surface", regression_surface),
            ("questions", build_questions(plan_verdict, regression_surface, action_summary)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    plan_status = summary["plan_status"]
    plan_changes = summary["plan_changes"]
    action_summary = summary["plan_action_summary"]
    regression_surface = summary["plan_regression_surface"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Plan Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Plan verdict: `{summary['plan_verdict']}`",
        f"- Baseline plan: `{summary['artifact_context']['baseline_plan_summary_path']}`",
        f"- Candidate plan: `{summary['artifact_context']['candidate_plan_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Plan Status",
        "- Baseline: `result={0} plan={1} actions={2} next={3} omitted={4} default={5} compare={6}`".format(
            plan_status["baseline_result"],
            plan_status["baseline_execution_plan_status"],
            plan_status["baseline_planned_action_count"],
            plan_status["baseline_next_action_count"],
            plan_status["baseline_omitted_entry_count"],
            plan_status["baseline_default_action_name"],
            plan_status["baseline_compare_action_name"],
        ),
        "- Candidate: `result={0} plan={1} actions={2} next={3} omitted={4} default={5} compare={6}`".format(
            plan_status["candidate_result"],
            plan_status["candidate_execution_plan_status"],
            plan_status["candidate_planned_action_count"],
            plan_status["candidate_next_action_count"],
            plan_status["candidate_omitted_entry_count"],
            plan_status["candidate_default_action_name"],
            plan_status["candidate_compare_action_name"],
        ),
        "",
        "## Plan Changes",
        "- actions_delta=`{0}` next_delta=`{1}` omitted_delta=`{2}` order_changed=`{3}`".format(
            plan_changes["planned_action_count_change"]["delta"],
            plan_changes["next_action_count_change"]["delta"],
            plan_changes["omitted_entry_count_change"]["delta"],
            plan_changes["ordered_action_order_changed"],
        ),
        "- default: `{0}` -> `{1}`".format(
            plan_changes["default_action_change"]["baseline"],
            plan_changes["default_action_change"]["candidate"],
        ),
        "- compare: `{0}` -> `{1}`".format(
            plan_changes["compare_action_change"]["baseline"],
            plan_changes["compare_action_change"]["candidate"],
        ),
        "",
        "## Action Summary",
        "- actions baseline=`{0}` candidate=`{1}` changed=`{2}` added=`{3}` removed=`{4}` unchanged=`{5}`".format(
            action_summary["baseline_action_count"],
            action_summary["candidate_action_count"],
            action_summary["changed_action_count"],
            action_summary["added_action_count"],
            action_summary["removed_action_count"],
            action_summary["unchanged_action_count"],
        ),
        "- impacts regression=`{0}` improvement=`{1}` neutral=`{2}` order_changed=`{3}` operation_changed=`{4}` target_changed=`{5}`".format(
            action_summary["regression_count"],
            action_summary["improvement_count"],
            action_summary["neutral_change_count"],
            action_summary["order_changed_count"],
            action_summary["operation_changed_count"],
            action_summary["target_changed_count"],
        ),
    ]

    if summary["plan_action_changes"]:
        lines.extend(["", "## Action Changes"])
        for change in summary["plan_action_changes"]:
            lines.append(
                "- `{0}` kind=`{1}` impact=`{2}` rank=`{3}->{4}` action=`{5}->{6}` entry=`{7}->{8}` projection=`{9}->{10}`".format(
                    change["action_id"],
                    change["change_kind"],
                    change["impact"],
                    change["baseline_rank"],
                    change["candidate_rank"],
                    change["baseline_action_kind"] or "none",
                    change["candidate_action_kind"] or "none",
                    change["baseline_entry_name"] or "none",
                    change["candidate_entry_name"] or "none",
                    change["baseline_projection_kind"] or "none",
                    change["candidate_projection_kind"] or "none",
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
    plan_status = summary["plan_status"]
    action_summary = summary["plan_action_summary"]
    return "\n".join(
        [
            f"baseline_plan_summary_path: {summary['artifact_context']['baseline_plan_summary_path']}",
            f"candidate_plan_summary_path: {summary['artifact_context']['candidate_plan_summary_path']}",
            f"plan_verdict: {summary['plan_verdict']}",
            f"baseline_result: {plan_status['baseline_result']}",
            f"candidate_result: {plan_status['candidate_result']}",
            f"baseline_default_action_name: {plan_status['baseline_default_action_name']}",
            f"candidate_default_action_name: {plan_status['candidate_default_action_name']}",
            f"baseline_compare_action_name: {plan_status['baseline_compare_action_name']}",
            f"candidate_compare_action_name: {plan_status['candidate_compare_action_name']}",
            f"baseline_planned_action_count: {plan_status['baseline_planned_action_count']}",
            f"candidate_planned_action_count: {plan_status['candidate_planned_action_count']}",
            f"changed_action_count: {action_summary['changed_action_count']}",
            f"added_action_count: {action_summary['added_action_count']}",
            f"removed_action_count: {action_summary['removed_action_count']}",
            f"regression_count: {action_summary['regression_count']}",
            f"improvement_count: {action_summary['improvement_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page entry opening-flow consumer plan summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline opening-flow consumer plan summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening-flow consumer plan summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for plan compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for plan compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for plan compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for plan compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.plan.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.consumer.plan.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.plan.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE] verdict={summary['plan_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE] changed_actions={0}".format(
            summary["plan_action_summary"]["changed_action_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
