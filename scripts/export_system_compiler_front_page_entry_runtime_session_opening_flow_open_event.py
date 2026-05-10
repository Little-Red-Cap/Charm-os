from __future__ import annotations

import argparse
import hashlib
import json
from collections import OrderedDict
from datetime import datetime
from pathlib import Path
from typing import Any

from export_system_compiler_front_page_entry_opening_flow_open_event import (
    build_check as build_generic_check,
    build_report as build_generic_report,
)
from system_compiler_front_page_route_lib import (
    choose_text,
    get_mapping,
    load_json,
    normalize_optional_path,
    normalize_path,
    resolve_output_path,
    write_text,
)


BRIDGE_SCHEMA = "system_compiler.front_page_entry_runtime_session_opening_flow_plan_action/v0"
BRIDGE_KIND = "system_compiler.front_page_entry_runtime_session_opening_flow_plan_action"
CONSUMER_SCHEMA = "minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0"
CONSUMER_KIND = "minimal_kernel.runtime_session_witness_inspect_compare_consumer"
OPEN_EVENT_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event/v0"
OPEN_EVENT_KIND = "system_compiler.front_page_entry_opening_flow_open_event"


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def string_list(value: Any) -> list[str]:
    return [choose_text(item) for item in get_list(value) if choose_text(item)]


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


def load_bridge_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != BRIDGE_SCHEMA:
        raise ValueError(f"unsupported runtime-session opening-flow bridge schema: {path}")
    if choose_text(summary.get("kind")) != BRIDGE_KIND:
        raise ValueError(f"unsupported runtime-session opening-flow bridge kind: {path}")
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


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    bridge_summary: dict[str, Any],
    bridge_summary_path: Path,
) -> OrderedDict[str, Any]:
    bridge_artifact = get_mapping(bridge_summary.get("artifact_context"))
    facade_surface = get_mapping(bridge_summary.get("facade_surface"))
    supporting_surfaces = [
        make_surface(
            "source_runtime_session_bridge",
            "source runtime-session opening bridge",
            "source_runtime_session_bridge",
            BRIDGE_SCHEMA,
            normalize_path(bridge_summary_path),
            normalize_optional_path(bridge_artifact.get("report_markdown_path")),
            normalize_optional_path(bridge_artifact.get("check_text_path")),
        ),
        make_surface(
            "runtime_session_consumer_facade",
            choose_text(facade_surface.get("label")) or "runtime-session consumer facade",
            choose_text(facade_surface.get("role")) or "runtime_session_consumer_facade",
            choose_text(facade_surface.get("summary_schema")) or CONSUMER_SCHEMA,
            normalize_optional_path(facade_surface.get("summary_path")),
            normalize_optional_path(facade_surface.get("report_markdown_path")),
            normalize_optional_path(facade_surface.get("check_text_path")),
        ),
    ]
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", supporting_surfaces),
        ]
    )


def make_open_event_id(open_action: dict[str, Any], selected_artifact_ref: dict[str, Any]) -> str:
    stable_parts = [
        choose_text(open_action.get("action_id")),
        choose_text(open_action.get("entry_name")),
        choose_text(selected_artifact_ref.get("id")),
        choose_text(selected_artifact_ref.get("path")),
    ]
    digest = hashlib.sha256("|".join(stable_parts).encode("utf-8")).hexdigest()[:16]
    return f"open-event-{digest}"


def build_witness_ref(role: str, schema: str, path: str, report_path: str = "", check_path: str = "") -> OrderedDict[str, str]:
    return OrderedDict(
        [
            ("role", role),
            ("summary_schema", schema),
            ("summary_path", normalize_optional_path(path)),
            ("report_markdown_path", normalize_optional_path(report_path)),
            ("check_text_path", normalize_optional_path(check_path)),
        ]
    )


