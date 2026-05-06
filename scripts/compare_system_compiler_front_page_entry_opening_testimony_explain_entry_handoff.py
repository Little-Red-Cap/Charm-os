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


HANDOFF_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0"
HANDOFF_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff"
HANDOFF_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0"
HANDOFF_COMPARE_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare"


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


def load_handoff_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != HANDOFF_SCHEMA:
        raise ValueError(f"unsupported opening testimony explain-entry handoff schema: {path}")
    if choose_text(summary.get("kind")) != HANDOFF_KIND:
        raise ValueError(f"unsupported opening testimony explain-entry handoff kind: {path}")
    return summary


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


def normalize_question(question_value: Any) -> OrderedDict[str, str]:
    question = get_mapping(question_value)
    return OrderedDict(
        [
            ("kind", choose_text(question.get("kind"))),
            ("summary", choose_text(question.get("summary"))),
            ("target_ref", choose_text(question.get("target_ref"))),
        ]
    )


def normalize_handoff(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    source_ref = get_mapping(summary.get("source_explain_entry_ref"))
    decision = get_mapping(summary.get("handoff_decision"))
    open_target = normalize_surface(summary.get("open_target"))
    opening_reason = get_mapping(summary.get("opening_reason"))
    action = get_mapping(summary.get("handoff_action"))
    supporting_targets = [normalize_surface(surface) for surface in get_list(summary.get("supporting_targets"))]
    next_questions = [normalize_question(question) for question in get_list(summary.get("next_questions"))]
    blockers = ordered_unique([choose_text(item) for item in get_list(action.get("blockers"))])

    supporting_target_refs = ordered_unique(
        [
            "{0}:{1}:{2}".format(
                surface["surface_id"],
                surface["summary_schema"],
                surface["summary_path"],
            )
            for surface in supporting_targets
        ]
    )
    typed_question_refs = ordered_unique(
        [
            "{0}:{1}:{2}".format(
                question["kind"],
                question["target_ref"],
                question["summary"],
            )
            for question in next_questions
        ]
    )

    return OrderedDict(
        [
            ("result", choose_text(summary.get("result"))),
            ("source_explain_entry_summary_schema", choose_text(source_ref.get("summary_schema"))),
            ("source_explain_entry_summary_kind", choose_text(source_ref.get("summary_kind"))),
            ("source_explain_entry_summary_path", normalize_optional_path(source_ref.get("summary_path"))),
            ("source_explain_entry_report_markdown_path", normalize_optional_path(source_ref.get("report_markdown_path"))),
            ("source_explain_entry_check_text_path", normalize_optional_path(source_ref.get("check_text_path"))),
            ("source_explain_entry_result", choose_text(source_ref.get("result"))),
            ("source_explain_entry_decision_status", choose_text(source_ref.get("decision_status"))),
            ("source_explain_entry_selection_kind", choose_text(source_ref.get("selection_kind"))),
            ("source_explain_entry_selected_surface_id", choose_text(source_ref.get("selected_surface_id"))),
            ("handoff_status", choose_text(decision.get("status"))),
            ("handoff_id", choose_text(decision.get("handoff_id"))),
            ("selected_tab_id", choose_text(decision.get("selected_tab_id"))),
            ("selected_role", choose_text(decision.get("selected_role"))),
            ("open_target_surface_id", choose_text(open_target.get("surface_id"))),
            ("open_target_label", choose_text(open_target.get("label"))),
            ("open_target_role", choose_text(open_target.get("role"))),
            ("open_target_summary_schema", choose_text(open_target.get("summary_schema"))),
            ("open_target_summary_kind", choose_text(open_target.get("summary_kind"))),
            ("open_target_summary_path", normalize_optional_path(open_target.get("summary_path"))),
            ("open_target_report_markdown_path", normalize_optional_path(open_target.get("report_markdown_path"))),
            ("open_target_check_text_path", normalize_optional_path(open_target.get("check_text_path"))),
            ("open_target_route_id", choose_text(open_target.get("route_id"))),
            ("open_target_depth", open_target.get("depth")),
            ("open_target_source", choose_text(open_target.get("source"))),
            ("opening_reason_kind", choose_text(opening_reason.get("kind"))),
            ("opening_reason_source_summary", choose_text(opening_reason.get("source_reason_summary"))),
            ("opening_reason_summary", choose_text(opening_reason.get("summary"))),
            ("opening_reason_source_summary_path", normalize_optional_path(opening_reason.get("source_summary_path"))),
            ("handoff_action_status", choose_text(action.get("status"))),
            ("handoff_action_kind", choose_text(action.get("action_kind"))),
            ("handoff_query_kind", choose_text(action.get("query_kind"))),
            ("handoff_query_scope", choose_text(action.get("query_scope"))),
            ("handoff_expected_consumer_operation", choose_text(action.get("expected_consumer_operation"))),
            ("handoff_target_summary_schema", choose_text(action.get("target_summary_schema"))),
            ("handoff_target_summary_kind", choose_text(action.get("target_summary_kind"))),
            ("handoff_target_summary_path", normalize_optional_path(action.get("target_summary_path"))),
            ("handoff_target_report_markdown_path", normalize_optional_path(action.get("target_report_markdown_path"))),
            ("handoff_target_check_text_path", normalize_optional_path(action.get("target_check_text_path"))),
            ("handoff_blockers", blockers),
            ("supporting_targets", supporting_targets),
            ("supporting_target_refs", supporting_target_refs),
            ("supporting_target_count", len(supporting_targets)),
            ("next_questions", next_questions),
            ("typed_question_refs", typed_question_refs),
            ("typed_question_kinds", ordered_unique([choose_text(question.get("kind")) for question in next_questions])),
            ("typed_question_count", len(next_questions)),
            ("violations", ordered_unique([choose_text(item) for item in get_list(summary.get("violations"))])),
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


def build_handoff_surface(
    handoff_summary: dict[str, Any],
    handoff_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    artifact_context = get_mapping(handoff_summary.get("artifact_context"))
    return make_surface(
        surface_id,
        f"{role.replace('_', ' ')}: opening testimony explain-entry handoff",
        role,
        HANDOFF_SCHEMA,
        normalize_path(handoff_path),
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
                    build_handoff_surface(
                        baseline_summary,
                        baseline_path,
                        "baseline_opening_testimony_explain_entry_handoff",
                        "baseline_opening_testimony_explain_entry_handoff",
                    ),
                    build_handoff_surface(
                        candidate_summary,
                        candidate_path,
                        "candidate_opening_testimony_explain_entry_handoff",
                        "candidate_opening_testimony_explain_entry_handoff",
                    ),
                ],
            ),
        ]
    )


def build_handoff_provenance_entry(
    handoff_summary: dict[str, Any],
    handoff_path: Path,
    provenance_id: str,
    handoff_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(handoff_summary.get("artifact_context"))
    decision = get_mapping(handoff_summary.get("handoff_decision"))
    action = get_mapping(handoff_summary.get("handoff_action"))
    target = get_mapping(handoff_summary.get("open_target"))
    source_ref = get_mapping(handoff_summary.get("source_explain_entry_ref"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("handoff_role", handoff_role),
            ("source_summary_schema", HANDOFF_SCHEMA),
            ("source_summary_path", normalize_path(handoff_path)),
            ("source_report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("source_check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("result", choose_text(handoff_summary.get("result"))),
            ("handoff_status", choose_text(decision.get("status"))),
            ("handoff_action_status", choose_text(action.get("status"))),
            ("open_target_surface_id", choose_text(target.get("surface_id"))),
            ("open_target_summary_schema", choose_text(target.get("summary_schema"))),
            ("open_target_summary_path", normalize_optional_path(target.get("summary_path"))),
            ("source_explain_entry_summary_path", normalize_optional_path(source_ref.get("summary_path"))),
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


def build_handoff_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_handoff_status", baseline["handoff_status"]),
            ("candidate_handoff_status", candidate["handoff_status"]),
            ("baseline_action_status", baseline["handoff_action_status"]),
            ("candidate_action_status", candidate["handoff_action_status"]),
            ("baseline_open_target_surface_id", baseline["open_target_surface_id"]),
            ("candidate_open_target_surface_id", candidate["open_target_surface_id"]),
            ("baseline_open_target_summary_schema", baseline["open_target_summary_schema"]),
            ("candidate_open_target_summary_schema", candidate["open_target_summary_schema"]),
            ("baseline_open_target_summary_kind", baseline["open_target_summary_kind"]),
            ("candidate_open_target_summary_kind", candidate["open_target_summary_kind"]),
            ("baseline_open_target_summary_path", baseline["open_target_summary_path"]),
            ("candidate_open_target_summary_path", candidate["open_target_summary_path"]),
            ("baseline_action_kind", baseline["handoff_action_kind"]),
            ("candidate_action_kind", candidate["handoff_action_kind"]),
            ("baseline_expected_consumer_operation", baseline["handoff_expected_consumer_operation"]),
            ("candidate_expected_consumer_operation", candidate["handoff_expected_consumer_operation"]),
            ("baseline_source_explain_entry_summary_path", baseline["source_explain_entry_summary_path"]),
            ("candidate_source_explain_entry_summary_path", candidate["source_explain_entry_summary_path"]),
        ]
    )


def build_regression_surface(
    source_ref_changes: dict[str, Any],
    decision_changes: dict[str, Any],
    open_target_changes: dict[str, Any],
    opening_reason_changes: dict[str, Any],
    action_changes: dict[str, Any],
    supporting_target_changes: dict[str, Any],
    next_question_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    candidate_blocked = (
        candidate["result"] != "ok"
        or candidate["handoff_status"] != "ready"
        or candidate["handoff_action_status"] != "ready"
    )
    candidate_recovered = (
        (baseline["result"] != "ok" or baseline["handoff_status"] != "ready")
        and candidate["result"] == "ok"
        and candidate["handoff_status"] == "ready"
        and candidate["handoff_action_status"] == "ready"
    )
    source_ref_changed = bool(source_ref_changes.get("changed"))
    decision_changed = bool(decision_changes.get("changed"))
    open_target_changed = bool(open_target_changes.get("changed"))
    opening_reason_changed = bool(opening_reason_changes.get("changed"))
    action_changed = bool(action_changes.get("changed"))
    supporting_targets_changed = bool(supporting_target_changes.get("changed"))
    next_questions_changed = bool(next_question_changes.get("changed"))
    same_open_target = not bool(
        open_target_changes.get("open_target_surface_id", {}).get("changed")
        or open_target_changes.get("open_target_summary_schema", {}).get("changed")
        or open_target_changes.get("open_target_summary_kind", {}).get("changed")
        or open_target_changes.get("open_target_summary_path", {}).get("changed")
    )
    same_handoff_action = not bool(
        action_changes.get("handoff_action_status", {}).get("changed")
        or action_changes.get("handoff_action_kind", {}).get("changed")
        or action_changes.get("handoff_query_kind", {}).get("changed")
        or action_changes.get("handoff_query_scope", {}).get("changed")
        or action_changes.get("handoff_expected_consumer_operation", {}).get("changed")
        or action_changes.get("handoff_target_summary_path", {}).get("changed")
    )

    if candidate_blocked:
        narratives.append("candidate opening testimony explain-entry handoff is blocked")
    if candidate_recovered:
        narratives.append("candidate opening testimony explain-entry handoff recovered to ready")
    if source_ref_changed:
        narratives.append("source explain-entry reference changed")
    if decision_changes.get("handoff_status", {}).get("changed"):
        narratives.append(
            "handoff status changed: {0} -> {1}".format(
                baseline["handoff_status"],
                candidate["handoff_status"],
            )
        )
    if open_target_changed:
        narratives.append(
            "open target changed: {0} -> {1}".format(
                baseline["open_target_surface_id"],
                candidate["open_target_surface_id"],
            )
        )
    if opening_reason_changed:
        narratives.append("opening reason changed")
    if action_changed:
        narratives.append("handoff action changed")
    if supporting_targets_changed:
        narratives.append("supporting handoff targets changed")
    if next_questions_changed:
        narratives.append("handoff typed next questions changed")

    changed = bool(
        candidate_blocked
        or candidate_recovered
        or source_ref_changed
        or decision_changed
        or open_target_changed
        or opening_reason_changed
        or action_changed
        or supporting_targets_changed
        or next_questions_changed
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("candidate_blocked", candidate_blocked),
            ("candidate_recovered", candidate_recovered),
            ("source_explain_entry_ref_changed", source_ref_changed),
            ("handoff_decision_changed", decision_changed),
            ("open_target_changed", open_target_changed),
            ("opening_reason_changed", opening_reason_changed),
            ("handoff_action_changed", action_changed),
            ("supporting_targets_changed", supporting_targets_changed),
            ("next_questions_changed", next_questions_changed),
            ("same_open_target", same_open_target),
            ("same_handoff_action", same_handoff_action),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_handoff_verdict(regression_surface: dict[str, Any], candidate: dict[str, Any], baseline: dict[str, Any]) -> str:
    if (
        candidate["result"] != "ok"
        or candidate["handoff_status"] != "ready"
        or candidate["handoff_action_status"] != "ready"
    ):
        return "collapsed"
    if (
        (baseline["result"] != "ok" or baseline["handoff_status"] != "ready")
        and candidate["handoff_status"] == "ready"
    ):
        return "improved"
    if not bool(regression_surface.get("same_open_target")) or not bool(regression_surface.get("same_handoff_action")):
        return "drifted"
    if bool(regression_surface.get("handoff_decision_changed")):
        return "drifted"
    return "standing"


def build_questions(handoff_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []
    if handoff_verdict == "standing":
        compare_questions.append("Do these handoffs still open the same explain target with the same action?")
    elif handoff_verdict == "improved":
        compare_questions.append("Should the recovered handoff become the next explain-entry handoff baseline?")
    elif handoff_verdict == "drifted":
        compare_questions.append("Did the default open target drift enough to require a new explain-entry review?")
    else:
        compare_questions.append("Which handoff blocker prevents the candidate explain target from opening?")

    if bool(regression_surface.get("source_explain_entry_ref_changed")):
        next_questions.append("Inspect the source explain-entry because the handoff input changed.")
    if bool(regression_surface.get("open_target_changed")):
        next_questions.append("Inspect the selected open target before publishing this handoff.")
    if bool(regression_surface.get("handoff_action_changed")):
        next_questions.append("Inspect the handoff action because the downstream open instruction changed.")
    if bool(regression_surface.get("supporting_targets_changed")):
        next_questions.append("Inspect supporting targets because side context changed.")
    if bool(regression_surface.get("next_questions_changed")):
        next_questions.append("Inspect typed next questions because the follow-up surface changed.")
    if not next_questions:
        next_questions.append("Compare another opening testimony handoff before promoting this seam.")
    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_handoff_path: Path,
    candidate_handoff_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_handoff_summary(baseline_handoff_path)
    candidate_summary = load_handoff_summary(candidate_handoff_path)
    baseline = normalize_handoff(baseline_summary)
    candidate = normalize_handoff(candidate_summary)

    source_ref_changes = build_field_changes(
        baseline,
        candidate,
        (
            "source_explain_entry_summary_schema",
            "source_explain_entry_summary_kind",
            "source_explain_entry_summary_path",
            "source_explain_entry_report_markdown_path",
            "source_explain_entry_check_text_path",
            "source_explain_entry_result",
            "source_explain_entry_decision_status",
            "source_explain_entry_selection_kind",
            "source_explain_entry_selected_surface_id",
        ),
    )
    decision_changes = build_field_changes(
        baseline,
        candidate,
        (
            "result",
            "handoff_status",
            "handoff_id",
            "selected_tab_id",
            "selected_role",
        ),
    )
    open_target_changes = build_field_changes(
        baseline,
        candidate,
        (
            "open_target_surface_id",
            "open_target_label",
            "open_target_role",
            "open_target_summary_schema",
            "open_target_summary_kind",
            "open_target_summary_path",
            "open_target_report_markdown_path",
            "open_target_check_text_path",
            "open_target_route_id",
            "open_target_depth",
            "open_target_source",
        ),
    )
    opening_reason_changes = build_field_changes(
        baseline,
        candidate,
        (
            "opening_reason_kind",
            "opening_reason_source_summary",
            "opening_reason_summary",
            "opening_reason_source_summary_path",
        ),
    )
    action_changes = build_field_changes(
        baseline,
        candidate,
        (
            "handoff_action_status",
            "handoff_action_kind",
            "handoff_query_kind",
            "handoff_query_scope",
            "handoff_expected_consumer_operation",
            "handoff_target_summary_schema",
            "handoff_target_summary_kind",
            "handoff_target_summary_path",
            "handoff_target_report_markdown_path",
            "handoff_target_check_text_path",
            "handoff_blockers",
        ),
    )
    supporting_target_changes = build_field_changes(
        baseline,
        candidate,
        (
            "supporting_target_refs",
            "supporting_targets",
            "supporting_target_count",
        ),
    )
    next_question_changes = build_field_changes(
        baseline,
        candidate,
        (
            "typed_question_refs",
            "typed_question_kinds",
            "typed_question_count",
        ),
    )
    handoff_status = build_handoff_status(baseline, candidate)
    regression_surface = build_regression_surface(
        source_ref_changes,
        decision_changes,
        open_target_changes,
        opening_reason_changes,
        action_changes,
        supporting_target_changes,
        next_question_changes,
        baseline,
        candidate,
    )
    handoff_verdict = build_handoff_verdict(regression_surface, candidate, baseline)
    changed_field_count = sum(
        int(changes["changed_field_count"])
        for changes in (
            source_ref_changes,
            decision_changes,
            open_target_changes,
            opening_reason_changes,
            action_changes,
            supporting_target_changes,
            next_question_changes,
        )
    )

    return OrderedDict(
        [
            ("schema", HANDOFF_COMPARE_SCHEMA),
            ("kind", HANDOFF_COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"),
            ("result", "ok"),
            (
                "opening_testimony_explain_entry_handoff_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Explain Entry Handoff Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two opening testimony handoffs still open the same explain target with the same action.",
                        ),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(
                    summary_path,
                    report_path,
                    check_path,
                    baseline_summary,
                    baseline_handoff_path,
                    candidate_summary,
                    candidate_handoff_path,
                ),
            ),
            (
                "handoff_provenance",
                [
                    build_handoff_provenance_entry(
                        baseline_summary,
                        baseline_handoff_path,
                        "baseline_opening_testimony_explain_entry_handoff",
                        "baseline_handoff",
                    ),
                    build_handoff_provenance_entry(
                        candidate_summary,
                        candidate_handoff_path,
                        "candidate_opening_testimony_explain_entry_handoff",
                        "candidate_handoff",
                    ),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_opening_testimony_explain_entry_handoff_summary_path", normalize_path(baseline_handoff_path)),
                        ("candidate_opening_testimony_explain_entry_handoff_summary_path", normalize_path(candidate_handoff_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("handoff_verdict", handoff_verdict),
            ("handoff_status", handoff_status),
            ("source_explain_entry_ref_changes", source_ref_changes),
            ("handoff_decision_changes", decision_changes),
            ("open_target_changes", open_target_changes),
            ("opening_reason_changes", opening_reason_changes),
            ("handoff_action_changes", action_changes),
            ("supporting_target_changes", supporting_target_changes),
            ("next_question_changes", next_question_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("source_explain_entry_ref_changed_field_count", int(source_ref_changes["changed_field_count"])),
                        ("handoff_decision_changed_field_count", int(decision_changes["changed_field_count"])),
                        ("open_target_changed_field_count", int(open_target_changes["changed_field_count"])),
                        ("opening_reason_changed_field_count", int(opening_reason_changes["changed_field_count"])),
                        ("handoff_action_changed_field_count", int(action_changes["changed_field_count"])),
                        ("supporting_target_changed_field_count", int(supporting_target_changes["changed_field_count"])),
                        ("next_question_changed_field_count", int(next_question_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("handoff_regression_surface", regression_surface),
            ("questions", build_questions(handoff_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["handoff_status"]
    change_summary = summary["change_summary"]
    regression = summary["handoff_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Explain Entry Handoff Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Handoff verdict: `{summary['handoff_verdict']}`",
        f"- Baseline handoff: `{summary['artifact_context']['baseline_opening_testimony_explain_entry_handoff_summary_path']}`",
        f"- Candidate handoff: `{summary['artifact_context']['candidate_opening_testimony_explain_entry_handoff_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Handoff Status",
        "- Baseline: `result={0} status={1} action={2} target={3}`".format(
            status["baseline_result"],
            status["baseline_handoff_status"],
            status["baseline_action_kind"],
            status["baseline_open_target_surface_id"],
        ),
        "- Candidate: `result={0} status={1} action={2} target={3}`".format(
            status["candidate_result"],
            status["candidate_handoff_status"],
            status["candidate_action_kind"],
            status["candidate_open_target_surface_id"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` source_ref=`{1}` decision=`{2}` target=`{3}` reason=`{4}` action=`{5}` supporting=`{6}` questions=`{7}`".format(
            change_summary["changed_field_count"],
            change_summary["source_explain_entry_ref_changed_field_count"],
            change_summary["handoff_decision_changed_field_count"],
            change_summary["open_target_changed_field_count"],
            change_summary["opening_reason_changed_field_count"],
            change_summary["handoff_action_changed_field_count"],
            change_summary["supporting_target_changed_field_count"],
            change_summary["next_question_changed_field_count"],
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
    status = summary["handoff_status"]
    change_summary = summary["change_summary"]
    regression = summary["handoff_regression_surface"]
    return "\n".join(
        [
            f"baseline_opening_testimony_explain_entry_handoff_summary_path: {summary['artifact_context']['baseline_opening_testimony_explain_entry_handoff_summary_path']}",
            f"candidate_opening_testimony_explain_entry_handoff_summary_path: {summary['artifact_context']['candidate_opening_testimony_explain_entry_handoff_summary_path']}",
            f"handoff_verdict: {summary['handoff_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_handoff_status: {status['baseline_handoff_status']}",
            f"candidate_handoff_status: {status['candidate_handoff_status']}",
            f"baseline_open_target_surface_id: {status['baseline_open_target_surface_id']}",
            f"candidate_open_target_surface_id: {status['candidate_open_target_surface_id']}",
            f"baseline_open_target_summary_path: {status['baseline_open_target_summary_path']}",
            f"candidate_open_target_summary_path: {status['candidate_open_target_summary_path']}",
            f"baseline_action_kind: {status['baseline_action_kind']}",
            f"candidate_action_kind: {status['candidate_action_kind']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"open_target_changed: {regression['open_target_changed']}",
            f"handoff_action_changed: {regression['handoff_action_changed']}",
            f"same_open_target: {regression['same_open_target']}",
            f"same_handoff_action: {regression['same_handoff_action']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two opening testimony explain-entry handoff summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline opening testimony explain-entry handoff summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening testimony explain-entry handoff summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opening testimony explain-entry handoff compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opening testimony explain-entry handoff compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opening testimony explain-entry handoff compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opening testimony explain-entry handoff compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-testimony.explain-entry.handoff.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-testimony.explain-entry.handoff.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE] verdict={summary['handoff_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
