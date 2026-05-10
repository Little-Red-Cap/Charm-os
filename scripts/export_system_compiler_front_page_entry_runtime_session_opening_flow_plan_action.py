from __future__ import annotations

import argparse
import hashlib
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


CONSUMER_SCHEMA = "minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0"
CONSUMER_KIND = "minimal_kernel.runtime_session_witness_inspect_compare_consumer"
BRIDGE_SCHEMA = "system_compiler.front_page_entry_runtime_session_opening_flow_plan_action/v0"
BRIDGE_KIND = "system_compiler.front_page_entry_runtime_session_opening_flow_plan_action"


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


def load_consumer_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != CONSUMER_SCHEMA:
        raise ValueError(f"unsupported runtime-session inspect compare consumer schema: {path}")
    if choose_text(summary.get("kind")) != CONSUMER_KIND:
        raise ValueError(f"unsupported runtime-session inspect compare consumer kind: {path}")
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


def make_input_ref(
    ref_id: str,
    label: str,
    ref_kind: str,
    path_text: str,
    *,
    summary_schema: str = "",
    summary_kind: str = "",
    focus_kind: str = "",
    severity: str = "",
    reason_kind: str = "",
) -> OrderedDict[str, str]:
    ref = OrderedDict(
        [
            ("id", ref_id),
            ("label", label),
            ("ref_kind", ref_kind),
            ("path", normalize_optional_path(path_text)),
        ]
    )
    if summary_schema:
        ref["summary_schema"] = summary_schema
    if summary_kind:
        ref["summary_kind"] = summary_kind
    if focus_kind:
        ref["focus_kind"] = focus_kind
    if severity:
        ref["severity"] = severity
    if reason_kind:
        ref["reason_kind"] = reason_kind
    return ref


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    consumer_summary: dict[str, Any],
    consumer_summary_path: Path,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(consumer_summary.get("artifact_context"))
    supporting_surfaces = [
        make_surface(
            "source_consumer_summary",
            "source runtime-session inspect compare consumer",
            "source_consumer_summary",
            CONSUMER_SCHEMA,
            normalize_path(consumer_summary_path),
            normalize_optional_path(artifact_context.get("report_markdown_path")),
            normalize_optional_path(artifact_context.get("check_text_path")),
        )
    ]
    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", supporting_surfaces),
        ]
    )


def build_bridge_id(consumer_summary_path: Path, default_focus_id: str, explain_hop_id: str) -> str:
    stable = "|".join(
        [
            normalize_path(consumer_summary_path),
            default_focus_id,
            explain_hop_id,
        ]
    )
    digest = hashlib.sha256(stable.encode("utf-8")).hexdigest()[:16]
    return f"runtime-session-bridge-{digest}"


def build_opening_reason(consumer_summary_path: Path, changed: bool) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("kind", "route"),
            ("summary", "由 inspect compare consumer 的 default focus/default explain hop 决定本次 opening"),
            ("source_summary_path", normalize_path(consumer_summary_path)),
            ("drift_changed", changed),
            ("drift_verdict", "drifted" if changed else "standing"),
        ]
    )


