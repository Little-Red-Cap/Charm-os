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


OPEN_EVENT_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event/v0"
OPEN_EVENT_KIND = "system_compiler.front_page_entry_opening_flow_open_event"
OPEN_EVENT_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event_compare/v0"
OPEN_EVENT_COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_open_event_compare"


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


def load_open_event_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != OPEN_EVENT_SCHEMA:
        raise ValueError(f"unsupported opening-flow open-event schema: {path}")
    if choose_text(summary.get("kind")) != OPEN_EVENT_KIND:
        raise ValueError(f"unsupported opening-flow open-event kind: {path}")
    return summary


def normalize_compare_path(path_value: Any, output_root_value: Any) -> str:
    path_text = choose_text(path_value)
    if not path_text:
        return ""

    output_root_text = choose_text(output_root_value)
    if output_root_text:
        try:
            path = Path(path_text).resolve()
            output_root = Path(output_root_text).resolve()
            path.relative_to(output_root)
            return "$open-event-output"
        except Exception:
            pass

    return normalize_optional_path(path_text)


def normalize_reason(reason_value: Any) -> OrderedDict[str, Any]:
    reason = get_mapping(reason_value)
    return OrderedDict(
        [
            ("kind", choose_text(reason.get("kind"))),
            ("summary", choose_text(reason.get("summary"))),
            ("source_summary_path", normalize_optional_path(reason.get("source_summary_path"))),
            ("drift_changed", bool(reason.get("drift_changed"))),
            ("drift_verdict", choose_text(reason.get("drift_verdict"))),
        ]
    )


def normalize_witness_refs(refs_value: Any, output_root_value: Any) -> list[OrderedDict[str, str]]:
    refs: list[OrderedDict[str, str]] = []
    for ref_value in get_list(refs_value):
        ref = get_mapping(ref_value)
        role = choose_text(ref.get("role"))
        summary_schema = choose_text(ref.get("summary_schema"))
        summary_path = "$open-event-self" if role == "open_event" else normalize_compare_path(ref.get("summary_path"), output_root_value)
        refs.append(
            OrderedDict(
                [
                    ("role", role),
                    ("summary_schema", summary_schema),
                    ("summary_path", summary_path),
                ]
            )
        )
    return refs


def first_action_record(summary: dict[str, Any]) -> dict[str, Any]:
    records = get_list(summary.get("action_records"))
    return get_mapping(records[0]) if records else {}


