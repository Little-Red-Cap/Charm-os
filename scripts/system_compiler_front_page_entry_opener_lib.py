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


LANDING_SCHEMA = "system_compiler.front_page_entry_landing/v0"
LANDING_KIND = "system_compiler.front_page_entry_landing"
LANDING_COMPARE_SCHEMA = "system_compiler.front_page_entry_landing_compare/v0"
LANDING_COMPARE_KIND = "system_compiler.front_page_entry_landing_compare"
ARTIFACT_REPORT_SCHEMA = "system_compiler.artifact_report/v0"
BIOGRAPHY_SCHEMA = "system_compiler.biography/v0"
WORLD_COMPARE_SCHEMA = "system_compiler.world_compare/v0"
WORLD_SHELF_REVIEW_SCHEMA = "system_compiler.world_shelf_review/v0"
BIOGRAPHY_INDEX_SCHEMA = "system_compiler.biography_index/v0"
BIOGRAPHY_INDEX_COMPARE_SCHEMA = "system_compiler.biography_index_compare/v0"
WITNESS_BUNDLE_SCHEMA = "system_compiler.witness_bundle/v0"
RUNTIME_EVIDENCE_SCHEMA = "minimal_kernel.runtime_evidence_bundle.summary/v1"

INSPECTOR_SCRIPT = "scripts/inspect_system_compiler_artifact_report.ps1"

QUERY_SWITCHES = {
    "bringup_evidence": "-BringupEvidence",
    "resource_summary": "-ResourceSummary",
    "cap_list": "-CapList",
}


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


def normalize_optional_path(value: Any) -> str:
    text = choose_text(value)
    if not text:
        return ""
    return normalize_path(text)


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


def path_exists(path_value: Any) -> bool:
    text = choose_text(path_value)
    if not text:
        return False
    return Path(text).exists()


def existing_paths(values: list[Any]) -> list[str]:
    paths: list[str] = []
    for value in values:
        text = normalize_optional_path(value)
        if not text or not Path(text).exists():
            continue
        paths.append(text)
    return ordered_unique(paths)


def build_front_page_supporting_paths(summary: dict[str, Any]) -> list[str]:
    front_page = get_mapping(summary.get("front_page"))
    surfaces = get_list(front_page.get("supporting_surfaces"))
    return existing_paths(
        [get_mapping(surface).get("summary_path") for surface in surfaces if isinstance(surface, dict)]
    )


def join_texts(values: list[Any]) -> str:
    items = [choose_text(value) for value in values if choose_text(value)]
    return ", ".join(items) if items else "-"


def load_landing_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != LANDING_SCHEMA:
        raise ValueError(f"unsupported front page entry landing schema: {path}")
    if choose_text(summary.get("kind")) != LANDING_KIND:
        raise ValueError(f"unsupported front page entry landing kind: {path}")
    return summary


def load_landing_compare_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != LANDING_COMPARE_SCHEMA:
        raise ValueError(f"unsupported front page entry landing compare schema: {path}")
    if choose_text(summary.get("kind")) != LANDING_COMPARE_KIND:
        raise ValueError(f"unsupported front page entry landing compare kind: {path}")
    return summary


