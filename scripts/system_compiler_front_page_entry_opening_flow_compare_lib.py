from __future__ import annotations

from collections import OrderedDict
from pathlib import Path
from typing import Any

from system_compiler_front_page_route_lib import (
    load_json,
    normalize_path,
    resolve_output_path,
    write_text,
)


FLOW_SCHEMA = "system_compiler.front_page_entry_opening_flow/v0"
FLOW_KIND = "system_compiler.front_page_entry_opening_flow"


def choose_text(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def get_mapping(value: Any) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    return {}


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


def load_flow_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != FLOW_SCHEMA:
        raise ValueError(f"unsupported front page entry opening flow schema: {path}")
    if choose_text(summary.get("kind")) != FLOW_KIND:
        raise ValueError(f"unsupported front page entry opening flow kind: {path}")
    return summary


def build_front_page_surface(
    flow_summary: dict[str, Any],
    flow_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    front_page = get_mapping(flow_summary.get("front_page"))
    artifact_context = get_mapping(flow_summary.get("artifact_context"))
    title = choose_text(get_mapping(flow_summary.get("opening_flow")).get("title")) or "entry opening flow"
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", f"{role.replace('_', ' ')}: {title}"),
            ("role", role),
            ("summary_schema", FLOW_SCHEMA),
            ("summary_path", normalize_path(flow_summary_path)),
            (
                "report_markdown_path",
                normalize_path(
                    choose_text(front_page.get("report_markdown_path"))
                    or choose_text(artifact_context.get("report_markdown_path"))
                ),
            ),
            (
                "check_text_path",
                normalize_path(
                    choose_text(front_page.get("check_text_path"))
                    or choose_text(artifact_context.get("check_text_path"))
                ),
            ),
        ]
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


def build_flow_provenance_entry(
    flow_summary: dict[str, Any],
    flow_summary_path: Path,
    provenance_id: str,
    flow_role: str,
) -> OrderedDict[str, Any]:
    front_page = get_mapping(flow_summary.get("front_page"))
    artifact_context = get_mapping(flow_summary.get("artifact_context"))
    flow_status = get_mapping(flow_summary.get("flow_status"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("flow_role", flow_role),
            ("source_summary_schema", FLOW_SCHEMA),
            ("source_summary_path", normalize_path(flow_summary_path)),
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
            ("result", choose_text(flow_summary.get("result"))),
            ("expected_opener_count", int(flow_status.get("expected_opener_count", 0))),
            ("actual_opener_count", int(flow_status.get("actual_opener_count", 0))),
            ("available_projection_count", int(flow_status.get("available_projection_count", 0))),
            ("compare_context_count", int(flow_status.get("compare_context_count", 0))),
            ("inspector_ready_count", int(flow_status.get("inspector_ready_count", 0))),
            ("blocked_inspector_count", int(flow_status.get("blocked_inspector_count", 0))),
            ("completed_step_count", int(flow_status.get("completed_step_count", 0))),
        ]
    )


def normalize_flow_steps(flow_summary: dict[str, Any]) -> OrderedDict[str, OrderedDict[str, Any]]:
    steps: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for value in get_list(flow_summary.get("flow_steps")):
        step = get_mapping(value)
        step_id = choose_text(step.get("id"))
        if not step_id:
            continue
        steps[step_id] = OrderedDict(
            [
                ("id", step_id),
                ("label", choose_text(step.get("label"))),
                ("script_path", choose_text(step.get("script_path"))),
                ("output_root", choose_text(step.get("output_root"))),
                ("status", choose_text(step.get("status"))),
                ("produced_summary_count", int(step.get("produced_summary_count", 0))),
            ]
        )
    return steps


def normalize_opener_cases(flow_summary: dict[str, Any]) -> OrderedDict[str, OrderedDict[str, Any]]:
    artifact_context = get_mapping(flow_summary.get("artifact_context"))
    output_root = choose_text(artifact_context.get("output_root"))
    cases: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for value in get_list(flow_summary.get("opener_cases")):
        case = get_mapping(value)
        name = choose_text(case.get("name"))
        if not name:
            continue
        target_summary_path = choose_text(case.get("target_summary_path"))
        cases[name] = OrderedDict(
            [
                ("name", name),
                ("summary_path", choose_text(case.get("summary_path"))),
                ("report_markdown_path", choose_text(case.get("report_markdown_path"))),
                ("check_text_path", choose_text(case.get("check_text_path"))),
                ("open_action_status", choose_text(case.get("open_action_status"))),
                ("selected_tab_id", choose_text(case.get("selected_tab_id"))),
                ("selected_role", choose_text(case.get("selected_role"))),
                ("query_kind", choose_text(case.get("query_kind"))),
                ("query_scope", choose_text(case.get("query_scope"))),
                ("selection_rule", choose_text(case.get("selection_rule"))),
                ("target_summary_schema", choose_text(case.get("target_summary_schema"))),
                ("target_summary_kind", choose_text(case.get("target_summary_kind"))),
                ("target_summary_path", target_summary_path),
                (
                    "target_summary_compare_path",
                    normalize_path_for_compare(target_summary_path, output_root),
                ),
                ("projection_status", choose_text(case.get("projection_status"))),
                ("projection_kind", choose_text(case.get("projection_kind"))),
                ("projection_headline", choose_text(case.get("projection_headline"))),
                (
                    "projection_summary_lines",
                    ordered_unique([choose_text(item) for item in get_list(case.get("projection_summary_lines"))]),
                ),
                (
                    "projection_question_lines",
                    ordered_unique([choose_text(item) for item in get_list(case.get("projection_question_lines"))]),
                ),
                ("compare_context_available", bool(case.get("compare_context_available"))),
                ("landing_verdict", choose_text(case.get("landing_verdict"))),
                ("inspector_ready", bool(case.get("inspector_ready"))),
                ("inspector_mode", choose_text(case.get("inspector_mode"))),
                (
                    "inspector_blockers",
                    ordered_unique([choose_text(item) for item in get_list(case.get("inspector_blockers"))]),
                ),
            ]
        )
    return cases


def get_flow_status(summary: dict[str, Any]) -> dict[str, Any]:
    return get_mapping(summary.get("flow_status"))


def build_flow_status(baseline_summary: dict[str, Any], candidate_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    baseline_status = get_flow_status(baseline_summary)
    candidate_status = get_flow_status(candidate_summary)
    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_summary.get("result"))),
            ("candidate_result", choose_text(candidate_summary.get("result"))),
            ("baseline_expected_opener_count", int(baseline_status.get("expected_opener_count", 0))),
            ("candidate_expected_opener_count", int(candidate_status.get("expected_opener_count", 0))),
            ("baseline_actual_opener_count", int(baseline_status.get("actual_opener_count", 0))),
            ("candidate_actual_opener_count", int(candidate_status.get("actual_opener_count", 0))),
            ("baseline_available_projection_count", int(baseline_status.get("available_projection_count", 0))),
            ("candidate_available_projection_count", int(candidate_status.get("available_projection_count", 0))),
            ("baseline_compare_context_count", int(baseline_status.get("compare_context_count", 0))),
            ("candidate_compare_context_count", int(candidate_status.get("compare_context_count", 0))),
            ("baseline_inspector_ready_count", int(baseline_status.get("inspector_ready_count", 0))),
            ("candidate_inspector_ready_count", int(candidate_status.get("inspector_ready_count", 0))),
            ("baseline_blocked_inspector_count", int(baseline_status.get("blocked_inspector_count", 0))),
            ("candidate_blocked_inspector_count", int(candidate_status.get("blocked_inspector_count", 0))),
            ("baseline_completed_step_count", int(baseline_status.get("completed_step_count", 0))),
            ("candidate_completed_step_count", int(candidate_status.get("completed_step_count", 0))),
        ]
    )


