from __future__ import annotations

import argparse
import json
from collections import Counter, OrderedDict
from datetime import datetime
from pathlib import Path
from typing import Any


SOURCE_SCHEMA = "minimal_kernel.runtime_session_witness_inspect_compare/v0"
SOURCE_KIND = "minimal_kernel.runtime_session_witness_inspect_compare"
CONSUMER_SCHEMA = "minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0"
CONSUMER_KIND = "minimal_kernel.runtime_session_witness_inspect_compare_consumer"

SEVERITY_ORDER = {
    "critical": 4,
    "high": 3,
    "medium": 2,
    "low": 1,
    "info": 0,
}


def load_json(path: Path) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(document, dict):
        raise ValueError(f"expected JSON object: {path}")
    return document


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def resolve_output_path(explicit: str, output_root: Path, default_name: str) -> Path:
    if explicit:
        return Path(explicit).resolve()
    return (output_root / default_name).resolve()


def normalize_path(value: str | Path) -> str:
    return str(Path(value).resolve())


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


def string_list(value: Any) -> list[str]:
    result: list[str] = []
    for item in get_list(value):
        text = choose_text(item)
        if text:
            result.append(text)
    return result


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


def load_compare_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != SOURCE_SCHEMA:
        raise ValueError(f"unsupported compare summary schema: {path}")
    if choose_text(summary.get("kind")) != SOURCE_KIND:
        raise ValueError(f"unsupported compare summary kind: {path}")
    return summary


def make_artifact_ref(ref_id: str, label: str, path_value: Any) -> OrderedDict[str, str] | None:
    text = choose_text(path_value)
    if not text:
        return None
    return OrderedDict(
        [
            ("id", ref_id),
            ("label", label),
            ("path", normalize_path(text)),
        ]
    )


def build_supporting_artifacts(summary: dict[str, Any]) -> list[OrderedDict[str, str]]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    current = get_mapping(summary.get("current"))
    artifacts = get_mapping(current.get("artifacts"))
    session = get_mapping(artifacts.get("session"))
    world_compare = get_mapping(artifacts.get("world_compare_session_drift"))
    witness_compare = get_mapping(artifacts.get("witness_session_failure_export"))

    refs = [
        make_artifact_ref("source-compare-summary", "source compare summary", artifact_context.get("compare_summary_path")),
        make_artifact_ref("current-summary", "current witness summary", current.get("summary_path")),
        make_artifact_ref("current-report", "current witness report", artifacts.get("report_markdown")),
        make_artifact_ref("current-check", "current witness check", artifacts.get("check_text")),
        make_artifact_ref("session-summary", "kernel runtime session summary", session.get("summary")),
        make_artifact_ref("runtime-ledger", "kernel runtime session runtime ledger", session.get("runtime_ledger")),
        make_artifact_ref("session-report", "kernel runtime session report", session.get("report_markdown")),
        make_artifact_ref("session-check", "kernel runtime session check", session.get("check_text")),
        make_artifact_ref("world-compare-summary", "world compare session drift summary", world_compare.get("summary")),
        make_artifact_ref("world-compare-report", "world compare session drift report", world_compare.get("report_markdown")),
        make_artifact_ref("world-compare-check", "world compare session drift check", world_compare.get("check_text")),
        make_artifact_ref("witness-compare-summary", "witness export compare summary", witness_compare.get("world_compare_summary")),
        make_artifact_ref("witness-compare-report", "witness export compare report", witness_compare.get("world_compare_report_markdown")),
        make_artifact_ref("witness-compare-check", "witness export compare check", witness_compare.get("world_compare_check_text")),
        make_artifact_ref("witness-baseline-summary", "witness export baseline summary", witness_compare.get("baseline_summary")),
        make_artifact_ref("witness-candidate-summary", "witness export candidate summary", witness_compare.get("candidate_summary")),
    ]

    return [ref for ref in refs if ref is not None]


def build_ref_lookup(refs: list[OrderedDict[str, str]]) -> dict[str, OrderedDict[str, str]]:
    return {ref["id"]: ref for ref in refs}


def attach_refs(ref_lookup: dict[str, OrderedDict[str, str]], ref_ids: list[str]) -> list[OrderedDict[str, str]]:
    attached: list[OrderedDict[str, str]] = []
    for ref_id in ref_ids:
        if ref_id in ref_lookup:
            attached.append(ref_lookup[ref_id])
    return attached


