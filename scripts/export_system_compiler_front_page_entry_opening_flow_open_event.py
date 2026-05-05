from __future__ import annotations

import argparse
import hashlib
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


ACTION_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0"
ACTION_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_action"
ACTION_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
ACTION_COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"
PLAN_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan/v0"
PLAN_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan"
OPENER_SCHEMA = "system_compiler.front_page_entry_opener/v0"
OPEN_EVENT_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event/v0"
OPEN_EVENT_KIND = "system_compiler.front_page_entry_opening_flow_open_event"


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


def load_action_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != ACTION_SCHEMA:
        raise ValueError(f"unsupported opening-flow consumer plan action schema: {path}")
    if choose_text(summary.get("kind")) != ACTION_KIND:
        raise ValueError(f"unsupported opening-flow consumer plan action kind: {path}")
    return summary


def load_action_compare_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != ACTION_COMPARE_SCHEMA:
        raise ValueError(f"unsupported opening-flow consumer plan action compare schema: {path}")
    if choose_text(summary.get("kind")) != ACTION_COMPARE_KIND:
        raise ValueError(f"unsupported opening-flow consumer plan action compare kind: {path}")
    return summary


def load_plan_summary(path_text: str) -> dict[str, Any]:
    if not choose_text(path_text):
        return {}
    path = Path(path_text).resolve()
    summary = load_json(path)
    if choose_text(summary.get("schema")) != PLAN_SCHEMA:
        raise ValueError(f"unsupported opening-flow consumer plan schema: {path}")
    if choose_text(summary.get("kind")) != PLAN_KIND:
        raise ValueError(f"unsupported opening-flow consumer plan kind: {path}")
    return summary


def normalize_opening_reason(value: Any) -> OrderedDict[str, Any]:
    reason = get_mapping(value)
    return OrderedDict(
        [
            ("kind", choose_text(reason.get("kind"))),
            ("summary", choose_text(reason.get("summary"))),
            ("source_summary_path", normalize_optional_path(reason.get("source_summary_path"))),
            ("drift_changed", bool(reason.get("drift_changed"))),
            ("drift_verdict", choose_text(reason.get("drift_verdict"))),
        ]
    )


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


def build_action_surface(action_summary: dict[str, Any], action_summary_path: Path) -> OrderedDict[str, str]:
    artifact_context = get_mapping(action_summary.get("artifact_context"))
    return make_surface(
        "source_plan_action",
        "source opening-flow consumer plan action",
        "source_plan_action",
        ACTION_SCHEMA,
        normalize_path(action_summary_path),
        normalize_optional_path(artifact_context.get("report_markdown_path")),
        normalize_optional_path(artifact_context.get("check_text_path")),
    )


def find_supporting_surface(action_summary: dict[str, Any], surface_id: str) -> dict[str, Any]:
    front_page = get_mapping(action_summary.get("front_page"))
    for surface in get_list(front_page.get("supporting_surfaces")):
        surface_map = get_mapping(surface)
        if choose_text(surface_map.get("id")) == surface_id:
            return surface_map
    return {}


def build_plan_surface(action_summary: dict[str, Any]) -> OrderedDict[str, str] | None:
    surface = find_supporting_surface(action_summary, "source_consumer_plan")
    if not surface:
        return None
    return make_surface(
        "source_consumer_plan",
        "source opening-flow consumer plan",
        "source_consumer_plan",
        choose_text(surface.get("summary_schema")) or PLAN_SCHEMA,
        normalize_optional_path(surface.get("summary_path")),
        normalize_optional_path(surface.get("report_markdown_path")),
        normalize_optional_path(surface.get("check_text_path")),
    )