def build_summary_model(
    consumer_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    consumer_summary = load_consumer_summary(consumer_summary_path)
    artifact_context = get_mapping(consumer_summary.get("artifact_context"))
    source_compare = get_mapping(consumer_summary.get("source_compare"))
    default_focus = get_mapping(consumer_summary.get("default_focus"))
    default_hop = get_mapping(consumer_summary.get("default_explain_hop"))
    selected_artifact = get_mapping(default_hop.get("artifact_ref"))
    fallback_artifacts = [get_mapping(item) for item in get_list(default_hop.get("fallback_artifact_refs"))]

    consumer_summary_path_text = normalize_path(consumer_summary_path)
    consumer_report_path = normalize_optional_path(artifact_context.get("report_markdown_path"))
    consumer_check_path = normalize_optional_path(artifact_context.get("check_text_path"))
    source_compare_summary_path = normalize_optional_path(artifact_context.get("source_compare_summary_path"))
    changed = bool(source_compare.get("changed"))

    consumer_ref = make_input_ref(
        "consumer-summary",
        "runtime-session inspect compare consumer summary",
        "consumer_summary",
        consumer_summary_path_text,
        summary_schema=CONSUMER_SCHEMA,
        summary_kind=CONSUMER_KIND,
    )
    focus_ref = make_input_ref(
        choose_text(default_focus.get("focus_id")) or "default-focus",
        choose_text(default_focus.get("headline")) or "default focus",
        "default_focus",
        consumer_summary_path_text,
        focus_kind=choose_text(default_focus.get("focus_kind")),
        severity=choose_text(default_focus.get("severity")),
    )
    explain_hop_ref = make_input_ref(
        choose_text(default_hop.get("hop_id")) or "default-explain-hop",
        choose_text(default_hop.get("headline")) or "default explain hop",
        "default_explain_hop",
        consumer_summary_path_text,
        reason_kind=choose_text(default_hop.get("reason_kind")),
    )
    selected_artifact_ref = make_input_ref(
        choose_text(selected_artifact.get("id")) or "selected-artifact",
        choose_text(selected_artifact.get("label")) or "selected artifact",
        "artifact_ref",
        choose_text(selected_artifact.get("path")),
    )
    fallback_artifact_refs = [
        make_input_ref(
            choose_text(item.get("id")) or f"fallback-artifact-{index}",
            choose_text(item.get("label")) or f"fallback artifact {index}",
            "fallback_artifact_ref",
            choose_text(item.get("path")),
        )
        for index, item in enumerate(fallback_artifacts)
    ]

    blockers: list[str] = []
    if choose_text(consumer_summary.get("result")) != "ok":
        blockers.append("source consumer result is not ok")
    if not choose_text(default_focus.get("focus_id")):
        blockers.append("default focus is missing")
    if not choose_text(default_hop.get("hop_id")):
        blockers.append("default explain hop is missing")
    if not choose_text(selected_artifact.get("path")):
        blockers.append("default explain hop artifact ref path is missing")
    if not consumer_summary_path_text:
        blockers.append("consumer summary ref is missing")
    blockers = ordered_unique(blockers)
    bridge_status = "blocked" if blockers else "ready"

    preview_summary_lines = string_list(default_focus.get("summary_lines"))
    preview_question_lines = string_list(default_focus.get("question_lines"))
    preview_headline = choose_text(default_focus.get("headline"))
    bridge_id = build_bridge_id(
        consumer_summary_path,
        choose_text(default_focus.get("focus_id")),
        choose_text(default_hop.get("hop_id")),
    )
    opening_reason = build_opening_reason(consumer_summary_path, changed)
    facade_surface = make_surface(
        "runtime_session_consumer_facade",
        "runtime-session inspect compare consumer facade",
        "runtime_session_consumer_facade",
        CONSUMER_SCHEMA,
        consumer_summary_path_text,
        consumer_report_path,
        consumer_check_path,
    )

    return OrderedDict(
        [
            ("schema", BRIDGE_SCHEMA),
            ("kind", BRIDGE_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"),
            ("result", "ok" if choose_text(consumer_summary.get("result")) == "ok" else "fail"),
            (
                "runtime_session_opening_flow_plan_action",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Runtime Session Opening Flow Plan Action"),
                        (
                            "summary",
                            "A narrow runtime-session bridge that projects inspect compare consumer judgment into one opening-flow action.",
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
                    consumer_summary,
                    consumer_summary_path,
                ),
            ),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_consumer_summary_path", consumer_summary_path_text),
                        ("output_root", normalize_path(output_root)),
                        ("bridge_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "source_consumer",
                OrderedDict(
                    [
                        ("result", choose_text(consumer_summary.get("result"))),
                        (
                            "source_compare",
                            OrderedDict(
                                [
                                    ("changed", changed),
                                    ("summary_path", source_compare_summary_path),
                                ]
                            ),
                        ),
                        ("default_focus_id", choose_text(default_focus.get("focus_id"))),
                        ("default_focus_kind", choose_text(default_focus.get("focus_kind"))),
                        ("default_focus_severity", choose_text(default_focus.get("severity"))),
                        ("default_explain_hop_id", choose_text(default_hop.get("hop_id"))),
                        ("default_explain_hop_reason_kind", choose_text(default_hop.get("reason_kind"))),
                    ]
                ),
            ),
            (
                "judgment_inputs",
                OrderedDict(
                    [
                        ("consumer_summary_ref", consumer_ref),
                        ("default_focus_ref", focus_ref),
                        ("selected_explain_hop_ref", explain_hop_ref),
                        ("selected_artifact_ref", selected_artifact_ref),
                        ("fallback_artifact_refs", fallback_artifact_refs),
                    ]
                ),
            ),
            ("facade_surface", facade_surface),
            (
                "artifact_target",
                OrderedDict(
                    [
                        ("selected_artifact_ref", selected_artifact_ref),
                        ("fallback_artifact_refs", fallback_artifact_refs),
                    ]
                ),
            ),
            (
                "open_action",
                OrderedDict(
                    [
                        ("status", bridge_status),
                        ("action_id", "open-default"),
                        ("action_kind", "default"),
                        ("entry_name", "runtime-session-inspect-consumer"),
                        ("selected_tab_id", "runtime_session"),
                        ("selected_role", "runtime_session_opening"),
                        ("query_kind", "default_overview"),
                        ("query_scope", "artifact_root"),
                        ("projection_kind", "kernel_runtime_session_opening_judgment"),
                        ("expected_consumer_operation", "open-consumer-summary"),
                        ("opening_reason", opening_reason),
                        ("blockers", blockers),
                    ]
                ),
            ),
            (
                "opening_preview",
                OrderedDict(
                    [
                        ("available", bool(preview_headline or preview_summary_lines or preview_question_lines)),
                        ("entry_name", "runtime-session-inspect-consumer"),
                        ("projection_kind", "kernel_runtime_session_opening_judgment"),
                        ("headline", preview_headline),
                        ("summary_lines", preview_summary_lines),
                        ("question_lines", preview_question_lines),
                        ("line_count", len(preview_summary_lines)),
                        ("question_count", len(preview_question_lines)),
                        ("blockers", blockers),
                    ]
                ),
            ),
            (
                "execution_receipt",
                OrderedDict(
                    [
                        ("chosen_by", "default_focus/default_explain_hop"),
                        ("planned_action_count", 1),
                        ("consumer_operation", "open-consumer-summary"),
                    ]
                ),
            ),
            (
                "questions",
                OrderedDict(
                    [
                        (
                            "bridge_questions",
                            [
                                "Should the runtime-session opening judgment open the inspect compare consumer facade before deeper artifact targets?"
                            ],
                        ),
                        (
                            "next_questions",
                            [
                                "Should the selected artifact target become the next explain surface only after the consumer facade is visible?",
                                "Should fallback artifact refs stay contextual until a richer opening policy is introduced?",
                            ],
                        ),
                    ]
                ),
            ),
            ("violations", blockers),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    source_consumer = summary["source_consumer"]
    judgment_inputs = summary["judgment_inputs"]
    open_action = summary["open_action"]
    preview = summary["opening_preview"]
    execution_receipt = summary["execution_receipt"]
    facade_surface = summary["facade_surface"]
    artifact_target = summary["artifact_target"]
    lines: list[str] = [
        "# Runtime Session Opening Flow Plan Action",
        "",
        f"- Result: `{summary['result']}`",
        f"- Summary JSON: `{summary['artifact_context']['bridge_summary_path']}`",
        f"- Status: `{open_action['status']}`",
        f"- Action: `{open_action['action_id']}` entry=`{open_action['entry_name']}`",
        "",
        "## Consumer Judgment",
        f"- source result: `{source_consumer['result']}`",
        f"- source compare changed: `{source_consumer['source_compare']['changed']}`",
        f"- default focus: `{source_consumer['default_focus_id']}` kind=`{source_consumer['default_focus_kind']}` severity=`{source_consumer['default_focus_severity']}`",
        f"- default explain hop: `{source_consumer['default_explain_hop_id']}` reason=`{source_consumer['default_explain_hop_reason_kind']}`",
        "",
        "## Opening Inputs",
        f"- consumer summary: `{judgment_inputs['consumer_summary_ref']['path']}`",
        f"- selected focus: `{judgment_inputs['default_focus_ref']['id']}`",
        f"- selected explain hop: `{judgment_inputs['selected_explain_hop_ref']['id']}`",
        f"- selected artifact: `{judgment_inputs['selected_artifact_ref']['id']}` -> `{judgment_inputs['selected_artifact_ref']['path']}`",
        f"- fallback artifacts: `{len(judgment_inputs['fallback_artifact_refs'])}`",
        "",
        "## Facade And Target",
        f"- facade summary: `{facade_surface['summary_path']}`",
        f"- facade report: `{facade_surface['report_markdown_path']}`",
        f"- selected artifact target: `{artifact_target['selected_artifact_ref']['path']}`",
        "",
        "## Preview",
        f"- headline: {preview['headline'] or 'none'}",
        f"- summary lines: `{preview['line_count']}`",
        f"- question lines: `{preview['question_count']}`",
        "",
        "## Execution Receipt",
        f"- chosen by: `{execution_receipt['chosen_by']}`",
        f"- planned actions: `{execution_receipt['planned_action_count']}`",
        f"- consumer operation: `{execution_receipt['consumer_operation']}`",
    ]
    if summary["violations"]:
        lines.extend(["", "## Violations"])
        for violation in summary["violations"]:
            lines.append(f"- {violation}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    source_consumer = summary["source_consumer"]
    open_action = summary["open_action"]
    preview = summary["opening_preview"]
    artifact_target = summary["artifact_target"]
    return "\n".join(
        [
            f"source_consumer_summary_path: {summary['artifact_context']['source_consumer_summary_path']}",
            f"result: {summary['result']}",
            f"status: {open_action['status']}",
            f"action_id: {open_action['action_id']}",
            f"entry_name: {open_action['entry_name']}",
            f"expected_consumer_operation: {open_action['expected_consumer_operation']}",
            f"source_compare_changed: {source_consumer['source_compare']['changed']}",
            f"default_focus_id: {source_consumer['default_focus_id']}",
            f"default_explain_hop_id: {source_consumer['default_explain_hop_id']}",
            f"selected_artifact_id: {artifact_target['selected_artifact_ref']['id']}",
            f"selected_artifact_path: {artifact_target['selected_artifact_ref']['path']}",
            f"opening_preview_line_count: {preview['line_count']}",
            f"opening_preview_question_count: {preview['question_count']}",
            f"fallback_artifact_count: {len(artifact_target['fallback_artifact_refs'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Project a runtime-session inspect compare consumer into a dedicated opening-flow plan-action bridge."
    )
    parser.add_argument("--consumer", required=True, help="Input runtime-session inspect compare consumer summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for bridge artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for bridge summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for bridge markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for bridge check text.")
    args = parser.parse_args()

    consumer_summary_path = Path(args.consumer).resolve()
    output_root = Path(
        args.output_root or "out/system-compiler-front-page-entry-runtime-session-opening-flow-plan-action"
    ).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(
        args.summary,
        output_root,
        "front-page.entry-runtime-session-opening-flow.plan-action.summary.json",
    )
    report_path = resolve_output_path(
        args.report_markdown,
        output_root,
        "front-page.entry-runtime-session-opening-flow.plan-action.report.md",
    )
    check_path = resolve_output_path(
        args.check_text,
        output_root,
        "front-page.entry-runtime-session-opening-flow.plan-action.check.txt",
    )

    try:
        summary = build_summary_model(consumer_summary_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    source_consumer = summary["source_consumer"]
    open_action = summary["open_action"]
    artifact_target = summary["artifact_target"]
    print(f"[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION] summary={summary_path}")
    print(
        "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION] status={0} changed={1} focus={2} hop={3}".format(
            open_action["status"],
            source_consumer["source_compare"]["changed"],
            source_consumer["default_focus_id"],
            source_consumer["default_explain_hop_id"],
        )
    )
    print(
        "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION] target={0} fallback={1}".format(
            artifact_target["selected_artifact_ref"]["id"],
            len(artifact_target["fallback_artifact_refs"]),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