def severity_rank(value: str) -> int:
    return SEVERITY_ORDER.get(value, -1)


def choose_highest_severity(entries: list[dict[str, Any]]) -> str:
    highest = "info"
    for entry in entries:
        severity = choose_text(entry.get("severity")) or "info"
        if severity_rank(severity) > severity_rank(highest):
            highest = severity
    return highest


def build_source_compare(summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    comparison = get_mapping(summary.get("comparison"))
    session = get_mapping(comparison.get("session"))
    runtime = get_mapping(session.get("runtime"))
    world_compare = get_mapping(comparison.get("world_compare_session_drift"))
    witness_compare = get_mapping(comparison.get("witness_session_failure_export"))
    violations = get_mapping(comparison.get("violations"))

    return OrderedDict(
        [
            ("source_result", choose_text(summary.get("result"))),
            ("changed", bool(comparison.get("changed"))),
            ("baseline_summary_path", normalize_path(artifact_context.get("baseline_summary_path"))),
            ("candidate_summary_path", normalize_path(artifact_context.get("summary_path"))),
            ("baseline_result", choose_text(get_mapping(comparison.get("result")).get("baseline"))),
            ("current_result", choose_text(get_mapping(comparison.get("result")).get("current"))),
            ("baseline_session_status", choose_text(get_mapping(session.get("status")).get("baseline"))),
            ("current_session_status", choose_text(get_mapping(session.get("status")).get("current"))),
            ("baseline_failure_domain", choose_text(get_mapping(session.get("failure_domain")).get("baseline"))),
            ("current_failure_domain", choose_text(get_mapping(session.get("failure_domain")).get("current"))),
            ("runtime_regression_count", len(string_list(runtime.get("regressed")))),
            ("runtime_improvement_count", len(string_list(runtime.get("improved")))),
            ("world_failure_code_delta_count", len(string_list(get_mapping(world_compare.get("failure_codes")).get("added")))),
            ("witness_failure_code_delta_count", len(string_list(get_mapping(witness_compare.get("failure_codes")).get("added")))),
            ("violation_delta_count", len(string_list(violations.get("added")))),
        ]
    )


def make_focus_entry(
    focus_id: str,
    focus_kind: str,
    priority: int,
    severity: str,
    changed: bool,
    headline: str,
    reason: str,
    session_status_baseline: str,
    session_status_current: str,
    failure_domain_baseline: str,
    failure_domain_current: str,
    runtime_regressions: list[str],
    runtime_improvements: list[str],
    added_failure_codes: list[str],
    removed_failure_codes: list[str],
    added_missing_runtime_facts: list[str],
    removed_missing_runtime_facts: list[str],
    added_affected_focus: list[str],
    removed_affected_focus: list[str],
    added_violations: list[str],
    removed_violations: list[str],
    artifact_refs: list[OrderedDict[str, str]],
    summary_lines: list[str],
    question_lines: list[str],
) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("focus_id", focus_id),
            ("focus_kind", focus_kind),
            ("priority", priority),
            ("severity", severity),
            ("changed", changed),
            ("headline", headline),
            ("reason", reason),
            ("session_status_baseline", session_status_baseline),
            ("session_status_current", session_status_current),
            ("failure_domain_baseline", failure_domain_baseline),
            ("failure_domain_current", failure_domain_current),
            ("runtime_regressions", runtime_regressions),
            ("runtime_improvements", runtime_improvements),
            ("added_failure_codes", added_failure_codes),
            ("removed_failure_codes", removed_failure_codes),
            ("added_missing_runtime_facts", added_missing_runtime_facts),
            ("removed_missing_runtime_facts", removed_missing_runtime_facts),
            ("added_affected_focus", added_affected_focus),
            ("removed_affected_focus", removed_affected_focus),
            ("added_violations", added_violations),
            ("removed_violations", removed_violations),
            ("artifact_refs", artifact_refs),
            ("summary_lines", summary_lines),
            ("question_lines", question_lines),
        ]
    )