def build_opener_surface(action_summary: dict[str, Any]) -> OrderedDict[str, str]:
    opener_surface = get_mapping(action_summary.get("opener_surface"))
    open_action = get_mapping(action_summary.get("open_action"))
    return make_surface(
        "selected_opener",
        f"selected opener: {choose_text(open_action.get('entry_name'))}",
        "selected_opener",
        OPENER_SCHEMA,
        normalize_optional_path(opener_surface.get("summary_path") or open_action.get("opener_summary_path")),
        normalize_optional_path(opener_surface.get("report_markdown_path") or open_action.get("opener_report_markdown_path")),
        normalize_optional_path(opener_surface.get("check_text_path") or open_action.get("opener_check_text_path")),
    )


def build_compare_surface(compare_summary: dict[str, Any] | None, compare_summary_path: Path | None) -> OrderedDict[str, str] | None:
    if compare_summary is None or compare_summary_path is None:
        return None
    artifact_context = get_mapping(compare_summary.get("artifact_context"))
    return make_surface(
        "source_action_compare",
        "source opening-flow consumer plan action compare",
        "source_action_compare",
        ACTION_COMPARE_SCHEMA,
        normalize_path(compare_summary_path),
        normalize_optional_path(artifact_context.get("report_markdown_path")),
        normalize_optional_path(artifact_context.get("check_text_path")),
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    action_summary: dict[str, Any],
    action_summary_path: Path,
    compare_summary: dict[str, Any] | None,
    compare_summary_path: Path | None,
) -> OrderedDict[str, Any]:
    surfaces: list[OrderedDict[str, str]] = [build_action_surface(action_summary, action_summary_path)]
    plan_surface = build_plan_surface(action_summary)
    if plan_surface is not None:
        surfaces.append(plan_surface)
    surfaces.append(build_opener_surface(action_summary))
    compare_surface = build_compare_surface(compare_summary, compare_summary_path)
    if compare_surface is not None:
        surfaces.append(compare_surface)
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", surfaces),
        ]
    )


def make_event_id(open_action: dict[str, Any], selection: dict[str, Any]) -> str:
    reason = normalize_opening_reason(open_action.get("opening_reason"))
    stable_parts = [
        choose_text(reason.get("kind")),
        choose_text(open_action.get("action_id")),
        choose_text(open_action.get("entry_name")),
        choose_text(open_action.get("target_summary_schema")),
        choose_text(open_action.get("target_summary_kind")),
        choose_text(open_action.get("projection_kind")),
        choose_text(selection.get("effective_selector")),
    ]
    digest = hashlib.sha256("|".join(stable_parts).encode("utf-8")).hexdigest()[:16]
    return f"open-event-{digest}"


def make_consumer_id(action: dict[str, Any]) -> str:
    role = choose_text(action.get("selected_role")) or "unknown-role"
    query_kind = choose_text(action.get("query_kind")) or "unknown-query"
    query_scope = choose_text(action.get("query_scope")) or "unknown-scope"
    return f"{role}:{query_kind}:{query_scope}"


def build_selected_consumer(open_action: dict[str, Any], selection: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("consumer_id", make_consumer_id(open_action)),
            ("selected_action_id", choose_text(open_action.get("action_id"))),
            ("entry_name", choose_text(open_action.get("entry_name"))),
            ("selected_role", choose_text(open_action.get("selected_role"))),
            ("query_kind", choose_text(open_action.get("query_kind"))),
            ("query_scope", choose_text(open_action.get("query_scope"))),
            ("operation", choose_text(open_action.get("expected_consumer_operation"))),
            ("projection_kind", choose_text(open_action.get("projection_kind"))),
            ("chosen_by", choose_text(selection.get("effective_selector"))),
        ]
    )


