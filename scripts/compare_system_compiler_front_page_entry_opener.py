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


OPENER_SCHEMA = "system_compiler.front_page_entry_opener/v0"
OPENER_KIND = "system_compiler.front_page_entry_opener"
COMPARE_SCHEMA = "system_compiler.front_page_entry_opener_compare/v0"
COMPARE_KIND = "system_compiler.front_page_entry_opener_compare"


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


def normalize_text_list(values: Any) -> list[str]:
    return ordered_unique([choose_text(item) for item in get_list(values)])


def format_value(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (list, tuple)):
        return json.dumps([format_value(item) for item in value], ensure_ascii=False)
    if value is None:
        return ""
    return str(value)


def normalize_path_for_compare(path_value: Any, root_value: Any) -> str:
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


def load_opener_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != OPENER_SCHEMA:
        raise ValueError(f"unsupported front page entry opener schema: {path}")
    if choose_text(summary.get("kind")) != OPENER_KIND:
        raise ValueError(f"unsupported front page entry opener kind: {path}")
    return summary


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


def build_opener_surface(
    opener_summary: dict[str, Any],
    opener_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    front_page = get_mapping(opener_summary.get("front_page"))
    artifact_context = get_mapping(opener_summary.get("artifact_context"))
    return make_surface(
        surface_id=surface_id,
        label=f"{role.replace('_', ' ')}: front page entry opener",
        role=role,
        summary_schema=OPENER_SCHEMA,
        summary_path=normalize_path(opener_summary_path),
        report_markdown_path=normalize_path(
            choose_text(front_page.get("report_markdown_path"))
            or choose_text(artifact_context.get("report_markdown_path"))
        ),
        check_text_path=normalize_path(
            choose_text(front_page.get("check_text_path")) or choose_text(artifact_context.get("check_text_path"))
        ),
    )


def build_opener_status(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
) -> OrderedDict[str, Any]:
    baseline_action = get_mapping(baseline_summary.get("open_action"))
    candidate_action = get_mapping(candidate_summary.get("open_action"))
    baseline_projection = get_mapping(baseline_summary.get("opened_projection"))
    candidate_projection = get_mapping(candidate_summary.get("opened_projection"))
    baseline_compare = get_mapping(baseline_summary.get("compare_context"))
    candidate_compare = get_mapping(candidate_summary.get("compare_context"))
    baseline_inspector = get_mapping(baseline_summary.get("inspector_invocation"))
    candidate_inspector = get_mapping(candidate_summary.get("inspector_invocation"))

    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_summary.get("result"))),
            ("candidate_result", choose_text(candidate_summary.get("result"))),
            ("baseline_open_action_status", choose_text(baseline_action.get("status"))),
            ("candidate_open_action_status", choose_text(candidate_action.get("status"))),
            ("baseline_selected_tab_id", choose_text(baseline_action.get("selected_tab_id"))),
            ("candidate_selected_tab_id", choose_text(candidate_action.get("selected_tab_id"))),
            ("baseline_query_kind", choose_text(baseline_action.get("query_kind"))),
            ("candidate_query_kind", choose_text(candidate_action.get("query_kind"))),
            ("baseline_query_scope", choose_text(baseline_action.get("query_scope"))),
            ("candidate_query_scope", choose_text(candidate_action.get("query_scope"))),
            ("baseline_target_summary_schema", choose_text(baseline_action.get("target_summary_schema"))),
            ("candidate_target_summary_schema", choose_text(candidate_action.get("target_summary_schema"))),
            ("baseline_target_summary_kind", choose_text(baseline_action.get("target_summary_kind"))),
            ("candidate_target_summary_kind", choose_text(candidate_action.get("target_summary_kind"))),
            ("baseline_projection_status", choose_text(baseline_projection.get("status"))),
            ("candidate_projection_status", choose_text(candidate_projection.get("status"))),
            ("baseline_projection_kind", choose_text(baseline_projection.get("projection_kind"))),
            ("candidate_projection_kind", choose_text(candidate_projection.get("projection_kind"))),
            ("baseline_compare_context_available", bool(baseline_compare.get("available"))),
            ("candidate_compare_context_available", bool(candidate_compare.get("available"))),
            ("baseline_landing_verdict", choose_text(baseline_compare.get("landing_verdict"))),
            ("candidate_landing_verdict", choose_text(candidate_compare.get("landing_verdict"))),
            ("baseline_inspector_ready", bool(baseline_inspector.get("ready"))),
            ("candidate_inspector_ready", bool(candidate_inspector.get("ready"))),
        ]
    )