def build_front_page_surface(
    summary: dict[str, Any],
    summary_path: Path,
    surface_id: str,
    role: str,
    schema: str,
) -> OrderedDict[str, str]:
    root_surface = get_mapping(summary.get("root_surface"))
    front_page = get_mapping(summary.get("front_page"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    root_label = choose_text(root_surface.get("label")) or choose_text(summary.get("kind")) or "front page entry"
    report_markdown_path = (
        normalize_optional_path(front_page.get("report_markdown_path"))
        or normalize_optional_path(artifact_context.get("report_markdown_path"))
    )
    check_text_path = (
        normalize_optional_path(front_page.get("check_text_path"))
        or normalize_optional_path(artifact_context.get("check_text_path"))
    )

    return OrderedDict(
        [
            ("id", surface_id),
            ("label", f"{role.replace('_', ' ')}: {root_label}"),
            ("role", role),
            ("summary_schema", schema),
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", report_markdown_path),
            ("check_text_path", check_text_path),
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


def clone_primary_entry(entry: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("tab_id", choose_text(entry.get("tab_id"))),
            ("title", choose_text(entry.get("title"))),
            ("route_id", choose_text(entry.get("route_id"))),
            ("surface_id", choose_text(entry.get("surface_id"))),
            ("role", choose_text(entry.get("role"))),
            ("summary_schema", choose_text(entry.get("summary_schema"))),
            ("summary_kind", choose_text(entry.get("summary_kind"))),
            ("summary_path", normalize_optional_path(entry.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(entry.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(entry.get("check_text_path"))),
        ]
    )


def clone_query_hint(query: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("tab_id", choose_text(query.get("tab_id"))),
            ("tab_title", choose_text(query.get("tab_title"))),
            ("entry_role", choose_text(query.get("entry_role"))),
            ("summary_schema", choose_text(query.get("summary_schema"))),
            ("summary_kind", choose_text(query.get("summary_kind"))),
            ("scope", choose_text(query.get("scope"))),
            ("selection_rule", choose_text(query.get("selection_rule"))),
            ("query_kind", choose_text(query.get("query_kind"))),
            ("compare_expected", bool(query.get("compare_expected"))),
            (
                "followup_query_kinds",
                ordered_unique([choose_text(item) for item in get_list(query.get("followup_query_kinds"))]),
            ),
            ("rationale", choose_text(query.get("rationale"))),
        ]
    )


def build_source_landing(
    landing_summary_path: Path,
    landing_summary: dict[str, Any],
) -> OrderedDict[str, Any]:
    root_surface = get_mapping(landing_summary.get("root_surface"))
    landing_status = get_mapping(landing_summary.get("landing_status"))
    primary_landing = get_mapping(landing_summary.get("primary_landing"))
    primary_entry = clone_primary_entry(get_mapping(primary_landing.get("entry")))
    primary_entry["tab_id"] = choose_text(primary_landing.get("tab_id"))
    primary_entry["title"] = choose_text(primary_landing.get("title"))
    query_hints = get_mapping(landing_summary.get("query_hints"))
    primary_query = clone_query_hint(get_mapping(query_hints.get("primary_query")))

    return OrderedDict(
        [
            ("source_summary_path", normalize_path(landing_summary_path)),
            ("root_label", choose_text(root_surface.get("label"))),
            ("root_summary_schema", choose_text(root_surface.get("summary_schema"))),
            ("root_summary_kind", choose_text(root_surface.get("summary_kind"))),
            ("root_summary_path", normalize_optional_path(root_surface.get("summary_path"))),
            ("landing_result", choose_text(landing_status.get("landing_result"))),
            ("recommended_entry_mode", choose_text(landing_status.get("recommended_entry_mode"))),
            ("entry_tier", choose_text(landing_status.get("entry_tier"))),
            ("primary_tab_id", choose_text(landing_status.get("primary_tab_id"))),
            (
                "available_tab_ids",
                ordered_unique([choose_text(item) for item in get_list(landing_status.get("available_tab_ids"))]),
            ),
            ("primary_entry", primary_entry),
            ("primary_query", primary_query),
        ]
    )


def get_compare_relation(
    landing_summary_path: Path,
    compare_summary: dict[str, Any],
) -> str:
    artifact_context = get_mapping(compare_summary.get("artifact_context"))
    source_path = normalize_path(landing_summary_path)
    baseline_path = normalize_optional_path(artifact_context.get("baseline_landing_summary_path"))
    candidate_path = normalize_optional_path(artifact_context.get("candidate_landing_summary_path"))

    if source_path == baseline_path:
        return "baseline_landing"
    if source_path == candidate_path:
        return "candidate_landing"
    raise ValueError(
        "landing compare does not reference the source landing as baseline or candidate: "
        f"{landing_summary_path}"
    )


def build_source_landing_compare(
    landing_summary_path: Path,
    compare_summary_path: Path,
    compare_summary: dict[str, Any],
) -> OrderedDict[str, Any]:
    relation = get_compare_relation(landing_summary_path, compare_summary)
    artifact_context = get_mapping(compare_summary.get("artifact_context"))
    landing_status = get_mapping(compare_summary.get("landing_status"))
    query_plan_changes = get_mapping(compare_summary.get("query_plan_changes"))
    landing_regression_surface = get_mapping(compare_summary.get("landing_regression_surface"))
    query_regression_surface = get_mapping(compare_summary.get("query_regression_surface"))
    narratives = ordered_unique(
        [choose_text(item) for item in get_list(landing_regression_surface.get("narratives"))]
        + [choose_text(item) for item in get_list(query_regression_surface.get("narratives"))]
    )

    return OrderedDict(
        [
            ("source_summary_path", normalize_path(compare_summary_path)),
            ("related_landing_role", relation),
            ("landing_verdict", choose_text(compare_summary.get("landing_verdict"))),
            (
                "baseline_landing_summary_path",
                normalize_optional_path(artifact_context.get("baseline_landing_summary_path")),
            ),
            (
                "candidate_landing_summary_path",
                normalize_optional_path(artifact_context.get("candidate_landing_summary_path")),
            ),
            ("baseline_primary_tab_id", choose_text(landing_status.get("baseline_primary_tab_id"))),
            ("candidate_primary_tab_id", choose_text(landing_status.get("candidate_primary_tab_id"))),
            ("primary_query_changed", bool(query_plan_changes.get("primary_query_changed"))),
            ("landing_regression_changed", bool(landing_regression_surface.get("changed"))),
            ("query_regression_changed", bool(query_regression_surface.get("changed"))),
            (
                "landing_regression_affected_tab_ids",
                ordered_unique(
                    [choose_text(item) for item in get_list(landing_regression_surface.get("affected_tab_ids"))]
                ),
            ),
            (
                "query_regression_affected_tab_ids",
                ordered_unique(
                    [choose_text(item) for item in get_list(query_regression_surface.get("affected_tab_ids"))]
                ),
            ),
            ("narratives", narratives),
        ]
    )


def build_compare_context(source_landing_compare: dict[str, Any] | None) -> OrderedDict[str, Any]:
    if not source_landing_compare:
        return OrderedDict(
            [
                ("available", False),
                ("related_landing_role", ""),
                ("landing_verdict", ""),
                ("primary_query_changed", False),
                ("landing_regression_changed", False),
                ("query_regression_changed", False),
                ("narratives", []),
            ]
        )

    return OrderedDict(
        [
            ("available", True),
            ("related_landing_role", choose_text(source_landing_compare.get("related_landing_role"))),
            ("landing_verdict", choose_text(source_landing_compare.get("landing_verdict"))),
            ("primary_query_changed", bool(source_landing_compare.get("primary_query_changed"))),
            ("landing_regression_changed", bool(source_landing_compare.get("landing_regression_changed"))),
            ("query_regression_changed", bool(source_landing_compare.get("query_regression_changed"))),
            (
                "narratives",
                ordered_unique([choose_text(item) for item in get_list(source_landing_compare.get("narratives"))]),
            ),
        ]
    )


def build_open_action(
    source_landing: dict[str, Any],
    compare_context: dict[str, Any],
) -> OrderedDict[str, Any]:
    primary_entry = get_mapping(source_landing.get("primary_entry"))
    primary_query = get_mapping(source_landing.get("primary_query"))
    blockers: list[str] = []
    selected_tab_id = choose_text(primary_entry.get("tab_id"))
    query_kind = choose_text(primary_query.get("query_kind"))
    query_scope = choose_text(primary_query.get("scope"))
    selection_rule = choose_text(primary_query.get("selection_rule"))
    target_summary_path = choose_text(primary_entry.get("summary_path"))

    if not selected_tab_id:
        blockers.append("source landing does not carry a primary tab")
    if not query_kind:
        blockers.append("source landing does not carry a primary explain query")
    if query_scope not in {"report", "artifact_root"}:
        blockers.append("source landing primary query has an unsupported scope")
    if not target_summary_path:
        blockers.append("source landing primary entry does not carry a target summary path")
    elif not Path(target_summary_path).exists():
        blockers.append("source landing primary entry target summary path does not exist")

    return OrderedDict(
        [
            ("action_id", "open-primary-explain-entry"),
            ("action_kind", "open_explain_entry"),
            ("status", "blocked" if blockers else "ready"),
            ("selected_tab_id", selected_tab_id),
            ("selected_tab_title", choose_text(primary_entry.get("title")) or choose_text(primary_query.get("tab_title"))),
            ("selected_route_id", choose_text(primary_entry.get("route_id"))),
            ("selected_surface_id", choose_text(primary_entry.get("surface_id"))),
            ("selected_role", choose_text(primary_entry.get("role"))),
            ("query_kind", query_kind),
            ("query_scope", query_scope),
            ("selection_rule", selection_rule),
            ("compare_expected", bool(primary_query.get("compare_expected"))),
            (
                "followup_query_kinds",
                ordered_unique([choose_text(item) for item in get_list(primary_query.get("followup_query_kinds"))]),
            ),
            ("target_summary_schema", choose_text(primary_entry.get("summary_schema"))),
            ("target_summary_kind", choose_text(primary_entry.get("summary_kind"))),
            ("target_summary_path", target_summary_path),
            ("target_report_markdown_path", choose_text(primary_entry.get("report_markdown_path"))),
            ("target_check_text_path", choose_text(primary_entry.get("check_text_path"))),
            ("rationale", choose_text(primary_query.get("rationale"))),
            ("compare_context_available", bool(compare_context.get("available"))),
            ("blockers", blockers),
        ]
    )


def is_artifact_report_target(summary_schema: str, summary_path: str) -> bool:
    if summary_schema == ARTIFACT_REPORT_SCHEMA:
        return True
    return Path(summary_path).name.endswith(".artifact_report.json")


def build_inspector_invocation(open_action: dict[str, Any]) -> OrderedDict[str, Any]:
    script_path = normalize_path(Path(INSPECTOR_SCRIPT))
    query_kind = choose_text(open_action.get("query_kind"))
    query_scope = choose_text(open_action.get("query_scope"))
    target_summary_schema = choose_text(open_action.get("target_summary_schema"))
    target_summary_path = choose_text(open_action.get("target_summary_path"))
    arguments: list[str] = []
    blockers: list[str] = []
    mode = query_scope if query_scope in {"report", "artifact_root"} else "unresolved"

    if not target_summary_path:
        blockers.append("open action does not carry a target summary path")
    elif not Path(target_summary_path).exists():
        blockers.append("open action target summary path does not exist")
    elif query_scope == "report":
        if is_artifact_report_target(target_summary_schema, target_summary_path):
            arguments.extend(["-Report", target_summary_path])
        else:
            blockers.append(
                "report-scope opening targets a front-page summary, not a system_compiler.artifact_report/v0 report"
            )
    elif query_scope == "artifact_root":
        if is_artifact_report_target(target_summary_schema, target_summary_path):
            arguments.extend(["-ArtifactRoot", str(Path(target_summary_path).resolve().parent)])
        else:
            blockers.append("artifact-root opening cannot derive a safe artifact report root from the landing target")
    else:
        blockers.append(f"unsupported inspector scope: {query_scope or '<missing>'}")

    query_switch = QUERY_SWITCHES.get(query_kind)
    if query_switch and not blockers:
        arguments.append(query_switch)
    elif query_kind and query_kind not in {"default_overview", *QUERY_SWITCHES.keys()}:
        blockers.append(f"unsupported inspector query kind: {query_kind}")

    if not blockers:
        arguments.append("-AsJson")
    else:
        arguments = []

    powershell_command = (
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            script_path,
        ]
        + arguments
        if not blockers
        else []
    )

    return OrderedDict(
        [
            ("script_path", script_path),
            ("ready", not blockers),
            ("mode", mode),
            ("query_kind", query_kind),
            ("output_format", "json"),
            ("arguments", arguments),
            ("powershell_command", powershell_command),
            ("blockers", blockers),
        ]
    )


def build_opened_projection_record(
    *,
    status: str,
    projection_kind: str,
    source_summary_schema: str,
    source_summary_kind: str,
    source_summary_path: str,
    headline: str,
    summary_lines: list[str],
    question_lines: list[str],
    supporting_summary_paths: list[str],
    evidence_paths: list[str],
    compare_paths: list[str],
    blockers: list[str],
) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("status", status),
            ("projection_kind", projection_kind),
            ("source_summary_schema", source_summary_schema),
            ("source_summary_kind", source_summary_kind),
            ("source_summary_path", source_summary_path),
            ("headline", headline),
            ("summary_lines", ordered_unique(summary_lines)),
            ("question_lines", ordered_unique(question_lines)),
            ("supporting_summary_paths", ordered_unique(supporting_summary_paths)),
            ("evidence_paths", ordered_unique(evidence_paths)),
            ("compare_paths", ordered_unique(compare_paths)),
            ("blockers", ordered_unique(blockers)),
        ]
    )


def build_biography_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    world = get_mapping(summary.get("world"))
    subject = get_mapping(world.get("subject"))
    biography = get_mapping(summary.get("biography"))
    runtime_evidence = get_mapping(summary.get("runtime_evidence"))
    runtime_qemu = get_mapping(runtime_evidence.get("qemu"))
    witness_bundle = get_mapping(summary.get("witness_bundle"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    questions = get_mapping(summary.get("questions"))
    compare_paths = existing_paths([artifact_context.get("world_compare_summary")])
    compare_attached = bool(compare_paths)
    headline = "biography world={0} compare={1}".format(
        choose_text(world.get("name")) or choose_text(world.get("title")) or "unknown",
        "attached" if compare_attached else "not-attached",
    )
    summary_lines = [
        "identity: {0}".format(choose_text(biography.get("identity")) or "-"),
        "thesis: {0}".format(choose_text(biography.get("thesis")) or "-"),
        "board={0} facets={1}".format(
            choose_text(subject.get("board")) or "-",
            join_texts(get_list(subject.get("active_facets"))),
        ),
        "runtime_evidence result={0} qemu_conclusion={1}".format(
            choose_text(runtime_evidence.get("result")) or "-",
            choose_text(runtime_qemu.get("witness_conclusion")) or "-",
        ),
        "witness_bundle entries={0} ok={1} required_missing={2}".format(
            choose_text(witness_bundle.get("entry_count")) or "0",
            choose_text(witness_bundle.get("ok_count")) or "0",
            choose_text(witness_bundle.get("required_missing_count")) or "0",
        ),
    ]
    world_verdict = choose_text(summary.get("world_verdict"))
    if world_verdict:
        summary_lines.append(f"world_verdict={world_verdict}")
    return build_opened_projection_record(
        status="available",
        projection_kind="biography_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(biography.get("next_questions")) + get_list(questions.get("compare_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=existing_paths(
            [
                artifact_context.get("runtime_evidence_summary"),
                artifact_context.get("witness_bundle_summary"),
            ]
        ),
        compare_paths=compare_paths,
        blockers=[],
    )


def build_world_compare_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    world = get_mapping(summary.get("world"))
    subject = get_mapping(world.get("subject"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    bundle_status = get_mapping(summary.get("bundle_status"))
    witness_summary = get_mapping(summary.get("witness_summary"))
    collapse_surface = get_mapping(summary.get("collapse_surface"))
    questions = get_mapping(summary.get("questions"))
    headline = "world_compare world={0} verdict={1}".format(
        choose_text(world.get("name")) or choose_text(world.get("title")) or "unknown",
        choose_text(summary.get("world_verdict")) or "-",
    )
    summary_lines = [
        "board={0} facets={1}".format(
            choose_text(subject.get("board")) or "-",
            join_texts(get_list(subject.get("active_facets"))),
        ),
        "baseline_state={0} candidate_state={1}".format(
            choose_text(bundle_status.get("baseline_state")) or "-",
            choose_text(bundle_status.get("candidate_state")) or "-",
        ),
        "witness_changes changed={0} regressions={1} improvements={2}".format(
            choose_text(witness_summary.get("changed_entry_count")) or "0",
            choose_text(witness_summary.get("regression_count")) or "0",
            choose_text(witness_summary.get("improvement_count")) or "0",
        ),
        "collapse_surface changed={0} regressed={1} narratives={2}".format(
            "yes" if bool(collapse_surface.get("changed")) else "no",
            len(get_list(collapse_surface.get("regressed_witnesses"))),
            len(get_list(collapse_surface.get("narratives"))),
        ),
    ]
    return build_opened_projection_record(
        status="available",
        projection_kind="world_compare_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(questions.get("compare_questions")) + get_list(questions.get("next_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=existing_paths(
            [
                artifact_context.get("baseline_witness_bundle"),
                artifact_context.get("candidate_witness_bundle"),
            ]
        ),
        compare_paths=[],
        blockers=[],
    )


def build_world_shelf_review_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(summary.get("artifact_context"))
    review_status = get_mapping(summary.get("review_status"))
    collapse_surface = get_mapping(summary.get("collapse_surface"))
    drift_digest = get_mapping(summary.get("drift_digest"))
    questions = get_mapping(summary.get("questions"))
    headline = "world_shelf_review verdict={0}".format(choose_text(summary.get("review_verdict")) or "-")
    summary_lines = [
        "compare_mode={0} compare_enabled={1}".format(
            choose_text(review_status.get("compare_mode")) or "-",
            "yes" if bool(review_status.get("compare_enabled")) else "no",
        ),
        "candidate_biographies={0} baseline_biographies={1}".format(
            choose_text(review_status.get("candidate_biography_count")) or "0",
            choose_text(review_status.get("baseline_biography_count")) or "0",
        ),
        "candidate_compare_attached={0} baseline_compare_attached={1}".format(
            choose_text(review_status.get("candidate_compare_attached_count")) or "0",
            choose_text(review_status.get("baseline_compare_attached_count")) or "0",
        ),
        "collapse_surface changed={0} narratives={1}".format(
            "yes" if bool(collapse_surface.get("changed")) else "no",
            len(get_list(collapse_surface.get("narratives"))),
        ),
    ]
    summary_lines.append(
        "drift_digest changed={0} verdict={1} entry_changed={2} regressions={3} improvements={4} front_page_detail={5}".format(
            "yes" if bool(drift_digest.get("changed")) else "no",
            choose_text(drift_digest.get("verdict")) or choose_text(summary.get("review_verdict")) or "-",
            choose_text(drift_digest.get("entry_changed_count")) or "0",
            choose_text(drift_digest.get("entry_regression_count")) or "0",
            choose_text(drift_digest.get("entry_improvement_count")) or "0",
            choose_text(drift_digest.get("front_page_entry_detail_changed_count")) or "0",
        )
    )
    return build_opened_projection_record(
        status="available",
        projection_kind="world_shelf_review_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(questions.get("candidate_questions"))
        + get_list(questions.get("compare_questions"))
        + get_list(questions.get("next_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=existing_paths(
            [
                artifact_context.get("candidate_shelf_summary"),
                artifact_context.get("baseline_shelf_summary"),
            ]
        ),
        compare_paths=existing_paths([artifact_context.get("compare_summary_path")]),
        blockers=[],
    )


def build_biography_index_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    shelf = get_mapping(summary.get("shelf"))
    summary_block = get_mapping(summary.get("summary"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    questions = get_mapping(summary.get("questions"))
    entries = [get_mapping(entry) for entry in get_list(summary.get("entries")) if isinstance(entry, dict)]
    headline = "shelf title={0} biographies={1}".format(
        choose_text(shelf.get("title")) or "unknown",
        choose_text(summary_block.get("biography_count")) or "0",
    )
    summary_lines = [
        "worlds={0} ok={1} fail={2}".format(
            choose_text(summary_block.get("unique_world_count")) or "0",
            choose_text(summary_block.get("ok_count")) or "0",
            choose_text(summary_block.get("fail_count")) or "0",
        ),
        "compare_attached={0} standing={1} drifted={2} collapsed={3}".format(
            choose_text(summary_block.get("compare_attached_count")) or "0",
            choose_text(summary_block.get("standing_count")) or "0",
            choose_text(summary_block.get("drifted_count")) or "0",
            choose_text(summary_block.get("collapsed_count")) or "0",
        ),
    ]
    if entries:
        first_entry = entries[0]
        summary_lines.append(
            "first_entry={0} verdict={1}".format(
                choose_text(first_entry.get("world_name")) or choose_text(first_entry.get("id")) or "unknown",
                choose_text(first_entry.get("world_verdict")) or "not-attached",
            )
        )
    return build_opened_projection_record(
        status="available",
        projection_kind="shelf_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(questions.get("compare_questions")) + get_list(questions.get("next_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=existing_paths(
            [artifact_context.get("biography_summaries")]
            + [entry.get("runtime_evidence_summary") for entry in entries]
            + [entry.get("witness_bundle_summary") for entry in entries]
        ),
        compare_paths=existing_paths([entry.get("world_compare_summary") for entry in entries]),
        blockers=[],
    )


def build_biography_index_compare_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    shelf = get_mapping(summary.get("shelf"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    shelf_status = get_mapping(summary.get("shelf_status"))
    entry_summary = get_mapping(summary.get("entry_summary"))
    questions = get_mapping(summary.get("questions"))
    entry_changes = [get_mapping(item) for item in get_list(summary.get("entry_changes")) if isinstance(item, dict)]
    headline = "shelf_compare title={0} verdict={1}".format(
        choose_text(shelf.get("title")) or "unknown",
        choose_text(summary.get("shelf_verdict")) or "-",
    )
    summary_lines = [
        "baseline_biographies={0} candidate_biographies={1}".format(
            choose_text(shelf_status.get("baseline_biography_count")) or "0",
            choose_text(shelf_status.get("candidate_biography_count")) or "0",
        ),
        "entry_changes changed={0} added={1} removed={2}".format(
            choose_text(entry_summary.get("changed_entry_count")) or "0",
            choose_text(entry_summary.get("added_entry_count")) or "0",
            choose_text(entry_summary.get("removed_entry_count")) or "0",
        ),
        "regressions={0} improvements={1} neutral={2}".format(
            choose_text(entry_summary.get("regression_count")) or "0",
            choose_text(entry_summary.get("improvement_count")) or "0",
            choose_text(entry_summary.get("neutral_change_count")) or "0",
        ),
    ]
    if entry_changes:
        summary_lines.append(
            "first_change={0} impact={1}".format(
                choose_text(entry_changes[0].get("anchor_id")) or "unknown",
                choose_text(entry_changes[0].get("impact")) or "-",
            )
        )
    return build_opened_projection_record(
        status="available",
        projection_kind="shelf_compare_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(questions.get("compare_questions")) + get_list(questions.get("next_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=[],
        compare_paths=existing_paths(
            [
                artifact_context.get("baseline_biography_index"),
                artifact_context.get("candidate_biography_index"),
            ]
            + [change.get("left_summary_path") for change in entry_changes]
            + [change.get("right_summary_path") for change in entry_changes]
        ),
        blockers=[],
    )


def build_witness_bundle_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    world = get_mapping(summary.get("world"))
    subject = get_mapping(world.get("subject"))
    contract_status = get_mapping(summary.get("contract_status"))
    witness_summary = get_mapping(summary.get("witness_summary"))
    entries = [get_mapping(entry) for entry in get_list(summary.get("witness_entries")) if isinstance(entry, dict)]
    artifact_context = get_mapping(summary.get("artifact_context"))
    headline = "witness_bundle world={0} ok={1}/{2}".format(
        choose_text(world.get("name")) or choose_text(world.get("title")) or "unknown",
        choose_text(witness_summary.get("ok_count")) or "0",
        choose_text(witness_summary.get("entry_count")) or "0",
    )
    summary_lines = [
        "board={0} facets={1}".format(
            choose_text(subject.get("board")) or "-",
            join_texts(get_list(subject.get("active_facets"))),
        ),
        "entries={0} missing={1} fail={2} required_missing={3}".format(
            choose_text(witness_summary.get("entry_count")) or "0",
            choose_text(witness_summary.get("missing_count")) or "0",
            choose_text(witness_summary.get("fail_count")) or "0",
            choose_text(witness_summary.get("required_missing_count")) or "0",
        ),
        "contracts present={0} missing={1}".format(
            choose_text(contract_status.get("present_count")) or "0",
            choose_text(contract_status.get("missing_count")) or "0",
        ),
    ]
    return build_opened_projection_record(
        status="available",
        projection_kind="witness_bundle_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind=choose_text(summary.get("kind")),
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(world.get("core_questions")) + get_list(world.get("compare_questions")),
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=existing_paths(
            [artifact_context.get("runtime_evidence_summary")]
            + [entry.get("source_path") for entry in entries]
            + [artifact for entry in entries for artifact in get_list(entry.get("artifact_refs"))]
        ),
        compare_paths=[],
        blockers=[],
    )


def build_runtime_evidence_projection(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    host = get_mapping(summary.get("host"))
    cold = get_mapping(host.get("cold"))
    warm = get_mapping(host.get("warm"))
    qemu = get_mapping(summary.get("qemu"))
    lower_half = get_mapping(qemu.get("lower_half"))
    lower_half_status = get_mapping(lower_half.get("status"))
    lower_half_witness = get_mapping(lower_half.get("witness_bundle"))
    lower_half_biography = get_mapping(lower_half.get("biography"))
    witness_bundle = get_mapping(summary.get("witness_bundle"))
    warm_comparison = get_mapping(warm.get("comparison"))
    headline = "runtime_evidence result={0} qemu_conclusion={1}".format(
        choose_text(summary.get("result")) or "-",
        choose_text(lower_half_witness.get("conclusion")) or "-",
    )
    summary_lines = [
        "host_cold ok={0} fail={1} other={2}".format(
            choose_text(get_mapping(cold.get("status")).get("ok")) or "0",
            choose_text(get_mapping(cold.get("status")).get("fail")) or "0",
            choose_text(get_mapping(cold.get("status")).get("other")) or "0",
        ),
        "host_warm ok={0} fail={1} other={2} improvements={3} regressions={4}".format(
            choose_text(get_mapping(warm.get("status")).get("ok")) or "0",
            choose_text(get_mapping(warm.get("status")).get("fail")) or "0",
            choose_text(get_mapping(warm.get("status")).get("other")) or "0",
            choose_text(warm_comparison.get("improvements")) or "0",
            choose_text(warm_comparison.get("regressions")) or "0",
        ),
        "qemu_cases total={0} completed={1} ok={2} fail={3} other={4}".format(
            choose_text(lower_half.get("case_count")) or "0",
            choose_text(lower_half.get("completed_case_count")) or "0",
            choose_text(lower_half_status.get("ok")) or "0",
            choose_text(lower_half_status.get("fail")) or "0",
            choose_text(lower_half_status.get("other")) or "0",
        ),
    ]
    return build_opened_projection_record(
        status="available",
        projection_kind="runtime_evidence_bundle_overview",
        source_summary_schema=choose_text(summary.get("schema")),
        source_summary_kind="minimal_kernel.runtime_evidence_bundle",
        source_summary_path=normalize_path(summary_path),
        headline=headline,
        summary_lines=summary_lines,
        question_lines=get_list(lower_half_biography.get("next_questions")),
        supporting_summary_paths=[],
        evidence_paths=existing_paths(
            [
                cold.get("summary_path"),
                warm.get("summary_path"),
                lower_half.get("summary_path"),
                witness_bundle.get("summary_path"),
            ]
        ),
        compare_paths=existing_paths([warm_comparison.get("baseline_summary_path")]),
        blockers=[],
    )


def build_opened_projection(open_action: dict[str, Any]) -> OrderedDict[str, Any]:
    target_summary_path = choose_text(open_action.get("target_summary_path"))
    target_summary_schema = choose_text(open_action.get("target_summary_schema"))
    target_summary_kind = choose_text(open_action.get("target_summary_kind"))
    blockers: list[str] = []

    if not target_summary_path:
        blockers.append("open action does not carry a target summary path")
    elif not Path(target_summary_path).exists():
        blockers.append("open action target summary path does not exist")

    if blockers:
        return build_opened_projection_record(
            status="unavailable",
            projection_kind="missing_target",
            source_summary_schema=target_summary_schema,
            source_summary_kind=target_summary_kind,
            source_summary_path=target_summary_path,
            headline="target summary unavailable",
            summary_lines=[],
            question_lines=[],
            supporting_summary_paths=[],
            evidence_paths=[],
            compare_paths=[],
            blockers=blockers,
        )

    summary_path = Path(target_summary_path).resolve()
    try:
        summary = load_json(summary_path)
    except Exception as exc:
        return build_opened_projection_record(
            status="unavailable",
            projection_kind="unsupported_target",
            source_summary_schema=target_summary_schema,
            source_summary_kind=target_summary_kind,
            source_summary_path=normalize_path(summary_path),
            headline="target summary could not be loaded",
            summary_lines=[],
            question_lines=[],
            supporting_summary_paths=[],
            evidence_paths=[],
            compare_paths=[],
            blockers=[f"failed to load target summary: {exc}"],
        )

    actual_schema = choose_text(summary.get("schema")) or target_summary_schema
    if actual_schema == BIOGRAPHY_SCHEMA:
        return build_biography_projection(summary_path, summary)
    if actual_schema == WORLD_COMPARE_SCHEMA:
        return build_world_compare_projection(summary_path, summary)
    if actual_schema == WORLD_SHELF_REVIEW_SCHEMA:
        return build_world_shelf_review_projection(summary_path, summary)
    if actual_schema == BIOGRAPHY_INDEX_SCHEMA:
        return build_biography_index_projection(summary_path, summary)
    if actual_schema == BIOGRAPHY_INDEX_COMPARE_SCHEMA:
        return build_biography_index_compare_projection(summary_path, summary)
    if actual_schema == WITNESS_BUNDLE_SCHEMA:
        return build_witness_bundle_projection(summary_path, summary)
    if actual_schema == RUNTIME_EVIDENCE_SCHEMA:
        return build_runtime_evidence_projection(summary_path, summary)

    return build_opened_projection_record(
        status="unavailable",
        projection_kind="unsupported_target",
        source_summary_schema=actual_schema,
        source_summary_kind=choose_text(summary.get("kind")) or target_summary_kind,
        source_summary_path=normalize_path(summary_path),
        headline="target summary has no opener projection yet",
        summary_lines=[],
        question_lines=[],
        supporting_summary_paths=build_front_page_supporting_paths(summary),
        evidence_paths=[],
        compare_paths=[],
        blockers=[f"unsupported opener projection target schema: {actual_schema or '<missing>'}"],
    )


def build_questions(
    open_action: dict[str, Any],
    inspector_invocation: dict[str, Any],
    compare_context: dict[str, Any],
    opened_projection: dict[str, Any],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if bool(compare_context.get("available")):
        compare_questions.append(
            "Does the `{0}` verdict change how this opener should prioritize the first explain action?".format(
                choose_text(compare_context.get("landing_verdict")) or "unknown"
            )
        )
        if bool(compare_context.get("primary_query_changed")):
            compare_questions.append("Did the primary opening query drift in a consumer-visible way?")
        if bool(compare_context.get("query_regression_changed")):
            compare_questions.append("Which opening query restores the regressed explain surface?")
    else:
        compare_questions.append("Should this opener be compared against a baseline landing before publishing?")

    if bool(inspector_invocation.get("ready")):
        next_questions.append("Should downstream tools execute this inspector invocation directly?")
    elif choose_text(opened_projection.get("status")) == "available":
        next_questions.append("Should downstream tools render this opened projection before asking for deeper explain?")
    else:
        next_questions.append(
            "Should the landing publish an artifact_report target or artifact_root hint for direct inspector execution?"
        )

    followups = get_list(open_action.get("followup_query_kinds"))
    if followups:
        next_questions.append(
            "Should `{0}` become the next deterministic follow-up query after the primary open?".format(
                choose_text(followups[0])
            )
        )

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_summary_model(
    landing_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    landing_compare_summary_path: Path | None = None,
) -> OrderedDict[str, Any]:
    landing_summary = load_landing_summary(landing_summary_path)
    compare_summary = load_landing_compare_summary(landing_compare_summary_path) if landing_compare_summary_path else None
    source_landing = build_source_landing(landing_summary_path, landing_summary)
    source_landing_compare = (
        build_source_landing_compare(landing_summary_path, landing_compare_summary_path, compare_summary)
        if compare_summary is not None and landing_compare_summary_path is not None
        else None
    )
    compare_context = build_compare_context(source_landing_compare)
    open_action = build_open_action(source_landing, compare_context)
    inspector_invocation = build_inspector_invocation(open_action)
    opened_projection = build_opened_projection(open_action)
    questions = build_questions(open_action, inspector_invocation, compare_context, opened_projection)

    supporting_surfaces = [
        build_front_page_surface(
            summary=landing_summary,
            summary_path=landing_summary_path,
            surface_id="source_landing",
            role="source_landing",
            schema=LANDING_SCHEMA,
        )
    ]
    if compare_summary is not None and landing_compare_summary_path is not None:
        supporting_surfaces.append(
            build_front_page_surface(
                summary=compare_summary,
                summary_path=landing_compare_summary_path,
                surface_id="source_landing_compare",
                role="source_landing_compare",
                schema=LANDING_COMPARE_SCHEMA,
            )
        )

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_entry_opener/v0"),
            ("kind", "system_compiler.front_page_entry_opener"),
            ("generator", "scripts/export_system_compiler_front_page_entry_opener.py"),
            ("result", "ok"),
            (
                "entry_opener",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opener"),
                        (
                            "summary",
                            "A consumer-side opening-plan facade that turns an entry landing and optional landing compare into one deterministic explain open action.",
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
                "artifact_context",
                OrderedDict(
                    [
                        ("source_landing_summary_path", normalize_path(landing_summary_path)),
                        (
                            "source_landing_compare_summary_path",
                            normalize_path(landing_compare_summary_path) if landing_compare_summary_path else "",
                        ),
                        ("output_root", normalize_path(output_root)),
                        ("opener_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("source_landing", source_landing),
            ("source_landing_compare", source_landing_compare),
            ("compare_context", compare_context),
            ("open_action", open_action),
            ("inspector_invocation", inspector_invocation),
            ("opened_projection", opened_projection),
            ("questions", questions),
            ("violations", []),
        ]
    )
