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
EXPLAIN_ENTRY_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_testimony_explain_entry_compare/v0"
EXPLAIN_ENTRY_COMPARE_KIND = "system_compiler.front_page_entry_opening_testimony_explain_entry_compare"


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


def load_explain_entry_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != EXPLAIN_ENTRY_SCHEMA:
        raise ValueError(f"unsupported opening testimony explain-entry schema: {path}")
    if choose_text(summary.get("kind")) != EXPLAIN_ENTRY_KIND:
        raise ValueError(f"unsupported opening testimony explain-entry kind: {path}")
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


def normalize_explain_entry(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    source_ref = get_mapping(summary.get("source_route_ref"))
    decision = get_mapping(summary.get("explain_entry_decision"))
    selected = normalize_surface(summary.get("selected_surface"))
    supporting_surfaces = [normalize_surface(surface) for surface in get_list(summary.get("supporting_surfaces"))]
    next_questions = [normalize_question(question) for question in get_list(summary.get("next_questions"))]

    supporting_surface_refs = ordered_unique(
        [
            "{0}:{1}:{2}".format(
                surface["surface_id"],
                surface["summary_schema"],
                surface["summary_path"],
            )
            for surface in supporting_surfaces
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
            ("source_route_summary_schema", choose_text(source_ref.get("summary_schema"))),
            ("source_route_summary_kind", choose_text(source_ref.get("summary_kind"))),
            ("source_route_summary_path", normalize_optional_path(source_ref.get("summary_path"))),
            ("source_route_report_markdown_path", normalize_optional_path(source_ref.get("report_markdown_path"))),
            ("source_route_check_text_path", normalize_optional_path(source_ref.get("check_text_path"))),
            ("source_route_root_summary_schema", choose_text(source_ref.get("root_summary_schema"))),
            ("source_route_verdict", choose_text(source_ref.get("route_verdict"))),
            ("source_route_baseline_route_summary_path", normalize_optional_path(source_ref.get("baseline_route_summary_path"))),
            ("source_route_candidate_route_summary_path", normalize_optional_path(source_ref.get("candidate_route_summary_path"))),
            ("decision_status", choose_text(decision.get("status"))),
            ("selection_kind", choose_text(decision.get("selection_kind"))),
            ("selected_entry_id", choose_text(decision.get("selected_entry_id"))),
            ("selected_tab_id", choose_text(decision.get("selected_tab_id"))),
            ("selected_role", choose_text(decision.get("selected_role"))),
            ("selected_source", choose_text(decision.get("selected_source"))),
            ("selected_surface_id", choose_text(selected.get("surface_id"))),
            ("selected_surface_label", choose_text(selected.get("label"))),
            ("selected_surface_role", choose_text(selected.get("role"))),
            ("selected_summary_schema", choose_text(selected.get("summary_schema"))),
            ("selected_summary_kind", choose_text(selected.get("summary_kind"))),
            ("selected_summary_path", normalize_optional_path(selected.get("summary_path"))),
            ("selected_report_markdown_path", normalize_optional_path(selected.get("report_markdown_path"))),
            ("selected_check_text_path", normalize_optional_path(selected.get("check_text_path"))),
            ("selected_route_id", choose_text(selected.get("route_id"))),
            ("selected_depth", selected.get("depth")),
            ("selected_surface_source", choose_text(selected.get("source"))),
            ("supporting_surfaces", supporting_surfaces),
            ("supporting_surface_refs", supporting_surface_refs),
            ("supporting_surface_count", len(supporting_surfaces)),
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


def build_explain_entry_surface(
    explain_entry_summary: dict[str, Any],
    explain_entry_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    artifact_context = get_mapping(explain_entry_summary.get("artifact_context"))
    return make_surface(
        surface_id,
        f"{role.replace('_', ' ')}: opening testimony explain entry",
        role,
        EXPLAIN_ENTRY_SCHEMA,
        normalize_path(explain_entry_path),
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
                    build_explain_entry_surface(
                        baseline_summary,
                        baseline_path,
                        "baseline_opening_testimony_explain_entry",
                        "baseline_opening_testimony_explain_entry",
                    ),
                    build_explain_entry_surface(
                        candidate_summary,
                        candidate_path,
                        "candidate_opening_testimony_explain_entry",
                        "candidate_opening_testimony_explain_entry",
                    ),
                ],
            ),
        ]
    )


def build_explain_entry_provenance_entry(
    explain_entry_summary: dict[str, Any],
    explain_entry_path: Path,
    provenance_id: str,
    explain_entry_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(explain_entry_summary.get("artifact_context"))
    decision = get_mapping(explain_entry_summary.get("explain_entry_decision"))
    selected = get_mapping(explain_entry_summary.get("selected_surface"))
    source_ref = get_mapping(explain_entry_summary.get("source_route_ref"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("explain_entry_role", explain_entry_role),
            ("source_summary_schema", EXPLAIN_ENTRY_SCHEMA),
            ("source_summary_path", normalize_path(explain_entry_path)),
            ("source_report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("source_check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("result", choose_text(explain_entry_summary.get("result"))),
            ("decision_status", choose_text(decision.get("status"))),
            ("selection_kind", choose_text(decision.get("selection_kind"))),
            ("selected_surface_id", choose_text(selected.get("surface_id"))),
            ("selected_summary_schema", choose_text(selected.get("summary_schema"))),
            ("selected_summary_path", normalize_optional_path(selected.get("summary_path"))),
            ("source_route_summary_path", normalize_optional_path(source_ref.get("summary_path"))),
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


def build_explain_entry_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_decision_status", baseline["decision_status"]),
            ("candidate_decision_status", candidate["decision_status"]),
            ("baseline_selection_kind", baseline["selection_kind"]),
            ("candidate_selection_kind", candidate["selection_kind"]),
            ("baseline_selected_surface_id", baseline["selected_surface_id"]),
            ("candidate_selected_surface_id", candidate["selected_surface_id"]),
            ("baseline_selected_summary_schema", baseline["selected_summary_schema"]),
            ("candidate_selected_summary_schema", candidate["selected_summary_schema"]),
            ("baseline_selected_summary_path", baseline["selected_summary_path"]),
            ("candidate_selected_summary_path", candidate["selected_summary_path"]),
            ("baseline_source_route_summary_path", baseline["source_route_summary_path"]),
            ("candidate_source_route_summary_path", candidate["source_route_summary_path"]),
        ]
    )


def build_regression_surface(
    source_route_changes: dict[str, Any],
    decision_changes: dict[str, Any],
    selected_surface_changes: dict[str, Any],
    supporting_surface_changes: dict[str, Any],
    next_question_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    candidate_blocked = candidate["result"] != "ok" or candidate["decision_status"] != "ready"
    candidate_recovered = baseline["decision_status"] != "ready" and candidate["decision_status"] == "ready"
    source_route_changed = bool(source_route_changes.get("changed"))
    decision_changed = bool(decision_changes.get("changed"))
    selection_changed = bool(
        decision_changes.get("selection_kind", {}).get("changed")
        or decision_changes.get("selected_entry_id", {}).get("changed")
        or decision_changes.get("decision_status", {}).get("changed")
    )
    selected_surface_changed = bool(
        selected_surface_changes.get("selected_surface_id", {}).get("changed")
        or selected_surface_changes.get("selected_summary_schema", {}).get("changed")
        or selected_surface_changes.get("selected_summary_path", {}).get("changed")
    )
    supporting_surfaces_changed = bool(supporting_surface_changes.get("changed"))
    next_questions_changed = bool(next_question_changes.get("changed"))
    same_default_surface = not selected_surface_changed and not selection_changed

    if candidate_blocked:
        narratives.append("candidate opening testimony explain entry is blocked")
    if candidate_recovered:
        narratives.append("candidate opening testimony explain entry recovered to ready")
    if source_route_changed:
        narratives.append("source route reference changed")
    if decision_changes.get("decision_status", {}).get("changed"):
        narratives.append(
            "decision status changed: {0} -> {1}".format(
                baseline["decision_status"],
                candidate["decision_status"],
            )
        )
    if decision_changes.get("selection_kind", {}).get("changed"):
        narratives.append(
            "selection kind changed: {0} -> {1}".format(
                baseline["selection_kind"],
                candidate["selection_kind"],
            )
        )
    if selected_surface_changed:
        narratives.append(
            "selected explain surface changed: {0} -> {1}".format(
                baseline["selected_surface_id"],
                candidate["selected_surface_id"],
            )
        )
    if supporting_surfaces_changed:
        narratives.append("supporting explain surfaces changed")
    if next_questions_changed:
        narratives.append("typed next questions changed")

    changed = bool(
        candidate_blocked
        or candidate_recovered
        or source_route_changed
        or decision_changed
        or selected_surface_changed
        or supporting_surfaces_changed
        or next_questions_changed
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("candidate_blocked", candidate_blocked),
            ("candidate_recovered", candidate_recovered),
            ("source_route_changed", source_route_changed),
            ("decision_changed", decision_changed),
            ("selection_changed", selection_changed),
            ("selected_surface_changed", selected_surface_changed),
            ("supporting_surfaces_changed", supporting_surfaces_changed),
            ("next_questions_changed", next_questions_changed),
            ("same_default_surface", same_default_surface),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_explain_entry_verdict(regression_surface: dict[str, Any], candidate: dict[str, Any], baseline: dict[str, Any]) -> str:
    if candidate["result"] != "ok" or candidate["decision_status"] != "ready":
        return "collapsed"
    if baseline["decision_status"] != "ready" and candidate["decision_status"] == "ready":
        return "improved"
    if bool(regression_surface.get("changed")):
        return "drifted"
    return "standing"


def build_questions(explain_entry_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []
    if explain_entry_verdict == "standing":
        compare_questions.append("Do these opening testimony explain entries still choose the same default explain surface?")
    elif explain_entry_verdict == "improved":
        compare_questions.append("Which recovered explain-entry decision should become the next route baseline?")
    elif explain_entry_verdict == "drifted":
        compare_questions.append("Did the default explain surface or source route drift enough to require a new landing review?")
    else:
        compare_questions.append("Which explain-entry selection blocked the candidate opening testimony route?")

    if bool(regression_surface.get("source_route_changed")):
        next_questions.append("Inspect the source route because the explain-entry input changed.")
    if bool(regression_surface.get("selected_surface_changed")):
        next_questions.append("Inspect the selected explain surface before publishing this route.")
    if bool(regression_surface.get("supporting_surfaces_changed")):
        next_questions.append("Inspect supporting explain surfaces because the side context changed.")
    if bool(regression_surface.get("next_questions_changed")):
        next_questions.append("Inspect typed next questions because the follow-up surface changed.")
    if not next_questions:
        next_questions.append("Compare another opening testimony explain entry before promoting this seam.")
    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_explain_entry_path: Path,
    candidate_explain_entry_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_explain_entry_summary(baseline_explain_entry_path)
    candidate_summary = load_explain_entry_summary(candidate_explain_entry_path)
    baseline = normalize_explain_entry(baseline_summary)
    candidate = normalize_explain_entry(candidate_summary)

    source_route_changes = build_field_changes(
        baseline,
        candidate,
        (
            "source_route_summary_schema",
            "source_route_summary_kind",
            "source_route_summary_path",
            "source_route_report_markdown_path",
            "source_route_check_text_path",
            "source_route_root_summary_schema",
            "source_route_verdict",
            "source_route_baseline_route_summary_path",
            "source_route_candidate_route_summary_path",
        ),
    )
    decision_changes = build_field_changes(
        baseline,
        candidate,
        (
            "result",
            "decision_status",
            "selection_kind",
            "selected_entry_id",
            "selected_tab_id",
            "selected_role",
            "selected_source",
        ),
    )
    selected_surface_changes = build_field_changes(
        baseline,
        candidate,
        (
            "selected_surface_id",
            "selected_surface_label",
            "selected_surface_role",
            "selected_summary_schema",
            "selected_summary_kind",
            "selected_summary_path",
            "selected_report_markdown_path",
            "selected_check_text_path",
            "selected_route_id",
            "selected_depth",
            "selected_surface_source",
        ),
    )
    supporting_surface_changes = build_field_changes(
        baseline,
        candidate,
        (
            "supporting_surface_refs",
            "supporting_surfaces",
            "supporting_surface_count",
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
    explain_entry_status = build_explain_entry_status(baseline, candidate)
    regression_surface = build_regression_surface(
        source_route_changes,
        decision_changes,
        selected_surface_changes,
        supporting_surface_changes,
        next_question_changes,
        baseline,
        candidate,
    )
    explain_entry_verdict = build_explain_entry_verdict(regression_surface, candidate, baseline)
    changed_field_count = sum(
        int(changes["changed_field_count"])
        for changes in (
            source_route_changes,
            decision_changes,
            selected_surface_changes,
            supporting_surface_changes,
            next_question_changes,
        )
    )

    return OrderedDict(
        [
            ("schema", EXPLAIN_ENTRY_COMPARE_SCHEMA),
            ("kind", EXPLAIN_ENTRY_COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_testimony_explain_entry.py"),
            ("result", "ok"),
            (
                "opening_testimony_explain_entry_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Explain Entry Compare"),
                        (
                            "summary",
                            "A compare object that checks whether two opening testimony explain entries preserve the same default explain surface.",
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
                    baseline_explain_entry_path,
                    candidate_summary,
                    candidate_explain_entry_path,
                ),
            ),
            (
                "explain_entry_provenance",
                [
                    build_explain_entry_provenance_entry(
                        baseline_summary,
                        baseline_explain_entry_path,
                        "baseline_opening_testimony_explain_entry",
                        "baseline_explain_entry",
                    ),
                    build_explain_entry_provenance_entry(
                        candidate_summary,
                        candidate_explain_entry_path,
                        "candidate_opening_testimony_explain_entry",
                        "candidate_explain_entry",
                    ),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_opening_testimony_explain_entry_summary_path", normalize_path(baseline_explain_entry_path)),
                        ("candidate_opening_testimony_explain_entry_summary_path", normalize_path(candidate_explain_entry_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("explain_entry_verdict", explain_entry_verdict),
            ("explain_entry_status", explain_entry_status),
            ("source_route_ref_changes", source_route_changes),
            ("explain_entry_decision_changes", decision_changes),
            ("selected_surface_changes", selected_surface_changes),
            ("supporting_surface_changes", supporting_surface_changes),
            ("next_question_changes", next_question_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("source_route_ref_changed_field_count", int(source_route_changes["changed_field_count"])),
                        ("explain_entry_decision_changed_field_count", int(decision_changes["changed_field_count"])),
                        ("selected_surface_changed_field_count", int(selected_surface_changes["changed_field_count"])),
                        ("supporting_surface_changed_field_count", int(supporting_surface_changes["changed_field_count"])),
                        ("next_question_changed_field_count", int(next_question_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("explain_entry_regression_surface", regression_surface),
            ("questions", build_questions(explain_entry_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["explain_entry_status"]
    change_summary = summary["change_summary"]
    regression = summary["explain_entry_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Explain Entry Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Explain entry verdict: `{summary['explain_entry_verdict']}`",
        f"- Baseline explain entry: `{summary['artifact_context']['baseline_opening_testimony_explain_entry_summary_path']}`",
        f"- Candidate explain entry: `{summary['artifact_context']['candidate_opening_testimony_explain_entry_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Explain Entry Status",
        "- Baseline: `result={0} status={1} selection={2} surface={3}`".format(
            status["baseline_result"],
            status["baseline_decision_status"],
            status["baseline_selection_kind"],
            status["baseline_selected_surface_id"],
        ),
        "- Candidate: `result={0} status={1} selection={2} surface={3}`".format(
            status["candidate_result"],
            status["candidate_decision_status"],
            status["candidate_selection_kind"],
            status["candidate_selected_surface_id"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` source_route=`{1}` decision=`{2}` selected_surface=`{3}` supporting=`{4}` questions=`{5}`".format(
            change_summary["changed_field_count"],
            change_summary["source_route_ref_changed_field_count"],
            change_summary["explain_entry_decision_changed_field_count"],
            change_summary["selected_surface_changed_field_count"],
            change_summary["supporting_surface_changed_field_count"],
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
    status = summary["explain_entry_status"]
    change_summary = summary["change_summary"]
    regression = summary["explain_entry_regression_surface"]
    return "\n".join(
        [
            f"baseline_opening_testimony_explain_entry_summary_path: {summary['artifact_context']['baseline_opening_testimony_explain_entry_summary_path']}",
            f"candidate_opening_testimony_explain_entry_summary_path: {summary['artifact_context']['candidate_opening_testimony_explain_entry_summary_path']}",
            f"explain_entry_verdict: {summary['explain_entry_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_decision_status: {status['baseline_decision_status']}",
            f"candidate_decision_status: {status['candidate_decision_status']}",
            f"baseline_selection_kind: {status['baseline_selection_kind']}",
            f"candidate_selection_kind: {status['candidate_selection_kind']}",
            f"baseline_selected_surface_id: {status['baseline_selected_surface_id']}",
            f"candidate_selected_surface_id: {status['candidate_selected_surface_id']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"source_route_changed: {regression['source_route_changed']}",
            f"selected_surface_changed: {regression['selected_surface_changed']}",
            f"supporting_surfaces_changed: {regression['supporting_surfaces_changed']}",
            f"next_questions_changed: {regression['next_questions_changed']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two opening testimony explain-entry summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline opening testimony explain-entry summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening testimony explain-entry summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opening testimony explain-entry compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opening testimony explain-entry compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opening testimony explain-entry compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opening testimony explain-entry compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-explain-entry-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-testimony.explain-entry.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-testimony.explain-entry.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-testimony.explain-entry.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE] verdict={summary['explain_entry_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