EXPLAIN_HOP_POLICY: dict[str, dict[str, Any]] = {
    "session_state_drift": {
        "artifact_ids": [
            "session-report",
            "session-summary",
            "current-report",
            "current-summary",
            "session-check",
            "source-compare-summary",
        ],
        "reason_kind": "session_report",
        "reason": "session report keeps session status, failure domain, and ledger delta in one explainable surface.",
    },
    "runtime_regression": {
        "artifact_ids": [
            "runtime-ledger",
            "session-report",
            "session-summary",
            "source-compare-summary",
        ],
        "reason_kind": "runtime_ledger",
        "reason": "runtime ledger is the smallest stable surface for tick, trap, thread, and continuity drift.",
    },
    "world_compare_drift": {
        "artifact_ids": [
            "world-compare-report",
            "world-compare-summary",
            "world-compare-check",
            "source-compare-summary",
        ],
        "reason_kind": "world_compare_report",
        "reason": "world compare report keeps failure taxonomy, missing runtime facts, and collapse projection together.",
    },
    "witness_compare_drift": {
        "artifact_ids": [
            "witness-compare-report",
            "witness-compare-summary",
            "witness-compare-check",
            "source-compare-summary",
        ],
        "reason_kind": "witness_compare_report",
        "reason": "witness compare report is the thinnest entry for export-side collapse and witness drift.",
    },
    "violation_drift": {
        "artifact_ids": [
            "current-check",
            "current-report",
            "current-summary",
            "source-compare-summary",
        ],
        "reason_kind": "check_surface",
        "reason": "check text is the shortest gate-facing surface once the lower compare chain has already converged.",
    },
    "steady_state": {
        "artifact_ids": [
            "current-summary",
            "session-summary",
            "source-compare-summary",
        ],
        "reason_kind": "standing_summary",
        "reason": "current witness summary is the simplest standing anchor when no actionable drift exists.",
    },
}


def choose_explain_hop_target(
    artifact_refs: list[OrderedDict[str, str]],
    preferred_ids: list[str],
) -> tuple[OrderedDict[str, str], list[OrderedDict[str, str]]]:
    ref_lookup = {choose_text(ref.get("id")): ref for ref in artifact_refs if choose_text(ref.get("id"))}

    selected: OrderedDict[str, str] | None = None
    for artifact_id in preferred_ids:
        if artifact_id in ref_lookup:
            selected = ref_lookup[artifact_id]
            break

    if selected is None:
        if not artifact_refs:
            raise ValueError("focus entry has no artifact refs for explain hop selection")
        selected = artifact_refs[0]

    selected_id = choose_text(selected.get("id"))
    fallback_refs = [ref for ref in artifact_refs if choose_text(ref.get("id")) != selected_id]
    return selected, fallback_refs


def build_preferred_explain_hop(entry: OrderedDict[str, Any]) -> OrderedDict[str, Any]:
    policy = EXPLAIN_HOP_POLICY.get(choose_text(entry.get("focus_kind")), EXPLAIN_HOP_POLICY["steady_state"])
    artifact_refs = [
        ref for ref in get_list(entry.get("artifact_refs")) if isinstance(ref, dict) and choose_text(ref.get("id"))
    ]
    selected_ref, fallback_refs = choose_explain_hop_target(
        artifact_refs=artifact_refs,
        preferred_ids=list(policy["artifact_ids"]),
    )

    return OrderedDict(
        [
            ("hop_id", "{0}-hop".format(choose_text(entry.get("focus_id")) or "focus")),
            ("focus_id", choose_text(entry.get("focus_id"))),
            ("focus_kind", choose_text(entry.get("focus_kind"))),
            ("artifact_ref", selected_ref),
            ("fallback_artifact_refs", fallback_refs),
            ("reason_kind", choose_text(policy["reason_kind"])),
            ("reason", choose_text(policy["reason"])),
            ("headline", choose_text(entry.get("headline"))),
            ("summary_lines", string_list(entry.get("summary_lines"))),
            ("question_lines", string_list(entry.get("question_lines"))),
        ]
    )


def attach_preferred_explain_hops(entries: list[OrderedDict[str, Any]]) -> list[OrderedDict[str, Any]]:
    for entry in entries:
        entry["preferred_explain_hop"] = build_preferred_explain_hop(entry)
    return entries