def build_input_refs(bridge_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    judgment_inputs = get_mapping(bridge_summary.get("judgment_inputs"))
    return OrderedDict(
        [
            ("consumer_summary_ref", get_mapping(judgment_inputs.get("consumer_summary_ref"))),
            ("selected_focus_ref", get_mapping(judgment_inputs.get("default_focus_ref"))),
            ("selected_explain_hop_ref", get_mapping(judgment_inputs.get("selected_explain_hop_ref"))),
            ("selected_artifact_ref", get_mapping(judgment_inputs.get("selected_artifact_ref"))),
            (
                "fallback_artifact_refs",
                [get_mapping(item) for item in get_list(judgment_inputs.get("fallback_artifact_refs"))],
            ),
        ]
    )


def build_summary_model(
    bridge_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    bridge_summary = load_bridge_summary(bridge_summary_path)
    bridge_artifact = get_mapping(bridge_summary.get("artifact_context"))
    source_consumer = get_mapping(bridge_summary.get("source_consumer"))
    source_compare = get_mapping(source_consumer.get("source_compare"))
    open_action = get_mapping(bridge_summary.get("open_action"))
    opening_reason = get_mapping(open_action.get("opening_reason"))
    preview = get_mapping(bridge_summary.get("opening_preview"))
    facade_surface = get_mapping(bridge_summary.get("facade_surface"))
    execution_receipt = get_mapping(bridge_summary.get("execution_receipt"))
    opening_input_refs = build_input_refs(bridge_summary)
    selected_artifact_ref = get_mapping(opening_input_refs.get("selected_artifact_ref"))
    selected_focus_ref = get_mapping(opening_input_refs.get("selected_focus_ref"))
    selected_explain_hop_ref = get_mapping(opening_input_refs.get("selected_explain_hop_ref"))
    fallback_artifact_refs = [get_mapping(item) for item in get_list(opening_input_refs.get("fallback_artifact_refs"))]

    event_status = "blocked"
    if choose_text(open_action.get("status")) == "ready":
        event_status = "accepted_with_drift" if bool(source_compare.get("changed")) else "accepted"

    compare_available = bool(choose_text(source_compare.get("summary_path")) or choose_text(open_action.get("status")))
    compare_verdict = "not_attached"
    if compare_available:
        compare_verdict = "drifted" if bool(source_compare.get("changed")) else "standing"
    selected_artifact_path = normalize_optional_path(selected_artifact_ref.get("path"))
    open_event_id = make_open_event_id(open_action, selected_artifact_ref)

    witness_refs = [
        build_witness_ref(
            "source_runtime_session_bridge",
            BRIDGE_SCHEMA,
            normalize_path(bridge_summary_path),
            normalize_optional_path(bridge_artifact.get("report_markdown_path")),
            normalize_optional_path(bridge_artifact.get("check_text_path")),
        ),
        build_witness_ref(
            "runtime_session_consumer_summary",
            choose_text(facade_surface.get("summary_schema")) or CONSUMER_SCHEMA,
            normalize_optional_path(facade_surface.get("summary_path")),
            normalize_optional_path(facade_surface.get("report_markdown_path")),
            normalize_optional_path(facade_surface.get("check_text_path")),
        ),
        build_witness_ref(
            "open_event",
            OPEN_EVENT_SCHEMA,
            normalize_path(summary_path),
            normalize_path(report_path),
            normalize_path(check_path),
        ),
    ]

    compare_line = (
        "consumer compare projection verdict={0} changed={1}".format(compare_verdict, bool(source_compare.get("changed")))
        if compare_available
        else "no runtime-session compare projection attached"
    )
    witness_lines = [f"{ref['role']}: {ref['summary_path']}" for ref in witness_refs if choose_text(ref.get("summary_path"))]
    explanation_text_lines = [
        "Why opened: {0}".format(choose_text(opening_reason.get("summary"))),
        "Chosen consumer: runtime_session_opening:default_overview:artifact_root via {0}".format(
            choose_text(execution_receipt.get("chosen_by"))
        ),
        "Diagnostic preview: {0} summary line(s), {1} question line(s)".format(
            int(preview.get("line_count", 0)),
            int(preview.get("question_count", 0)),
        ),
        "Plan: 1 action(s), selected {0}".format(choose_text(open_action.get("action_id"))),
        "Rejected candidates: 0",
        f"Compare: {compare_line}",
        f"Witness refs: {len(witness_refs)}",
        "Selected artifact target: {0}".format(selected_artifact_path or "none"),
    ]

    typed_next_questions = [
        OrderedDict(
            [
                ("kind", "inspect_action_compare"),
                (
                    "summary",
                    "Inspect the runtime-session selected explain hop before rendering the consumer facade as the primary opening surface.",
                ),
                ("target_ref", "open_event.opening_input_refs.selected_explain_hop_ref"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_rejected_consumers"),
                (
                    "summary",
                    "Inspect the empty rejected-consumer list to confirm fallback explain hops did not get promoted into consumer policy.",
                ),
                ("target_ref", "consumer_decision.rejected_consumers"),
            ]
        ),
    ]
    blockers = string_list(open_action.get("blockers"))
    judgment_basis = [
        "runtime_session_opening_bridge",
        "runtime_session_consumer_facade",
        "open_event",
    ]
    if compare_available:
        judgment_basis.append("runtime_session_consumer_compare_projection")

    return OrderedDict(
        [
            ("schema", OPEN_EVENT_SCHEMA),
            ("kind", OPEN_EVENT_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"),
            ("result", "ok" if event_status != "blocked" else "fail"),
            (
                "opening_flow_open_event",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Flow Open Event"),
                        (
                            "summary",
                            "A runtime-session opening judgment record projected from the dedicated inspect compare consumer bridge.",
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
                    bridge_summary,
                    bridge_summary_path,
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_action_summary_path", normalize_path(bridge_summary_path)),
                        ("source_action_compare_summary_path", normalize_optional_path(source_compare.get("summary_path"))),
                        ("output_root", normalize_path(output_root)),
                        ("open_event_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "open_event",
                OrderedDict(
                    [
                        ("open_event_id", open_event_id),
                        ("status", event_status),
                        (
                            "reason",
                            OrderedDict(
                                [
                                    ("kind", choose_text(opening_reason.get("kind"))),
                                    ("summary", choose_text(opening_reason.get("summary"))),
                                    ("source_summary_path", normalize_optional_path(opening_reason.get("source_summary_path"))),
                                    ("drift_changed", bool(opening_reason.get("drift_changed"))),
                                    ("drift_verdict", choose_text(opening_reason.get("drift_verdict"))),
                                ]
                            ),
                        ),
                        (
                            "source_artifact",
                            OrderedDict(
                                [
                                    ("summary_schema", choose_text(selected_artifact_ref.get("summary_schema")) or "artifact.ref/v0"),
                                    ("summary_kind", choose_text(selected_artifact_ref.get("summary_kind")) or "artifact_ref"),
                                    ("summary_path", selected_artifact_path),
                                    ("opener_summary_path", normalize_optional_path(facade_surface.get("summary_path"))),
                                    ("opener_report_markdown_path", normalize_optional_path(facade_surface.get("report_markdown_path"))),
                                    ("opener_check_text_path", normalize_optional_path(facade_surface.get("check_text_path"))),
                                ]
                            ),
                        ),
                        ("opening_input_refs", opening_input_refs),
                    ]
                ),
            ),
            (
                "consumer_decision",
                OrderedDict(
                    [
                        (
                            "selected_consumer",
                            OrderedDict(
                                [
                                    ("consumer_id", "runtime_session_opening:default_overview:artifact_root"),
                                    ("selected_action_id", choose_text(open_action.get("action_id"))),
                                    ("entry_name", choose_text(open_action.get("entry_name"))),
                                    ("selected_role", choose_text(open_action.get("selected_role"))),
                                    ("query_kind", choose_text(open_action.get("query_kind"))),
                                    ("query_scope", choose_text(open_action.get("query_scope"))),
                                    ("operation", choose_text(open_action.get("expected_consumer_operation"))),
                                    ("projection_kind", choose_text(open_action.get("projection_kind"))),
                                    ("chosen_by", choose_text(execution_receipt.get("chosen_by"))),
                                ]
                            ),
                        ),
                        (
                            "candidate_consumers",
                            [
                                OrderedDict(
                                    [
                                        ("consumer_id", "runtime_session_opening:default_overview:artifact_root"),
                                        ("action_id", choose_text(open_action.get("action_id"))),
                                        ("entry_name", choose_text(open_action.get("entry_name"))),
                                        ("rank", 0),
                                        ("action_kind", choose_text(open_action.get("action_kind"))),
                                        ("display_group", "runtime_session"),
                                        ("projection_kind", choose_text(open_action.get("projection_kind"))),
                                        ("target_summary_schema", choose_text(selected_artifact_ref.get("summary_schema")) or "artifact.ref/v0"),
                                        ("target_summary_kind", choose_text(selected_artifact_ref.get("summary_kind")) or "artifact_ref"),
                                        ("target_summary_path", selected_artifact_path),
                                        ("selected", True),
                                        ("selection_basis", "selected by default_focus/default_explain_hop"),
                                    ]
                                )
                            ],
                        ),
                        ("rejected_consumers", []),
                        ("candidate_consumer_count", 1),
                        ("rejected_consumer_count", 0),
                        ("decision_reason", choose_text(opening_reason.get("summary"))),
                    ]
                ),
            ),
            (
                "plan",
                OrderedDict(
                    [
                        ("plan_id", "runtime-session-opening-bridge"),
                        ("result", choose_text(bridge_summary.get("result"))),
                        ("execution_plan_status", "ready" if choose_text(open_action.get("status")) == "ready" else "blocked"),
                        ("planned_action_count", 1),
                        ("default_action_id", choose_text(open_action.get("action_id"))),
                        ("compare_action_id", ""),
                        ("selected_action_id", choose_text(open_action.get("action_id"))),
                    ]
                ),
            ),
            (
                "action_records",
                [
                    OrderedDict(
                        [
                            ("action_id", choose_text(open_action.get("action_id"))),
                            ("action_kind", choose_text(open_action.get("action_kind"))),
                            ("entry_name", choose_text(open_action.get("entry_name"))),
                            (
                                "expected",
                                OrderedDict(
                                    [
                                        ("operation", choose_text(open_action.get("expected_consumer_operation"))),
                                        ("projection_kind", choose_text(open_action.get("projection_kind"))),
                                        ("target_summary_schema", choose_text(selected_artifact_ref.get("summary_schema")) or "artifact.ref/v0"),
                                        ("target_summary_kind", choose_text(selected_artifact_ref.get("summary_kind")) or "artifact_ref"),
                                        ("target_summary_path", selected_artifact_path),
                                    ]
                                ),
                            ),
                            (
                                "result",
                                OrderedDict(
                                    [
                                        ("status", choose_text(open_action.get("status"))),
                                        ("opener_surface_available", bool(choose_text(facade_surface.get("summary_path")))),
                                        ("opener_summary_path", normalize_optional_path(facade_surface.get("summary_path"))),
                                        ("blockers", blockers),
                                    ]
                                ),
                            ),
                            (
                                "compare",
                                OrderedDict(
                                    [
                                        ("available", compare_available),
                                        ("summary_path", normalize_path(bridge_summary_path) if compare_available else ""),
                                        ("action_verdict", compare_verdict),
                                        ("changed_field_count", 1 if bool(source_compare.get("changed")) else 0),
                                        ("reason_changed", bool(source_compare.get("changed"))),
                                        (
                                            "narratives",
                                            [
                                                "runtime-session inspect compare consumer bridge supplied the opening judgment"
                                            ]
                                            if compare_available
                                            else [],
                                        ),
                                    ]
                                ),
                            ),
                        ]
                    )
                ],
            ),
            (
                "compare_summary",
                OrderedDict(
                    [
                        ("available", compare_available),
                        ("summary_path", normalize_path(bridge_summary_path) if compare_available else ""),
                        ("action_verdict", compare_verdict),
                        ("changed_field_count", 1 if bool(source_compare.get("changed")) else 0),
                        ("reason_changed", bool(source_compare.get("changed"))),
                        (
                            "narratives",
                            [
                                "consumer-level compare judgment was projected from runtime-session inspect compare consumer"
                            ]
                            if compare_available
                            else [],
                        ),
                    ]
                ),
            ),
            (
                "workspace_facade",
                OrderedDict(
                    [
                        ("status", "projected" if event_status != "blocked" else "blocked"),
                        ("facade_kind", "runtime_session_consumer_facade"),
                        ("primary_surface_role", choose_text(facade_surface.get("role")) or "runtime_session_consumer_facade"),
                        ("primary_summary_path", normalize_optional_path(facade_surface.get("summary_path"))),
                        ("primary_report_markdown_path", normalize_optional_path(facade_surface.get("report_markdown_path"))),
                        ("primary_check_text_path", normalize_optional_path(facade_surface.get("check_text_path"))),
                    ]
                ),
            ),
            (
                "diagnostic_preview",
                OrderedDict(
                    [
                        ("available", bool(preview.get("available"))),
                        ("entry_name", choose_text(preview.get("entry_name"))),
                        ("projection_kind", choose_text(preview.get("projection_kind"))),
                        ("headline", choose_text(preview.get("headline"))),
                        ("summary_lines", string_list(preview.get("summary_lines"))),
                        ("question_lines", string_list(preview.get("question_lines"))),
                        ("line_count", int(preview.get("line_count", len(string_list(preview.get("summary_lines")))))),
                        ("question_count", int(preview.get("question_count", len(string_list(preview.get("question_lines")))))),
                        ("blockers", blockers),
                    ]
                ),
            ),
            ("witness_refs", witness_refs),
            (
                "judgment",
                OrderedDict(
                    [
                        ("semantic_role", "opening_judgment_carrier"),
                        ("status", event_status),
                        ("grade", "compared" if compare_available else "described"),
                        ("basis", judgment_basis),
                        ("accepted", event_status != "blocked"),
                        (
                            "summary",
                            "This opening judgment is projected from the runtime-session inspect compare consumer bridge and keeps the facade-first, artifact-targeted opening route explicit."
                            if event_status != "blocked"
                            else "This opening judgment is blocked because the runtime-session bridge could not produce a ready explain hop target.",
                        ),
                    ]
                ),
            ),
            (
                "explanation_view",
                OrderedDict(
                    [
                        ("view_kind", "explain_open_event_view"),
                        ("status", event_status),
                        ("why_opened", choose_text(opening_reason.get("summary"))),
                        ("chosen_consumer", "runtime_session_opening:default_overview:artifact_root selected by default_focus/default_explain_hop"),
                        ("diagnostic_headline", choose_text(preview.get("headline"))),
                        ("diagnostic_summary_lines", string_list(preview.get("summary_lines"))),
                        ("diagnostic_question_lines", string_list(preview.get("question_lines"))),
                        ("plan_actions", [f"1. {choose_text(open_action.get('action_id'))} -> {choose_text(open_action.get('status'))}"]),
                        ("compare_result", compare_line),
                        ("witness_refs", witness_lines),
                        ("text_lines", explanation_text_lines),
                    ]
                ),
            ),
            (
                "questions",
                OrderedDict(
                    [
                        (
                            "open_event_questions",
                            [
                                "Should the runtime-session consumer facade open before the selected artifact target so the reading judgment stays explainable?"
                            ],
                        ),
                        (
                            "next_questions",
                            [
                                "Should the selected artifact target remain secondary until a richer opening policy layer is introduced?",
                                "Should fallback explain hops stay contextual instead of becoming rejected consumers?",
                            ],
                        ),
                        ("typed_next_questions", typed_next_questions),
                    ]
                ),
            ),
            ("violations", blockers if event_status == "blocked" else []),
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a standard open-event from the runtime-session opening-flow bridge."
    )
    parser.add_argument("--bridge", required=True, help="Input runtime-session opening-flow bridge summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for open-event artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for open-event summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for open-event markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for open-event check text.")
    args = parser.parse_args()

    bridge_summary_path = Path(args.bridge).resolve()
    output_root = Path(
        args.output_root or "out/system-compiler-front-page-entry-runtime-session-opening-flow-open-event"
    ).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-flow.open-event.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-flow.open-event.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-flow.open-event.check.txt")

    try:
        summary = build_summary_model(bridge_summary_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_generic_report(summary))
        write_text(check_path, build_generic_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    event = summary["open_event"]
    selected = summary["consumer_decision"]["selected_consumer"]
    print(f"[RUNTIME-SESSION-OPENING-FLOW-OPEN-EVENT] summary={summary_path}")
    print(f"[RUNTIME-SESSION-OPENING-FLOW-OPEN-EVENT] event={event['open_event_id']} status={event['status']}")
    print(
        "[RUNTIME-SESSION-OPENING-FLOW-OPEN-EVENT] selected={0} action={1} target={2}".format(
            selected["consumer_id"],
            selected["selected_action_id"],
            event["source_artifact"]["summary_path"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