def build_flow_changes(
    flow_status: dict[str, Any],
    baseline_steps: OrderedDict[str, OrderedDict[str, Any]],
    candidate_steps: OrderedDict[str, OrderedDict[str, Any]],
    baseline_cases: OrderedDict[str, OrderedDict[str, Any]],
    candidate_cases: OrderedDict[str, OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    step_changes = string_array_changes(list(baseline_steps.keys()), list(candidate_steps.keys()))
    case_changes = string_array_changes(list(baseline_cases.keys()), list(candidate_cases.keys()))
    changed = (
        bool(flow_status.get("baseline_result") != flow_status.get("candidate_result"))
        or flow_status.get("baseline_expected_opener_count") != flow_status.get("candidate_expected_opener_count")
        or flow_status.get("baseline_actual_opener_count") != flow_status.get("candidate_actual_opener_count")
        or flow_status.get("baseline_available_projection_count")
        != flow_status.get("candidate_available_projection_count")
        or flow_status.get("baseline_compare_context_count") != flow_status.get("candidate_compare_context_count")
        or flow_status.get("baseline_inspector_ready_count") != flow_status.get("candidate_inspector_ready_count")
        or flow_status.get("baseline_completed_step_count") != flow_status.get("candidate_completed_step_count")
        or has_array_changes(step_changes)
        or has_array_changes(case_changes)
    )
    return OrderedDict(
        [
            ("result_changed", bool(flow_status.get("baseline_result") != flow_status.get("candidate_result"))),
            (
                "expected_opener_count_change",
                count_change(
                    int(flow_status.get("baseline_expected_opener_count", 0)),
                    int(flow_status.get("candidate_expected_opener_count", 0)),
                ),
            ),
            (
                "actual_opener_count_change",
                count_change(
                    int(flow_status.get("baseline_actual_opener_count", 0)),
                    int(flow_status.get("candidate_actual_opener_count", 0)),
                ),
            ),
            (
                "available_projection_count_change",
                count_change(
                    int(flow_status.get("baseline_available_projection_count", 0)),
                    int(flow_status.get("candidate_available_projection_count", 0)),
                ),
            ),
            (
                "compare_context_count_change",
                count_change(
                    int(flow_status.get("baseline_compare_context_count", 0)),
                    int(flow_status.get("candidate_compare_context_count", 0)),
                ),
            ),
            (
                "inspector_ready_count_change",
                count_change(
                    int(flow_status.get("baseline_inspector_ready_count", 0)),
                    int(flow_status.get("candidate_inspector_ready_count", 0)),
                ),
            ),
            (
                "completed_step_count_change",
                count_change(
                    int(flow_status.get("baseline_completed_step_count", 0)),
                    int(flow_status.get("candidate_completed_step_count", 0)),
                ),
            ),
            ("step_id_changes", step_changes),
            ("opener_case_changes", case_changes),
            ("changed", changed),
        ]
    )


def compare_optional_field(
    notes: list[str],
    field_name: str,
    baseline_case: dict[str, Any],
    candidate_case: dict[str, Any],
) -> None:
    if baseline_case.get(field_name) != candidate_case.get(field_name):
        notes.append(
            "{0}: {1} -> {2}".format(
                field_name,
                baseline_case.get(field_name),
                candidate_case.get(field_name),
            )
        )


def classify_changed_case(baseline_case: dict[str, Any], candidate_case: dict[str, Any]) -> tuple[str, list[str]]:
    notes: list[str] = []
    for field_name in (
        "open_action_status",
        "selected_tab_id",
        "selected_role",
        "query_kind",
        "query_scope",
        "selection_rule",
        "target_summary_schema",
        "target_summary_kind",
        "target_summary_compare_path",
        "projection_status",
        "projection_kind",
        "projection_headline",
        "compare_context_available",
        "landing_verdict",
        "inspector_ready",
        "inspector_mode",
        "inspector_blockers",
    ):
        compare_optional_field(notes, field_name, baseline_case, candidate_case)

    impact = "neutral"
    if baseline_case.get("open_action_status") == "ready" and candidate_case.get("open_action_status") == "blocked":
        impact = "regression"
    if baseline_case.get("projection_status") == "available" and candidate_case.get("projection_status") != "available":
        impact = "regression"
    if bool(baseline_case.get("compare_context_available")) and not bool(candidate_case.get("compare_context_available")):
        impact = "regression"
    if bool(baseline_case.get("inspector_ready")) and not bool(candidate_case.get("inspector_ready")):
        impact = "regression"

    projection_summary_changes = string_array_changes(
        get_list(baseline_case.get("projection_summary_lines")),
        get_list(candidate_case.get("projection_summary_lines")),
    )
    if has_array_changes(projection_summary_changes):
        notes.append("projection_summary_lines changed")

    projection_question_changes = string_array_changes(
        get_list(baseline_case.get("projection_question_lines")),
        get_list(candidate_case.get("projection_question_lines")),
    )
    if has_array_changes(projection_question_changes):
        notes.append("projection_question_lines changed")

    if impact == "neutral":
        if baseline_case.get("projection_status") != "available" and candidate_case.get("projection_status") == "available":
            impact = "improvement"
        if not bool(baseline_case.get("compare_context_available")) and bool(candidate_case.get("compare_context_available")):
            impact = "improvement"
        if not bool(baseline_case.get("inspector_ready")) and bool(candidate_case.get("inspector_ready")):
            impact = "improvement"

    return impact, notes


def build_case_change_record(
    name: str,
    change_kind: str,
    impact: str,
    baseline_case: dict[str, Any] | None,
    candidate_case: dict[str, Any] | None,
    change_notes: list[str],
) -> OrderedDict[str, Any]:
    baseline = baseline_case or {}
    candidate = candidate_case or {}
    return OrderedDict(
        [
            ("name", name),
            ("change_kind", change_kind),
            ("impact", impact),
            ("baseline_summary_path", choose_text(baseline.get("summary_path"))),
            ("candidate_summary_path", choose_text(candidate.get("summary_path"))),
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
            ("baseline_projection_status", choose_text(baseline.get("projection_status"))),
            ("candidate_projection_status", choose_text(candidate.get("projection_status"))),
            ("baseline_projection_kind", choose_text(baseline.get("projection_kind"))),
            ("candidate_projection_kind", choose_text(candidate.get("projection_kind"))),
            ("baseline_projection_headline", choose_text(baseline.get("projection_headline"))),
            ("candidate_projection_headline", choose_text(candidate.get("projection_headline"))),
            (
                "projection_summary_line_changes",
                string_array_changes(
                    get_list(baseline.get("projection_summary_lines")),
                    get_list(candidate.get("projection_summary_lines")),
                ),
            ),
            (
                "projection_question_line_changes",
                string_array_changes(
                    get_list(baseline.get("projection_question_lines")),
                    get_list(candidate.get("projection_question_lines")),
                ),
            ),
            ("baseline_compare_context_available", bool(baseline.get("compare_context_available"))),
            ("candidate_compare_context_available", bool(candidate.get("compare_context_available"))),
            ("baseline_landing_verdict", choose_text(baseline.get("landing_verdict"))),
            ("candidate_landing_verdict", choose_text(candidate.get("landing_verdict"))),
            ("baseline_inspector_ready", bool(baseline.get("inspector_ready"))),
            ("candidate_inspector_ready", bool(candidate.get("inspector_ready"))),
            ("change_notes", ordered_unique(change_notes)),
        ]
    )


def build_opener_case_changes(
    baseline_cases: OrderedDict[str, OrderedDict[str, Any]],
    candidate_cases: OrderedDict[str, OrderedDict[str, Any]],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    changes: list[OrderedDict[str, Any]] = []
    summary = OrderedDict(
        [
            ("baseline_case_count", len(baseline_cases)),
            ("candidate_case_count", len(candidate_cases)),
            ("changed_case_count", 0),
            ("added_case_count", 0),
            ("removed_case_count", 0),
            ("unchanged_case_count", 0),
            ("regression_count", 0),
            ("improvement_count", 0),
            ("neutral_change_count", 0),
            ("projection_regression_count", 0),
            ("projection_improvement_count", 0),
            ("projection_headline_changed_count", 0),
            ("projection_summary_changed_count", 0),
            ("projection_question_changed_count", 0),
            ("compare_context_lost_count", 0),
            ("compare_context_gained_count", 0),
            ("inspector_readiness_changed_count", 0),
            ("query_changed_count", 0),
            ("target_changed_count", 0),
        ]
    )

    ordered_names = ordered_unique(list(baseline_cases.keys()) + list(candidate_cases.keys()))
    for name in ordered_names:
        baseline_case = baseline_cases.get(name)
        candidate_case = candidate_cases.get(name)
        if baseline_case is None and candidate_case is not None:
            impact = "improvement" if candidate_case.get("projection_status") == "available" else "neutral"
            changes.append(build_case_change_record(name, "added", impact, None, candidate_case, ["case added"]))
            summary["added_case_count"] += 1
        elif baseline_case is not None and candidate_case is None:
            impact = "regression" if baseline_case.get("projection_status") == "available" else "neutral"
            changes.append(build_case_change_record(name, "removed", impact, baseline_case, None, ["case removed"]))
            summary["removed_case_count"] += 1
        elif baseline_case is not None and candidate_case is not None:
            impact, notes = classify_changed_case(baseline_case, candidate_case)
            if notes:
                changes.append(build_case_change_record(name, "changed", impact, baseline_case, candidate_case, notes))
                summary["changed_case_count"] += 1
            else:
                summary["unchanged_case_count"] += 1

            if (
                baseline_case.get("projection_status") == "available"
                and candidate_case.get("projection_status") != "available"
            ):
                summary["projection_regression_count"] += 1
            if (
                baseline_case.get("projection_status") != "available"
                and candidate_case.get("projection_status") == "available"
            ):
                summary["projection_improvement_count"] += 1
            if baseline_case.get("projection_headline") != candidate_case.get("projection_headline"):
                summary["projection_headline_changed_count"] += 1
            if has_array_changes(
                string_array_changes(
                    get_list(baseline_case.get("projection_summary_lines")),
                    get_list(candidate_case.get("projection_summary_lines")),
                )
            ):
                summary["projection_summary_changed_count"] += 1
            if has_array_changes(
                string_array_changes(
                    get_list(baseline_case.get("projection_question_lines")),
                    get_list(candidate_case.get("projection_question_lines")),
                )
            ):
                summary["projection_question_changed_count"] += 1
            if bool(baseline_case.get("compare_context_available")) and not bool(
                candidate_case.get("compare_context_available")
            ):
                summary["compare_context_lost_count"] += 1
            if not bool(baseline_case.get("compare_context_available")) and bool(
                candidate_case.get("compare_context_available")
            ):
                summary["compare_context_gained_count"] += 1
            if bool(baseline_case.get("inspector_ready")) != bool(candidate_case.get("inspector_ready")):
                summary["inspector_readiness_changed_count"] += 1
            if (
                baseline_case.get("query_kind") != candidate_case.get("query_kind")
                or baseline_case.get("query_scope") != candidate_case.get("query_scope")
                or baseline_case.get("selection_rule") != candidate_case.get("selection_rule")
            ):
                summary["query_changed_count"] += 1
            if (
                baseline_case.get("target_summary_schema") != candidate_case.get("target_summary_schema")
                or baseline_case.get("target_summary_kind") != candidate_case.get("target_summary_kind")
                or baseline_case.get("target_summary_compare_path") != candidate_case.get("target_summary_compare_path")
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
    flow_status: dict[str, Any],
    case_changes: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    removed_cases: list[str] = []
    unavailable_projection_cases: list[str] = []
    lost_compare_context_cases: list[str] = []
    lost_inspector_ready_cases: list[str] = []
    narratives: list[str] = []

    for change in case_changes:
        name = choose_text(change.get("name"))
        if change.get("change_kind") == "removed":
            removed_cases.append(name)
        if (
            choose_text(change.get("baseline_projection_status")) == "available"
            and choose_text(change.get("candidate_projection_status")) != "available"
        ):
            unavailable_projection_cases.append(name)
        if bool(change.get("baseline_compare_context_available")) and not bool(
            change.get("candidate_compare_context_available")
        ):
            lost_compare_context_cases.append(name)
        if bool(change.get("baseline_inspector_ready")) and not bool(change.get("candidate_inspector_ready")):
            lost_inspector_ready_cases.append(name)

    failed_result_transition = (
        choose_text(flow_status.get("baseline_result")) == "ok"
        and choose_text(flow_status.get("candidate_result")) != "ok"
    )
    if failed_result_transition:
        narratives.append("candidate opening flow no longer reports ok")
    if removed_cases:
        narratives.append("candidate removed opener cases: {0}".format(", ".join(removed_cases)))
    if unavailable_projection_cases:
        narratives.append(
            "candidate lost available opener projections: {0}".format(", ".join(unavailable_projection_cases))
        )
    if lost_compare_context_cases:
        narratives.append("candidate lost compare context: {0}".format(", ".join(lost_compare_context_cases)))
    if lost_inspector_ready_cases:
        narratives.append("candidate lost inspector readiness: {0}".format(", ".join(lost_inspector_ready_cases)))

    changed = bool(
        failed_result_transition
        or removed_cases
        or unavailable_projection_cases
        or lost_compare_context_cases
        or lost_inspector_ready_cases
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("removed_case_names", ordered_unique(removed_cases)),
            ("unavailable_projection_case_names", ordered_unique(unavailable_projection_cases)),
            ("lost_compare_context_case_names", ordered_unique(lost_compare_context_cases)),
            ("lost_inspector_ready_case_names", ordered_unique(lost_inspector_ready_cases)),
            ("failed_result_transition", failed_result_transition),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_flow_verdict(
    flow_changes: dict[str, Any],
    case_summary: dict[str, Any],
    regression_surface: dict[str, Any],
    flow_status: dict[str, Any],
) -> str:
    if choose_text(flow_status.get("candidate_result")) != "ok":
        return "collapsed"
    if bool(regression_surface.get("changed")):
        return "drifted"
    if not bool(flow_changes.get("changed")) and int(case_summary.get("changed_case_count", 0)) == 0:
        return "standing"
    if (
        int(case_summary.get("improvement_count", 0)) > 0
        or int(case_summary.get("added_case_count", 0)) > 0
        or int(case_summary.get("projection_improvement_count", 0)) > 0
        or int(case_summary.get("compare_context_gained_count", 0)) > 0
        or int(flow_changes.get("available_projection_count_change", {}).get("delta", 0)) > 0
        or int(flow_changes.get("compare_context_count_change", {}).get("delta", 0)) > 0
        or int(flow_changes.get("inspector_ready_count_change", {}).get("delta", 0)) > 0
    ):
        return "improved"
    return "drifted"


def build_questions(
    flow_verdict: str,
    regression_surface: dict[str, Any],
    case_summary: dict[str, Any],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if flow_verdict == "standing":
        compare_questions.append("Should this opening flow baseline become the canonical explain-opening witness?")
    elif flow_verdict == "improved":
        compare_questions.append("Which added or improved opener case should be promoted into the next explain surface?")
    elif flow_verdict == "drifted":
        compare_questions.append("Which opener case changed first and does that drift affect the default explain entry?")
    else:
        compare_questions.append("Which opening-flow dependency collapsed before opener projection could be trusted?")

    if bool(regression_surface.get("changed")):
        next_questions.append("Should the removed or regressed opener cases block publishing this front-page entry?")
    if int(case_summary.get("query_changed_count", 0)) > 0:
        next_questions.append("Should query drift be surfaced as a first-class explain opening warning?")
    if int(case_summary.get("target_changed_count", 0)) > 0:
        next_questions.append("Should target summary drift trigger a deeper route or landing compare?")
    if int(case_summary.get("projection_summary_changed_count", 0)) > 0:
        next_questions.append("Which opener projection summary lines changed the user-facing diagnosis?")
    if int(case_summary.get("projection_question_changed_count", 0)) > 0:
        next_questions.append("Which opener projection questions should become the next diagnostic handoff?")
    if not next_questions:
        next_questions.append("Should later tools consume this compare object instead of re-reading smoke folders?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_compare_summary_model(
    baseline_flow_path: Path,
    candidate_flow_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_flow_summary(baseline_flow_path)
    candidate_summary = load_flow_summary(candidate_flow_path)
    baseline_steps = normalize_flow_steps(baseline_summary)
    candidate_steps = normalize_flow_steps(candidate_summary)
    baseline_cases = normalize_opener_cases(baseline_summary)
    candidate_cases = normalize_opener_cases(candidate_summary)

    flow_status = build_flow_status(baseline_summary, candidate_summary)
    flow_changes = build_flow_changes(flow_status, baseline_steps, candidate_steps, baseline_cases, candidate_cases)
    case_changes, case_summary = build_opener_case_changes(baseline_cases, candidate_cases)
    regression_surface = build_regression_surface(flow_status, case_changes)
    flow_verdict = build_flow_verdict(flow_changes, case_summary, regression_surface, flow_status)

    supporting_surfaces = [
        build_front_page_surface(baseline_summary, baseline_flow_path, "baseline_opening_flow", "baseline_opening_flow"),
        build_front_page_surface(candidate_summary, candidate_flow_path, "candidate_opening_flow", "candidate_opening_flow"),
    ]

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_entry_opening_flow_compare/v0"),
            ("kind", "system_compiler.front_page_entry_opening_flow_compare"),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow.py"),
            ("result", "ok"),
            (
                "opening_flow_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two front-page entry opening-flow witnesses preserve the same opener case surface.",
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
                "flow_provenance",
                [
                    build_flow_provenance_entry(baseline_summary, baseline_flow_path, "baseline_opening_flow", "baseline_flow"),
                    build_flow_provenance_entry(candidate_summary, candidate_flow_path, "candidate_opening_flow", "candidate_flow"),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_flow_summary_path", normalize_path(baseline_flow_path)),
                        ("candidate_flow_summary_path", normalize_path(candidate_flow_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("flow_verdict", flow_verdict),
            ("flow_status", flow_status),
            ("flow_changes", flow_changes),
            ("opener_case_summary", case_summary),
            ("opener_case_changes", case_changes),
            ("flow_regression_surface", regression_surface),
            ("questions", build_questions(flow_verdict, regression_surface, case_summary)),
            ("violations", []),
        ]
    )