def build_default_explain_hop(entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    if not entries:
        raise ValueError("expected at least one focus entry for default explain hop")
    return entries[0]["preferred_explain_hop"]


def build_fallback_explain_hops(entries: list[OrderedDict[str, Any]]) -> list[OrderedDict[str, Any]]:
    return [entry["preferred_explain_hop"] for entry in entries[1:]]


def build_focus_entries(summary: dict[str, Any], ref_lookup: dict[str, OrderedDict[str, str]]) -> list[OrderedDict[str, Any]]:
    comparison = get_mapping(summary.get("comparison"))
    session = get_mapping(comparison.get("session"))
    result_compare = get_mapping(comparison.get("result"))
    session_status = get_mapping(session.get("status"))
    failure_domain = get_mapping(session.get("failure_domain"))
    runtime = get_mapping(session.get("runtime"))
    world_compare = get_mapping(comparison.get("world_compare_session_drift"))
    witness_compare = get_mapping(comparison.get("witness_session_failure_export"))
    violations = get_mapping(comparison.get("violations"))

    baseline_session_status = choose_text(session_status.get("baseline"))
    current_session_status = choose_text(session_status.get("current"))
    baseline_failure_domain = choose_text(failure_domain.get("baseline"))
    current_failure_domain = choose_text(failure_domain.get("current"))

    runtime_regressions = string_list(runtime.get("regressed"))
    runtime_improvements = string_list(runtime.get("improved"))

    world_failure_added = string_list(get_mapping(world_compare.get("failure_codes")).get("added"))
    world_failure_removed = string_list(get_mapping(world_compare.get("failure_codes")).get("removed"))
    world_missing_added = string_list(get_mapping(world_compare.get("missing_runtime_facts")).get("added"))
    world_missing_removed = string_list(get_mapping(world_compare.get("missing_runtime_facts")).get("removed"))
    world_focus_added = string_list(get_mapping(world_compare.get("affected_focus")).get("added"))
    world_focus_removed = string_list(get_mapping(world_compare.get("affected_focus")).get("removed"))

    witness_failure_added = string_list(get_mapping(witness_compare.get("failure_codes")).get("added"))
    witness_failure_removed = string_list(get_mapping(witness_compare.get("failure_codes")).get("removed"))
    witness_missing_added = string_list(get_mapping(witness_compare.get("missing_runtime_facts")).get("added"))
    witness_missing_removed = string_list(get_mapping(witness_compare.get("missing_runtime_facts")).get("removed"))
    witness_focus_added = string_list(get_mapping(witness_compare.get("affected_focus")).get("added"))
    witness_focus_removed = string_list(get_mapping(witness_compare.get("affected_focus")).get("removed"))

    violation_added = string_list(violations.get("added"))
    violation_removed = string_list(violations.get("removed"))

    entries: list[OrderedDict[str, Any]] = []

    session_changed = bool(result_compare.get("changed")) or bool(session_status.get("changed")) or bool(failure_domain.get("changed"))
    if session_changed or int(session.get("failure_count_delta", 0)) != 0 or int(session.get("ledger_event_count_delta", 0)) != 0:
        severity = "critical" if choose_text(result_compare.get("current")) == "fail" or current_session_status == "collapsed" else "high"
        summary_lines = [
            "result {0} -> {1}".format(choose_text(result_compare.get("baseline")) or "-", choose_text(result_compare.get("current")) or "-"),
            "session {0} -> {1}".format(baseline_session_status or "-", current_session_status or "-"),
            "failure domain {0} -> {1}".format(baseline_failure_domain or "-", current_failure_domain or "-"),
            "failure delta {0}, ledger delta {1}".format(
                int(session.get("failure_count_delta", 0)),
                int(session.get("ledger_event_count_delta", 0)),
            ),
        ]
        question_lines = [
            "Did the session collapse before runtime continuity recovered?",
            "Which runtime regression best explains the new session state?",
        ]
        entries.append(
            make_focus_entry(
                focus_id="session-state-drift",
                focus_kind="session_state_drift",
                priority=0,
                severity=severity,
                changed=True,
                headline="Session state drifted {0} -> {1}".format(baseline_session_status or "-", current_session_status or "-"),
                reason="result or session status no longer matches the baseline witness",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=runtime_regressions,
                runtime_improvements=runtime_improvements,
                added_failure_codes=[],
                removed_failure_codes=[],
                added_missing_runtime_facts=[],
                removed_missing_runtime_facts=[],
                added_affected_focus=[],
                removed_affected_focus=[],
                added_violations=[],
                removed_violations=[],
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "current-summary", "session-summary", "session-report", "session-check"]),
                summary_lines=summary_lines,
                question_lines=question_lines,
            )
        )

    if runtime_regressions or runtime_improvements:
        severity = "high" if runtime_regressions else "low"
        summary_lines = [
            "regressed runtime facts: {0}".format(", ".join(runtime_regressions) or "none"),
            "improved runtime facts: {0}".format(", ".join(runtime_improvements) or "none"),
        ]
        question_lines = [
            "Should runtime diagnosis start from the session runtime ledger?",
            "Which missing runtime fact is most likely to explain the drift?",
        ]
        entries.append(
            make_focus_entry(
                focus_id="runtime-regression",
                focus_kind="runtime_regression",
                priority=10,
                severity=severity,
                changed=True,
                headline="Runtime facts changed: regressed [{0}] improved [{1}]".format(
                    ", ".join(runtime_regressions) or "-",
                    ", ".join(runtime_improvements) or "-",
                ),
                reason="runtime fact drift is the smallest direct explanation surface",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=runtime_regressions,
                runtime_improvements=runtime_improvements,
                added_failure_codes=[],
                removed_failure_codes=[],
                added_missing_runtime_facts=[],
                removed_missing_runtime_facts=[],
                added_affected_focus=[],
                removed_affected_focus=[],
                added_violations=[],
                removed_violations=[],
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "session-summary", "runtime-ledger", "session-report"]),
                summary_lines=summary_lines,
                question_lines=question_lines,
            )
        )

    world_changed = bool(get_mapping(world_compare.get("verdict")).get("changed")) or bool(get_mapping(world_compare.get("changed_flag")).get("changed"))
    if world_changed or world_failure_added or world_failure_removed or world_missing_added or world_missing_removed or world_focus_added or world_focus_removed:
        severity = "high" if world_failure_added or world_missing_added else "medium"
        summary_lines = [
            "world verdict {0} -> {1}".format(
                choose_text(get_mapping(world_compare.get("verdict")).get("baseline")) or "-",
                choose_text(get_mapping(world_compare.get("verdict")).get("current")) or "-",
            ),
            "added failure codes: {0}".format(", ".join(world_failure_added) or "none"),
            "added missing runtime facts: {0}".format(", ".join(world_missing_added) or "none"),
        ]
        question_lines = [
            "Did world compare add a new failure taxonomy that should be reviewed first?",
            "Should the world compare report become the next explain hop?",
        ]
        entries.append(
            make_focus_entry(
                focus_id="world-compare-drift",
                focus_kind="world_compare_drift",
                priority=20,
                severity=severity,
                changed=True,
                headline="World compare drift added failure taxonomy [{0}]".format(", ".join(world_failure_added) or "-"),
                reason="world compare projected new session drift evidence",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=[],
                runtime_improvements=[],
                added_failure_codes=world_failure_added,
                removed_failure_codes=world_failure_removed,
                added_missing_runtime_facts=world_missing_added,
                removed_missing_runtime_facts=world_missing_removed,
                added_affected_focus=world_focus_added,
                removed_affected_focus=world_focus_removed,
                added_violations=[],
                removed_violations=[],
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "world-compare-summary", "world-compare-report", "world-compare-check"]),
                summary_lines=summary_lines,
                question_lines=question_lines,
            )
        )

    witness_changed = bool(get_mapping(witness_compare.get("verdict")).get("changed")) or bool(get_mapping(witness_compare.get("changed_flag")).get("changed"))
    if witness_changed or witness_failure_added or witness_failure_removed or witness_missing_added or witness_missing_removed or witness_focus_added or witness_focus_removed:
        severity = "high" if witness_failure_added or witness_missing_added else "medium"
        summary_lines = [
            "witness verdict {0} -> {1}".format(
                choose_text(get_mapping(witness_compare.get("verdict")).get("baseline")) or "-",
                choose_text(get_mapping(witness_compare.get("verdict")).get("current")) or "-",
            ),
            "added failure codes: {0}".format(", ".join(witness_failure_added) or "none"),
            "added missing runtime facts: {0}".format(", ".join(witness_missing_added) or "none"),
        ]
        question_lines = [
            "Should witness-export collapse be reviewed before deeper runtime details?",
            "Which witness compare artifact best explains the new export drift?",
        ]
        entries.append(
            make_focus_entry(
                focus_id="witness-compare-drift",
                focus_kind="witness_compare_drift",
                priority=30,
                severity=severity,
                changed=True,
                headline="Witness compare drift added failure taxonomy [{0}]".format(", ".join(witness_failure_added) or "-"),
                reason="witness export compare exposed new collapse evidence",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=[],
                runtime_improvements=[],
                added_failure_codes=witness_failure_added,
                removed_failure_codes=witness_failure_removed,
                added_missing_runtime_facts=witness_missing_added,
                removed_missing_runtime_facts=witness_missing_removed,
                added_affected_focus=witness_focus_added,
                removed_affected_focus=witness_focus_removed,
                added_violations=[],
                removed_violations=[],
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "witness-compare-summary", "witness-compare-report", "witness-compare-check"]),
                summary_lines=summary_lines,
                question_lines=question_lines,
            )
        )

    if violation_added or violation_removed:
        severity = "medium" if violation_added else "low"
        summary_lines = [
            "added violations: {0}".format(", ".join(violation_added) or "none"),
            "removed violations: {0}".format(", ".join(violation_removed) or "none"),
        ]
        question_lines = [
            "Should the added violations become the next gate review surface?",
            "Did violation drift happen after the runtime facts regressed?",
        ]
        entries.append(
            make_focus_entry(
                focus_id="violation-drift",
                focus_kind="violation_drift",
                priority=40,
                severity=severity,
                changed=True,
                headline="Violation drift changed [{0}]".format(", ".join(violation_added) or ", ".join(violation_removed) or "-"),
                reason="summary-level violations changed even after lower-level comparison completed",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=[],
                runtime_improvements=[],
                added_failure_codes=[],
                removed_failure_codes=[],
                added_missing_runtime_facts=[],
                removed_missing_runtime_facts=[],
                added_affected_focus=[],
                removed_affected_focus=[],
                added_violations=violation_added,
                removed_violations=violation_removed,
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "current-summary", "current-check"]),
                summary_lines=summary_lines,
                question_lines=question_lines,
            )
        )

    if not entries:
        entries.append(
            make_focus_entry(
                focus_id="steady-state",
                focus_kind="steady_state",
                priority=100,
                severity="info",
                changed=False,
                headline="No compare drift detected",
                reason="baseline and candidate currently preserve the same inspect compare surface",
                session_status_baseline=baseline_session_status,
                session_status_current=current_session_status,
                failure_domain_baseline=baseline_failure_domain,
                failure_domain_current=current_failure_domain,
                runtime_regressions=[],
                runtime_improvements=[],
                added_failure_codes=[],
                removed_failure_codes=[],
                added_missing_runtime_facts=[],
                removed_missing_runtime_facts=[],
                added_affected_focus=[],
                removed_affected_focus=[],
                added_violations=[],
                removed_violations=[],
                artifact_refs=attach_refs(ref_lookup, ["source-compare-summary", "current-summary", "session-summary"]),
                summary_lines=["compare summary reports no actionable drift"],
                question_lines=["Should a higher explain layer keep this compare witness as a standing anchor?"],
            )
        )

    entries.sort(key=lambda entry: (int(entry["priority"]), -severity_rank(entry["severity"]), entry["focus_id"]))
    return entries


