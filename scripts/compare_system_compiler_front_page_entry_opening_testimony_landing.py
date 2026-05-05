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


OPENING_TESTIMONY_LANDING_SCHEMA = "system_compiler.front_page_entry_opening_testimony_landing/v0"
OPENING_TESTIMONY_LANDING_KIND = "system_compiler.front_page_entry_opening_testimony_landing"
OPENING_TESTIMONY_LANDING_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_testimony_landing_compare/v0"
OPENING_TESTIMONY_LANDING_COMPARE_KIND = "system_compiler.front_page_entry_opening_testimony_landing_compare"


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


def load_opening_testimony_landing_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != OPENING_TESTIMONY_LANDING_SCHEMA:
        raise ValueError(f"unsupported opening testimony landing schema: {path}")
    if choose_text(summary.get("kind")) != OPENING_TESTIMONY_LANDING_KIND:
        raise ValueError(f"unsupported opening testimony landing kind: {path}")
    return summary


def normalize_compare_path(path_value: Any, landing_output_root_value: Any, source_witness_summary_value: Any) -> str:
    path_text = choose_text(path_value)
    if not path_text:
        return ""

    try:
        path = Path(path_text).resolve()
    except Exception:
        return normalize_optional_path(path_text)

    landing_output_root_text = choose_text(landing_output_root_value)
    if landing_output_root_text:
        try:
            landing_output_root = Path(landing_output_root_text).resolve()
            path.relative_to(landing_output_root)
            return "$opening-testimony-landing-output"
        except Exception:
            pass

    source_witness_summary_text = choose_text(source_witness_summary_value)
    if source_witness_summary_text:
        try:
            source_witness_root = Path(source_witness_summary_text).resolve().parent
            path.relative_to(source_witness_root)
            if path == Path(source_witness_summary_text).resolve():
                return "$source-open-event-witness-self"
            return "$source-open-event-witness-output"
        except Exception:
            pass

    return normalize_optional_path(path_text)


def normalize_evidence_ref(ref_value: Any, landing_output_root_value: Any, source_witness_summary_value: Any) -> OrderedDict[str, str]:
    ref = get_mapping(ref_value)
    return OrderedDict(
        [
            ("role", choose_text(ref.get("role"))),
            ("summary_schema", choose_text(ref.get("summary_schema"))),
            ("summary_path", normalize_compare_path(ref.get("summary_path"), landing_output_root_value, source_witness_summary_value)),
        ]
    )


