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
OPEN_EVENT_WITNESS_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
OPEN_EVENT_WITNESS_KIND = "system_compiler.front_page_entry_opening_flow_open_event_witness"


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


def normalize_witness_ref(value: Any) -> OrderedDict[str, str]:
    ref = get_mapping(value)
    return OrderedDict(
        [
            ("role", choose_text(ref.get("role"))),
            ("summary_schema", choose_text(ref.get("summary_schema"))),
            ("summary_path", normalize_optional_path(ref.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(ref.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(ref.get("check_text_path"))),
        ]
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    open_event_summary: dict[str, Any],
    open_event_summary_path: Path,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(open_event_summary.get("artifact_context"))
    supporting_surfaces: list[OrderedDict[str, str]] = [
        make_surface(
            "source_open_event",
            "source opening-flow open event",
            "source_open_event",
            OPEN_EVENT_SCHEMA,
            normalize_path(open_event_summary_path),
            normalize_optional_path(artifact_context.get("report_markdown_path")),
            normalize_optional_path(artifact_context.get("check_text_path")),
        )
    ]

    for index, ref_value in enumerate(get_list(open_event_summary.get("witness_refs"))):
        ref = normalize_witness_ref(ref_value)
        role = choose_text(ref.get("role")) or f"witness_ref_{index}"
        supporting_surfaces.append(
            make_surface(
                f"source_witness_ref_{index}_{role}",
                f"source witness ref: {role}",
                f"source_witness_ref:{role}",
                ref["summary_schema"],
                ref["summary_path"],
                ref["report_markdown_path"],
                ref["check_text_path"],
            )
        )

    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", supporting_surfaces),
        ]
    )


def build_artifact_refs(open_event_summary: dict[str, Any], open_event_summary_path: Path) -> list[str]:
    artifact_context = get_mapping(open_event_summary.get("artifact_context"))
    event = get_mapping(open_event_summary.get("open_event"))
    source_artifact = get_mapping(event.get("source_artifact"))
    refs: list[str] = [
        normalize_path(open_event_summary_path),
        normalize_optional_path(artifact_context.get("report_markdown_path")),
        normalize_optional_path(artifact_context.get("check_text_path")),
        normalize_optional_path(source_artifact.get("summary_path")),
        normalize_optional_path(source_artifact.get("opener_summary_path")),
        normalize_optional_path(source_artifact.get("opener_report_markdown_path")),
        normalize_optional_path(source_artifact.get("opener_check_text_path")),
    ]
    for ref_value in get_list(open_event_summary.get("witness_refs")):
        ref = normalize_witness_ref(ref_value)
        refs.extend([ref["summary_path"], ref["report_markdown_path"], ref["check_text_path"]])
    return ordered_unique(refs)


def build_evidence_refs(open_event_summary: dict[str, Any]) -> list[OrderedDict[str, str]]:
    return [normalize_witness_ref(ref_value) for ref_value in get_list(open_event_summary.get("witness_refs"))]


def build_observations(open_event_summary: dict[str, Any]) -> list[str]:
    event = get_mapping(open_event_summary.get("open_event"))
    reason = get_mapping(event.get("reason"))
    decision = get_mapping(open_event_summary.get("consumer_decision"))
    selected = get_mapping(decision.get("selected_consumer"))
    compare = get_mapping(open_event_summary.get("compare_summary"))
    workspace = get_mapping(open_event_summary.get("workspace_facade"))
    observations = [
        f"open_event_status={choose_text(event.get('status'))}",
        f"opening_reason={choose_text(reason.get('kind'))}",
        "selected_consumer={0} action={1}".format(
            choose_text(selected.get("consumer_id")),
            choose_text(selected.get("selected_action_id")),
        ),
        "candidate_consumers={0} rejected={1}".format(
            int(decision.get("candidate_consumer_count", 0)),
            int(decision.get("rejected_consumer_count", 0)),
        ),
        "compare=available:{0} verdict:{1} changed_fields:{2}".format(
            bool(compare.get("available")),
            choose_text(compare.get("action_verdict")),
            int(compare.get("changed_field_count", 0)),
        ),
        "workspace={0} facade={1}".format(
            choose_text(workspace.get("status")),
            choose_text(workspace.get("facade_kind")),
        ),
        f"witness_refs={len(get_list(open_event_summary.get('witness_refs')))}",
    ]
    if choose_text(event.get("status")) == "accepted_with_drift":
        observations.append("opening_drift=attached_compare_context")
    return ordered_unique(observations)


def build_subject(open_event_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    compare = get_mapping(open_event_summary.get("compare_summary"))
    event = get_mapping(open_event_summary.get("open_event"))
    active_facets = ["front_page", "opening_flow", "open_event", "witness"]
    if bool(compare.get("available")):
        active_facets.append("compare")
    if choose_text(event.get("status")) == "accepted_with_drift":
        active_facets.append("drift")
    return OrderedDict(
        [
            ("case", choose_text(event.get("open_event_id")) or None),
            ("profile", None),
            ("board", None),
            ("active_facets", active_facets),
        ]
    )


def build_questions(witness_status: str, open_event_summary: dict[str, Any]) -> OrderedDict[str, list[str]]:
    event = get_mapping(open_event_summary.get("open_event"))
    compare = get_mapping(open_event_summary.get("compare_summary"))
    witness_questions: list[str] = []
    next_questions: list[str] = []
    if witness_status == "fail":
        witness_questions.append("Which blocked opening dependency prevented this open-event witness from standing?")
    elif choose_text(event.get("status")) == "accepted_with_drift":
        witness_questions.append("Should this drift-aware open-event witness be rendered before the selected workspace surface?")
    else:
        witness_questions.append("Should this open-event witness become the canonical explainable-opening baseline?")

    if bool(compare.get("available")):
        next_questions.append("Should action compare context be promoted into a first-class OpenEventWitness comparison?")
    else:
        next_questions.append("Should future open-event witnesses require compare context before publication?")
    next_questions.append("Should witness_bundle/v0 learn to consume open-event witness entries after enough samples accumulate?")
    return OrderedDict(
        [
            ("witness_questions", ordered_unique(witness_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_summary_model(
    open_event_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    open_event_summary = load_open_event_summary(open_event_summary_path)
    event = get_mapping(open_event_summary.get("open_event"))
    reason = get_mapping(event.get("reason"))
    source_artifact = get_mapping(event.get("source_artifact"))
    decision = get_mapping(open_event_summary.get("consumer_decision"))
    selected = get_mapping(decision.get("selected_consumer"))
    compare = get_mapping(open_event_summary.get("compare_summary"))
    workspace = get_mapping(open_event_summary.get("workspace_facade"))
    explanation = get_mapping(open_event_summary.get("explanation_view"))
    source_artifact_context = get_mapping(open_event_summary.get("artifact_context"))
    source_violations = [choose_text(item) for item in get_list(open_event_summary.get("violations")) if choose_text(item)]
    event_status = choose_text(event.get("status"))
    violations = list(source_violations)
    if event_status == "blocked":
        violations.append("open event is blocked")
    violations = ordered_unique(violations)
    witness_status = "fail" if violations else "ok"
    result = "ok" if witness_status == "ok" else "fail"
    evidence_refs = build_evidence_refs(open_event_summary)
    artifact_refs = build_artifact_refs(open_event_summary, open_event_summary_path)
    observations = build_observations(open_event_summary)
    witness_id = f"open-event-witness::{choose_text(event.get('open_event_id'))}"

    return OrderedDict(
        [
            ("schema", OPEN_EVENT_WITNESS_SCHEMA),
            ("kind", OPEN_EVENT_WITNESS_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"),
            ("result", result),
            (
                "opening_flow_open_event_witness",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Open Event Witness"),
                        ("summary", "A compact witness entry projected from an explainable opening judgment."),
                    ]
                ),
            ),
            ("front_page", build_front_page(summary_path, report_path, check_path, open_event_summary, open_event_summary_path)),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_open_event_summary_path", normalize_path(open_event_summary_path)),
                        ("source_open_event_report_markdown_path", normalize_optional_path(source_artifact_context.get("report_markdown_path"))),
                        ("source_open_event_check_text_path", normalize_optional_path(source_artifact_context.get("check_text_path"))),
                        ("output_root", normalize_path(output_root)),
                        ("open_event_witness_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "open_event_identity",
                OrderedDict(
                    [
                        ("open_event_id", choose_text(event.get("open_event_id"))),
                        ("open_event_status", event_status),
                        ("open_event_result", choose_text(open_event_summary.get("result"))),
                        ("reason_kind", choose_text(reason.get("kind"))),
                        ("reason_summary", choose_text(reason.get("summary"))),
                        ("source_summary_schema", choose_text(source_artifact.get("summary_schema"))),
                        ("source_summary_kind", choose_text(source_artifact.get("summary_kind"))),
                        ("source_summary_path", normalize_optional_path(source_artifact.get("summary_path"))),
                    ]
                ),
            ),
            (
                "judgment",
                OrderedDict(
                    [
                        ("witness_id", witness_id),
                        ("witness_status", witness_status),
                        ("accepted", witness_status == "ok"),
                        ("selected_consumer_id", choose_text(selected.get("consumer_id"))),
                        ("selected_action_id", choose_text(selected.get("selected_action_id"))),
                        ("candidate_consumer_count", int(decision.get("candidate_consumer_count", 0))),
                        ("rejected_consumer_count", int(decision.get("rejected_consumer_count", 0))),
                        ("compare_available", bool(compare.get("available"))),
                        ("compare_verdict", choose_text(compare.get("action_verdict"))),
                        ("compare_changed_field_count", int(compare.get("changed_field_count", 0))),
                        ("workspace_facade_status", choose_text(workspace.get("status"))),
                        ("workspace_facade_kind", choose_text(workspace.get("facade_kind"))),
                        ("evidence_ref_count", len(evidence_refs)),
                        ("artifact_ref_count", len(artifact_refs)),
                    ]
                ),
            ),
            (
                "witness_entry",
                OrderedDict(
                    [
                        ("id", witness_id),
                        ("kind", "open_event_witness"),
                        ("label", "explainable-opening-open-event"),
                        ("role", "explainable opening judgment witness"),
                        ("layer", "opening_flow"),
                        ("required", True),
                        ("status", witness_status),
                        ("witness_focus", ["front_page", "opening_flow", "consumer", "plan", "compare", "workspace"]),
                        ("case", choose_text(event.get("open_event_id")) or None),
                        ("source_path", normalize_path(open_event_summary_path)),
                        ("subject", build_subject(open_event_summary)),
                        ("observations", observations),
                        ("artifact_refs", artifact_refs),
                    ]
                ),
            ),
            ("evidence_refs", evidence_refs),
            (
                "explanation",
                OrderedDict(
                    [
                        ("view_kind", choose_text(explanation.get("view_kind"))),
                        ("why_opened", choose_text(explanation.get("why_opened"))),
                        ("chosen_consumer", choose_text(explanation.get("chosen_consumer"))),
                        ("compare_result", choose_text(explanation.get("compare_result"))),
                        ("text_lines", [choose_text(item) for item in get_list(explanation.get("text_lines"))]),
                    ]
                ),
            ),
            ("questions", build_questions(witness_status, open_event_summary)),
            ("violations", violations),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    identity = summary["open_event_identity"]
    judgment = summary["judgment"]
    witness_entry = summary["witness_entry"]
    explanation = summary["explanation"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Flow Open Event Witness",
        "",
        f"- Result: `{summary['result']}`",
        f"- Witness: `{judgment['witness_id']}`",
        f"- Witness status: `{judgment['witness_status']}`",
        f"- Source open event: `{summary['artifact_context']['source_open_event_summary_path']}`",
        f"- Summary JSON: `{summary['artifact_context']['open_event_witness_summary_path']}`",
        "",
        "## Opening Judgment",
        f"- event: `{identity['open_event_id']}` status=`{identity['open_event_status']}` result=`{identity['open_event_result']}`",
        f"- reason: `{identity['reason_kind']}` {identity['reason_summary']}",
        f"- selected consumer: `{judgment['selected_consumer_id']}` action=`{judgment['selected_action_id']}`",
        f"- candidates=`{judgment['candidate_consumer_count']}` rejected=`{judgment['rejected_consumer_count']}`",
        f"- compare=`{judgment['compare_verdict']}` available=`{judgment['compare_available']}` changed_fields=`{judgment['compare_changed_field_count']}`",
        f"- workspace=`{judgment['workspace_facade_status']}` facade=`{judgment['workspace_facade_kind']}`",
        "",
        "## Witness Entry",
        f"- id: `{witness_entry['id']}`",
        f"- kind: `{witness_entry['kind']}`",
        f"- layer: `{witness_entry['layer']}`",
        f"- status: `{witness_entry['status']}`",
        f"- artifact refs: `{len(witness_entry['artifact_refs'])}`",
        "",
        "## Observations",
    ]
    for observation in witness_entry["observations"]:
        lines.append(f"- {observation}")
    lines.extend(["", "## Explanation"])
    for line in explanation["text_lines"]:
        lines.append(f"- {line}")
    lines.extend(["", "## Evidence Refs"])
    for ref in summary["evidence_refs"]:
        lines.append(f"- `{ref['role']}`: `{ref['summary_path']}`")
    if summary["violations"]:
        lines.extend(["", "## Violations"])
        for violation in summary["violations"]:
            lines.append(f"- {violation}")
    lines.extend(["", "## Questions"])
    for question in summary["questions"]["witness_questions"]:
        lines.append(f"- witness: {question}")
    for question in summary["questions"]["next_questions"]:
        lines.append(f"- next: {question}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    identity = summary["open_event_identity"]
    judgment = summary["judgment"]
    return "\n".join(
        [
            f"source_open_event_summary_path: {summary['artifact_context']['source_open_event_summary_path']}",
            f"result: {summary['result']}",
            f"witness_id: {judgment['witness_id']}",
            f"witness_status: {judgment['witness_status']}",
            f"open_event_id: {identity['open_event_id']}",
            f"open_event_status: {identity['open_event_status']}",
            f"reason_kind: {identity['reason_kind']}",
            f"selected_consumer_id: {judgment['selected_consumer_id']}",
            f"selected_action_id: {judgment['selected_action_id']}",
            f"compare_available: {judgment['compare_available']}",
            f"compare_verdict: {judgment['compare_verdict']}",
            f"workspace_facade_status: {judgment['workspace_facade_status']}",
            f"evidence_ref_count: {judgment['evidence_ref_count']}",
            f"artifact_ref_count: {judgment['artifact_ref_count']}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Export an OpenEventWitness from an opening-flow open-event summary.")
    parser.add_argument("--open-event", required=True, help="Input opening-flow open-event summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for open-event witness artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for open-event witness summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for open-event witness markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for open-event witness check text.")
    args = parser.parse_args()

    open_event_path = Path(args.open_event).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-flow-open-event-witness").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.open-event.witness.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.open-event.witness.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.open-event.witness.check.txt")

    try:
        summary = build_summary_model(open_event_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    judgment = summary["judgment"]
    identity = summary["open_event_identity"]
    print(f"[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS] summary={summary_path}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS] witness={0} status={1} event_status={2}".format(
            judgment["witness_id"],
            judgment["witness_status"],
            identity["open_event_status"],
        )
    )
    print(
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS] selected={0} action={1} compare={2}/{3}".format(
            judgment["selected_consumer_id"],
            judgment["selected_action_id"],
            judgment["compare_available"],
            judgment["compare_verdict"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