def build_readiness_surface(entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    severity_counts = Counter(entry["severity"] for entry in entries)
    focus_kind_counts = Counter(entry["focus_kind"] for entry in entries)
    changed_focus_ids = [entry["focus_id"] for entry in entries if bool(entry["changed"])]
    actionable_focus_ids = [entry["focus_id"] for entry in entries if entry["focus_kind"] != "steady_state"]

    return OrderedDict(
        [
            ("severity_counts", OrderedDict(sorted(severity_counts.items()))),
            ("focus_kind_counts", OrderedDict(sorted(focus_kind_counts.items()))),
            ("changed_focus_ids", changed_focus_ids),
            ("actionable_focus_ids", actionable_focus_ids),
        ]
    )


def build_consumer_status(summary: dict[str, Any], entries: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    comparison = get_mapping(summary.get("comparison"))
    world_compare = get_mapping(comparison.get("world_compare_session_drift"))
    witness_compare = get_mapping(comparison.get("witness_session_failure_export"))
    violations = get_mapping(comparison.get("violations"))
    runtime = get_mapping(get_mapping(comparison.get("session")).get("runtime"))

    added_failure_code_count = len(string_list(get_mapping(world_compare.get("failure_codes")).get("added"))) + len(
        string_list(get_mapping(witness_compare.get("failure_codes")).get("added"))
    )
    added_missing_runtime_fact_count = len(string_list(get_mapping(world_compare.get("missing_runtime_facts")).get("added"))) + len(
        string_list(get_mapping(witness_compare.get("missing_runtime_facts")).get("added"))
    )
    added_violation_count = len(string_list(violations.get("added")))
    runtime_regression_count = len(string_list(runtime.get("regressed")))
    changed_focus_count = sum(1 for entry in entries if bool(entry["changed"]))
    actionable_focus_count = sum(1 for entry in entries if entry["focus_kind"] != "steady_state")

    source_result = choose_text(summary.get("result"))
    result = "ok" if source_result == "ok" and len(entries) > 0 else "fail"

    return OrderedDict(
        [
            ("result", result),
            ("total_focus_count", len(entries)),
            ("changed_focus_count", changed_focus_count),
            ("actionable_focus_count", actionable_focus_count),
            ("default_focus_id", entries[0]["focus_id"]),
            ("highest_severity", choose_highest_severity(entries)),
            ("runtime_regression_count", runtime_regression_count),
            ("added_failure_code_count", added_failure_code_count),
            ("added_missing_runtime_fact_count", added_missing_runtime_fact_count),
            ("added_violation_count", added_violation_count),
        ]
    )


def build_questions() -> OrderedDict[str, list[str]]:
    return OrderedDict(
        [
            (
                "consumer_questions",
                [
                    "Which session drift should a reader inspect first?",
                    "Which runtime regression or failure taxonomy best explains the compare verdict?",
                ],
            ),
            (
                "next_questions",
                [
                    "Should the next explain hop open the runtime ledger, world compare, or witness compare artifact?",
                    "Which unchanged runtime fact still anchors the current session despite drift elsewhere?",
                ],
            ),
        ]
    )


def build_summary_model(
    compare_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    compare_summary = load_compare_summary(compare_summary_path)
    supporting_artifacts = build_supporting_artifacts(compare_summary)
    ref_lookup = build_ref_lookup(supporting_artifacts)
    focus_entries = attach_preferred_explain_hops(build_focus_entries(compare_summary, ref_lookup))
    consumer_status = build_consumer_status(compare_summary, focus_entries)
    default_explain_hop = build_default_explain_hop(focus_entries)
    fallback_explain_hops = build_fallback_explain_hops(focus_entries)

    return OrderedDict(
        [
            ("schema", CONSUMER_SCHEMA),
            ("kind", CONSUMER_KIND),
            ("generator", "scripts/export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py"),
            ("result", consumer_status["result"]),
            (
                "compare_consumer",
                OrderedDict(
                    [
                        ("title", "Minimal Kernel Runtime Session Witness Inspect Compare Consumer"),
                        (
                            "summary",
                            "A thin consumer handoff that turns session-witness inspect compare drift into ordered explainable focus entries.",
                        ),
                    ]
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_compare_summary_path", normalize_path(compare_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("consumer_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("source_compare", build_source_compare(compare_summary)),
            ("consumer_status", consumer_status),
            ("default_focus", focus_entries[0]),
            ("default_explain_hop", default_explain_hop),
            ("fallback_explain_hops", fallback_explain_hops),
            ("focus_entries", focus_entries),
            ("readiness_surface", build_readiness_surface(focus_entries)),
            ("supporting_artifacts", supporting_artifacts),
            ("questions", build_questions()),
            (
                "violations",
                []
                if consumer_status["result"] == "ok"
                else ["source compare summary is not consumable"],
            ),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    status = get_mapping(summary.get("consumer_status"))
    default_focus = get_mapping(summary.get("default_focus"))
    default_explain_hop = get_mapping(summary.get("default_explain_hop"))

    lines: list[str] = [
        "# Minimal Kernel Runtime Session Witness Inspect Compare Consumer",
        "",
        f"- Result: `{summary['result']}`",
        f"- Source compare summary: `{summary['artifact_context']['source_compare_summary_path']}`",
        f"- Consumer summary: `{summary['artifact_context']['consumer_summary_path']}`",
        "",
        "## Consumer Status",
        "- focuses=`{0}` changed=`{1}` actionable=`{2}` highest_severity=`{3}`".format(
            status["total_focus_count"],
            status["changed_focus_count"],
            status["actionable_focus_count"],
            status["highest_severity"],
        ),
        "- runtime regressions=`{0}` added failure codes=`{1}` added missing runtime facts=`{2}` added violations=`{3}`".format(
            status["runtime_regression_count"],
            status["added_failure_code_count"],
            status["added_missing_runtime_fact_count"],
            status["added_violation_count"],
        ),
        "",
        "## Default Focus",
        "- `{0}` kind=`{1}` severity=`{2}`".format(
            default_focus["focus_id"],
            default_focus["focus_kind"],
            default_focus["severity"],
        ),
        f"- headline: {default_focus['headline']}",
        "- next explain hop: `{0}` -> `{1}` reason=`{2}`".format(
            choose_text(get_mapping(default_explain_hop.get("artifact_ref")).get("id")) or "-",
            choose_text(get_mapping(default_explain_hop.get("artifact_ref")).get("path")) or "-",
            choose_text(default_explain_hop.get("reason_kind")) or "-",
        ),
    ]
    for line in default_focus["summary_lines"]:
        lines.append(f"  - {line}")

    lines.extend(["", "## Focus Entries"])
    for entry in summary["focus_entries"]:
        lines.append(
            "- `{0}` kind=`{1}` severity=`{2}` changed=`{3}`".format(
                entry["focus_id"],
                entry["focus_kind"],
                entry["severity"],
                entry["changed"],
            )
        )
        lines.append(f"  - {entry['headline']}")
        explain_hop = get_mapping(entry.get("preferred_explain_hop"))
        explain_ref = get_mapping(explain_hop.get("artifact_ref"))
        lines.append(
            "  - explain hop: `{0}` reason=`{1}`".format(
                choose_text(explain_ref.get("id")) or "-",
                choose_text(explain_hop.get("reason_kind")) or "-",
            )
        )
        for line in entry["summary_lines"][:3]:
            lines.append(f"  - {line}")

    lines.extend(["", "## Fallback Explain Hops"])
    for hop in get_list(summary.get("fallback_explain_hops")):
        hop_map = get_mapping(hop)
        ref = get_mapping(hop_map.get("artifact_ref"))
        lines.append(
            "- `{0}` focus=`{1}` artifact=`{2}` reason=`{3}`".format(
                choose_text(hop_map.get("hop_id")) or "-",
                choose_text(hop_map.get("focus_id")) or "-",
                choose_text(ref.get("id")) or "-",
                choose_text(hop_map.get("reason_kind")) or "-",
            )
        )

    lines.extend(["", "## Supporting Artifacts"])
    for ref in summary["supporting_artifacts"]:
        lines.append(f"- `{ref['id']}` -> `{ref['path']}`")

    lines.extend(["", "## Questions"])
    for question in summary["questions"]["consumer_questions"]:
        lines.append(f"- consumer: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")

    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    status = get_mapping(summary.get("consumer_status"))
    source_compare = get_mapping(summary.get("source_compare"))
    default_focus = get_mapping(summary.get("default_focus"))
    default_explain_hop = get_mapping(summary.get("default_explain_hop"))
    default_explain_ref = get_mapping(default_explain_hop.get("artifact_ref"))

    return "\n".join(
        [
            f"result: {summary['result']}",
            f"source_compare_summary_path: {summary['artifact_context']['source_compare_summary_path']}",
            f"source_compare_changed: {source_compare['changed']}",
            f"baseline_session_status: {source_compare['baseline_session_status']}",
            f"current_session_status: {source_compare['current_session_status']}",
            f"total_focus_count: {status['total_focus_count']}",
            f"changed_focus_count: {status['changed_focus_count']}",
            f"actionable_focus_count: {status['actionable_focus_count']}",
            f"default_focus_id: {status['default_focus_id']}",
            f"highest_severity: {status['highest_severity']}",
            f"runtime_regression_count: {status['runtime_regression_count']}",
            f"default_focus_headline: {default_focus['headline']}",
            f"default_explain_hop_id: {default_explain_hop.get('hop_id', '')}",
            f"default_explain_artifact_id: {default_explain_ref.get('id', '')}",
            f"default_explain_reason_kind: {default_explain_hop.get('reason_kind', '')}",
            f"fallback_explain_hop_count: {len(get_list(summary.get('fallback_explain_hops')))}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a consumer handoff for a minimal-kernel runtime session witness inspect compare summary."
    )
    parser.add_argument("--compare", required=True, help="Input inspect compare summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for compare consumer artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for consumer summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for consumer markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for consumer check text.")
    args = parser.parse_args()

    compare_path = Path(args.compare).resolve()
    output_root = Path(args.output_root or "out/minimal-kernel-runtime-session-witness-inspect-compare-consumer").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "session-witness.inspect.compare.consumer.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "session-witness.inspect.compare.consumer.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "session-witness.inspect.compare.consumer.check.txt")

    try:
        summary = build_summary_model(
            compare_summary_path=compare_path,
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

    print(f"[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER] summary={summary_path}")
    print(f"[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER] default={summary['consumer_status']['default_focus_id']}")
    print(f"[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER] focuses={summary['consumer_status']['total_focus_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