def build_opener_provenance_entry(
    opener_summary: dict[str, Any],
    opener_summary_path: Path,
    provenance_id: str,
    opener_role: str,
) -> OrderedDict[str, Any]:
    front_page = get_mapping(opener_summary.get("front_page"))
    artifact_context = get_mapping(opener_summary.get("artifact_context"))
    open_action = get_mapping(opener_summary.get("open_action"))
    opened_projection = get_mapping(opener_summary.get("opened_projection"))
    compare_context = get_mapping(opener_summary.get("compare_context"))
    inspector = get_mapping(opener_summary.get("inspector_invocation"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("opener_role", opener_role),
            ("source_summary_schema", OPENER_SCHEMA),
            ("source_summary_path", normalize_path(opener_summary_path)),
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
                    choose_text(front_page.get("check_text_path")) or choose_text(artifact_context.get("check_text_path"))
                ),
            ),
            ("result", choose_text(opener_summary.get("result"))),
            ("open_action_status", choose_text(open_action.get("status"))),
            ("selected_tab_id", choose_text(open_action.get("selected_tab_id"))),
            ("projection_kind", choose_text(opened_projection.get("projection_kind"))),
            ("compare_context_available", bool(compare_context.get("available"))),
            ("inspector_ready", bool(inspector.get("ready"))),
        ]
    )