def build_candidate_consumer(action: dict[str, Any], selected_action_id: str, effective_selector: str) -> OrderedDict[str, Any]:
    action_id = choose_text(action.get("action_id"))
    selected = action_id == selected_action_id
    return OrderedDict(
        [
            ("consumer_id", make_consumer_id(action)),
            ("action_id", action_id),
            ("entry_name", choose_text(action.get("entry_name"))),
            ("rank", int(action.get("rank", 0))),
            ("action_kind", choose_text(action.get("action_kind"))),
            ("display_group", choose_text(action.get("display_group"))),
            ("projection_kind", choose_text(action.get("projection_kind"))),
            ("target_summary_schema", choose_text(action.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(action.get("target_summary_kind"))),
            ("target_summary_path", normalize_optional_path(action.get("target_summary_path"))),
            ("selected", selected),
            ("selection_basis", f"selected by {effective_selector}" if selected else "available but not selected"),
        ]
    )


def build_reject_reason(action: dict[str, Any], effective_selector: str) -> str:
    action_kind = choose_text(action.get("action_kind"))
    if action_kind == "default":
        return f"default action was bypassed by selector {effective_selector}"
    if action_kind == "compare-neighbor":
        return f"compare neighbor stayed available but selector {effective_selector} chose another action"
    if action_kind == "next":
        return f"fallback action was not requested by selector {effective_selector}"
    return f"candidate action was not selected by selector {effective_selector}"


def build_consumer_decision(
    plan_summary: dict[str, Any],
    open_action: dict[str, Any],
    selection: dict[str, Any],
) -> OrderedDict[str, Any]:
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    source_actions = [get_mapping(action) for action in get_list(execution_plan.get("action_entries"))]
    if not source_actions:
        source_actions = [open_action]

    selected_action_id = choose_text(open_action.get("action_id"))
    effective_selector = choose_text(selection.get("effective_selector"))
    candidates = [build_candidate_consumer(action, selected_action_id, effective_selector) for action in source_actions]
    rejected = [
        OrderedDict(
            [
                ("consumer_id", candidate["consumer_id"]),
                ("action_id", candidate["action_id"]),
                ("entry_name", candidate["entry_name"]),
                ("reason", build_reject_reason(source_actions[index], effective_selector)),
            ]
        )
        for index, candidate in enumerate(candidates)
        if not bool(candidate.get("selected"))
    ]

    return OrderedDict(
        [
            ("selected_consumer", build_selected_consumer(open_action, selection)),
            ("candidate_consumers", candidates),
            ("rejected_consumers", rejected),
            ("candidate_consumer_count", len(candidates)),
            ("rejected_consumer_count", len(rejected)),
            (
                "decision_reason",
                choose_text(open_action.get("reason")) or f"opening action selected by {effective_selector}",
            ),
        ]
    )


def build_plan_summary(plan_summary: dict[str, Any], open_action: dict[str, Any]) -> OrderedDict[str, Any]:
    planner_status = get_mapping(plan_summary.get("planner_status"))
    execution_plan = get_mapping(plan_summary.get("execution_plan"))
    default_action = get_mapping(execution_plan.get("default_action"))
    compare_action = get_mapping(execution_plan.get("compare_action"))
    return OrderedDict(
        [
            ("plan_id", "opening-flow-consumer-plan"),
            ("result", choose_text(plan_summary.get("result"))),
            ("execution_plan_status", choose_text(planner_status.get("execution_plan_status"))),
            ("planned_action_count", int(planner_status.get("planned_action_count", 0))),
            ("default_action_id", choose_text(default_action.get("action_id"))),
            ("compare_action_id", choose_text(compare_action.get("action_id"))),
            ("selected_action_id", choose_text(open_action.get("action_id"))),
        ]
    )


def build_compare_result(compare_summary: dict[str, Any] | None, compare_summary_path: Path | None) -> OrderedDict[str, Any]:
    if compare_summary is None:
        return OrderedDict(
            [
                ("available", False),
                ("summary_path", ""),
                ("action_verdict", "not_attached"),
                ("changed_field_count", 0),
                ("reason_changed", False),
                ("narratives", []),
            ]
        )

    regression = get_mapping(compare_summary.get("action_regression_surface"))
    change_summary = get_mapping(compare_summary.get("change_summary"))
    return OrderedDict(
        [
            ("available", True),
            ("summary_path", normalize_path(compare_summary_path) if compare_summary_path is not None else ""),
            ("action_verdict", choose_text(compare_summary.get("action_verdict"))),
            ("changed_field_count", int(change_summary.get("changed_field_count", 0))),
            ("reason_changed", bool(regression.get("reason_changed"))),
            ("narratives", ordered_unique([choose_text(item) for item in get_list(regression.get("narratives"))])),
        ]
    )


def build_action_record(open_action: dict[str, Any], opener_surface: dict[str, Any], compare_result: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("action_id", choose_text(open_action.get("action_id"))),
            ("action_kind", choose_text(open_action.get("action_kind"))),
            ("entry_name", choose_text(open_action.get("entry_name"))),
            (
                "expected",
                OrderedDict(
                    [
                        ("operation", choose_text(open_action.get("expected_consumer_operation"))),
                        ("projection_kind", choose_text(open_action.get("projection_kind"))),
                        ("target_summary_schema", choose_text(open_action.get("target_summary_schema"))),
                        ("target_summary_kind", choose_text(open_action.get("target_summary_kind"))),
                        ("target_summary_path", normalize_optional_path(open_action.get("target_summary_path"))),
                    ]
                ),
            ),
            (
                "result",
                OrderedDict(
                    [
                        ("status", choose_text(open_action.get("status"))),
                        ("opener_surface_available", bool(opener_surface.get("available"))),
                        ("opener_summary_path", normalize_optional_path(open_action.get("opener_summary_path"))),
                        ("blockers", [choose_text(item) for item in get_list(open_action.get("blockers")) if choose_text(item)]),
                    ]
                ),
            ),
            ("compare", compare_result),
        ]
    )


