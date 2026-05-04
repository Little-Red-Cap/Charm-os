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


OPEN_EVENT_WITNESS_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
OPEN_EVENT_WITNESS_KIND = "system_compiler.front_page_entry_opening_flow_open_event_witness"
OPEN_EVENT_WITNESS_COMPARE_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
OPEN_EVENT_WITNESS_COMPARE_KIND = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"


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


def load_open_event_witness_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != OPEN_EVENT_WITNESS_SCHEMA:
        raise ValueError(f"unsupported opening-flow open-event witness schema: {path}")
    if choose_text(summary.get("kind")) != OPEN_EVENT_WITNESS_KIND:
        raise ValueError(f"unsupported opening-flow open-event witness kind: {path}")
    return summary


def normalize_compare_path(path_value: Any, witness_output_root_value: Any, source_open_event_summary_value: Any) -> str:
    path_text = choose_text(path_value)
    if not path_text:
        return ""

    try:
        path = Path(path_text).resolve()
    except Exception:
        return normalize_optional_path(path_text)

    witness_output_root_text = choose_text(witness_output_root_value)
    if witness_output_root_text:
        try:
            witness_output_root = Path(witness_output_root_text).resolve()
            path.relative_to(witness_output_root)
            return "$open-event-witness-output"
        except Exception:
            pass

    source_open_event_summary_text = choose_text(source_open_event_summary_value)
    if source_open_event_summary_text:
        try:
            source_open_event_root = Path(source_open_event_summary_text).resolve().parent
            path.relative_to(source_open_event_root)
            if path == Path(source_open_event_summary_text).resolve():
                return "$source-open-event-self"
            return "$source-open-event-output"
        except Exception:
            pass

    return normalize_optional_path(path_text)


def normalize_evidence_ref(ref_value: Any, witness_output_root_value: Any, source_open_event_summary_value: Any) -> OrderedDict[str, str]:
    ref = get_mapping(ref_value)
    role = choose_text(ref.get("role"))
    return OrderedDict(
        [
            ("role", role),
            ("summary_schema", choose_text(ref.get("summary_schema"))),
            ("summary_path", normalize_compare_path(ref.get("summary_path"), witness_output_root_value, source_open_event_summary_value)),
        ]
    )


def normalize_artifact_refs(refs_value: Any, witness_output_root_value: Any, source_open_event_summary_value: Any) -> list[str]:
    return ordered_unique(
        [
            normalize_compare_path(ref, witness_output_root_value, source_open_event_summary_value)
            for ref in get_list(refs_value)
        ]
    )