def normalize_landing_summary(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    identity = get_mapping(summary.get("opening_identity"))
    decision = get_mapping(summary.get("landing_decision"))
    opening_reason = get_mapping(decision.get("opening_reason"))
    preview = get_mapping(summary.get("testimony_preview"))
    targets = get_mapping(summary.get("artifact_targets"))
    output_root = artifact_context.get("output_root")
    source_witness_summary_path = artifact_context.get("source_witness_summary_path")
    evidence_refs = [
        normalize_evidence_ref(ref, output_root, source_witness_summary_path)
        for ref in get_list(targets.get("evidence_refs"))
    ]
    witness_artifact_refs = ordered_unique(
        [
            normalize_compare_path(ref, output_root, source_witness_summary_path)
            for ref in get_list(targets.get("witness_artifact_refs"))
        ]
    )
    typed_questions = [
        OrderedDict(
            [
                ("kind", choose_text(get_mapping(question).get("kind"))),
                ("target_ref", choose_text(get_mapping(question).get("target_ref"))),
            ]
        )
        for question in get_list(summary.get("next_questions"))
    ]
    return OrderedDict(
        [
            ("result", choose_text(summary.get("result"))),
            ("open_event_id", choose_text(identity.get("open_event_id"))),
            ("open_event_status", choose_text(identity.get("open_event_status"))),
            ("open_event_result", choose_text(identity.get("open_event_result"))),
            ("reason_kind", choose_text(identity.get("reason_kind"))),
            ("reason_summary", choose_text(identity.get("reason_summary"))),
            ("source_judgment_status", choose_text(identity.get("source_judgment_status"))),
            ("source_judgment_grade", choose_text(identity.get("source_judgment_grade"))),
            ("source_witness_id", choose_text(identity.get("source_witness_id"))),
            ("source_witness_status", choose_text(identity.get("source_witness_status"))),
            (
                "source_open_event_summary_path",
                normalize_compare_path(identity.get("source_open_event_summary_path"), output_root, source_witness_summary_path),
            ),
            ("landing_status", choose_text(decision.get("status"))),
            ("selected_entry_id", choose_text(decision.get("selected_entry_id"))),
            ("selected_tab_id", choose_text(decision.get("selected_tab_id"))),
            ("selected_role", choose_text(decision.get("selected_role"))),
            ("opening_reason_kind", choose_text(opening_reason.get("kind"))),
            ("opening_reason_summary", choose_text(opening_reason.get("summary"))),
            ("headline", choose_text(preview.get("headline"))),
            ("source_judgment_summary", choose_text(preview.get("source_judgment_summary"))),
            ("summary_lines", [choose_text(item) for item in get_list(preview.get("summary_lines"))]),
            ("explanation_text_lines", [choose_text(item) for item in get_list(preview.get("explanation_text_lines"))]),
            ("observation_lines", [choose_text(item) for item in get_list(preview.get("observation_lines"))]),
            ("summary_line_count", int(preview.get("summary_line_count", 0))),
            ("explanation_line_count", int(preview.get("explanation_line_count", 0))),
            ("observation_count", int(preview.get("observation_count", 0))),
            ("evidence_refs", evidence_refs),
            ("evidence_summary_refs", ordered_unique([f"{ref['role']}:{ref['summary_schema']}:{ref['summary_path']}" for ref in evidence_refs])),
            ("witness_artifact_refs", witness_artifact_refs),
            ("evidence_ref_count", int(targets.get("evidence_ref_count", 0))),
            ("witness_artifact_ref_count", int(targets.get("witness_artifact_ref_count", 0))),
            ("typed_questions", typed_questions),
            ("typed_question_kinds", ordered_unique([choose_text(question.get("kind")) for question in typed_questions])),
            ("typed_question_targets", ordered_unique([f"{question['kind']}:{question['target_ref']}" for question in typed_questions])),
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


def build_landing_surface(
    landing_summary: dict[str, Any],
    landing_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    artifact_context = get_mapping(landing_summary.get("artifact_context"))
    return make_surface(
        surface_id,
        f"{role.replace('_', ' ')}: opening testimony landing",
        role,
        OPENING_TESTIMONY_LANDING_SCHEMA,
        normalize_path(landing_path),
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
                    build_landing_surface(baseline_summary, baseline_path, "baseline_opening_testimony_landing", "baseline_opening_testimony_landing"),
                    build_landing_surface(candidate_summary, candidate_path, "candidate_opening_testimony_landing", "candidate_opening_testimony_landing"),
                ],
            ),
        ]
    )


def build_landing_provenance_entry(
    landing_summary: dict[str, Any],
    landing_path: Path,
    provenance_id: str,
    landing_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(landing_summary.get("artifact_context"))
    identity = get_mapping(landing_summary.get("opening_identity"))
    decision = get_mapping(landing_summary.get("landing_decision"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("landing_role", landing_role),
            ("source_summary_schema", OPENING_TESTIMONY_LANDING_SCHEMA),
            ("source_summary_path", normalize_path(landing_path)),
            ("source_report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("source_check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("result", choose_text(landing_summary.get("result"))),
            ("landing_status", choose_text(decision.get("status"))),
            ("open_event_id", choose_text(identity.get("open_event_id"))),
            ("source_judgment_status", choose_text(identity.get("source_judgment_status"))),
            ("source_witness_status", choose_text(identity.get("source_witness_status"))),
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


def build_landing_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_landing_status", baseline["landing_status"]),
            ("candidate_landing_status", candidate["landing_status"]),
            ("baseline_open_event_id", baseline["open_event_id"]),
            ("candidate_open_event_id", candidate["open_event_id"]),
            ("baseline_source_judgment_status", baseline["source_judgment_status"]),
            ("candidate_source_judgment_status", candidate["source_judgment_status"]),
            ("baseline_source_witness_status", baseline["source_witness_status"]),
            ("candidate_source_witness_status", candidate["source_witness_status"]),
            ("baseline_selected_entry_id", baseline["selected_entry_id"]),
            ("candidate_selected_entry_id", candidate["selected_entry_id"]),
            ("baseline_selected_tab_id", baseline["selected_tab_id"]),
            ("candidate_selected_tab_id", candidate["selected_tab_id"]),
            ("baseline_selected_role", baseline["selected_role"]),
            ("candidate_selected_role", candidate["selected_role"]),
        ]
    )


def build_regression_surface(
    identity_changes: dict[str, Any],
    decision_changes: dict[str, Any],
    preview_changes: dict[str, Any],
    target_changes: dict[str, Any],
    question_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    result_changed = bool(identity_changes.get("result", {}).get("changed"))
    landing_status_changed = bool(decision_changes.get("landing_status", {}).get("changed"))
    landing_collapsed = baseline["landing_status"] == "ready" and candidate["landing_status"] != "ready"
    landing_recovered = baseline["landing_status"] != "ready" and candidate["landing_status"] == "ready"
    selected_entry_changed = bool(decision_changes.get("selected_entry_id", {}).get("changed"))
    selected_tab_changed = bool(decision_changes.get("selected_tab_id", {}).get("changed"))
    selected_role_changed = bool(decision_changes.get("selected_role", {}).get("changed"))
    source_open_event_changed = bool(identity_changes.get("open_event_id", {}).get("changed")) or bool(
        identity_changes.get("reason_kind", {}).get("changed")
    )
    source_judgment_changed = bool(identity_changes.get("source_judgment_status", {}).get("changed")) or bool(
        identity_changes.get("source_judgment_grade", {}).get("changed")
    )
    source_witness_status_changed = bool(identity_changes.get("source_witness_status", {}).get("changed"))
    preview_changed = bool(preview_changes.get("summary_lines", {}).get("changed")) or bool(
        preview_changes.get("explanation_text_lines", {}).get("changed")
    )
    evidence_refs_changed = bool(target_changes.get("evidence_summary_refs", {}).get("changed")) or bool(
        target_changes.get("evidence_ref_count", {}).get("changed")
    )
    witness_artifact_refs_changed = bool(target_changes.get("witness_artifact_refs", {}).get("changed")) or bool(
        target_changes.get("witness_artifact_ref_count", {}).get("changed")
    )
    next_questions_changed = bool(question_changes.get("typed_question_targets", {}).get("changed")) or bool(
        question_changes.get("typed_question_kinds", {}).get("changed")
    )

    if result_changed:
        narratives.append(f"landing result changed: {baseline['result']} -> {candidate['result']}")
    if landing_status_changed:
        narratives.append(f"landing status changed: {baseline['landing_status']} -> {candidate['landing_status']}")
    if landing_collapsed:
        narratives.append("candidate opening testimony landing can no longer open")
    if landing_recovered:
        narratives.append("candidate opening testimony landing recovered to ready")
    if selected_entry_changed:
        narratives.append(f"selected entry changed: {baseline['selected_entry_id']} -> {candidate['selected_entry_id']}")
    if selected_tab_changed:
        narratives.append(f"selected tab changed: {baseline['selected_tab_id']} -> {candidate['selected_tab_id']}")
    if selected_role_changed:
        narratives.append(f"selected role changed: {baseline['selected_role']} -> {candidate['selected_role']}")
    if source_open_event_changed:
        narratives.append("source opening judgment identity changed")
    if source_judgment_changed:
        narratives.append("source judgment status or grade changed")
    if source_witness_status_changed:
        narratives.append("source witness status changed")
    if preview_changed:
        narratives.append("testimony preview changed")
    if evidence_refs_changed:
        narratives.append("landing evidence refs changed")
    if witness_artifact_refs_changed:
        narratives.append("landing witness artifact refs changed")
    if next_questions_changed:
        narratives.append("typed next questions changed")

    changed = bool(
        result_changed
        or landing_status_changed
        or selected_entry_changed
        or selected_tab_changed
        or selected_role_changed
        or source_open_event_changed
        or source_judgment_changed
        or source_witness_status_changed
        or preview_changed
        or evidence_refs_changed
        or witness_artifact_refs_changed
        or next_questions_changed
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("result_changed", result_changed),
            ("landing_status_changed", landing_status_changed),
            ("landing_collapsed", landing_collapsed),
            ("landing_recovered", landing_recovered),
            ("selected_entry_changed", selected_entry_changed),
            ("selected_tab_changed", selected_tab_changed),
            ("selected_role_changed", selected_role_changed),
            ("source_open_event_changed", source_open_event_changed),
            ("source_judgment_changed", source_judgment_changed),
            ("source_witness_status_changed", source_witness_status_changed),
            ("preview_changed", preview_changed),
            ("evidence_refs_changed", evidence_refs_changed),
            ("witness_artifact_refs_changed", witness_artifact_refs_changed),
            ("next_questions_changed", next_questions_changed),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_landing_verdict(regression_surface: dict[str, Any], candidate: dict[str, Any], baseline: dict[str, Any]) -> str:
    if candidate["result"] != "ok" or candidate["landing_status"] != "ready":
        return "collapsed"
    if not bool(regression_surface.get("changed")):
        return "standing"
    if baseline["landing_status"] != "ready" and candidate["landing_status"] == "ready":
        return "improved"
    return "drifted"


def build_questions(landing_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []
    if landing_verdict == "standing":
        compare_questions.append("Should this opening testimony landing compare become the default self-check?")
    elif landing_verdict == "improved":
        compare_questions.append("Which recovered testimony landing input should be promoted into the explain entry?")
    elif landing_verdict == "drifted":
        compare_questions.append("Did this testimony landing drift enough to require a new open-event witness review?")
    else:
        compare_questions.append("Which testimony landing input collapsed before the explain entry could open?")

    if bool(regression_surface.get("source_judgment_changed")):
        next_questions.append("Inspect the source open event because the judgment status or grade changed.")
    if bool(regression_surface.get("preview_changed")):
        next_questions.append("Inspect the testimony preview before publishing the explain entry.")
    if bool(regression_surface.get("evidence_refs_changed")):
        next_questions.append("Inspect evidence refs because landing targets changed.")
    if not next_questions:
        next_questions.append("Compare another opening testimony landing before promoting this explain-entry seam.")
    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_landing_path: Path,
    candidate_landing_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_opening_testimony_landing_summary(baseline_landing_path)
    candidate_summary = load_opening_testimony_landing_summary(candidate_landing_path)
    baseline = normalize_landing_summary(baseline_summary)
    candidate = normalize_landing_summary(candidate_summary)

    identity_changes = build_field_changes(
        baseline,
        candidate,
        (
            "result",
            "open_event_id",
            "open_event_status",
            "open_event_result",
            "reason_kind",
            "reason_summary",
            "source_judgment_status",
            "source_judgment_grade",
            "source_witness_id",
            "source_witness_status",
            "source_open_event_summary_path",
        ),
    )
    decision_changes = build_field_changes(
        baseline,
        candidate,
        (
            "landing_status",
            "selected_entry_id",
            "selected_tab_id",
            "selected_role",
            "opening_reason_kind",
            "opening_reason_summary",
        ),
    )
    preview_changes = build_field_changes(
        baseline,
        candidate,
        (
            "headline",
            "source_judgment_summary",
            "summary_lines",
            "explanation_text_lines",
            "observation_lines",
            "summary_line_count",
            "explanation_line_count",
            "observation_count",
        ),
    )
    target_changes = build_field_changes(
        baseline,
        candidate,
        (
            "evidence_summary_refs",
            "witness_artifact_refs",
            "evidence_ref_count",
            "witness_artifact_ref_count",
        ),
    )
    question_changes = build_field_changes(
        baseline,
        candidate,
        (
            "typed_question_kinds",
            "typed_question_targets",
        ),
    )
    landing_status = build_landing_status(baseline, candidate)
    regression_surface = build_regression_surface(
        identity_changes,
        decision_changes,
        preview_changes,
        target_changes,
        question_changes,
        baseline,
        candidate,
    )
    landing_verdict = build_landing_verdict(regression_surface, candidate, baseline)
    changed_field_count = sum(
        int(changes["changed_field_count"])
        for changes in (
            identity_changes,
            decision_changes,
            preview_changes,
            target_changes,
            question_changes,
        )
    )

    return OrderedDict(
        [
            ("schema", OPENING_TESTIMONY_LANDING_COMPARE_SCHEMA),
            ("kind", OPENING_TESTIMONY_LANDING_COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_testimony_landing.py"),
            ("result", "ok"),
            (
                "opening_testimony_landing_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Landing Compare"),
                        ("summary", "A compare object that checks whether two testimony landings preserve the same explain entry."),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(summary_path, report_path, check_path, baseline_summary, baseline_landing_path, candidate_summary, candidate_landing_path),
            ),
            (
                "landing_provenance",
                [
                    build_landing_provenance_entry(baseline_summary, baseline_landing_path, "baseline_opening_testimony_landing", "baseline_landing"),
                    build_landing_provenance_entry(candidate_summary, candidate_landing_path, "candidate_opening_testimony_landing", "candidate_landing"),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_opening_testimony_landing_summary_path", normalize_path(baseline_landing_path)),
                        ("candidate_opening_testimony_landing_summary_path", normalize_path(candidate_landing_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("landing_verdict", landing_verdict),
            ("landing_status", landing_status),
            ("opening_identity_changes", identity_changes),
            ("landing_decision_changes", decision_changes),
            ("testimony_preview_changes", preview_changes),
            ("artifact_target_changes", target_changes),
            ("next_question_changes", question_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("opening_identity_changed_field_count", int(identity_changes["changed_field_count"])),
                        ("landing_decision_changed_field_count", int(decision_changes["changed_field_count"])),
                        ("testimony_preview_changed_field_count", int(preview_changes["changed_field_count"])),
                        ("artifact_target_changed_field_count", int(target_changes["changed_field_count"])),
                        ("next_question_changed_field_count", int(question_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("landing_regression_surface", regression_surface),
            ("questions", build_questions(landing_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["landing_status"]
    change_summary = summary["change_summary"]
    regression = summary["landing_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Landing Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Landing verdict: `{summary['landing_verdict']}`",
        f"- Baseline landing: `{summary['artifact_context']['baseline_opening_testimony_landing_summary_path']}`",
        f"- Candidate landing: `{summary['artifact_context']['candidate_opening_testimony_landing_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Landing Status",
        "- Baseline: `result={0} landing={1} event={2} judgment={3} witness={4} tab={5}`".format(
            status["baseline_result"],
            status["baseline_landing_status"],
            status["baseline_open_event_id"],
            status["baseline_source_judgment_status"],
            status["baseline_source_witness_status"],
            status["baseline_selected_tab_id"],
        ),
        "- Candidate: `result={0} landing={1} event={2} judgment={3} witness={4} tab={5}`".format(
            status["candidate_result"],
            status["candidate_landing_status"],
            status["candidate_open_event_id"],
            status["candidate_source_judgment_status"],
            status["candidate_source_witness_status"],
            status["candidate_selected_tab_id"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` identity=`{1}` decision=`{2}` preview=`{3}` targets=`{4}` questions=`{5}`".format(
            change_summary["changed_field_count"],
            change_summary["opening_identity_changed_field_count"],
            change_summary["landing_decision_changed_field_count"],
            change_summary["testimony_preview_changed_field_count"],
            change_summary["artifact_target_changed_field_count"],
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
    status = summary["landing_status"]
    change_summary = summary["change_summary"]
    regression = summary["landing_regression_surface"]
    return "\n".join(
        [
            f"baseline_opening_testimony_landing_summary_path: {summary['artifact_context']['baseline_opening_testimony_landing_summary_path']}",
            f"candidate_opening_testimony_landing_summary_path: {summary['artifact_context']['candidate_opening_testimony_landing_summary_path']}",
            f"landing_verdict: {summary['landing_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_landing_status: {status['baseline_landing_status']}",
            f"candidate_landing_status: {status['candidate_landing_status']}",
            f"baseline_source_judgment_status: {status['baseline_source_judgment_status']}",
            f"candidate_source_judgment_status: {status['candidate_source_judgment_status']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"source_open_event_changed: {regression['source_open_event_changed']}",
            f"source_judgment_changed: {regression['source_judgment_changed']}",
            f"preview_changed: {regression['preview_changed']}",
            f"evidence_refs_changed: {regression['evidence_refs_changed']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two opening testimony landing summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline opening testimony landing summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening testimony landing summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opening testimony landing compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opening testimony landing compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opening testimony landing compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opening testimony landing compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-landing-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-testimony.landing.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-testimony.landing.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-testimony.landing.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE] verdict={summary['landing_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
