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


ACTION_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0"
ACTION_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_action"
COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"


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


def get_plan_workspace_root(action_summary: dict[str, Any]) -> str:
    source_plan_summary_path = choose_text(get_mapping(action_summary.get("artifact_context")).get("source_plan_summary_path"))
    if not source_plan_summary_path:
        return ""

    try:
        source_path = Path(source_plan_summary_path).resolve()
        if source_path.parent.name.lower() == "plan":
            return normalize_path(source_path.parent.parent)
        return normalize_path(source_path.parent)
    except Exception:
        return ""


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


def normalize_opening_reason_for_compare(value: Any, root_value: str) -> OrderedDict[str, Any]:
    reason = get_mapping(value)
    return OrderedDict(
        [
            ("kind", choose_text(reason.get("kind"))),
            ("summary", choose_text(reason.get("summary"))),
            ("source_summary_compare_path", normalize_path_for_compare(choose_text(reason.get("source_summary_path")), root_value)),
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


def build_action_surface(action_summary: dict[str, Any], action_summary_path: Path, surface_id: str, role: str) -> OrderedDict[str, str]:
    front_page = get_mapping(action_summary.get("front_page"))
    artifact_context = get_mapping(action_summary.get("artifact_context"))
    return make_surface(
        surface_id=surface_id,
        label=f"{role.replace('_', ' ')}: opening-flow consumer plan action",
        role=role,
        summary_schema=ACTION_SCHEMA,
        summary_path=normalize_path(action_summary_path),
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


def normalize_action_summary(action_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    plan_root = get_plan_workspace_root(action_summary)
    selection = get_mapping(action_summary.get("selection_request"))
    source_plan = get_mapping(action_summary.get("source_plan"))
    selected = get_mapping(action_summary.get("selected_action"))
    open_action = get_mapping(action_summary.get("open_action"))
    opener_surface = get_mapping(action_summary.get("opener_surface"))
    receipt = get_mapping(action_summary.get("execution_receipt"))

    return OrderedDict(
        [
            ("result", choose_text(action_summary.get("result"))),
            ("source_plan_result", choose_text(source_plan.get("result"))),
            ("source_plan_status", choose_text(source_plan.get("execution_plan_status"))),
            ("source_plan_action_count", int(source_plan.get("planned_action_count", 0))),
            ("requested_action_id", choose_text(selection.get("requested_action_id"))),
            ("requested_action_kind", choose_text(selection.get("requested_action_kind"))),
            ("requested_entry_name", choose_text(selection.get("requested_entry_name"))),
            ("effective_selector", choose_text(selection.get("effective_selector"))),
            ("matched_action_count", int(selection.get("matched_action_count", 0))),
            ("open_status", choose_text(open_action.get("status"))),
            ("action_id", choose_text(open_action.get("action_id"))),
            ("selected_action_id", choose_text(selected.get("action_id"))),
            ("action_kind", choose_text(open_action.get("action_kind"))),
            ("entry_name", choose_text(open_action.get("entry_name"))),
            ("display_group", choose_text(open_action.get("display_group"))),
            ("selected_tab_id", choose_text(open_action.get("selected_tab_id"))),
            ("selected_role", choose_text(open_action.get("selected_role"))),
            ("query_kind", choose_text(open_action.get("query_kind"))),
            ("query_scope", choose_text(open_action.get("query_scope"))),
            ("target_summary_schema", choose_text(open_action.get("target_summary_schema"))),
            ("target_summary_kind", choose_text(open_action.get("target_summary_kind"))),
            ("target_summary_path", choose_text(open_action.get("target_summary_path"))),
            (
                "target_summary_compare_path",
                normalize_path_for_compare(choose_text(open_action.get("target_summary_path")), plan_root),
            ),
            ("projection_kind", choose_text(open_action.get("projection_kind"))),
            ("opening_reason", normalize_opening_reason_for_compare(open_action.get("opening_reason"), plan_root)),
            ("opening_reason_kind", choose_text(get_mapping(open_action.get("opening_reason")).get("kind"))),
            ("projection_headline", choose_text(open_action.get("projection_headline"))),
            ("reason", choose_text(open_action.get("reason"))),
            ("compare_context_available", bool(open_action.get("compare_context_available"))),
            ("landing_verdict", choose_text(open_action.get("landing_verdict"))),
            ("opener_summary_path", choose_text(open_action.get("opener_summary_path"))),
            (
                "opener_summary_compare_path",
                normalize_path_for_compare(choose_text(open_action.get("opener_summary_path")), plan_root),
            ),
            (
                "opener_report_compare_path",
                normalize_path_for_compare(choose_text(open_action.get("opener_report_markdown_path")), plan_root),
            ),
            (
                "opener_check_compare_path",
                normalize_path_for_compare(choose_text(open_action.get("opener_check_text_path")), plan_root),
            ),
            ("expected_consumer_operation", choose_text(open_action.get("expected_consumer_operation"))),
            ("open_blockers", ordered_unique([choose_text(item) for item in get_list(open_action.get("blockers"))])),
            ("opener_surface_available", bool(opener_surface.get("available"))),
            (
                "opener_surface_summary_compare_path",
                normalize_path_for_compare(choose_text(opener_surface.get("summary_path")), plan_root),
            ),
            ("consumer_operation", choose_text(receipt.get("consumer_operation"))),
            ("selected_rank", int(receipt.get("selected_rank", 0))),
            ("source_rank", int(receipt.get("source_rank", 0))),
            ("chosen_by", choose_text(receipt.get("chosen_by"))),
            ("inspector_ready", bool(receipt.get("inspector_ready"))),
            ("inspector_mode", choose_text(receipt.get("inspector_mode"))),
            (
                "inspector_blockers",
                ordered_unique([choose_text(item) for item in get_list(receipt.get("inspector_blockers"))]),
            ),
        ]
    )


def build_action_provenance_entry(
    action_summary: dict[str, Any],
    action_summary_path: Path,
    provenance_id: str,
    action_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(action_summary.get("artifact_context"))
    open_action = get_mapping(action_summary.get("open_action"))
    selection = get_mapping(action_summary.get("selection_request"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("action_role", action_role),
            ("source_summary_schema", ACTION_SCHEMA),
            ("source_summary_path", normalize_path(action_summary_path)),
            ("source_report_markdown_path", normalize_path(artifact_context.get("report_markdown_path", ""))),
            ("source_check_text_path", normalize_path(artifact_context.get("check_text_path", ""))),
            ("result", choose_text(action_summary.get("result"))),
            ("open_status", choose_text(open_action.get("status"))),
            ("effective_selector", choose_text(selection.get("effective_selector"))),
            ("action_id", choose_text(open_action.get("action_id"))),
            ("action_kind", choose_text(open_action.get("action_kind"))),
            ("entry_name", choose_text(open_action.get("entry_name"))),
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


def build_action_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_open_status", baseline["open_status"]),
            ("candidate_open_status", candidate["open_status"]),
            ("baseline_action_id", baseline["action_id"]),
            ("candidate_action_id", candidate["action_id"]),
            ("baseline_action_kind", baseline["action_kind"]),
            ("candidate_action_kind", candidate["action_kind"]),
            ("baseline_entry_name", baseline["entry_name"]),
            ("candidate_entry_name", candidate["entry_name"]),
        ]
    )


def build_regression_surface(
    action_status: dict[str, Any],
    selection_changes: dict[str, Any],
    open_action_changes: dict[str, Any],
    opener_surface_changes: dict[str, Any],
    receipt_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    failed_result_transition = baseline["result"] == "ok" and candidate["result"] != "ok"
    blocked_open_transition = baseline["open_status"] == "ready" and candidate["open_status"] != "ready"
    action_id_changed = bool(open_action_changes.get("action_id", {}).get("changed"))
    action_kind_changed = bool(open_action_changes.get("action_kind", {}).get("changed"))
    entry_changed = bool(open_action_changes.get("entry_name", {}).get("changed"))
    target_changed = bool(open_action_changes.get("target_summary_compare_path", {}).get("changed"))
    opener_changed = bool(open_action_changes.get("opener_summary_compare_path", {}).get("changed"))
    reason_changed = bool(open_action_changes.get("opening_reason", {}).get("changed")) or bool(
        open_action_changes.get("projection_headline", {}).get("changed")
    )
    operation_changed = bool(open_action_changes.get("expected_consumer_operation", {}).get("changed")) or bool(
        receipt_changes.get("consumer_operation", {}).get("changed")
    )
    lost_compare_context = bool(baseline["compare_context_available"]) and not bool(candidate["compare_context_available"])
    lost_inspector_ready = bool(baseline["inspector_ready"]) and not bool(candidate["inspector_ready"])
    opener_surface_lost = bool(baseline["opener_surface_available"]) and not bool(candidate["opener_surface_available"])
    blockers_added = len(get_list(candidate.get("open_blockers"))) > len(get_list(baseline.get("open_blockers")))

    if failed_result_transition:
        narratives.append("candidate action no longer reports ok")
    if blocked_open_transition:
        narratives.append("candidate open action is no longer ready")
    if action_id_changed:
        narratives.append(f"selected action id changed: {baseline['action_id']} -> {candidate['action_id']}")
    if action_kind_changed:
        narratives.append(f"selected action kind changed: {baseline['action_kind']} -> {candidate['action_kind']}")
    if entry_changed:
        narratives.append(f"selected entry changed: {baseline['entry_name']} -> {candidate['entry_name']}")
    if target_changed:
        narratives.append("target summary path changed")
    if opener_changed:
        narratives.append("opener summary path changed")
    if reason_changed:
        narratives.append("opening reason changed")
    if operation_changed:
        narratives.append("consumer operation changed")
    if lost_compare_context:
        narratives.append("candidate lost compare context")
    if lost_inspector_ready:
        narratives.append("candidate lost inspector readiness")
    if opener_surface_lost:
        narratives.append("candidate lost opener surface")
    if blockers_added:
        narratives.append("candidate introduced more open blockers")

    changed = bool(
        failed_result_transition
        or blocked_open_transition
        or action_id_changed
        or action_kind_changed
        or entry_changed
        or target_changed
        or opener_changed
        or reason_changed
        or operation_changed
        or lost_compare_context
        or lost_inspector_ready
        or opener_surface_lost
        or blockers_added
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("failed_result_transition", failed_result_transition),
            ("blocked_open_transition", blocked_open_transition),
            ("action_id_changed", action_id_changed),
            ("action_kind_changed", action_kind_changed),
            ("entry_changed", entry_changed),
            ("target_changed", target_changed),
            ("opener_changed", opener_changed),
            ("reason_changed", reason_changed),
            ("operation_changed", operation_changed),
            ("lost_compare_context", lost_compare_context),
            ("lost_inspector_ready", lost_inspector_ready),
            ("opener_surface_lost", opener_surface_lost),
            ("blockers_added", blockers_added),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_action_verdict(
    regression_surface: dict[str, Any],
    selection_changes: dict[str, Any],
    open_action_changes: dict[str, Any],
    opener_surface_changes: dict[str, Any],
    receipt_changes: dict[str, Any],
    candidate: dict[str, Any],
    baseline: dict[str, Any],
) -> str:
    if candidate["result"] != "ok" or candidate["open_status"] != "ready":
        return "collapsed"
    if bool(regression_surface.get("changed")):
        return "drifted"
    if not any(
        bool(changes.get("changed"))
        for changes in (selection_changes, open_action_changes, opener_surface_changes, receipt_changes)
    ):
        return "standing"
    if (not bool(baseline["inspector_ready"]) and bool(candidate["inspector_ready"])) or (
        not bool(baseline["compare_context_available"]) and bool(candidate["compare_context_available"])
    ):
        return "improved"
    return "drifted"


def build_questions(action_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if action_verdict == "standing":
        compare_questions.append("Should this action witness become the canonical explain-open comparison unit?")
    elif action_verdict == "improved":
        compare_questions.append("Which gained action capability should be surfaced first to the explain consumer?")
    elif action_verdict == "drifted":
        compare_questions.append("Did the selected explain-open action drift enough to require a consumer review?")
    else:
        compare_questions.append("Which selected action dependency collapsed before the explain consumer could open?")

    if bool(regression_surface.get("action_id_changed")):
        next_questions.append("Should selected-action drift block publishing this opening flow?")
    if bool(regression_surface.get("opener_changed")):
        next_questions.append("Should opener target drift trigger an opener-level compare?")
    if bool(regression_surface.get("target_changed")):
        next_questions.append("Should target drift trigger a deeper front-page route compare?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this action compare instead of diffing action JSON?")

    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_action_path: Path,
    candidate_action_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_action_summary(baseline_action_path)
    candidate_summary = load_action_summary(candidate_action_path)
    baseline = normalize_action_summary(baseline_summary)
    candidate = normalize_action_summary(candidate_summary)

    selection_changes = build_field_changes(
        baseline,
        candidate,
        (
            "requested_action_id",
            "requested_action_kind",
            "requested_entry_name",
            "effective_selector",
            "matched_action_count",
        ),
    )
    open_action_changes = build_field_changes(
        baseline,
        candidate,
        (
            "open_status",
            "action_id",
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
            "opening_reason",
            "opening_reason_kind",
            "projection_headline",
            "reason",
            "compare_context_available",
            "landing_verdict",
            "opener_summary_compare_path",
            "opener_report_compare_path",
            "opener_check_compare_path",
            "expected_consumer_operation",
            "open_blockers",
        ),
    )
    opener_surface_changes = build_field_changes(
        baseline,
        candidate,
        (
            "opener_surface_available",
            "opener_surface_summary_compare_path",
        ),
    )
    receipt_changes = build_field_changes(
        baseline,
        candidate,
        (
            "consumer_operation",
            "selected_rank",
            "source_rank",
            "chosen_by",
            "source_plan_action_count",
            "inspector_ready",
            "inspector_mode",
            "inspector_blockers",
        ),
    )
    action_status = build_action_status(baseline, candidate)
    regression_surface = build_regression_surface(
        action_status,
        selection_changes,
        open_action_changes,
        opener_surface_changes,
        receipt_changes,
        baseline,
        candidate,
    )
    action_verdict = build_action_verdict(
        regression_surface,
        selection_changes,
        open_action_changes,
        opener_surface_changes,
        receipt_changes,
        candidate,
        baseline,
    )
    changed_field_count = int(selection_changes["changed_field_count"]) + int(open_action_changes["changed_field_count"]) + int(
        opener_surface_changes["changed_field_count"]
    ) + int(receipt_changes["changed_field_count"])
    supporting_surfaces = [
        build_action_surface(baseline_summary, baseline_action_path, "baseline_plan_action", "baseline_plan_action"),
        build_action_surface(candidate_summary, candidate_action_path, "candidate_plan_action", "candidate_plan_action"),
    ]

    return OrderedDict(
        [
            ("schema", COMPARE_SCHEMA),
            ("kind", COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"),
            ("result", "ok"),
            (
                "opening_flow_consumer_plan_action_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Consumer Plan Action Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two consumer plan action witnesses preserve the same explain-open action.",
                        ),
                    ]
                ),
            ),
            ("front_page", build_front_page(summary_path, report_path, check_path, supporting_surfaces)),
            (
                "action_provenance",
                [
                    build_action_provenance_entry(
                        baseline_summary,
                        baseline_action_path,
                        "baseline_plan_action",
                        "baseline_action",
                    ),
                    build_action_provenance_entry(
                        candidate_summary,
                        candidate_action_path,
                        "candidate_plan_action",
                        "candidate_action",
                    ),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_action_summary_path", normalize_path(baseline_action_path)),
                        ("candidate_action_summary_path", normalize_path(candidate_action_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("action_verdict", action_verdict),
            ("action_status", action_status),
            ("selection_changes", selection_changes),
            ("open_action_changes", open_action_changes),
            ("opener_surface_changes", opener_surface_changes),
            ("execution_receipt_changes", receipt_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("selection_changed_field_count", int(selection_changes["changed_field_count"])),
                        ("open_action_changed_field_count", int(open_action_changes["changed_field_count"])),
                        ("opener_surface_changed_field_count", int(opener_surface_changes["changed_field_count"])),
                        ("execution_receipt_changed_field_count", int(receipt_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("action_regression_surface", regression_surface),
            ("questions", build_questions(action_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["action_status"]
    change_summary = summary["change_summary"]
    regression_surface = summary["action_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Consumer Plan Action Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Action verdict: `{summary['action_verdict']}`",
        f"- Baseline action: `{summary['artifact_context']['baseline_action_summary_path']}`",
        f"- Candidate action: `{summary['artifact_context']['candidate_action_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Action Status",
        "- Baseline: `result={0} open={1} action={2} kind={3} entry={4}`".format(
            status["baseline_result"],
            status["baseline_open_status"],
            status["baseline_action_id"],
            status["baseline_action_kind"],
            status["baseline_entry_name"],
        ),
        "- Candidate: `result={0} open={1} action={2} kind={3} entry={4}`".format(
            status["candidate_result"],
            status["candidate_open_status"],
            status["candidate_action_id"],
            status["candidate_action_kind"],
            status["candidate_entry_name"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` selection=`{1}` open_action=`{2}` opener_surface=`{3}` receipt=`{4}`".format(
            change_summary["changed_field_count"],
            change_summary["selection_changed_field_count"],
            change_summary["open_action_changed_field_count"],
            change_summary["opener_surface_changed_field_count"],
            change_summary["execution_receipt_changed_field_count"],
        ),
    ]

    if regression_surface["changed"]:
        lines.extend(["", "## Regression Surface"])
        for narrative in regression_surface["narratives"]:
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
    status = summary["action_status"]
    change_summary = summary["change_summary"]
    regression_surface = summary["action_regression_surface"]
    return "\n".join(
        [
            f"baseline_action_summary_path: {summary['artifact_context']['baseline_action_summary_path']}",
            f"candidate_action_summary_path: {summary['artifact_context']['candidate_action_summary_path']}",
            f"action_verdict: {summary['action_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_open_status: {status['baseline_open_status']}",
            f"candidate_open_status: {status['candidate_open_status']}",
            f"baseline_action_id: {status['baseline_action_id']}",
            f"candidate_action_id: {status['candidate_action_id']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"action_id_changed: {regression_surface['action_id_changed']}",
            f"opener_changed: {regression_surface['opener_changed']}",
            f"target_changed: {regression_surface['target_changed']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler front_page entry opening-flow consumer plan action summaries."
    )
    parser.add_argument("--baseline", required=True, help="Baseline opening-flow consumer plan action summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening-flow consumer plan action summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for action compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for action compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for action compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for action compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json")
    report_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-opening-flow.consumer.plan-action.compare.report.md",
    )
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.consumer.plan-action.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE] verdict={summary['action_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