def normalize_open_event_summary(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    output_root = artifact_context.get("output_root")
    event = get_mapping(summary.get("open_event"))
    source_artifact = get_mapping(event.get("source_artifact"))
    reason = normalize_reason(event.get("reason"))
    decision = get_mapping(summary.get("consumer_decision"))
    selected = get_mapping(decision.get("selected_consumer"))
    plan = get_mapping(summary.get("plan"))
    record = first_action_record(summary)
    expected = get_mapping(record.get("expected"))
    action_result = get_mapping(record.get("result"))
    compare = get_mapping(summary.get("compare_summary"))
    workspace = get_mapping(summary.get("workspace_facade"))
    diagnostic = get_mapping(summary.get("diagnostic_preview"))
    explanation = get_mapping(summary.get("explanation_view"))
    candidates = [get_mapping(candidate) for candidate in get_list(decision.get("candidate_consumers"))]
    rejected = [get_mapping(candidate) for candidate in get_list(decision.get("rejected_consumers"))]
    witness_refs = normalize_witness_refs(summary.get("witness_refs"), output_root)

    return OrderedDict(
        [
            ("result", choose_text(summary.get("result"))),
            ("open_event_id", choose_text(event.get("open_event_id"))),
            ("open_event_status", choose_text(event.get("status"))),
            ("reason", reason),
            ("reason_kind", choose_text(reason.get("kind"))),
            ("reason_summary", choose_text(reason.get("summary"))),
            ("source_summary_schema", choose_text(source_artifact.get("summary_schema"))),
            ("source_summary_kind", choose_text(source_artifact.get("summary_kind"))),
            ("source_summary_path", normalize_compare_path(source_artifact.get("summary_path"), output_root)),
            ("opener_summary_path", normalize_compare_path(source_artifact.get("opener_summary_path"), output_root)),
            ("selected_consumer_id", choose_text(selected.get("consumer_id"))),
            ("selected_action_id", choose_text(selected.get("selected_action_id"))),
            ("selected_entry_name", choose_text(selected.get("entry_name"))),
            ("selected_role", choose_text(selected.get("selected_role"))),
            ("query_kind", choose_text(selected.get("query_kind"))),
            ("query_scope", choose_text(selected.get("query_scope"))),
            ("operation", choose_text(selected.get("operation"))),
            ("projection_kind", choose_text(selected.get("projection_kind"))),
            ("chosen_by", choose_text(selected.get("chosen_by"))),
            ("candidate_action_ids", ordered_unique([choose_text(candidate.get("action_id")) for candidate in candidates])),
            ("candidate_consumer_ids", ordered_unique([choose_text(candidate.get("consumer_id")) for candidate in candidates])),
            ("candidate_consumer_count", int(decision.get("candidate_consumer_count", 0))),
            ("rejected_action_ids", ordered_unique([choose_text(candidate.get("action_id")) for candidate in rejected])),
            ("rejected_consumer_ids", ordered_unique([choose_text(candidate.get("consumer_id")) for candidate in rejected])),
            ("rejected_reasons", ordered_unique([choose_text(candidate.get("reason")) for candidate in rejected])),
            ("rejected_consumer_count", int(decision.get("rejected_consumer_count", 0))),
            ("decision_reason", choose_text(decision.get("decision_reason"))),
            ("plan_result", choose_text(plan.get("result"))),
            ("plan_status", choose_text(plan.get("execution_plan_status"))),
            ("planned_action_count", int(plan.get("planned_action_count", 0))),
            ("plan_default_action_id", choose_text(plan.get("default_action_id"))),
            ("plan_compare_action_id", choose_text(plan.get("compare_action_id"))),
            ("plan_selected_action_id", choose_text(plan.get("selected_action_id"))),
            ("record_action_id", choose_text(record.get("action_id"))),
            ("record_action_kind", choose_text(record.get("action_kind"))),
            ("record_entry_name", choose_text(record.get("entry_name"))),
            ("expected_operation", choose_text(expected.get("operation"))),
            ("expected_projection_kind", choose_text(expected.get("projection_kind"))),
            ("expected_target_summary_schema", choose_text(expected.get("target_summary_schema"))),
            ("expected_target_summary_kind", choose_text(expected.get("target_summary_kind"))),
            ("expected_target_summary_path", normalize_compare_path(expected.get("target_summary_path"), output_root)),
            ("action_result_status", choose_text(action_result.get("status"))),
            ("opener_surface_available", bool(action_result.get("opener_surface_available"))),
            ("action_result_opener_summary_path", normalize_compare_path(action_result.get("opener_summary_path"), output_root)),
            ("action_blockers", ordered_unique([choose_text(item) for item in get_list(action_result.get("blockers"))])),
            ("compare_available", bool(compare.get("available"))),
            ("compare_summary_path", normalize_compare_path(compare.get("summary_path"), output_root)),
            ("compare_action_verdict", choose_text(compare.get("action_verdict"))),
            ("compare_changed_field_count", int(compare.get("changed_field_count", 0))),
            ("compare_reason_changed", bool(compare.get("reason_changed"))),
            ("compare_narratives", ordered_unique([choose_text(item) for item in get_list(compare.get("narratives"))])),
            ("workspace_status", choose_text(workspace.get("status"))),
            ("workspace_facade_kind", choose_text(workspace.get("facade_kind"))),
            ("workspace_primary_summary_path", normalize_compare_path(workspace.get("primary_summary_path"), output_root)),
            ("diagnostic_available", bool(diagnostic.get("available"))),
            ("diagnostic_entry_name", choose_text(diagnostic.get("entry_name"))),
            ("diagnostic_projection_kind", choose_text(diagnostic.get("projection_kind"))),
            ("diagnostic_headline", choose_text(diagnostic.get("headline"))),
            ("diagnostic_summary_lines", [choose_text(item) for item in get_list(diagnostic.get("summary_lines"))]),
            ("diagnostic_question_lines", [choose_text(item) for item in get_list(diagnostic.get("question_lines"))]),
            ("diagnostic_line_count", int(diagnostic.get("line_count", 0))),
            ("diagnostic_question_count", int(diagnostic.get("question_count", 0))),
            ("diagnostic_blockers", ordered_unique([choose_text(item) for item in get_list(diagnostic.get("blockers"))])),
            ("witness_refs", witness_refs),
            ("witness_roles", ordered_unique([choose_text(ref.get("role")) for ref in witness_refs])),
            ("witness_summary_refs", ordered_unique([f"{ref['role']}:{ref['summary_schema']}:{ref['summary_path']}" for ref in witness_refs])),
            ("witness_ref_count", len(witness_refs)),
            ("explanation_status", choose_text(explanation.get("status"))),
            ("explanation_why_opened", choose_text(explanation.get("why_opened"))),
            ("explanation_chosen_consumer", choose_text(explanation.get("chosen_consumer"))),
            ("explanation_compare_result", choose_text(explanation.get("compare_result"))),
            ("explanation_text_lines", [choose_text(item) for item in get_list(explanation.get("text_lines"))]),
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


def build_event_surface(open_event_summary: dict[str, Any], open_event_path: Path, surface_id: str, role: str) -> OrderedDict[str, str]:
    artifact_context = get_mapping(open_event_summary.get("artifact_context"))
    return make_surface(
        surface_id,
        f"{role.replace('_', ' ')}: opening-flow open event",
        role,
        OPEN_EVENT_SCHEMA,
        normalize_path(open_event_path),
        normalize_optional_path(artifact_context.get("report_markdown_path")),
        normalize_optional_path(artifact_context.get("check_text_path")),
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    baseline_summary: dict[str, Any],
    baseline_path: Path,
    candidate_summary: dict[str, Any],
    candidate_path: Path,
) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            (
                "supporting_surfaces",
                [
                    build_event_surface(baseline_summary, baseline_path, "baseline_open_event", "baseline_open_event"),
                    build_event_surface(candidate_summary, candidate_path, "candidate_open_event", "candidate_open_event"),
                ],
            ),
        ]
    )


def build_event_provenance_entry(
    open_event_summary: dict[str, Any],
    open_event_path: Path,
    provenance_id: str,
    event_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(open_event_summary.get("artifact_context"))
    event = get_mapping(open_event_summary.get("open_event"))
    decision = get_mapping(open_event_summary.get("consumer_decision"))
    selected = get_mapping(decision.get("selected_consumer"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("event_role", event_role),
            ("source_summary_schema", OPEN_EVENT_SCHEMA),
            ("source_summary_path", normalize_path(open_event_path)),
            ("source_report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("source_check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("result", choose_text(open_event_summary.get("result"))),
            ("open_event_id", choose_text(event.get("open_event_id"))),
            ("open_event_status", choose_text(event.get("status"))),
            ("selected_consumer_id", choose_text(selected.get("consumer_id"))),
            ("selected_action_id", choose_text(selected.get("selected_action_id"))),
        ]
    )


def build_field_change(baseline: Any, candidate: Any) -> OrderedDict[str, Any]:
    return OrderedDict([("baseline", baseline), ("candidate", candidate), ("changed", baseline != candidate)])


def build_field_changes(
    baseline: dict[str, Any],
    candidate: dict[str, Any],
    fields: tuple[str, ...],
) -> OrderedDict[str, Any]:
    changes = OrderedDict()
    changed_fields: list[str] = []
    for field in fields:
        change = build_field_change(baseline.get(field), candidate.get(field))
        changes[field] = change
        if bool(change["changed"]):
            changed_fields.append(field)
    changes["changed_fields"] = changed_fields
    changes["changed_field_count"] = len(changed_fields)
    changes["changed"] = bool(changed_fields)
    return changes


def build_event_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_open_event_id", baseline["open_event_id"]),
            ("candidate_open_event_id", candidate["open_event_id"]),
            ("baseline_open_event_status", baseline["open_event_status"]),
            ("candidate_open_event_status", candidate["open_event_status"]),
            ("baseline_selected_consumer_id", baseline["selected_consumer_id"]),
            ("candidate_selected_consumer_id", candidate["selected_consumer_id"]),
            ("baseline_selected_action_id", baseline["selected_action_id"]),
            ("candidate_selected_action_id", candidate["selected_action_id"]),
        ]
    )


def build_regression_surface(
    event_changes: dict[str, Any],
    consumer_changes: dict[str, Any],
    plan_changes: dict[str, Any],
    action_changes: dict[str, Any],
    compare_changes: dict[str, Any],
    workspace_changes: dict[str, Any],
    witness_changes: dict[str, Any],
    diagnostic_changes: dict[str, Any],
    explanation_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    failed_result_transition = baseline["result"] == "ok" and candidate["result"] != "ok"
    blocked_event_transition = baseline["open_event_status"] != "blocked" and candidate["open_event_status"] == "blocked"
    event_id_changed = bool(event_changes.get("open_event_id", {}).get("changed"))
    event_status_changed = bool(event_changes.get("open_event_status", {}).get("changed"))
    reason_changed = bool(event_changes.get("reason", {}).get("changed"))
    selected_consumer_changed = bool(consumer_changes.get("selected_consumer_id", {}).get("changed"))
    selected_action_changed = bool(consumer_changes.get("selected_action_id", {}).get("changed"))
    candidate_set_changed = bool(consumer_changes.get("candidate_action_ids", {}).get("changed")) or bool(
        consumer_changes.get("candidate_consumer_ids", {}).get("changed")
    )
    rejected_reason_changed = bool(consumer_changes.get("rejected_reasons", {}).get("changed"))
    plan_selected_action_changed = bool(plan_changes.get("plan_selected_action_id", {}).get("changed"))
    action_result_changed = bool(action_changes.get("action_result_status", {}).get("changed")) or bool(
        action_changes.get("action_blockers", {}).get("changed")
    )
    compare_context_changed = bool(compare_changes.get("compare_available", {}).get("changed")) or bool(
        compare_changes.get("compare_action_verdict", {}).get("changed")
    )
    compare_drift_added = choose_text(baseline["compare_action_verdict"]) not in ("drifted", "collapsed") and choose_text(
        candidate["compare_action_verdict"]
    ) in ("drifted", "collapsed")
    compare_context_lost = bool(baseline["compare_available"]) and not bool(candidate["compare_available"])
    workspace_facade_changed = bool(workspace_changes.get("workspace_status", {}).get("changed")) or bool(
        workspace_changes.get("workspace_primary_summary_path", {}).get("changed")
    )
    witness_set_changed = bool(witness_changes.get("witness_summary_refs", {}).get("changed"))
    diagnostic_preview_changed = bool(diagnostic_changes.get("diagnostic_headline", {}).get("changed")) or bool(
        diagnostic_changes.get("diagnostic_summary_lines", {}).get("changed")
    ) or bool(diagnostic_changes.get("diagnostic_question_lines", {}).get("changed"))
    explanation_changed = bool(explanation_changes.get("explanation_text_lines", {}).get("changed"))

    if failed_result_transition:
        narratives.append("candidate open event no longer reports ok")
    if blocked_event_transition:
        narratives.append("candidate open event became blocked")
    if event_id_changed:
        narratives.append(f"open event id changed: {baseline['open_event_id']} -> {candidate['open_event_id']}")
    if event_status_changed:
        narratives.append(f"open event status changed: {baseline['open_event_status']} -> {candidate['open_event_status']}")
    if reason_changed:
        narratives.append("opening reason changed")
    if selected_consumer_changed:
        narratives.append(f"selected consumer changed: {baseline['selected_consumer_id']} -> {candidate['selected_consumer_id']}")
    if selected_action_changed:
        narratives.append(f"selected action changed: {baseline['selected_action_id']} -> {candidate['selected_action_id']}")
    if candidate_set_changed:
        narratives.append("candidate consumer set changed")
    if rejected_reason_changed:
        narratives.append("rejected consumer reasons changed")
    if plan_selected_action_changed:
        narratives.append("plan selected action changed")
    if action_result_changed:
        narratives.append("action result changed")
    if compare_context_changed:
        narratives.append("attached compare context changed")
    if compare_drift_added:
        narratives.append("candidate introduced drift compare context")
    if compare_context_lost:
        narratives.append("candidate lost compare context")
    if workspace_facade_changed:
        narratives.append("workspace facade changed")
    if witness_set_changed:
        narratives.append("witness refs changed")
    if diagnostic_preview_changed:
        narratives.append("diagnostic preview changed")
    if explanation_changed:
        narratives.append("explanation view changed")

    changed = bool(
        failed_result_transition
        or blocked_event_transition
        or event_id_changed
        or event_status_changed
        or reason_changed
        or selected_consumer_changed
        or selected_action_changed
        or candidate_set_changed
        or rejected_reason_changed
        or plan_selected_action_changed
        or action_result_changed
        or compare_context_changed
        or compare_drift_added
        or compare_context_lost
        or workspace_facade_changed
        or witness_set_changed
        or diagnostic_preview_changed
        or explanation_changed
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("failed_result_transition", failed_result_transition),
            ("blocked_event_transition", blocked_event_transition),
            ("event_id_changed", event_id_changed),
            ("event_status_changed", event_status_changed),
            ("reason_changed", reason_changed),
            ("selected_consumer_changed", selected_consumer_changed),
            ("selected_action_changed", selected_action_changed),
            ("candidate_set_changed", candidate_set_changed),
            ("rejected_reason_changed", rejected_reason_changed),
            ("plan_selected_action_changed", plan_selected_action_changed),
            ("action_result_changed", action_result_changed),
            ("compare_context_changed", compare_context_changed),
            ("compare_drift_added", compare_drift_added),
            ("compare_context_lost", compare_context_lost),
            ("workspace_facade_changed", workspace_facade_changed),
            ("witness_set_changed", witness_set_changed),
            ("diagnostic_preview_changed", diagnostic_preview_changed),
            ("explanation_changed", explanation_changed),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_event_verdict(regression_surface: dict[str, Any], candidate: dict[str, Any], baseline: dict[str, Any]) -> str:
    if candidate["result"] != "ok" or candidate["open_event_status"] == "blocked":
        return "collapsed"
    if not bool(regression_surface.get("changed")):
        return "standing"
    if baseline["open_event_status"] == "accepted_with_drift" and candidate["open_event_status"] == "accepted":
        return "improved"
    return "drifted"


def build_questions(event_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []
    if event_verdict == "standing":
        compare_questions.append("Should this open-event witness become a canonical self-compare baseline?")
    elif event_verdict == "improved":
        compare_questions.append("Which removed opening drift should be reflected in the explain surface first?")
    elif event_verdict == "drifted":
        compare_questions.append("Did this opening judgment drift enough to change the workspace facade?")
    else:
        compare_questions.append("Which opening dependency collapsed before the event could be accepted?")

    if bool(regression_surface.get("selected_consumer_changed")):
        next_questions.append("Should selected consumer drift require a rejected-consumer review?")
    if bool(regression_surface.get("compare_context_changed")):
        next_questions.append("Should compare context drift be rendered before opening the selected surface?")
    if bool(regression_surface.get("witness_set_changed")):
        next_questions.append("Should witness ref drift trigger an OpenEventWitness bundle refresh?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this open-event compare instead of diffing event JSON?")
    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_open_event_path: Path,
    candidate_open_event_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_open_event_summary(baseline_open_event_path)
    candidate_summary = load_open_event_summary(candidate_open_event_path)
    baseline = normalize_open_event_summary(baseline_summary)
    candidate = normalize_open_event_summary(candidate_summary)

    event_changes = build_field_changes(
        baseline,
        candidate,
        (
            "result",
            "open_event_id",
            "open_event_status",
            "reason",
            "reason_kind",
            "reason_summary",
            "source_summary_schema",
            "source_summary_kind",
            "source_summary_path",
            "opener_summary_path",
        ),
    )
    consumer_changes = build_field_changes(
        baseline,
        candidate,
        (
            "selected_consumer_id",
            "selected_action_id",
            "selected_entry_name",
            "selected_role",
            "query_kind",
            "query_scope",
            "operation",
            "projection_kind",
            "chosen_by",
            "candidate_action_ids",
            "candidate_consumer_ids",
            "candidate_consumer_count",
            "rejected_action_ids",
            "rejected_consumer_ids",
            "rejected_reasons",
            "rejected_consumer_count",
            "decision_reason",
        ),
    )
    plan_changes = build_field_changes(
        baseline,
        candidate,
        (
            "plan_result",
            "plan_status",
            "planned_action_count",
            "plan_default_action_id",
            "plan_compare_action_id",
            "plan_selected_action_id",
        ),
    )
    action_changes = build_field_changes(
        baseline,
        candidate,
        (
            "record_action_id",
            "record_action_kind",
            "record_entry_name",
            "expected_operation",
            "expected_projection_kind",
            "expected_target_summary_schema",
            "expected_target_summary_kind",
            "expected_target_summary_path",
            "action_result_status",
            "opener_surface_available",
            "action_result_opener_summary_path",
            "action_blockers",
        ),
    )
    compare_changes = build_field_changes(
        baseline,
        candidate,
        (
            "compare_available",
            "compare_summary_path",
            "compare_action_verdict",
            "compare_changed_field_count",
            "compare_reason_changed",
            "compare_narratives",
        ),
    )
    workspace_changes = build_field_changes(
        baseline,
        candidate,
        (
            "workspace_status",
            "workspace_facade_kind",
            "workspace_primary_summary_path",
        ),
    )
    witness_changes = build_field_changes(
        baseline,
        candidate,
        (
            "witness_roles",
            "witness_summary_refs",
            "witness_ref_count",
        ),
    )
    diagnostic_changes = build_field_changes(
        baseline,
        candidate,
        (
            "diagnostic_available",
            "diagnostic_entry_name",
            "diagnostic_projection_kind",
            "diagnostic_headline",
            "diagnostic_summary_lines",
            "diagnostic_question_lines",
            "diagnostic_line_count",
            "diagnostic_question_count",
            "diagnostic_blockers",
        ),
    )
    explanation_changes = build_field_changes(
        baseline,
        candidate,
        (
            "explanation_status",
            "explanation_why_opened",
            "explanation_chosen_consumer",
            "explanation_compare_result",
            "explanation_text_lines",
        ),
    )
    event_status = build_event_status(baseline, candidate)
    regression_surface = build_regression_surface(
        event_changes,
        consumer_changes,
        plan_changes,
        action_changes,
        compare_changes,
        workspace_changes,
        witness_changes,
        diagnostic_changes,
        explanation_changes,
        baseline,
        candidate,
    )
    event_verdict = build_event_verdict(regression_surface, candidate, baseline)
    changed_field_count = sum(
        int(changes["changed_field_count"])
        for changes in (
            event_changes,
            consumer_changes,
            plan_changes,
            action_changes,
            compare_changes,
            workspace_changes,
            witness_changes,
            diagnostic_changes,
            explanation_changes,
        )
    )
    return OrderedDict(
        [
            ("schema", OPEN_EVENT_COMPARE_SCHEMA),
            ("kind", OPEN_EVENT_COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow_open_event.py"),
            ("result", "ok"),
            (
                "opening_flow_open_event_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Open Event Compare"),
                        ("summary", "A compare object that checks whether two opening judgments preserve the same explainable opening semantics."),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(summary_path, report_path, check_path, baseline_summary, baseline_open_event_path, candidate_summary, candidate_open_event_path),
            ),
            (
                "event_provenance",
                [
                    build_event_provenance_entry(baseline_summary, baseline_open_event_path, "baseline_open_event", "baseline_event"),
                    build_event_provenance_entry(candidate_summary, candidate_open_event_path, "candidate_open_event", "candidate_event"),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_open_event_summary_path", normalize_path(baseline_open_event_path)),
                        ("candidate_open_event_summary_path", normalize_path(candidate_open_event_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("event_verdict", event_verdict),
            ("event_status", event_status),
            ("event_changes", event_changes),
            ("consumer_decision_changes", consumer_changes),
            ("plan_changes", plan_changes),
            ("action_record_changes", action_changes),
            ("compare_context_changes", compare_changes),
            ("workspace_facade_changes", workspace_changes),
            ("witness_ref_changes", witness_changes),
            ("diagnostic_preview_changes", diagnostic_changes),
            ("explanation_view_changes", explanation_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("event_changed_field_count", int(event_changes["changed_field_count"])),
                        ("consumer_changed_field_count", int(consumer_changes["changed_field_count"])),
                        ("plan_changed_field_count", int(plan_changes["changed_field_count"])),
                        ("action_changed_field_count", int(action_changes["changed_field_count"])),
                        ("compare_changed_field_count", int(compare_changes["changed_field_count"])),
                        ("workspace_changed_field_count", int(workspace_changes["changed_field_count"])),
                        ("witness_changed_field_count", int(witness_changes["changed_field_count"])),
                        ("diagnostic_changed_field_count", int(diagnostic_changes["changed_field_count"])),
                        ("explanation_changed_field_count", int(explanation_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("event_regression_surface", regression_surface),
            ("questions", build_questions(event_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["event_status"]
    change_summary = summary["change_summary"]
    regression = summary["event_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Open Event Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Event verdict: `{summary['event_verdict']}`",
        f"- Baseline event: `{summary['artifact_context']['baseline_open_event_summary_path']}`",
        f"- Candidate event: `{summary['artifact_context']['candidate_open_event_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Event Status",
        "- Baseline: `result={0} status={1} event={2} consumer={3} action={4}`".format(
            status["baseline_result"],
            status["baseline_open_event_status"],
            status["baseline_open_event_id"],
            status["baseline_selected_consumer_id"],
            status["baseline_selected_action_id"],
        ),
        "- Candidate: `result={0} status={1} event={2} consumer={3} action={4}`".format(
            status["candidate_result"],
            status["candidate_open_event_status"],
            status["candidate_open_event_id"],
            status["candidate_selected_consumer_id"],
            status["candidate_selected_action_id"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` event=`{1}` consumer=`{2}` plan=`{3}` action=`{4}` compare=`{5}` workspace=`{6}` witness=`{7}` diagnostic=`{8}` explanation=`{9}`".format(
            change_summary["changed_field_count"],
            change_summary["event_changed_field_count"],
            change_summary["consumer_changed_field_count"],
            change_summary["plan_changed_field_count"],
            change_summary["action_changed_field_count"],
            change_summary["compare_changed_field_count"],
            change_summary["workspace_changed_field_count"],
            change_summary["witness_changed_field_count"],
            change_summary["diagnostic_changed_field_count"],
            change_summary["explanation_changed_field_count"],
        ),
    ]
    if regression["changed"]:
        lines.extend(["", "## Regression Surface"])
        for narrative in regression["narratives"]:
            lines.append(f"- {narrative}")
    else:
        lines.extend(["", "## Regression Surface", "- none"])
    lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        lines.append(f"- next: {question}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    status = summary["event_status"]
    change_summary = summary["change_summary"]
    regression = summary["event_regression_surface"]
    return "\n".join(
        [
            f"baseline_open_event_summary_path: {summary['artifact_context']['baseline_open_event_summary_path']}",
            f"candidate_open_event_summary_path: {summary['artifact_context']['candidate_open_event_summary_path']}",
            f"event_verdict: {summary['event_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_open_event_status: {status['baseline_open_event_status']}",
            f"candidate_open_event_status: {status['candidate_open_event_status']}",
            f"baseline_selected_consumer_id: {status['baseline_selected_consumer_id']}",
            f"candidate_selected_consumer_id: {status['candidate_selected_consumer_id']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"event_status_changed: {regression['event_status_changed']}",
            f"selected_consumer_changed: {regression['selected_consumer_changed']}",
            f"compare_context_changed: {regression['compare_context_changed']}",
            f"witness_set_changed: {regression['witness_set_changed']}",
            f"diagnostic_preview_changed: {regression['diagnostic_preview_changed']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two opening-flow open-event summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline opening-flow open-event summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening-flow open-event summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for open-event compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for open-event compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for open-event compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for open-event compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-open-event-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.open-event.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.open-event.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.open-event.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE] verdict={summary['event_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