def build_workspace_facade(open_action: dict[str, Any], opener_surface: dict[str, Any], event_status: str) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("status", "projected" if event_status != "blocked" else "blocked"),
            ("facade_kind", "explain_open_event_view"),
            ("primary_surface_role", "selected_opener"),
            ("primary_summary_path", normalize_optional_path(opener_surface.get("summary_path") or open_action.get("opener_summary_path"))),
            ("primary_report_markdown_path", normalize_optional_path(opener_surface.get("report_markdown_path") or open_action.get("opener_report_markdown_path"))),
            ("primary_check_text_path", normalize_optional_path(opener_surface.get("check_text_path") or open_action.get("opener_check_text_path"))),
        ]
    )


def build_witness_ref(role: str, schema: str, path: str, report_path: str = "", check_path: str = "") -> OrderedDict[str, str]:
    return OrderedDict(
        [
            ("role", role),
            ("summary_schema", schema),
            ("summary_path", normalize_optional_path(path)),
            ("report_markdown_path", normalize_optional_path(report_path)),
            ("check_text_path", normalize_optional_path(check_path)),
        ]
    )


def build_witness_refs(
    action_summary: dict[str, Any],
    action_summary_path: Path,
    compare_summary: dict[str, Any] | None,
    compare_summary_path: Path | None,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> list[OrderedDict[str, str]]:
    action_artifact = get_mapping(action_summary.get("artifact_context"))
    opener_surface = get_mapping(action_summary.get("opener_surface"))
    refs = [
        build_witness_ref(
            "source_plan_action",
            ACTION_SCHEMA,
            normalize_path(action_summary_path),
            choose_text(action_artifact.get("report_markdown_path")),
            choose_text(action_artifact.get("check_text_path")),
        ),
        build_witness_ref(
            "selected_opener",
            OPENER_SCHEMA,
            choose_text(opener_surface.get("summary_path")),
            choose_text(opener_surface.get("report_markdown_path")),
            choose_text(opener_surface.get("check_text_path")),
        ),
        build_witness_ref(
            "open_event",
            OPEN_EVENT_SCHEMA,
            normalize_path(summary_path),
            normalize_path(report_path),
            normalize_path(check_path),
        ),
    ]
    if compare_summary is not None and compare_summary_path is not None:
        compare_artifact = get_mapping(compare_summary.get("artifact_context"))
        refs.append(
            build_witness_ref(
                "source_action_compare",
                ACTION_COMPARE_SCHEMA,
                normalize_path(compare_summary_path),
                choose_text(compare_artifact.get("report_markdown_path")),
                choose_text(compare_artifact.get("check_text_path")),
            )
        )
    return refs


def build_event_status(action_summary: dict[str, Any], compare_result: dict[str, Any]) -> str:
    open_action = get_mapping(action_summary.get("open_action"))
    blockers = get_list(open_action.get("blockers"))
    if choose_text(action_summary.get("result")) != "ok" or choose_text(open_action.get("status")) != "ready" or blockers:
        return "blocked"
    if bool(compare_result.get("available")) and choose_text(compare_result.get("action_verdict")) in ("drifted", "collapsed"):
        return "accepted_with_drift"
    return "accepted"


def build_judgment(event_status: str, compare_result: dict[str, Any]) -> OrderedDict[str, Any]:
    compared = bool(compare_result.get("available"))
    basis = ["source_plan_action", "selected_opener", "open_event"]
    if compared:
        basis.append("source_action_compare")

    if event_status == "blocked":
        summary = (
            "This opening judgment is blocked because the selected consumer action did not produce a ready "
            "opening surface."
        )
    elif compared:
        summary = (
            "This opening judgment stands with compare context because the selected consumer action produced a "
            "projected opener surface and the attached action compare preserves its decision context."
        )
    else:
        summary = (
            "This opening judgment stands as described because the selected consumer action produced a projected "
            "opener surface and the open event preserves its decision context."
        )

    return OrderedDict(
        [
            ("semantic_role", "opening_judgment_carrier"),
            ("status", event_status),
            ("grade", "compared" if compared else "described"),
            ("basis", basis),
            ("accepted", event_status != "blocked"),
            ("summary", summary),
        ]
    )


def build_explanation_view(
    event_status: str,
    open_action: dict[str, Any],
    consumer_decision: dict[str, Any],
    plan: dict[str, Any],
    action_records: list[OrderedDict[str, Any]],
    compare_result: dict[str, Any],
    witness_refs: list[OrderedDict[str, str]],
) -> OrderedDict[str, Any]:
    reason = normalize_opening_reason(open_action.get("opening_reason"))
    selected = get_mapping(consumer_decision.get("selected_consumer"))
    rejected_count = int(consumer_decision.get("rejected_consumer_count", 0))
    action_lines = [
        "{0}. {1} -> {2}".format(index + 1, record["action_id"], get_mapping(record.get("result")).get("status", ""))
        for index, record in enumerate(action_records)
    ]
    witness_lines = [f"{ref['role']}: {ref['summary_path']}" for ref in witness_refs if choose_text(ref.get("summary_path"))]
    compare_line = (
        "no action compare attached"
        if not bool(compare_result.get("available"))
        else "action compare verdict={0} changed_fields={1}".format(
            compare_result.get("action_verdict"),
            compare_result.get("changed_field_count"),
        )
    )
    text_lines = [
        f"Why opened: {choose_text(reason.get('summary')) or choose_text(open_action.get('reason'))}",
        f"Chosen consumer: {selected.get('consumer_id', '')} via {selected.get('chosen_by', '')}",
        f"Plan: {plan.get('planned_action_count', 0)} action(s), selected {plan.get('selected_action_id', '')}",
        f"Rejected candidates: {rejected_count}",
        f"Compare: {compare_line}",
        f"Witness refs: {len(witness_refs)}",
    ]
    return OrderedDict(
        [
            ("view_kind", "explain_open_event_view"),
            ("status", event_status),
            ("why_opened", choose_text(reason.get("summary")) or choose_text(open_action.get("reason"))),
            ("chosen_consumer", f"{selected.get('consumer_id', '')} selected by {selected.get('chosen_by', '')}"),
            ("plan_actions", action_lines),
            ("compare_result", compare_line),
            ("witness_refs", witness_lines),
            ("text_lines", text_lines),
        ]
    )


def build_typed_question(kind: str, summary: str, target_ref: str) -> OrderedDict[str, str]:
    return OrderedDict([("kind", kind), ("summary", summary), ("target_ref", target_ref)])


def build_questions(event_status: str, compare_result: dict[str, Any]) -> OrderedDict[str, Any]:
    open_event_questions: list[str] = []
    next_questions: list[str] = []
    typed_next_questions: list[OrderedDict[str, str]] = []

    if event_status == "blocked":
        open_event_questions.append("Which opening dependency blocked this event before it could produce a workspace facade?")
    elif event_status == "accepted_with_drift":
        open_event_questions.append("Should the explain surface foreground the action drift before opening the selected facade?")
    else:
        open_event_questions.append("Should this OpenEventRecord become the canonical opening judgment witness?")

    if bool(compare_result.get("available")):
        next_questions.append("Should the action compare be rendered beside the selected opener as counterfactual context?")
        typed_next_questions.append(
            build_typed_question(
                "inspect_action_compare",
                "Inspect the attached action compare before rendering the selected opener as counterfactual context.",
                "compare_summary.summary_path",
            )
        )
    else:
        next_questions.append("Should future opening flows attach an action compare before publishing the open event?")
        typed_next_questions.append(
            build_typed_question(
                "attach_action_compare",
                "Attach an action compare before publishing this open event as a compared opening judgment.",
                "artifact_context.source_action_compare_summary_path",
            )
        )
    next_questions.append("Should rejected consumer reasons become a first-class selector output?")
    typed_next_questions.append(
        build_typed_question(
            "inspect_rejected_consumers",
            "Inspect rejected consumer reasons as the next selector-facing explanation surface.",
            "consumer_decision.rejected_consumers",
        )
    )

    return OrderedDict(
        [
            ("open_event_questions", ordered_unique(open_event_questions)),
            ("next_questions", ordered_unique(next_questions)),
            ("typed_next_questions", typed_next_questions),
        ]
    )


def build_summary_model(
    action_summary_path: Path,
    action_compare_summary_path: Path | None,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    action_summary = load_action_summary(action_summary_path)
    action_artifact = get_mapping(action_summary.get("artifact_context"))
    plan_summary = load_plan_summary(choose_text(action_artifact.get("source_plan_summary_path")))
    open_action = get_mapping(action_summary.get("open_action"))
    opener_surface = get_mapping(action_summary.get("opener_surface"))
    selection = get_mapping(action_summary.get("selection_request"))
    compare_summary = load_action_compare_summary(action_compare_summary_path) if action_compare_summary_path is not None else None
    compare_result = build_compare_result(compare_summary, action_compare_summary_path)
    event_status = build_event_status(action_summary, compare_result)
    judgment = build_judgment(event_status, compare_result)
    event_id = make_event_id(open_action, selection)
    consumer_decision = build_consumer_decision(plan_summary, open_action, selection)
    plan = build_plan_summary(plan_summary, open_action)
    action_records = [build_action_record(open_action, opener_surface, compare_result)]
    workspace_facade = build_workspace_facade(open_action, opener_surface, event_status)
    witness_refs = build_witness_refs(
        action_summary,
        action_summary_path,
        compare_summary,
        action_compare_summary_path,
        summary_path,
        report_path,
        check_path,
    )
    explanation_view = build_explanation_view(
        event_status,
        open_action,
        consumer_decision,
        plan,
        action_records,
        compare_result,
        witness_refs,
    )

    return OrderedDict(
        [
            ("schema", OPEN_EVENT_SCHEMA),
            ("kind", OPEN_EVENT_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_open_event.py"),
            ("result", "ok" if event_status != "blocked" else "fail"),
            (
                "opening_flow_open_event",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Open Event"),
                        ("summary", "A deterministic opening judgment record emitted from a selected consumer plan action."),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(
                    summary_path,
                    report_path,
                    check_path,
                    action_summary,
                    action_summary_path,
                    compare_summary,
                    action_compare_summary_path,
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_action_summary_path", normalize_path(action_summary_path)),
                        (
                            "source_action_compare_summary_path",
                            normalize_path(action_compare_summary_path) if action_compare_summary_path is not None else "",
                        ),
                        ("output_root", normalize_path(output_root)),
                        ("open_event_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "open_event",
                OrderedDict(
                    [
                        ("open_event_id", event_id),
                        ("status", event_status),
                        ("reason", normalize_opening_reason(open_action.get("opening_reason"))),
                        (
                            "source_artifact",
                            OrderedDict(
                                [
                                    ("summary_schema", choose_text(open_action.get("target_summary_schema"))),
                                    ("summary_kind", choose_text(open_action.get("target_summary_kind"))),
                                    ("summary_path", normalize_optional_path(open_action.get("target_summary_path"))),
                                    (
                                        "opener_summary_path",
                                        normalize_optional_path(open_action.get("opener_summary_path")),
                                    ),
                                    (
                                        "opener_report_markdown_path",
                                        normalize_optional_path(open_action.get("opener_report_markdown_path")),
                                    ),
                                    (
                                        "opener_check_text_path",
                                        normalize_optional_path(open_action.get("opener_check_text_path")),
                                    ),
                                ]
                            ),
                        ),
                    ]
                ),
            ),
            ("consumer_decision", consumer_decision),
            ("plan", plan),
            ("action_records", action_records),
            ("compare_summary", compare_result),
            ("workspace_facade", workspace_facade),
            ("witness_refs", witness_refs),
            ("judgment", judgment),
            ("explanation_view", explanation_view),
            ("questions", build_questions(event_status, compare_result)),
            (
                "violations",
                [] if event_status != "blocked" else [choose_text(item) for item in get_list(open_action.get("blockers"))],
            ),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    event = summary["open_event"]
    judgment = summary["judgment"]
    reason = event["reason"]
    consumer = summary["consumer_decision"]
    selected = consumer["selected_consumer"]
    plan = summary["plan"]
    compare = summary["compare_summary"]
    workspace = summary["workspace_facade"]
    explanation = summary["explanation_view"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Open Event",
        "",
        f"- Result: `{summary['result']}`",
        f"- Event: `{event['open_event_id']}`",
        f"- Status: `{event['status']}`",
        f"- Judgment: `{judgment['semantic_role']}` grade=`{judgment['grade']}` accepted=`{judgment['accepted']}`",
        f"- Summary JSON: `{summary['artifact_context']['open_event_summary_path']}`",
        "",
        "## Judgment Summary",
        f"- {judgment['summary']}",
        f"- basis: `{', '.join(judgment['basis'])}`",
        "",
        "## Why Opened",
        f"- reason kind: `{reason['kind']}`",
        f"- reason summary: {reason['summary'] or selected['chosen_by']}",
        f"- projection headline: {explanation['why_opened'] or 'none'}",
        "",
        "## Consumer Decision",
        "- selected consumer: `{0}` action=`{1}` entry=`{2}`".format(
            selected["consumer_id"],
            selected["selected_action_id"],
            selected["entry_name"],
        ),
        "- candidates=`{0}` rejected=`{1}` decision=`{2}`".format(
            consumer["candidate_consumer_count"],
            consumer["rejected_consumer_count"],
            consumer["decision_reason"],
        ),
    ]
    if consumer["rejected_consumers"]:
        lines.append("")
        lines.append("## Rejected Candidates")
        for rejected in consumer["rejected_consumers"]:
            lines.append("- `{0}` action=`{1}` reason={2}".format(rejected["consumer_id"], rejected["action_id"], rejected["reason"]))

    lines.extend(
        [
            "",
            "## Plan And Actions",
            "- plan=`{0}` status=`{1}` actions=`{2}` selected=`{3}`".format(
                plan["plan_id"],
                plan["execution_plan_status"],
                plan["planned_action_count"],
                plan["selected_action_id"],
            ),
        ]
    )
    for record in summary["action_records"]:
        result = get_mapping(record.get("result"))
        expected = get_mapping(record.get("expected"))
        lines.append(
            "- action=`{0}` kind=`{1}` entry=`{2}` status=`{3}` operation=`{4}`".format(
                record["action_id"],
                record["action_kind"],
                record["entry_name"],
                result.get("status", ""),
                expected.get("operation", ""),
            )
        )

    lines.extend(
        [
            "",
            "## Compare",
            "- available=`{0}` verdict=`{1}` changed_fields=`{2}` reason_changed=`{3}`".format(
                compare["available"],
                compare["action_verdict"],
                compare["changed_field_count"],
                compare["reason_changed"],
            ),
            "",
            "## Workspace Facade",
            "- status=`{0}` kind=`{1}` primary=`{2}`".format(
                workspace["status"],
                workspace["facade_kind"],
                workspace["primary_summary_path"],
            ),
            "",
            "## Explain View",
        ]
    )
    for line in explanation["text_lines"]:
        lines.append(f"- {line}")
    lines.extend(["", "## Witness Refs"])
    for ref in summary["witness_refs"]:
        lines.append(f"- `{ref['role']}`: `{ref['summary_path']}`")
    lines.extend(["", "## Questions"])
    for question in summary["questions"]["open_event_questions"]:
        lines.append(f"- open-event: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")
    for question in summary["questions"]["typed_next_questions"]:
        lines.append(f"- typed-next `{question['kind']}` target=`{question['target_ref']}`: {question['summary']}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    event = summary["open_event"]
    judgment = summary["judgment"]
    consumer = summary["consumer_decision"]
    selected = consumer["selected_consumer"]
    compare = summary["compare_summary"]
    workspace = summary["workspace_facade"]
    return "\n".join(
        [
            f"source_action_summary_path: {summary['artifact_context']['source_action_summary_path']}",
            f"source_action_compare_summary_path: {summary['artifact_context']['source_action_compare_summary_path']}",
            f"result: {summary['result']}",
            f"open_event_id: {event['open_event_id']}",
            f"open_event_status: {event['status']}",
            f"judgment_semantic_role: {judgment['semantic_role']}",
            f"judgment_status: {judgment['status']}",
            f"judgment_grade: {judgment['grade']}",
            f"judgment_accepted: {judgment['accepted']}",
            f"judgment_basis: {','.join(judgment['basis'])}",
            f"reason_kind: {event['reason']['kind']}",
            f"selected_consumer_id: {selected['consumer_id']}",
            f"selected_action_id: {selected['selected_action_id']}",
            f"candidate_consumer_count: {consumer['candidate_consumer_count']}",
            f"rejected_consumer_count: {consumer['rejected_consumer_count']}",
            f"compare_available: {compare['available']}",
            f"compare_verdict: {compare['action_verdict']}",
            f"workspace_facade_status: {workspace['status']}",
            f"witness_ref_count: {len(summary['witness_refs'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export an explainable open-event record from a selected opening-flow consumer plan action."
    )
    parser.add_argument("--action", required=True, help="Input opening-flow consumer plan action summary JSON.")
    parser.add_argument("--action-compare", default="", help="Optional action compare summary JSON to bind as judgment context.")
    parser.add_argument("--output-root", default="", help="Output root for open-event artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for open-event summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for open-event markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for open-event check text.")
    args = parser.parse_args()

    action_path = Path(args.action).resolve()
    compare_path = Path(args.action_compare).resolve() if choose_text(args.action_compare) else None
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-open-event").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.open-event.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.open-event.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.open-event.check.txt")

    try:
        summary = build_summary_model(action_path, compare_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    event = summary["open_event"]
    selected = summary["consumer_decision"]["selected_consumer"]
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT] event={event['open_event_id']} status={event['status']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT] selected={0} action={1} rejected={2}".format(
            selected["consumer_id"],
            selected["selected_action_id"],
            summary["consumer_decision"]["rejected_consumer_count"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