def build_semantic_record(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    open_action = get_mapping(summary.get("open_action"))
    opening_reason = get_mapping(open_action.get("opening_reason"))
    compare_context = get_mapping(summary.get("compare_context"))
    inspector = get_mapping(summary.get("inspector_invocation"))
    projection = get_mapping(summary.get("opened_projection"))
    questions = get_mapping(summary.get("questions"))
    root = choose_text(artifact_context.get("output_root"))

    return OrderedDict(
        [
            ("result", choose_text(summary.get("result"))),
            ("open_action.status", choose_text(open_action.get("status"))),
            ("open_action.selected_tab_id", choose_text(open_action.get("selected_tab_id"))),
            ("open_action.selected_role", choose_text(open_action.get("selected_role"))),
            ("open_action.query_kind", choose_text(open_action.get("query_kind"))),
            ("open_action.query_scope", choose_text(open_action.get("query_scope"))),
            ("open_action.selection_rule", choose_text(open_action.get("selection_rule"))),
            ("open_action.compare_expected", bool(open_action.get("compare_expected"))),
            ("open_action.followup_query_kinds", normalize_text_list(open_action.get("followup_query_kinds"))),
            ("open_action.opening_reason_kind", choose_text(opening_reason.get("kind"))),
            ("open_action.opening_reason_drift_changed", bool(opening_reason.get("drift_changed"))),
            ("open_action.opening_reason_drift_verdict", choose_text(opening_reason.get("drift_verdict"))),
            ("open_action.target_summary_schema", choose_text(open_action.get("target_summary_schema"))),
            ("open_action.target_summary_kind", choose_text(open_action.get("target_summary_kind"))),
            (
                "open_action.target_summary_path",
                normalize_path_for_compare(open_action.get("target_summary_path"), root),
            ),
            ("compare_context.available", bool(compare_context.get("available"))),
            ("compare_context.related_landing_role", choose_text(compare_context.get("related_landing_role"))),
            ("compare_context.landing_verdict", choose_text(compare_context.get("landing_verdict"))),
            ("compare_context.primary_query_changed", bool(compare_context.get("primary_query_changed"))),
            ("compare_context.landing_regression_changed", bool(compare_context.get("landing_regression_changed"))),
            ("compare_context.query_regression_changed", bool(compare_context.get("query_regression_changed"))),
            ("compare_context.narratives", normalize_text_list(compare_context.get("narratives"))),
            ("inspector.ready", bool(inspector.get("ready"))),
            ("inspector.mode", choose_text(inspector.get("mode"))),
            ("inspector.query_kind", choose_text(inspector.get("query_kind"))),
            ("inspector.blockers", normalize_text_list(inspector.get("blockers"))),
            ("opened_projection.status", choose_text(projection.get("status"))),
            ("opened_projection.projection_kind", choose_text(projection.get("projection_kind"))),
            ("opened_projection.source_summary_schema", choose_text(projection.get("source_summary_schema"))),
            ("opened_projection.source_summary_kind", choose_text(projection.get("source_summary_kind"))),
            (
                "opened_projection.source_summary_path",
                normalize_path_for_compare(projection.get("source_summary_path"), root),
            ),
            ("opened_projection.headline", choose_text(projection.get("headline"))),
            ("opened_projection.summary_lines", normalize_text_list(projection.get("summary_lines"))),
            ("opened_projection.question_lines", normalize_text_list(projection.get("question_lines"))),
            (
                "opened_projection.evidence_paths",
                [normalize_path_for_compare(path, root) for path in normalize_text_list(projection.get("evidence_paths"))],
            ),
            (
                "opened_projection.compare_paths",
                [normalize_path_for_compare(path, root) for path in normalize_text_list(projection.get("compare_paths"))],
            ),
            ("opened_projection.blockers", normalize_text_list(projection.get("blockers"))),
            ("questions.compare_questions", normalize_text_list(questions.get("compare_questions"))),
            ("questions.next_questions", normalize_text_list(questions.get("next_questions"))),
        ]
    )


def field_group(field: str) -> str:
    if field.startswith("compare_context."):
        return "compare_context"
    if field.startswith("inspector."):
        return "inspector"
    if field.startswith("opened_projection."):
        return "projection"
    if field.startswith("questions."):
        return "questions"
    return "opening"


def classify_field_change(field: str, baseline_value: Any, candidate_value: Any) -> str:
    if field == "result" and choose_text(baseline_value) == "ok" and choose_text(candidate_value) != "ok":
        return "regression"
    if field == "open_action.status" and choose_text(baseline_value) == "ready" and choose_text(candidate_value) != "ready":
        return "regression"
    if field == "open_action.status" and choose_text(baseline_value) != "ready" and choose_text(candidate_value) == "ready":
        return "improvement"
    if field == "compare_context.available":
        if bool(baseline_value) and not bool(candidate_value):
            return "regression"
        if not bool(baseline_value) and bool(candidate_value):
            return "improvement"
    if field == "inspector.ready":
        if bool(baseline_value) and not bool(candidate_value):
            return "regression"
        if not bool(baseline_value) and bool(candidate_value):
            return "improvement"
    if field == "opened_projection.status":
        if choose_text(baseline_value) == "available" and choose_text(candidate_value) != "available":
            return "regression"
        if choose_text(baseline_value) != "available" and choose_text(candidate_value) == "available":
            return "improvement"
    if field == "opened_projection.projection_kind":
        if choose_text(baseline_value) and not choose_text(candidate_value):
            return "regression"
        if not choose_text(baseline_value) and choose_text(candidate_value):
            return "improvement"
    return "neutral"


def build_field_changes(
    baseline_record: dict[str, Any],
    candidate_record: dict[str, Any],
) -> tuple[list[OrderedDict[str, str]], OrderedDict[str, int]]:
    changes: list[OrderedDict[str, str]] = []
    summary = OrderedDict(
        [
            ("changed_field_count", 0),
            ("opening_field_changed_count", 0),
            ("projection_field_changed_count", 0),
            ("compare_context_field_changed_count", 0),
            ("inspector_field_changed_count", 0),
            ("question_field_changed_count", 0),
            ("regression_count", 0),
            ("improvement_count", 0),
            ("neutral_change_count", 0),
        ]
    )

    for field in baseline_record:
        baseline_value = baseline_record.get(field)
        candidate_value = candidate_record.get(field)
        if baseline_value == candidate_value:
            continue
        group = field_group(field)
        impact = classify_field_change(field, baseline_value, candidate_value)
        changes.append(
            OrderedDict(
                [
                    ("field", field),
                    ("group", group),
                    ("impact", impact),
                    ("baseline_value", format_value(baseline_value)),
                    ("candidate_value", format_value(candidate_value)),
                ]
            )
        )
        summary["changed_field_count"] += 1
        summary_key = "question_field_changed_count" if group == "questions" else f"{group}_field_changed_count"
        summary[summary_key] += 1
        if impact == "regression":
            summary["regression_count"] += 1
        elif impact == "improvement":
            summary["improvement_count"] += 1
        else:
            summary["neutral_change_count"] += 1

    return changes, summary


def build_opener_changes(field_changes: list[dict[str, str]]) -> OrderedDict[str, Any]:
    changed_fields = {choose_text(change.get("field")) for change in field_changes}
    return OrderedDict(
        [
            ("result_changed", "result" in changed_fields),
            ("open_action_status_changed", "open_action.status" in changed_fields),
            ("selected_tab_changed", "open_action.selected_tab_id" in changed_fields),
            (
                "query_changed",
                bool(
                    {
                        "open_action.query_kind",
                        "open_action.query_scope",
                        "open_action.selection_rule",
                        "open_action.followup_query_kinds",
                    }
                    & changed_fields
                ),
            ),
            (
                "target_changed",
                bool(
                    {
                        "open_action.target_summary_schema",
                        "open_action.target_summary_kind",
                        "open_action.target_summary_path",
                    }
                    & changed_fields
                ),
            ),
            ("projection_changed", any(field.startswith("opened_projection.") for field in changed_fields)),
            ("compare_context_changed", any(field.startswith("compare_context.") for field in changed_fields)),
            ("inspector_ready_changed", "inspector.ready" in changed_fields),
            ("question_changed", any(field.startswith("questions.") for field in changed_fields)),
            ("changed", bool(changed_fields)),
        ]
    )


def build_surface(
    field_changes: list[dict[str, str]],
    impact: str,
) -> OrderedDict[str, Any]:
    selected = [change for change in field_changes if choose_text(change.get("impact")) == impact]
    narratives: list[str] = []
    for change in selected[:5]:
        narratives.append(
            "{0} changed: {1} -> {2}".format(
                choose_text(change.get("field")),
                choose_text(change.get("baseline_value")) or "-",
                choose_text(change.get("candidate_value")) or "-",
            )
        )
    return OrderedDict(
        [
            ("changed", bool(selected)),
            ("affected_fields", ordered_unique([choose_text(change.get("field")) for change in selected])),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_verdict(
    opener_status: dict[str, Any],
    change_summary: dict[str, int],
    regression_surface: dict[str, Any],
    improvement_surface: dict[str, Any],
) -> str:
    if choose_text(opener_status.get("candidate_result")) != "ok":
        return "collapsed"
    if choose_text(opener_status.get("candidate_open_action_status")) != "ready":
        return "collapsed"
    if bool(regression_surface.get("changed")):
        return "drifted"
    if int(change_summary.get("changed_field_count", 0)) == 0:
        return "standing"
    if bool(improvement_surface.get("changed")):
        return "improved"
    return "drifted"


def build_questions(
    opener_verdict: str,
    opener_changes: dict[str, Any],
    change_summary: dict[str, int],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if opener_verdict == "standing":
        compare_questions.append("Should this opener action become the stable consumer-side opening baseline?")
    elif opener_verdict == "improved":
        compare_questions.append("Which gained opener context should be rendered first by the explain surface?")
    elif opener_verdict == "drifted":
        compare_questions.append("Did opener drift change what a reader sees before deeper explain?")
    else:
        compare_questions.append("Which opener dependency collapsed before a ready opening action could be trusted?")

    if bool(opener_changes.get("compare_context_changed")):
        next_questions.append("Should compare-context drift be shown beside the opener projection?")
    if bool(opener_changes.get("projection_changed")):
        next_questions.append("Should projection drift trigger a deeper target summary compare?")
    if bool(opener_changes.get("inspector_ready_changed")):
        next_questions.append("Should inspector readiness drift block direct execution by downstream tools?")
    if int(change_summary.get("question_field_changed_count", 0)) > 0:
        next_questions.append("Should opener question drift feed the next explain-surface prompt?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this opener compare instead of diffing opener JSON?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_compare_summary_model(
    baseline_opener_path: Path,
    candidate_opener_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_opener_summary(baseline_opener_path)
    candidate_summary = load_opener_summary(candidate_opener_path)
    baseline_record = build_semantic_record(baseline_summary)
    candidate_record = build_semantic_record(candidate_summary)
    field_changes, change_summary = build_field_changes(baseline_record, candidate_record)
    opener_status = build_opener_status(baseline_summary, candidate_summary)
    opener_changes = build_opener_changes(field_changes)
    regression_surface = build_surface(field_changes, "regression")
    improvement_surface = build_surface(field_changes, "improvement")
    opener_verdict = build_verdict(opener_status, change_summary, regression_surface, improvement_surface)

    return OrderedDict(
        [
            ("schema", COMPARE_SCHEMA),
            ("kind", COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opener.py"),
            ("result", "ok"),
            (
                "entry_opener_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opener Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two entry opener facades preserve the same explain open judgment.",
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
                    supporting_surfaces=[
                        build_opener_surface(baseline_summary, baseline_opener_path, "baseline_opener", "baseline_opener"),
                        build_opener_surface(candidate_summary, candidate_opener_path, "candidate_opener", "candidate_opener"),
                    ],
                ),
            ),
            (
                "opener_provenance",
                [
                    build_opener_provenance_entry(
                        baseline_summary,
                        baseline_opener_path,
                        "baseline_opener",
                        "baseline_opener",
                    ),
                    build_opener_provenance_entry(
                        candidate_summary,
                        candidate_opener_path,
                        "candidate_opener",
                        "candidate_opener",
                    ),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_opener_summary_path", normalize_path(baseline_opener_path)),
                        ("candidate_opener_summary_path", normalize_path(candidate_opener_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("opener_verdict", opener_verdict),
            ("opener_status", opener_status),
            ("opener_changes", opener_changes),
            ("change_summary", change_summary),
            ("field_changes", field_changes),
            ("opener_regression_surface", regression_surface),
            ("opener_improvement_surface", improvement_surface),
            ("questions", build_questions(opener_verdict, opener_changes, change_summary)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["opener_status"]
    changes = summary["opener_changes"]
    change_summary = summary["change_summary"]
    regression = summary["opener_regression_surface"]
    improvement = summary["opener_improvement_surface"]
    questions = summary["questions"]

    lines: list[str] = [
        "# System Compiler Front Page Entry Opener Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Opener verdict: `{summary['opener_verdict']}`",
        f"- Baseline opener: `{summary['artifact_context']['baseline_opener_summary_path']}`",
        f"- Candidate opener: `{summary['artifact_context']['candidate_opener_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Opener Status",
        "- Baseline: `result={0} action={1} tab={2} query={3}/{4} projection={5}/{6} compare={7}/{8} inspector={9}`".format(
            status["baseline_result"],
            status["baseline_open_action_status"],
            status["baseline_selected_tab_id"],
            status["baseline_query_kind"],
            status["baseline_query_scope"],
            status["baseline_projection_status"],
            status["baseline_projection_kind"],
            status["baseline_compare_context_available"],
            status["baseline_landing_verdict"] or "none",
            status["baseline_inspector_ready"],
        ),
        "- Candidate: `result={0} action={1} tab={2} query={3}/{4} projection={5}/{6} compare={7}/{8} inspector={9}`".format(
            status["candidate_result"],
            status["candidate_open_action_status"],
            status["candidate_selected_tab_id"],
            status["candidate_query_kind"],
            status["candidate_query_scope"],
            status["candidate_projection_status"],
            status["candidate_projection_kind"],
            status["candidate_compare_context_available"],
            status["candidate_landing_verdict"] or "none",
            status["candidate_inspector_ready"],
        ),
        "",
        "## Change Summary",
        "- changed=`{0}` opening=`{1}` projection=`{2}` compare_context=`{3}` inspector=`{4}` questions=`{5}`".format(
            change_summary["changed_field_count"],
            change_summary["opening_field_changed_count"],
            change_summary["projection_field_changed_count"],
            change_summary["compare_context_field_changed_count"],
            change_summary["inspector_field_changed_count"],
            change_summary["question_field_changed_count"],
        ),
        "- impacts regression=`{0}` improvement=`{1}` neutral=`{2}`".format(
            change_summary["regression_count"],
            change_summary["improvement_count"],
            change_summary["neutral_change_count"],
        ),
        "- flags selected_tab_changed=`{0}` query_changed=`{1}` target_changed=`{2}` projection_changed=`{3}` compare_context_changed=`{4}` inspector_ready_changed=`{5}`".format(
            changes["selected_tab_changed"],
            changes["query_changed"],
            changes["target_changed"],
            changes["projection_changed"],
            changes["compare_context_changed"],
            changes["inspector_ready_changed"],
        ),
    ]

    if summary["field_changes"]:
        lines.extend(["", "## Field Changes"])
        for change in summary["field_changes"]:
            lines.append(
                "- `{0}` group=`{1}` impact=`{2}` `{3}` -> `{4}`".format(
                    change["field"],
                    change["group"],
                    change["impact"],
                    change["baseline_value"] or "none",
                    change["candidate_value"] or "none",
                )
            )

    lines.extend(["", "## Regression Surface"])
    if regression["changed"]:
        for narrative in regression["narratives"]:
            lines.append(f"- {narrative}")
    else:
        lines.append("- none")

    lines.extend(["", "## Improvement Surface"])
    if improvement["changed"]:
        for narrative in improvement["narratives"]:
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
    status = summary["opener_status"]
    change_summary = summary["change_summary"]
    return "\n".join(
        [
            f"baseline_opener_summary_path: {summary['artifact_context']['baseline_opener_summary_path']}",
            f"candidate_opener_summary_path: {summary['artifact_context']['candidate_opener_summary_path']}",
            f"opener_verdict: {summary['opener_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_open_action_status: {status['baseline_open_action_status']}",
            f"candidate_open_action_status: {status['candidate_open_action_status']}",
            f"baseline_selected_tab_id: {status['baseline_selected_tab_id']}",
            f"candidate_selected_tab_id: {status['candidate_selected_tab_id']}",
            f"baseline_projection_kind: {status['baseline_projection_kind']}",
            f"candidate_projection_kind: {status['candidate_projection_kind']}",
            f"baseline_compare_context_available: {status['baseline_compare_context_available']}",
            f"candidate_compare_context_available: {status['candidate_compare_context_available']}",
            f"baseline_inspector_ready: {status['baseline_inspector_ready']}",
            f"candidate_inspector_ready: {status['candidate_inspector_ready']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"regression_count: {change_summary['regression_count']}",
            f"improvement_count: {change_summary['improvement_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two system compiler front_page entry opener summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline entry opener summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate entry opener summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opener compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opener compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opener compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opener compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opener-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opener.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opener.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opener.compare.check.txt")

    try:
        summary = build_compare_summary_model(
            baseline_opener_path=baseline_path,
            candidate_opener_path=candidate_path,
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

    print(f"[FRONT-PAGE-ENTRY-OPENER-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENER-COMPARE] verdict={summary['opener_verdict']}")
    print(f"[FRONT-PAGE-ENTRY-OPENER-COMPARE] changed_fields={summary['change_summary']['changed_field_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