def normalize_witness_summary(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    identity = get_mapping(summary.get("open_event_identity"))
    judgment = get_mapping(summary.get("judgment"))
    witness_entry = get_mapping(summary.get("witness_entry"))
    subject = get_mapping(witness_entry.get("subject"))
    explanation = get_mapping(summary.get("explanation"))
    output_root = artifact_context.get("output_root")
    source_open_event_summary_path = artifact_context.get("source_open_event_summary_path")
    evidence_refs = [
        normalize_evidence_ref(ref, output_root, source_open_event_summary_path)
        for ref in get_list(summary.get("evidence_refs"))
    ]
    artifact_refs = normalize_artifact_refs(witness_entry.get("artifact_refs"), output_root, source_open_event_summary_path)

    return OrderedDict(
        [
            ("result", choose_text(summary.get("result"))),
            ("open_event_id", choose_text(identity.get("open_event_id"))),
            ("open_event_status", choose_text(identity.get("open_event_status"))),
            ("open_event_result", choose_text(identity.get("open_event_result"))),
            ("reason_kind", choose_text(identity.get("reason_kind"))),
            ("reason_summary", choose_text(identity.get("reason_summary"))),
            ("source_summary_schema", choose_text(identity.get("source_summary_schema"))),
            ("source_summary_kind", choose_text(identity.get("source_summary_kind"))),
            ("source_summary_path", normalize_compare_path(identity.get("source_summary_path"), output_root, source_open_event_summary_path)),
            ("witness_id", choose_text(judgment.get("witness_id"))),
            ("witness_status", choose_text(judgment.get("witness_status"))),
            ("accepted", bool(judgment.get("accepted"))),
            ("selected_consumer_id", choose_text(judgment.get("selected_consumer_id"))),
            ("selected_action_id", choose_text(judgment.get("selected_action_id"))),
            ("candidate_consumer_count", int(judgment.get("candidate_consumer_count", 0))),
            ("rejected_consumer_count", int(judgment.get("rejected_consumer_count", 0))),
            ("compare_available", bool(judgment.get("compare_available"))),
            ("compare_verdict", choose_text(judgment.get("compare_verdict"))),
            ("compare_changed_field_count", int(judgment.get("compare_changed_field_count", 0))),
            ("workspace_facade_status", choose_text(judgment.get("workspace_facade_status"))),
            ("workspace_facade_kind", choose_text(judgment.get("workspace_facade_kind"))),
            ("evidence_ref_count", int(judgment.get("evidence_ref_count", 0))),
            ("artifact_ref_count", int(judgment.get("artifact_ref_count", 0))),
            ("witness_entry_id", choose_text(witness_entry.get("id"))),
            ("witness_entry_kind", choose_text(witness_entry.get("kind"))),
            ("witness_entry_label", choose_text(witness_entry.get("label"))),
            ("witness_entry_role", choose_text(witness_entry.get("role"))),
            ("witness_entry_layer", choose_text(witness_entry.get("layer"))),
            ("witness_entry_required", bool(witness_entry.get("required"))),
            ("witness_entry_status", choose_text(witness_entry.get("status"))),
            ("witness_focus", ordered_unique([choose_text(item) for item in get_list(witness_entry.get("witness_focus"))])),
            ("witness_case", choose_text(witness_entry.get("case"))),
            ("subject_case", choose_text(subject.get("case"))),
            ("subject_profile", choose_text(subject.get("profile"))),
            ("subject_board", choose_text(subject.get("board"))),
            ("subject_active_facets", ordered_unique([choose_text(item) for item in get_list(subject.get("active_facets"))])),
            ("observations", ordered_unique([choose_text(item) for item in get_list(witness_entry.get("observations"))])),
            ("artifact_refs", artifact_refs),
            ("evidence_refs", evidence_refs),
            ("evidence_roles", ordered_unique([choose_text(ref.get("role")) for ref in evidence_refs])),
            (
                "evidence_summary_refs",
                ordered_unique([f"{ref['role']}:{ref['summary_schema']}:{ref['summary_path']}" for ref in evidence_refs]),
            ),
            ("explanation_view_kind", choose_text(explanation.get("view_kind"))),
            ("explanation_why_opened", choose_text(explanation.get("why_opened"))),
            ("explanation_chosen_consumer", choose_text(explanation.get("chosen_consumer"))),
            ("explanation_compare_result", choose_text(explanation.get("compare_result"))),
            ("explanation_text_lines", [choose_text(item) for item in get_list(explanation.get("text_lines"))]),
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


def build_witness_surface(
    witness_summary: dict[str, Any],
    witness_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    artifact_context = get_mapping(witness_summary.get("artifact_context"))
    return make_surface(
        surface_id,
        f"{role.replace('_', ' ')}: opening-flow open event witness",
        role,
        OPEN_EVENT_WITNESS_SCHEMA,
        normalize_path(witness_path),
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
                    build_witness_surface(baseline_summary, baseline_path, "baseline_open_event_witness", "baseline_open_event_witness"),
                    build_witness_surface(candidate_summary, candidate_path, "candidate_open_event_witness", "candidate_open_event_witness"),
                ],
            ),
        ]
    )


def build_witness_provenance_entry(
    witness_summary: dict[str, Any],
    witness_path: Path,
    provenance_id: str,
    witness_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(witness_summary.get("artifact_context"))
    identity = get_mapping(witness_summary.get("open_event_identity"))
    judgment = get_mapping(witness_summary.get("judgment"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("witness_role", witness_role),
            ("source_summary_schema", OPEN_EVENT_WITNESS_SCHEMA),
            ("source_summary_path", normalize_path(witness_path)),
            ("source_report_markdown_path", normalize_optional_path(artifact_context.get("report_markdown_path"))),
            ("source_check_text_path", normalize_optional_path(artifact_context.get("check_text_path"))),
            ("source_open_event_summary_path", normalize_optional_path(artifact_context.get("source_open_event_summary_path"))),
            ("result", choose_text(witness_summary.get("result"))),
            ("open_event_id", choose_text(identity.get("open_event_id"))),
            ("open_event_status", choose_text(identity.get("open_event_status"))),
            ("witness_id", choose_text(judgment.get("witness_id"))),
            ("witness_status", choose_text(judgment.get("witness_status"))),
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


def build_witness_status(baseline: dict[str, Any], candidate: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("baseline_result", baseline["result"]),
            ("candidate_result", candidate["result"]),
            ("baseline_open_event_id", baseline["open_event_id"]),
            ("candidate_open_event_id", candidate["open_event_id"]),
            ("baseline_open_event_status", baseline["open_event_status"]),
            ("candidate_open_event_status", candidate["open_event_status"]),
            ("baseline_witness_id", baseline["witness_id"]),
            ("candidate_witness_id", candidate["witness_id"]),
            ("baseline_witness_status", baseline["witness_status"]),
            ("candidate_witness_status", candidate["witness_status"]),
            ("baseline_selected_consumer_id", baseline["selected_consumer_id"]),
            ("candidate_selected_consumer_id", candidate["selected_consumer_id"]),
            ("baseline_selected_action_id", baseline["selected_action_id"]),
            ("candidate_selected_action_id", candidate["selected_action_id"]),
        ]
    )


def build_regression_surface(
    identity_changes: dict[str, Any],
    judgment_changes: dict[str, Any],
    entry_changes: dict[str, Any],
    evidence_changes: dict[str, Any],
    explanation_changes: dict[str, Any],
    violation_changes: dict[str, Any],
    baseline: dict[str, Any],
    candidate: dict[str, Any],
) -> OrderedDict[str, Any]:
    narratives: list[str] = []
    result_changed = bool(identity_changes.get("result", {}).get("changed"))
    witness_status_changed = bool(judgment_changes.get("witness_status", {}).get("changed"))
    witness_collapsed = baseline["witness_status"] == "ok" and candidate["witness_status"] != "ok"
    witness_recovered = baseline["witness_status"] != "ok" and candidate["witness_status"] == "ok"
    event_identity_changed = bool(identity_changes.get("open_event_id", {}).get("changed")) or bool(
        identity_changes.get("reason_kind", {}).get("changed")
    )
    event_status_changed = bool(identity_changes.get("open_event_status", {}).get("changed"))
    selected_consumer_changed = bool(judgment_changes.get("selected_consumer_id", {}).get("changed"))
    selected_action_changed = bool(judgment_changes.get("selected_action_id", {}).get("changed"))
    compare_context_changed = bool(judgment_changes.get("compare_available", {}).get("changed")) or bool(
        judgment_changes.get("compare_verdict", {}).get("changed")
    )
    workspace_facade_changed = bool(judgment_changes.get("workspace_facade_status", {}).get("changed")) or bool(
        judgment_changes.get("workspace_facade_kind", {}).get("changed")
    )
    evidence_refs_changed = bool(evidence_changes.get("evidence_summary_refs", {}).get("changed")) or bool(
        judgment_changes.get("evidence_ref_count", {}).get("changed")
    )
    artifact_refs_changed = bool(entry_changes.get("artifact_refs", {}).get("changed")) or bool(
        judgment_changes.get("artifact_ref_count", {}).get("changed")
    )
    explanation_changed = bool(explanation_changes.get("explanation_text_lines", {}).get("changed")) or bool(
        explanation_changes.get("explanation_compare_result", {}).get("changed")
    )
    violations_changed = bool(violation_changes.get("violations", {}).get("changed"))

    if result_changed:
        narratives.append(f"witness result changed: {baseline['result']} -> {candidate['result']}")
    if witness_status_changed:
        narratives.append(f"witness status changed: {baseline['witness_status']} -> {candidate['witness_status']}")
    if witness_collapsed:
        narratives.append("candidate witness no longer stands as testimony")
    if witness_recovered:
        narratives.append("candidate witness recovered from a failing testimony")
    if event_identity_changed:
        narratives.append("source opening judgment identity changed")
    if event_status_changed:
        narratives.append(f"source open event status changed: {baseline['open_event_status']} -> {candidate['open_event_status']}")
    if selected_consumer_changed:
        narratives.append(f"selected consumer changed: {baseline['selected_consumer_id']} -> {candidate['selected_consumer_id']}")
    if selected_action_changed:
        narratives.append(f"selected action changed: {baseline['selected_action_id']} -> {candidate['selected_action_id']}")
    if compare_context_changed:
        narratives.append("witness compare context changed")
    if workspace_facade_changed:
        narratives.append("witness workspace facade changed")
    if evidence_refs_changed:
        narratives.append("witness evidence refs changed")
    if artifact_refs_changed:
        narratives.append("witness artifact refs changed")
    if explanation_changed:
        narratives.append("witness explanation changed")
    if violations_changed:
        narratives.append("witness violations changed")

    changed = bool(
        result_changed
        or witness_status_changed
        or event_identity_changed
        or event_status_changed
        or selected_consumer_changed
        or selected_action_changed
        or compare_context_changed
        or workspace_facade_changed
        or evidence_refs_changed
        or artifact_refs_changed
        or explanation_changed
        or violations_changed
    )
    return OrderedDict(
        [
            ("changed", changed),
            ("result_changed", result_changed),
            ("witness_status_changed", witness_status_changed),
            ("witness_collapsed", witness_collapsed),
            ("witness_recovered", witness_recovered),
            ("event_identity_changed", event_identity_changed),
            ("event_status_changed", event_status_changed),
            ("selected_consumer_changed", selected_consumer_changed),
            ("selected_action_changed", selected_action_changed),
            ("compare_context_changed", compare_context_changed),
            ("workspace_facade_changed", workspace_facade_changed),
            ("evidence_refs_changed", evidence_refs_changed),
            ("artifact_refs_changed", artifact_refs_changed),
            ("explanation_changed", explanation_changed),
            ("violations_changed", violations_changed),
            ("narratives", ordered_unique(narratives)),
        ]
    )


def build_witness_verdict(regression_surface: dict[str, Any], candidate: dict[str, Any], baseline: dict[str, Any]) -> str:
    if candidate["result"] != "ok" or candidate["witness_status"] != "ok":
        return "collapsed"
    if not bool(regression_surface.get("changed")):
        return "standing"
    if baseline["witness_status"] != "ok" and candidate["witness_status"] == "ok":
        return "improved"
    return "drifted"


def build_questions(witness_verdict: str, regression_surface: dict[str, Any]) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []
    if witness_verdict == "standing":
        compare_questions.append("Should this open-event witness compare become the canonical testimony self-check?")
    elif witness_verdict == "improved":
        compare_questions.append("Which recovered witness condition should be promoted into explain surface first?")
    elif witness_verdict == "drifted":
        compare_questions.append("Did this testimony drift enough to require a fresh OpenEventRecord review?")
    else:
        compare_questions.append("Which source opening condition collapsed before the witness could stand?")

    if bool(regression_surface.get("event_identity_changed")):
        next_questions.append("Should source open-event identity drift force a witness bundle refresh?")
    if bool(regression_surface.get("compare_context_changed")):
        next_questions.append("Should compare context drift be rendered next to the witness entry?")
    if bool(regression_surface.get("evidence_refs_changed")):
        next_questions.append("Should evidence ref drift become a first-class witness bundle gate?")
    if not next_questions:
        next_questions.append("Should later explain tools consume this witness compare before opening witness details?")
    return OrderedDict([("compare_questions", ordered_unique(compare_questions)), ("next_questions", ordered_unique(next_questions))])


def build_compare_summary_model(
    baseline_witness_path: Path,
    candidate_witness_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_open_event_witness_summary(baseline_witness_path)
    candidate_summary = load_open_event_witness_summary(candidate_witness_path)
    baseline = normalize_witness_summary(baseline_summary)
    candidate = normalize_witness_summary(candidate_summary)

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
            "source_summary_schema",
            "source_summary_kind",
            "source_summary_path",
        ),
    )
    judgment_changes = build_field_changes(
        baseline,
        candidate,
        (
            "witness_id",
            "witness_status",
            "accepted",
            "selected_consumer_id",
            "selected_action_id",
            "candidate_consumer_count",
            "rejected_consumer_count",
            "compare_available",
            "compare_verdict",
            "compare_changed_field_count",
            "workspace_facade_status",
            "workspace_facade_kind",
            "evidence_ref_count",
            "artifact_ref_count",
        ),
    )
    entry_changes = build_field_changes(
        baseline,
        candidate,
        (
            "witness_entry_id",
            "witness_entry_kind",
            "witness_entry_label",
            "witness_entry_role",
            "witness_entry_layer",
            "witness_entry_required",
            "witness_entry_status",
            "witness_focus",
            "witness_case",
            "subject_case",
            "subject_profile",
            "subject_board",
            "subject_active_facets",
            "observations",
            "artifact_refs",
        ),
    )
    evidence_changes = build_field_changes(
        baseline,
        candidate,
        (
            "evidence_roles",
            "evidence_summary_refs",
        ),
    )
    explanation_changes = build_field_changes(
        baseline,
        candidate,
        (
            "explanation_view_kind",
            "explanation_why_opened",
            "explanation_chosen_consumer",
            "explanation_compare_result",
            "explanation_text_lines",
        ),
    )
    violation_changes = build_field_changes(baseline, candidate, ("violations",))
    witness_status = build_witness_status(baseline, candidate)
    regression_surface = build_regression_surface(
        identity_changes,
        judgment_changes,
        entry_changes,
        evidence_changes,
        explanation_changes,
        violation_changes,
        baseline,
        candidate,
    )
    witness_verdict = build_witness_verdict(regression_surface, candidate, baseline)
    changed_field_count = sum(
        int(changes["changed_field_count"])
        for changes in (
            identity_changes,
            judgment_changes,
            entry_changes,
            evidence_changes,
            explanation_changes,
            violation_changes,
        )
    )

    return OrderedDict(
        [
            ("schema", OPEN_EVENT_WITNESS_COMPARE_SCHEMA),
            ("kind", OPEN_EVENT_WITNESS_COMPARE_KIND),
            ("generator", "scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py"),
            ("result", "ok"),
            (
                "opening_flow_open_event_witness_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Open Event Witness Compare"),
                        ("summary", "A compare object that checks whether two opening testimonies preserve the same judgment."),
                    ]
                ),
            ),
            (
                "front_page",
                build_front_page(summary_path, report_path, check_path, baseline_summary, baseline_witness_path, candidate_summary, candidate_witness_path),
            ),
            (
                "witness_provenance",
                [
                    build_witness_provenance_entry(baseline_summary, baseline_witness_path, "baseline_open_event_witness", "baseline_witness"),
                    build_witness_provenance_entry(candidate_summary, candidate_witness_path, "candidate_open_event_witness", "candidate_witness"),
                ],
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_open_event_witness_summary_path", normalize_path(baseline_witness_path)),
                        ("candidate_open_event_witness_summary_path", normalize_path(candidate_witness_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("witness_verdict", witness_verdict),
            ("witness_status", witness_status),
            ("identity_changes", identity_changes),
            ("judgment_changes", judgment_changes),
            ("witness_entry_changes", entry_changes),
            ("evidence_ref_changes", evidence_changes),
            ("explanation_changes", explanation_changes),
            ("violation_changes", violation_changes),
            (
                "change_summary",
                OrderedDict(
                    [
                        ("changed_field_count", changed_field_count),
                        ("identity_changed_field_count", int(identity_changes["changed_field_count"])),
                        ("judgment_changed_field_count", int(judgment_changes["changed_field_count"])),
                        ("witness_entry_changed_field_count", int(entry_changes["changed_field_count"])),
                        ("evidence_changed_field_count", int(evidence_changes["changed_field_count"])),
                        ("explanation_changed_field_count", int(explanation_changes["changed_field_count"])),
                        ("violation_changed_field_count", int(violation_changes["changed_field_count"])),
                    ]
                ),
            ),
            ("witness_regression_surface", regression_surface),
            ("questions", build_questions(witness_verdict, regression_surface)),
            ("violations", []),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = summary["witness_status"]
    change_summary = summary["change_summary"]
    regression = summary["witness_regression_surface"]
    questions = summary["questions"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Open Event Witness Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Witness verdict: `{summary['witness_verdict']}`",
        f"- Baseline witness: `{summary['artifact_context']['baseline_open_event_witness_summary_path']}`",
        f"- Candidate witness: `{summary['artifact_context']['candidate_open_event_witness_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['compare_summary_path']}`",
        "",
        "## Witness Status",
        "- Baseline: `result={0} witness={1} status={2} event={3} event_status={4}`".format(
            status["baseline_result"],
            status["baseline_witness_id"],
            status["baseline_witness_status"],
            status["baseline_open_event_id"],
            status["baseline_open_event_status"],
        ),
        "- Candidate: `result={0} witness={1} status={2} event={3} event_status={4}`".format(
            status["candidate_result"],
            status["candidate_witness_id"],
            status["candidate_witness_status"],
            status["candidate_open_event_id"],
            status["candidate_open_event_status"],
        ),
        "",
        "## Change Summary",
        "- changed_fields=`{0}` identity=`{1}` judgment=`{2}` entry=`{3}` evidence=`{4}` explanation=`{5}` violation=`{6}`".format(
            change_summary["changed_field_count"],
            change_summary["identity_changed_field_count"],
            change_summary["judgment_changed_field_count"],
            change_summary["witness_entry_changed_field_count"],
            change_summary["evidence_changed_field_count"],
            change_summary["explanation_changed_field_count"],
            change_summary["violation_changed_field_count"],
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
    status = summary["witness_status"]
    change_summary = summary["change_summary"]
    regression = summary["witness_regression_surface"]
    return "\n".join(
        [
            f"baseline_open_event_witness_summary_path: {summary['artifact_context']['baseline_open_event_witness_summary_path']}",
            f"candidate_open_event_witness_summary_path: {summary['artifact_context']['candidate_open_event_witness_summary_path']}",
            f"witness_verdict: {summary['witness_verdict']}",
            f"baseline_result: {status['baseline_result']}",
            f"candidate_result: {status['candidate_result']}",
            f"baseline_witness_status: {status['baseline_witness_status']}",
            f"candidate_witness_status: {status['candidate_witness_status']}",
            f"baseline_open_event_status: {status['baseline_open_event_status']}",
            f"candidate_open_event_status: {status['candidate_open_event_status']}",
            f"changed_field_count: {change_summary['changed_field_count']}",
            f"event_identity_changed: {regression['event_identity_changed']}",
            f"compare_context_changed: {regression['compare_context_changed']}",
            f"evidence_refs_changed: {regression['evidence_refs_changed']}",
            f"explanation_changed: {regression['explanation_changed']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two opening-flow open-event witness summaries.")
    parser.add_argument("--baseline", required=True, help="Baseline opening-flow open-event witness summary JSON.")
    parser.add_argument("--candidate", required=True, help="Candidate opening-flow open-event witness summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for open-event witness compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for open-event witness compare summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for open-event witness compare markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for open-event witness compare check text.")
    args = parser.parse_args()

    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-open-event-witness-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.open-event.witness.compare.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.open-event.witness.compare.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.open-event.witness.compare.check.txt")

    try:
        summary = build_compare_summary_model(baseline_path, candidate_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE] summary={summary_path}")
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE] verdict={summary['witness_verdict']}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE] changed_fields={0}".format(
            summary["change_summary"]["changed_field_count"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
