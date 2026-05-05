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

DIRECT_MODE_TO_TAB_ID = OrderedDict(
    [
        ("review", "grouped_review"),
        ("compare", "counterfactual_verdict"),
        ("biography", "delivery_biography"),
        ("evidence", "supporting_evidence"),
        ("runtime_session", "runtime_session"),
    ]
)

ENTRY_TIER_RANK = {
    "route_only": 0,
    "evidence_only": 1,
    "biography_ready": 2,
    "compare_ready": 3,
    "review_ready": 4,
}


def choose_text(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def nullable_text(value: Any) -> str | None:
    text = choose_text(value)
    return text or None


def nullable_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def nullable_bool(value: Any) -> bool | None:
    if value is None:
        return None
    return bool(value)


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


def normalize_provenance_root(root_value: Any) -> OrderedDict[str, str] | None:
    root = get_mapping(root_value)
    root_id = choose_text(root.get("root_id"))
    if not root_id:
        return None

    return OrderedDict(
        [
            ("root_id", root_id),
            ("root_kind", choose_text(root.get("root_kind"))),
            ("source_summary_schema", choose_text(root.get("source_summary_schema"))),
            ("source_summary_path", normalize_path(root.get("source_summary_path", ""))),
            (
                "source_front_page_summary_path",
                normalize_path(root.get("source_front_page_summary_path", ""))
                if choose_text(root.get("source_front_page_summary_path"))
                else "",
            ),
        ]
    )


def provenance_roots_by_id(summary: dict[str, Any]) -> OrderedDict[str, OrderedDict[str, str]]:
    roots: OrderedDict[str, OrderedDict[str, str]] = OrderedDict()
    for root_value in get_list(summary.get("provenance_roots")):
        root = normalize_provenance_root(root_value)
        if root is None or root["root_id"] in roots:
            continue
        roots[root["root_id"]] = root
    return roots


def build_provenance_root_detail_changes(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
) -> list[OrderedDict[str, Any]]:
    baseline_roots = provenance_roots_by_id(baseline_summary)
    candidate_roots = provenance_roots_by_id(candidate_summary)
    changes: list[OrderedDict[str, Any]] = []

    for root_id, baseline_root in baseline_roots.items():
        candidate_root = candidate_roots.get(root_id)
        if candidate_root is None:
            continue

        root_kind_changed = baseline_root["root_kind"] != candidate_root["root_kind"]
        schema_changed = baseline_root["source_summary_schema"] != candidate_root["source_summary_schema"]
        source_path_changed = baseline_root["source_summary_path"] != candidate_root["source_summary_path"]
        front_page_path_changed = (
            baseline_root["source_front_page_summary_path"]
            != candidate_root["source_front_page_summary_path"]
        )
        if not any([root_kind_changed, schema_changed, source_path_changed, front_page_path_changed]):
            continue

        changes.append(
            OrderedDict(
                [
                    ("root_id", root_id),
                    ("baseline_root_kind", baseline_root["root_kind"]),
                    ("candidate_root_kind", candidate_root["root_kind"]),
                    ("baseline_source_summary_schema", baseline_root["source_summary_schema"]),
                    ("candidate_source_summary_schema", candidate_root["source_summary_schema"]),
                    ("baseline_source_summary_path", baseline_root["source_summary_path"]),
                    ("candidate_source_summary_path", candidate_root["source_summary_path"]),
                    (
                        "baseline_source_front_page_summary_path",
                        baseline_root["source_front_page_summary_path"],
                    ),
                    (
                        "candidate_source_front_page_summary_path",
                        candidate_root["source_front_page_summary_path"],
                    ),
                    ("root_kind_changed", root_kind_changed),
                    ("schema_changed", schema_changed),
                    ("source_path_changed", source_path_changed),
                    ("front_page_path_changed", front_page_path_changed),
                ]
            )
        )

    return changes


def load_landing_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != LANDING_SCHEMA:
        raise ValueError(f"unsupported front page entry landing schema: {path}")
    if choose_text(summary.get("kind")) != LANDING_KIND:
        raise ValueError(f"unsupported front page entry landing kind: {path}")
    return summary


def build_front_page_surface(
    landing_summary: dict[str, Any],
    landing_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    root_surface = get_mapping(landing_summary.get("root_surface"))
    artifact_context = get_mapping(landing_summary.get("artifact_context"))
    root_label = choose_text(root_surface.get("label")) or "entry landing"
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", f"{role.replace('_', ' ')}: {root_label}"),
            ("role", role),
            ("summary_schema", LANDING_SCHEMA),
            ("summary_path", normalize_path(landing_summary_path)),
            ("report_markdown_path", normalize_path(artifact_context.get("report_markdown_path", ""))),
            ("check_text_path", normalize_path(artifact_context.get("check_text_path", ""))),
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


def build_landing_provenance_entry(
    landing_summary: dict[str, Any],
    landing_summary_path: Path,
    provenance_id: str,
    landing_role: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(landing_summary.get("artifact_context"))
    root_surface = get_mapping(landing_summary.get("root_surface"))
    landing_status = get_mapping(landing_summary.get("landing_status"))
    return OrderedDict(
        [
            ("id", provenance_id),
            ("landing_role", landing_role),
            ("source_summary_schema", LANDING_SCHEMA),
            ("source_summary_path", normalize_path(landing_summary_path)),
            (
                "source_input_capability_summary_path",
                normalize_path(artifact_context.get("input_capability_summary_path", "")),
            ),
            ("source_root_summary_path", normalize_path(root_surface.get("summary_path", ""))),
            ("source_report_markdown_path", normalize_path(artifact_context.get("report_markdown_path", ""))),
            ("source_check_text_path", normalize_path(artifact_context.get("check_text_path", ""))),
            ("primary_tab_id", nullable_text(landing_status.get("primary_tab_id"))),
            (
                "available_tab_ids",
                ordered_unique(
                    [choose_text(tab_id) for tab_id in get_list(landing_status.get("available_tab_ids"))]
                ),
            ),
            ("provenance_root_count", int(landing_status.get("provenance_root_count", 0))),
            ("route_provenance_entry_count", int(landing_status.get("route_provenance_entry_count", 0))),
        ]
    )


def normalize_landing_tabs(
    landing_summary: dict[str, Any],
) -> tuple[list[OrderedDict[str, Any]], list[str]]:
    normalized_tabs: list[OrderedDict[str, Any]] = []
    anchor_order: list[str] = []
    occurrence_count: dict[str, int] = {}

    for index, tab_value in enumerate(get_list(landing_summary.get("landing_tabs"))):
        tab = get_mapping(tab_value)
        entry = get_mapping(tab.get("entry"))
        if not tab or not entry:
            continue

        anchor_base = choose_text(tab.get("tab_id"))
        if not anchor_base:
            anchor_base = "|".join(
                [
                    choose_text(entry.get("surface_id")),
                    choose_text(entry.get("role")),
                    choose_text(entry.get("summary_schema")),
                    choose_text(entry.get("summary_kind")) or "_",
                ]
            )
        occurrence = occurrence_count.get(anchor_base, 0) + 1
        occurrence_count[anchor_base] = occurrence
        anchor_id = anchor_base if occurrence == 1 else f"{anchor_base}#{occurrence}"

        normalized_tabs.append(
            OrderedDict(
                [
                    ("anchor_id", anchor_id),
                    ("tab_id", choose_text(tab.get("tab_id"))),
                    ("title", choose_text(tab.get("title"))),
                    (
                        "capability_ids",
                        ordered_unique(
                            [choose_text(capability_id) for capability_id in get_list(tab.get("capability_ids"))]
                        ),
                    ),
                    ("order_index", index),
                    ("primary", index == 0),
                    ("route_id", choose_text(entry.get("route_id"))),
                    ("depth", int(entry.get("depth", 0))),
                    ("surface_id", choose_text(entry.get("surface_id"))),
                    ("label", choose_text(entry.get("label"))),
                    ("role", choose_text(entry.get("role"))),
                    ("summary_schema", choose_text(entry.get("summary_schema"))),
                    ("summary_kind", choose_text(entry.get("summary_kind"))),
                    ("summary_path", normalize_path(entry.get("summary_path", ""))),
                    ("report_markdown_path", normalize_path(entry.get("report_markdown_path", ""))),
                    ("check_text_path", normalize_path(entry.get("check_text_path", ""))),
                    ("revisit", bool(entry.get("revisit"))),
                    ("cycle", bool(entry.get("cycle"))),
                    ("expanded", bool(entry.get("expanded"))),
                    ("route_provenance_count", int(entry.get("route_provenance_count", 0))),
                    ("supporting_surface_count", int(entry.get("supporting_surface_count", 0))),
                ]
            )
        )
        anchor_order.append(anchor_id)

    return normalized_tabs, anchor_order


def build_landing_status(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
    baseline_tabs: list[OrderedDict[str, Any]],
    candidate_tabs: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    baseline_root = get_mapping(baseline_summary.get("root_surface"))
    candidate_root = get_mapping(candidate_summary.get("root_surface"))
    baseline_landing_status = get_mapping(baseline_summary.get("landing_status"))
    candidate_landing_status = get_mapping(candidate_summary.get("landing_status"))

    baseline_primary_anchor = baseline_tabs[0]["anchor_id"] if baseline_tabs else None
    candidate_primary_anchor = candidate_tabs[0]["anchor_id"] if candidate_tabs else None

    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_summary.get("result"))),
            ("candidate_result", choose_text(candidate_summary.get("result"))),
            ("baseline_root_label", choose_text(baseline_root.get("label"))),
            ("candidate_root_label", choose_text(candidate_root.get("label"))),
            ("baseline_root_summary_schema", choose_text(baseline_root.get("summary_schema"))),
            ("candidate_root_summary_schema", choose_text(candidate_root.get("summary_schema"))),
            ("baseline_root_summary_kind", choose_text(baseline_root.get("summary_kind"))),
            ("candidate_root_summary_kind", choose_text(candidate_root.get("summary_kind"))),
            (
                "baseline_recommended_entry_mode",
                choose_text(baseline_landing_status.get("recommended_entry_mode")),
            ),
            (
                "candidate_recommended_entry_mode",
                choose_text(candidate_landing_status.get("recommended_entry_mode")),
            ),
            ("baseline_entry_tier", choose_text(baseline_landing_status.get("entry_tier"))),
            ("candidate_entry_tier", choose_text(candidate_landing_status.get("entry_tier"))),
            ("baseline_primary_tab_id", nullable_text(baseline_landing_status.get("primary_tab_id"))),
            ("candidate_primary_tab_id", nullable_text(candidate_landing_status.get("primary_tab_id"))),
            ("baseline_primary_anchor", baseline_primary_anchor),
            ("candidate_primary_anchor", candidate_primary_anchor),
            (
                "baseline_available_tab_ids",
                ordered_unique(
                    [choose_text(tab_id) for tab_id in get_list(baseline_landing_status.get("available_tab_ids"))]
                ),
            ),
            (
                "candidate_available_tab_ids",
                ordered_unique(
                    [choose_text(tab_id) for tab_id in get_list(candidate_landing_status.get("available_tab_ids"))]
                ),
            ),
            (
                "baseline_fallback_mode_order",
                ordered_unique(
                    [choose_text(mode) for mode in get_list(baseline_summary.get("fallback_mode_order"))]
                ),
            ),
            (
                "candidate_fallback_mode_order",
                ordered_unique(
                    [choose_text(mode) for mode in get_list(candidate_summary.get("fallback_mode_order"))]
                ),
            ),
            ("baseline_tab_count", len(baseline_tabs)),
            ("candidate_tab_count", len(candidate_tabs)),
            ("baseline_provenance_root_count", len(get_list(baseline_summary.get("provenance_roots")))),
            ("candidate_provenance_root_count", len(get_list(candidate_summary.get("provenance_roots")))),
            (
                "baseline_route_provenance_entry_count",
                len(get_list(baseline_summary.get("route_provenance"))),
            ),
            (
                "candidate_route_provenance_entry_count",
                len(get_list(candidate_summary.get("route_provenance"))),
            ),
            (
                "baseline_direct_review_available",
                bool(baseline_landing_status.get("direct_review_available")),
            ),
            (
                "candidate_direct_review_available",
                bool(candidate_landing_status.get("direct_review_available")),
            ),
            (
                "baseline_direct_compare_available",
                bool(baseline_landing_status.get("direct_compare_available")),
            ),
            (
                "candidate_direct_compare_available",
                bool(candidate_landing_status.get("direct_compare_available")),
            ),
            (
                "baseline_direct_biography_available",
                bool(baseline_landing_status.get("direct_biography_available")),
            ),
            (
                "candidate_direct_biography_available",
                bool(candidate_landing_status.get("direct_biography_available")),
            ),
            (
                "baseline_direct_evidence_available",
                bool(baseline_landing_status.get("direct_evidence_available")),
            ),
            (
                "candidate_direct_evidence_available",
                bool(candidate_landing_status.get("direct_evidence_available")),
            ),
            (
                "baseline_direct_runtime_session_available",
                bool(baseline_landing_status.get("direct_runtime_session_available")),
            ),
            (
                "candidate_direct_runtime_session_available",
                bool(candidate_landing_status.get("direct_runtime_session_available")),
            ),
        ]
    )


def build_landing_changes(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
    landing_status: dict[str, Any],
) -> OrderedDict[str, Any]:
    baseline_root = get_mapping(baseline_summary.get("root_surface"))
    candidate_root = get_mapping(candidate_summary.get("root_surface"))

    baseline_direct_modes = [
        mode
        for mode in DIRECT_MODE_TO_TAB_ID
        if bool(landing_status.get(f"baseline_direct_{mode}_available"))
    ]
    candidate_direct_modes = [
        mode
        for mode in DIRECT_MODE_TO_TAB_ID
        if bool(landing_status.get(f"candidate_direct_{mode}_available"))
    ]
    baseline_provenance_root_ids = list(provenance_roots_by_id(baseline_summary).keys())
    candidate_provenance_root_ids = list(provenance_roots_by_id(candidate_summary).keys())
    provenance_root_detail_changes = build_provenance_root_detail_changes(
        baseline_summary,
        candidate_summary,
    )

    landing_changes = OrderedDict(
        [
            ("root_label_changed", choose_text(baseline_root.get("label")) != choose_text(candidate_root.get("label"))),
            (
                "root_schema_changed",
                choose_text(baseline_root.get("summary_schema")) != choose_text(candidate_root.get("summary_schema")),
            ),
            (
                "root_kind_changed",
                choose_text(baseline_root.get("summary_kind")) != choose_text(candidate_root.get("summary_kind")),
            ),
            (
                "recommended_mode_changed",
                choose_text(landing_status.get("baseline_recommended_entry_mode"))
                != choose_text(landing_status.get("candidate_recommended_entry_mode")),
            ),
            (
                "entry_tier_changed",
                choose_text(landing_status.get("baseline_entry_tier"))
                != choose_text(landing_status.get("candidate_entry_tier")),
            ),
            (
                "primary_tab_changed",
                choose_text(landing_status.get("baseline_primary_tab_id"))
                != choose_text(landing_status.get("candidate_primary_tab_id")),
            ),
            (
                "primary_anchor_changed",
                choose_text(landing_status.get("baseline_primary_anchor"))
                != choose_text(landing_status.get("candidate_primary_anchor")),
            ),
            (
                "fallback_mode_order_changed",
                get_list(landing_status.get("baseline_fallback_mode_order"))
                != get_list(landing_status.get("candidate_fallback_mode_order")),
            ),
            (
                "tab_order_changed",
                get_list(landing_status.get("baseline_available_tab_ids"))
                != get_list(landing_status.get("candidate_available_tab_ids")),
            ),
            (
                "available_tab_changes",
                string_array_changes(
                    list(landing_status.get("baseline_available_tab_ids", [])),
                    list(landing_status.get("candidate_available_tab_ids", [])),
                ),
            ),
            (
                "direct_capability_changes",
                string_array_changes(baseline_direct_modes, candidate_direct_modes),
            ),
            (
                "provenance_root_changes",
                string_array_changes(baseline_provenance_root_ids, candidate_provenance_root_ids),
            ),
            ("provenance_root_detail_changes", provenance_root_detail_changes),
        ]
    )
    landing_changes["changed"] = any(
        [
            bool(landing_changes["root_label_changed"]),
            bool(landing_changes["root_schema_changed"]),
            bool(landing_changes["root_kind_changed"]),
            bool(landing_changes["recommended_mode_changed"]),
            bool(landing_changes["entry_tier_changed"]),
            bool(landing_changes["primary_tab_changed"]),
            bool(landing_changes["primary_anchor_changed"]),
            bool(landing_changes["fallback_mode_order_changed"]),
            bool(landing_changes["tab_order_changed"]),
            has_array_changes(landing_changes["available_tab_changes"]),
            has_array_changes(landing_changes["direct_capability_changes"]),
            has_array_changes(landing_changes["provenance_root_changes"]),
            bool(landing_changes["provenance_root_detail_changes"]),
        ]
    )
    return landing_changes


def normalize_query_hint_entry(value: Any) -> OrderedDict[str, Any] | None:
    hint = get_mapping(value)
    if not hint:
        return None

    tab_id = choose_text(hint.get("tab_id"))
    query_kind = choose_text(hint.get("query_kind"))
    scope = choose_text(hint.get("scope"))
    selection_rule = choose_text(hint.get("selection_rule"))
    followup_query_kinds = ordered_unique(
        [choose_text(item) for item in get_list(hint.get("followup_query_kinds"))]
    )
    if not any([tab_id, query_kind, scope, selection_rule, followup_query_kinds]):
        return None

    return OrderedDict(
        [
            ("tab_id", tab_id),
            ("tab_title", choose_text(hint.get("tab_title"))),
            ("entry_role", choose_text(hint.get("entry_role"))),
            ("summary_schema", choose_text(hint.get("summary_schema"))),
            ("summary_kind", choose_text(hint.get("summary_kind"))),
            ("scope", scope),
            ("selection_rule", selection_rule),
            ("query_kind", query_kind),
            ("compare_expected", bool(hint.get("compare_expected"))),
            ("followup_query_kinds", followup_query_kinds),
            ("rationale", choose_text(hint.get("rationale"))),
        ]
    )


def normalize_query_hints(
    landing_summary: dict[str, Any],
) -> tuple[OrderedDict[str, Any] | None, list[OrderedDict[str, Any]], list[str]]:
    query_hints = get_mapping(landing_summary.get("query_hints"))
    primary_query = normalize_query_hint_entry(query_hints.get("primary_query"))

    tab_queries: list[OrderedDict[str, Any]] = []
    anchor_order: list[str] = []
    seen_tab_ids: set[str] = set()
    for index, value in enumerate(get_list(query_hints.get("tab_queries"))):
        normalized = normalize_query_hint_entry(value)
        if normalized is None:
            continue
        tab_id = choose_text(normalized.get("tab_id")) or f"query#{index}"
        if tab_id in seen_tab_ids:
            continue
        seen_tab_ids.add(tab_id)
        tab_queries.append(normalized)
        anchor_order.append(tab_id)

    return primary_query, tab_queries, anchor_order


def build_primary_query_status(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
) -> OrderedDict[str, Any]:
    baseline_primary_query, _, _ = normalize_query_hints(baseline_summary)
    candidate_primary_query, _, _ = normalize_query_hints(candidate_summary)

    return OrderedDict(
        [
            ("baseline_present", baseline_primary_query is not None),
            ("candidate_present", candidate_primary_query is not None),
            (
                "baseline_tab_id",
                nullable_text(baseline_primary_query.get("tab_id")) if baseline_primary_query else None,
            ),
            (
                "candidate_tab_id",
                nullable_text(candidate_primary_query.get("tab_id")) if candidate_primary_query else None,
            ),
            (
                "baseline_query_kind",
                nullable_text(baseline_primary_query.get("query_kind")) if baseline_primary_query else None,
            ),
            (
                "candidate_query_kind",
                nullable_text(candidate_primary_query.get("query_kind")) if candidate_primary_query else None,
            ),
            (
                "baseline_scope",
                nullable_text(baseline_primary_query.get("scope")) if baseline_primary_query else None,
            ),
            (
                "candidate_scope",
                nullable_text(candidate_primary_query.get("scope")) if candidate_primary_query else None,
            ),
            (
                "baseline_selection_rule",
                nullable_text(baseline_primary_query.get("selection_rule")) if baseline_primary_query else None,
            ),
            (
                "candidate_selection_rule",
                nullable_text(candidate_primary_query.get("selection_rule")) if candidate_primary_query else None,
            ),
            (
                "baseline_compare_expected",
                nullable_bool(baseline_primary_query.get("compare_expected")) if baseline_primary_query else None,
            ),
            (
                "candidate_compare_expected",
                nullable_bool(candidate_primary_query.get("compare_expected")) if candidate_primary_query else None,
            ),
            (
                "baseline_followup_query_kinds",
                list(baseline_primary_query.get("followup_query_kinds", [])) if baseline_primary_query else [],
            ),
            (
                "candidate_followup_query_kinds",
                list(candidate_primary_query.get("followup_query_kinds", [])) if candidate_primary_query else [],
            ),
        ]
    )


def build_query_plan_changes(
    baseline_summary: dict[str, Any],
    candidate_summary: dict[str, Any],
    primary_query_status: dict[str, Any],
) -> OrderedDict[str, Any]:
    _, baseline_tab_queries, _ = normalize_query_hints(baseline_summary)
    _, candidate_tab_queries, _ = normalize_query_hints(candidate_summary)

    baseline_query_tab_ids = [
        choose_text(query.get("tab_id"))
        for query in baseline_tab_queries
        if choose_text(query.get("tab_id"))
    ]
    candidate_query_tab_ids = [
        choose_text(query.get("tab_id"))
        for query in candidate_tab_queries
        if choose_text(query.get("tab_id"))
    ]
    primary_followup_query_changes = string_array_changes(
        list(primary_query_status.get("baseline_followup_query_kinds", [])),
        list(primary_query_status.get("candidate_followup_query_kinds", [])),
    )

    query_plan_changes = OrderedDict(
        [
            (
                "primary_query_presence_changed",
                bool(primary_query_status.get("baseline_present")) != bool(primary_query_status.get("candidate_present")),
            ),
            (
                "primary_query_tab_changed",
                choose_text(primary_query_status.get("baseline_tab_id"))
                != choose_text(primary_query_status.get("candidate_tab_id")),
            ),
            (
                "primary_query_kind_changed",
                choose_text(primary_query_status.get("baseline_query_kind"))
                != choose_text(primary_query_status.get("candidate_query_kind")),
            ),
            (
                "primary_query_scope_changed",
                choose_text(primary_query_status.get("baseline_scope"))
                != choose_text(primary_query_status.get("candidate_scope")),
            ),
            (
                "primary_query_selection_rule_changed",
                choose_text(primary_query_status.get("baseline_selection_rule"))
                != choose_text(primary_query_status.get("candidate_selection_rule")),
            ),
            (
                "primary_query_compare_expected_changed",
                nullable_bool(primary_query_status.get("baseline_compare_expected"))
                != nullable_bool(primary_query_status.get("candidate_compare_expected")),
            ),
            ("primary_followup_query_changes", primary_followup_query_changes),
            (
                "available_query_tab_changes",
                string_array_changes(baseline_query_tab_ids, candidate_query_tab_ids),
            ),
        ]
    )
    query_plan_changes["primary_query_changed"] = any(
        [
            bool(query_plan_changes["primary_query_presence_changed"]),
            bool(query_plan_changes["primary_query_tab_changed"]),
            bool(query_plan_changes["primary_query_kind_changed"]),
            bool(query_plan_changes["primary_query_scope_changed"]),
            bool(query_plan_changes["primary_query_selection_rule_changed"]),
            bool(query_plan_changes["primary_query_compare_expected_changed"]),
            has_array_changes(query_plan_changes["primary_followup_query_changes"]),
        ]
    )
    query_plan_changes["changed"] = bool(query_plan_changes["primary_query_changed"]) or has_array_changes(
        query_plan_changes["available_query_tab_changes"]
    )
    return query_plan_changes


def append_scalar_change(changes: list[str], label: str, baseline_value: Any, candidate_value: Any) -> None:
    if baseline_value != candidate_value:
        changes.append(f"{label}:{baseline_value}->{candidate_value}")


def compare_landing_tab(
    anchor_id: str,
    baseline_tab: dict[str, Any] | None,
    candidate_tab: dict[str, Any] | None,
) -> OrderedDict[str, Any] | None:
    if baseline_tab is None and candidate_tab is None:
        return None

    if baseline_tab is None:
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_tab_id", None),
                ("candidate_tab_id", choose_text(candidate_tab.get("tab_id"))),
                ("change_kind", "added"),
                ("impact", "improvement"),
                ("surface_id", choose_text(candidate_tab.get("surface_id"))),
                ("role", choose_text(candidate_tab.get("role"))),
                ("baseline_title", None),
                ("candidate_title", choose_text(candidate_tab.get("title"))),
                ("baseline_summary_schema", None),
                ("candidate_summary_schema", choose_text(candidate_tab.get("summary_schema"))),
                ("baseline_summary_kind", None),
                ("candidate_summary_kind", choose_text(candidate_tab.get("summary_kind"))),
                ("baseline_summary_path", None),
                ("candidate_summary_path", normalize_path(candidate_tab.get("summary_path", ""))),
                ("baseline_order_index", None),
                ("candidate_order_index", int(candidate_tab.get("order_index", 0))),
                ("baseline_capability_ids", []),
                ("candidate_capability_ids", list(candidate_tab.get("capability_ids", []))),
                ("baseline_route_id", None),
                ("candidate_route_id", choose_text(candidate_tab.get("route_id"))),
                ("baseline_primary", None),
                ("candidate_primary", bool(candidate_tab.get("primary"))),
                ("baseline_route_provenance_count", None),
                ("candidate_route_provenance_count", int(candidate_tab.get("route_provenance_count", 0))),
                ("baseline_supporting_surface_count", None),
                ("candidate_supporting_surface_count", int(candidate_tab.get("supporting_surface_count", 0))),
                ("metadata_changes", []),
                ("path_changes", []),
            ]
        )

    if candidate_tab is None:
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_tab_id", choose_text(baseline_tab.get("tab_id"))),
                ("candidate_tab_id", None),
                ("change_kind", "removed"),
                ("impact", "regression"),
                ("surface_id", choose_text(baseline_tab.get("surface_id"))),
                ("role", choose_text(baseline_tab.get("role"))),
                ("baseline_title", choose_text(baseline_tab.get("title"))),
                ("candidate_title", None),
                ("baseline_summary_schema", choose_text(baseline_tab.get("summary_schema"))),
                ("candidate_summary_schema", None),
                ("baseline_summary_kind", choose_text(baseline_tab.get("summary_kind"))),
                ("candidate_summary_kind", None),
                ("baseline_summary_path", normalize_path(baseline_tab.get("summary_path", ""))),
                ("candidate_summary_path", None),
                ("baseline_order_index", int(baseline_tab.get("order_index", 0))),
                ("candidate_order_index", None),
                ("baseline_capability_ids", list(baseline_tab.get("capability_ids", []))),
                ("candidate_capability_ids", []),
                ("baseline_route_id", choose_text(baseline_tab.get("route_id"))),
                ("candidate_route_id", None),
                ("baseline_primary", bool(baseline_tab.get("primary"))),
                ("candidate_primary", None),
                ("baseline_route_provenance_count", int(baseline_tab.get("route_provenance_count", 0))),
                ("candidate_route_provenance_count", None),
                ("baseline_supporting_surface_count", int(baseline_tab.get("supporting_surface_count", 0))),
                ("candidate_supporting_surface_count", None),
                ("metadata_changes", []),
                ("path_changes", []),
            ]
        )

    metadata_changes: list[str] = []
    path_changes: list[str] = []
    capability_changes = string_array_changes(
        list(baseline_tab.get("capability_ids", [])),
        list(candidate_tab.get("capability_ids", [])),
    )
    if has_array_changes(capability_changes):
        metadata_changes.append(
            "capability_ids:+[{0}] -[{1}]".format(
                ", ".join(capability_changes["added"]),
                ", ".join(capability_changes["removed"]),
            )
        )

    append_scalar_change(metadata_changes, "tab_id", choose_text(baseline_tab.get("tab_id")), choose_text(candidate_tab.get("tab_id")))
    append_scalar_change(metadata_changes, "title", choose_text(baseline_tab.get("title")), choose_text(candidate_tab.get("title")))
    append_scalar_change(
        metadata_changes,
        "order_index",
        int(baseline_tab.get("order_index", 0)),
        int(candidate_tab.get("order_index", 0)),
    )
    append_scalar_change(
        metadata_changes,
        "primary",
        bool(baseline_tab.get("primary")),
        bool(candidate_tab.get("primary")),
    )
    append_scalar_change(
        metadata_changes,
        "route_id",
        choose_text(baseline_tab.get("route_id")),
        choose_text(candidate_tab.get("route_id")),
    )
    append_scalar_change(metadata_changes, "depth", int(baseline_tab.get("depth", 0)), int(candidate_tab.get("depth", 0)))
    append_scalar_change(
        metadata_changes,
        "revisit",
        bool(baseline_tab.get("revisit")),
        bool(candidate_tab.get("revisit")),
    )
    append_scalar_change(
        metadata_changes,
        "cycle",
        bool(baseline_tab.get("cycle")),
        bool(candidate_tab.get("cycle")),
    )
    append_scalar_change(
        metadata_changes,
        "expanded",
        bool(baseline_tab.get("expanded")),
        bool(candidate_tab.get("expanded")),
    )
    append_scalar_change(
        metadata_changes,
        "route_provenance_count",
        int(baseline_tab.get("route_provenance_count", 0)),
        int(candidate_tab.get("route_provenance_count", 0)),
    )
    append_scalar_change(
        metadata_changes,
        "supporting_surface_count",
        int(baseline_tab.get("supporting_surface_count", 0)),
        int(candidate_tab.get("supporting_surface_count", 0)),
    )
    append_scalar_change(
        path_changes,
        "summary_path",
        normalize_path(baseline_tab.get("summary_path", "")),
        normalize_path(candidate_tab.get("summary_path", "")),
    )
    append_scalar_change(
        path_changes,
        "report_markdown_path",
        normalize_path(baseline_tab.get("report_markdown_path", "")),
        normalize_path(candidate_tab.get("report_markdown_path", "")),
    )
    append_scalar_change(
        path_changes,
        "check_text_path",
        normalize_path(baseline_tab.get("check_text_path", "")),
        normalize_path(candidate_tab.get("check_text_path", "")),
    )

    if not metadata_changes and not path_changes:
        return None

    score = 0
    score += len(capability_changes["added"])
    score -= len(capability_changes["removed"])

    baseline_route_provenance_count = int(baseline_tab.get("route_provenance_count", 0))
    candidate_route_provenance_count = int(candidate_tab.get("route_provenance_count", 0))
    if candidate_route_provenance_count > baseline_route_provenance_count:
        score += 1
    elif candidate_route_provenance_count < baseline_route_provenance_count:
        score -= 1

    baseline_supporting_surface_count = int(baseline_tab.get("supporting_surface_count", 0))
    candidate_supporting_surface_count = int(candidate_tab.get("supporting_surface_count", 0))

    if score > 0:
        impact = "improvement"
    elif score < 0:
        impact = "regression"
    else:
        impact = "neutral"

    return OrderedDict(
        [
            ("anchor_id", anchor_id),
            ("baseline_tab_id", choose_text(baseline_tab.get("tab_id"))),
            ("candidate_tab_id", choose_text(candidate_tab.get("tab_id"))),
            ("change_kind", "changed"),
            ("impact", impact),
            ("surface_id", choose_text(candidate_tab.get("surface_id")) or choose_text(baseline_tab.get("surface_id"))),
            ("role", choose_text(candidate_tab.get("role")) or choose_text(baseline_tab.get("role"))),
            ("baseline_title", choose_text(baseline_tab.get("title"))),
            ("candidate_title", choose_text(candidate_tab.get("title"))),
            ("baseline_summary_schema", choose_text(baseline_tab.get("summary_schema"))),
            ("candidate_summary_schema", choose_text(candidate_tab.get("summary_schema"))),
            ("baseline_summary_kind", choose_text(baseline_tab.get("summary_kind"))),
            ("candidate_summary_kind", choose_text(candidate_tab.get("summary_kind"))),
            ("baseline_summary_path", normalize_path(baseline_tab.get("summary_path", ""))),
            ("candidate_summary_path", normalize_path(candidate_tab.get("summary_path", ""))),
            ("baseline_order_index", int(baseline_tab.get("order_index", 0))),
            ("candidate_order_index", int(candidate_tab.get("order_index", 0))),
            ("baseline_capability_ids", list(baseline_tab.get("capability_ids", []))),
            ("candidate_capability_ids", list(candidate_tab.get("capability_ids", []))),
            ("baseline_route_id", choose_text(baseline_tab.get("route_id"))),
            ("candidate_route_id", choose_text(candidate_tab.get("route_id"))),
            ("baseline_primary", bool(baseline_tab.get("primary"))),
            ("candidate_primary", bool(candidate_tab.get("primary"))),
            ("baseline_route_provenance_count", baseline_route_provenance_count),
            ("candidate_route_provenance_count", candidate_route_provenance_count),
            ("baseline_supporting_surface_count", baseline_supporting_surface_count),
            ("candidate_supporting_surface_count", candidate_supporting_surface_count),
            ("metadata_changes", metadata_changes),
            ("path_changes", path_changes),
        ]
    )


def compare_landing_tabs(
    baseline_tabs: list[OrderedDict[str, Any]],
    candidate_tabs: list[OrderedDict[str, Any]],
    baseline_anchor_order: list[str],
    candidate_anchor_order: list[str],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    baseline_map = {tab["anchor_id"]: tab for tab in baseline_tabs}
    candidate_map = {tab["anchor_id"]: tab for tab in candidate_tabs}
    anchor_order = ordered_unique(candidate_anchor_order + baseline_anchor_order)

    tab_changes: list[OrderedDict[str, Any]] = []
    added_count = 0
    removed_count = 0
    regression_count = 0
    improvement_count = 0
    neutral_change_count = 0
    order_changed_count = 0
    capability_alias_changed_count = 0
    primary_flag_changed_count = 0
    path_changed_count = 0

    for anchor_id in anchor_order:
        baseline_tab = baseline_map.get(anchor_id)
        candidate_tab = candidate_map.get(anchor_id)
        change = compare_landing_tab(anchor_id, baseline_tab, candidate_tab)
        if change is None:
            continue

        tab_changes.append(change)
        if change["change_kind"] == "added":
            added_count += 1
        elif change["change_kind"] == "removed":
            removed_count += 1

        if change["impact"] == "regression":
            regression_count += 1
        elif change["impact"] == "improvement":
            improvement_count += 1
        else:
            neutral_change_count += 1

        if change["baseline_order_index"] != change["candidate_order_index"]:
            order_changed_count += 1
        if any(item.startswith("capability_ids:") for item in change["metadata_changes"]):
            capability_alias_changed_count += 1
        if change["baseline_primary"] != change["candidate_primary"]:
            primary_flag_changed_count += 1
        if change["path_changes"]:
            path_changed_count += 1

    tab_summary = OrderedDict(
        [
            ("baseline_tab_count", len(baseline_tabs)),
            ("candidate_tab_count", len(candidate_tabs)),
            ("changed_tab_count", len(tab_changes)),
            ("added_tab_count", added_count),
            ("removed_tab_count", removed_count),
            ("unchanged_tab_count", max(len(anchor_order) - len(tab_changes), 0)),
            ("regression_count", regression_count),
            ("improvement_count", improvement_count),
            ("neutral_change_count", neutral_change_count),
            ("order_changed_count", order_changed_count),
            ("capability_alias_changed_count", capability_alias_changed_count),
            ("primary_flag_changed_count", primary_flag_changed_count),
            ("path_changed_count", path_changed_count),
        ]
    )
    return tab_changes, tab_summary


def compare_query_hint(
    anchor_id: str,
    baseline_query: dict[str, Any] | None,
    candidate_query: dict[str, Any] | None,
) -> OrderedDict[str, Any] | None:
    if baseline_query is None and candidate_query is None:
        return None

    if baseline_query is None:
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_tab_id", None),
                ("candidate_tab_id", choose_text(candidate_query.get("tab_id"))),
                ("change_kind", "added"),
                ("impact", "improvement"),
                ("baseline_tab_title", None),
                ("candidate_tab_title", choose_text(candidate_query.get("tab_title"))),
                ("baseline_scope", None),
                ("candidate_scope", choose_text(candidate_query.get("scope"))),
                ("baseline_selection_rule", None),
                ("candidate_selection_rule", choose_text(candidate_query.get("selection_rule"))),
                ("baseline_query_kind", None),
                ("candidate_query_kind", choose_text(candidate_query.get("query_kind"))),
                ("baseline_compare_expected", None),
                ("candidate_compare_expected", bool(candidate_query.get("compare_expected"))),
                ("baseline_followup_query_kinds", []),
                ("candidate_followup_query_kinds", list(candidate_query.get("followup_query_kinds", []))),
                (
                    "followup_query_changes",
                    string_array_changes([], list(candidate_query.get("followup_query_kinds", []))),
                ),
                ("change_notes", []),
            ]
        )

    if candidate_query is None:
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_tab_id", choose_text(baseline_query.get("tab_id"))),
                ("candidate_tab_id", None),
                ("change_kind", "removed"),
                ("impact", "regression"),
                ("baseline_tab_title", choose_text(baseline_query.get("tab_title"))),
                ("candidate_tab_title", None),
                ("baseline_scope", choose_text(baseline_query.get("scope"))),
                ("candidate_scope", None),
                ("baseline_selection_rule", choose_text(baseline_query.get("selection_rule"))),
                ("candidate_selection_rule", None),
                ("baseline_query_kind", choose_text(baseline_query.get("query_kind"))),
                ("candidate_query_kind", None),
                ("baseline_compare_expected", bool(baseline_query.get("compare_expected"))),
                ("candidate_compare_expected", None),
                ("baseline_followup_query_kinds", list(baseline_query.get("followup_query_kinds", []))),
                ("candidate_followup_query_kinds", []),
                (
                    "followup_query_changes",
                    string_array_changes(list(baseline_query.get("followup_query_kinds", [])), []),
                ),
                ("change_notes", []),
            ]
        )

    change_notes: list[str] = []
    append_scalar_change(
        change_notes,
        "scope",
        choose_text(baseline_query.get("scope")),
        choose_text(candidate_query.get("scope")),
    )
    append_scalar_change(
        change_notes,
        "selection_rule",
        choose_text(baseline_query.get("selection_rule")),
        choose_text(candidate_query.get("selection_rule")),
    )
    append_scalar_change(
        change_notes,
        "query_kind",
        choose_text(baseline_query.get("query_kind")),
        choose_text(candidate_query.get("query_kind")),
    )
    append_scalar_change(
        change_notes,
        "compare_expected",
        bool(baseline_query.get("compare_expected")),
        bool(candidate_query.get("compare_expected")),
    )
    followup_query_changes = string_array_changes(
        list(baseline_query.get("followup_query_kinds", [])),
        list(candidate_query.get("followup_query_kinds", [])),
    )
    if has_array_changes(followup_query_changes):
        change_notes.append(
            "followup_query_kinds:+[{0}] -[{1}]".format(
                ", ".join(followup_query_changes["added"]),
                ", ".join(followup_query_changes["removed"]),
            )
        )

    if not change_notes:
        return None

    score = 0
    baseline_compare_expected = bool(baseline_query.get("compare_expected"))
    candidate_compare_expected = bool(candidate_query.get("compare_expected"))
    if baseline_compare_expected and not candidate_compare_expected:
        score -= 2
    elif not baseline_compare_expected and candidate_compare_expected:
        score += 2

    baseline_scope = choose_text(baseline_query.get("scope"))
    candidate_scope = choose_text(candidate_query.get("scope"))
    if baseline_scope == "artifact_root" and candidate_scope == "report":
        score -= 1
    elif baseline_scope == "report" and candidate_scope == "artifact_root" and candidate_compare_expected:
        score += 1

    followup_delta = len(followup_query_changes["added"]) - len(followup_query_changes["removed"])
    if followup_delta > 0:
        score += 1
    elif followup_delta < 0:
        score -= 1

    if score > 0:
        impact = "improvement"
    elif score < 0:
        impact = "regression"
    else:
        impact = "neutral"

    return OrderedDict(
        [
            ("anchor_id", anchor_id),
            ("baseline_tab_id", choose_text(baseline_query.get("tab_id"))),
            ("candidate_tab_id", choose_text(candidate_query.get("tab_id"))),
            ("change_kind", "changed"),
            ("impact", impact),
            ("baseline_tab_title", choose_text(baseline_query.get("tab_title"))),
            ("candidate_tab_title", choose_text(candidate_query.get("tab_title"))),
            ("baseline_scope", baseline_scope),
            ("candidate_scope", candidate_scope),
            ("baseline_selection_rule", choose_text(baseline_query.get("selection_rule"))),
            ("candidate_selection_rule", choose_text(candidate_query.get("selection_rule"))),
            ("baseline_query_kind", choose_text(baseline_query.get("query_kind"))),
            ("candidate_query_kind", choose_text(candidate_query.get("query_kind"))),
            ("baseline_compare_expected", baseline_compare_expected),
            ("candidate_compare_expected", candidate_compare_expected),
            ("baseline_followup_query_kinds", list(baseline_query.get("followup_query_kinds", []))),
            ("candidate_followup_query_kinds", list(candidate_query.get("followup_query_kinds", []))),
            ("followup_query_changes", followup_query_changes),
            ("change_notes", change_notes),
        ]
    )


def compare_query_hints(
    baseline_queries: list[OrderedDict[str, Any]],
    candidate_queries: list[OrderedDict[str, Any]],
    baseline_anchor_order: list[str],
    candidate_anchor_order: list[str],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    baseline_map = {choose_text(query.get("tab_id")): query for query in baseline_queries if choose_text(query.get("tab_id"))}
    candidate_map = {choose_text(query.get("tab_id")): query for query in candidate_queries if choose_text(query.get("tab_id"))}
    anchor_order = ordered_unique(candidate_anchor_order + baseline_anchor_order)

    query_changes: list[OrderedDict[str, Any]] = []
    added_count = 0
    removed_count = 0
    regression_count = 0
    improvement_count = 0
    neutral_change_count = 0
    scope_changed_count = 0
    selection_rule_changed_count = 0
    query_kind_changed_count = 0
    compare_expectation_changed_count = 0
    followup_changed_count = 0

    for anchor_id in anchor_order:
        baseline_query = baseline_map.get(anchor_id)
        candidate_query = candidate_map.get(anchor_id)
        change = compare_query_hint(anchor_id, baseline_query, candidate_query)
        if change is None:
            continue

        query_changes.append(change)
        if change["change_kind"] == "added":
            added_count += 1
        elif change["change_kind"] == "removed":
            removed_count += 1

        if change["impact"] == "regression":
            regression_count += 1
        elif change["impact"] == "improvement":
            improvement_count += 1
        else:
            neutral_change_count += 1

        if change["change_kind"] == "changed":
            if change["baseline_scope"] != change["candidate_scope"]:
                scope_changed_count += 1
            if change["baseline_selection_rule"] != change["candidate_selection_rule"]:
                selection_rule_changed_count += 1
            if change["baseline_query_kind"] != change["candidate_query_kind"]:
                query_kind_changed_count += 1
            if change["baseline_compare_expected"] != change["candidate_compare_expected"]:
                compare_expectation_changed_count += 1
            if has_array_changes(change["followup_query_changes"]):
                followup_changed_count += 1

    query_summary = OrderedDict(
        [
            ("baseline_query_tab_count", len(baseline_queries)),
            ("candidate_query_tab_count", len(candidate_queries)),
            ("changed_query_count", len(query_changes)),
            ("added_query_count", added_count),
            ("removed_query_count", removed_count),
            ("unchanged_query_count", max(len(anchor_order) - len(query_changes), 0)),
            ("regression_count", regression_count),
            ("improvement_count", improvement_count),
            ("neutral_change_count", neutral_change_count),
            ("scope_changed_count", scope_changed_count),
            ("selection_rule_changed_count", selection_rule_changed_count),
            ("query_kind_changed_count", query_kind_changed_count),
            ("compare_expectation_changed_count", compare_expectation_changed_count),
            ("followup_changed_count", followup_changed_count),
            ("primary_query_change_count", 0),
        ]
    )
    return query_changes, query_summary


def build_landing_regression_surface(
    landing_status: dict[str, Any],
    landing_changes: dict[str, Any],
    tab_changes: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    removed_tab_ids = list(landing_changes["available_tab_changes"]["removed"])
    lost_direct_modes = list(landing_changes["direct_capability_changes"]["removed"])
    downgraded_tier = ENTRY_TIER_RANK.get(choose_text(landing_status.get("candidate_entry_tier")), -1) < ENTRY_TIER_RANK.get(
        choose_text(landing_status.get("baseline_entry_tier")),
        -1,
    )
    candidate_tab_ids = set(landing_status.get("candidate_available_tab_ids", []))
    baseline_primary_tab_id = choose_text(landing_status.get("baseline_primary_tab_id"))
    missing_primary_tab_id = (
        baseline_primary_tab_id
        if baseline_primary_tab_id and baseline_primary_tab_id not in candidate_tab_ids
        else None
    )
    regressed_tabs = [
        choose_text(change.get("candidate_tab_id")) or choose_text(change.get("baseline_tab_id"))
        for change in tab_changes
        if change["impact"] == "regression"
    ]
    provenance_root_detail_changed_ids = [
        choose_text(change.get("root_id"))
        for change in landing_changes["provenance_root_detail_changes"]
        if choose_text(change.get("root_id"))
    ]
    affected_tab_ids = ordered_unique(
        removed_tab_ids + regressed_tabs + ([missing_primary_tab_id] if missing_primary_tab_id else [])
    )

    narratives: list[str] = []
    if removed_tab_ids:
        narratives.append("candidate landing no longer exposes tab(s): `{0}`".format("`, `".join(removed_tab_ids)))
    if lost_direct_modes:
        narratives.append("candidate landing lost direct mode(s): `{0}`".format("`, `".join(lost_direct_modes)))
    if downgraded_tier:
        narratives.append(
            "entry tier regressed `{0}` -> `{1}`".format(
                choose_text(landing_status.get("baseline_entry_tier")),
                choose_text(landing_status.get("candidate_entry_tier")),
            )
        )
    if missing_primary_tab_id:
        narratives.append(f"baseline primary tab `{missing_primary_tab_id}` is no longer reachable")
    for change in [item for item in tab_changes if item["impact"] == "regression"][:3]:
        narratives.append(
            "landing tab `{0}` regressed with `capabilities {1} -> {2}`".format(
                choose_text(change.get("candidate_tab_id")) or choose_text(change.get("baseline_tab_id")),
                ", ".join(change.get("baseline_capability_ids", [])),
                ", ".join(change.get("candidate_capability_ids", [])),
            )
        )
    for root_id in provenance_root_detail_changed_ids[:3]:
        narratives.append(f"provenance root `{root_id}` changed source detail")

    return OrderedDict(
        [
            (
                "changed",
                bool(
                    removed_tab_ids
                    or lost_direct_modes
                    or downgraded_tier
                    or missing_primary_tab_id
                    or regressed_tabs
                    or provenance_root_detail_changed_ids
                ),
            ),
            ("removed_tab_ids", removed_tab_ids),
            ("lost_direct_modes", lost_direct_modes),
            ("missing_primary_tab_id", missing_primary_tab_id),
            ("downgraded_tier", downgraded_tier),
            ("regressed_tabs", regressed_tabs),
            ("provenance_root_detail_changed_ids", provenance_root_detail_changed_ids),
            ("affected_tab_ids", affected_tab_ids),
            ("narratives", narratives),
        ]
    )


def build_query_regression_surface(
    primary_query_status: dict[str, Any],
    query_plan_changes: dict[str, Any],
    query_changes: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    removed_query_tab_ids = list(query_plan_changes["available_query_tab_changes"]["removed"])
    lost_compare_expected_tab_ids = ordered_unique(
        [
            choose_text(change.get("baseline_tab_id")) or choose_text(change.get("candidate_tab_id"))
            for change in query_changes
            if change["change_kind"] == "changed"
            and bool(change.get("baseline_compare_expected"))
            and not bool(change.get("candidate_compare_expected"))
        ]
    )
    primary_query_missing = bool(primary_query_status.get("baseline_present")) and not bool(
        primary_query_status.get("candidate_present")
    )
    primary_query_scope_narrowed = (
        choose_text(primary_query_status.get("baseline_scope")) == "artifact_root"
        and choose_text(primary_query_status.get("candidate_scope")) == "report"
    )
    primary_query_compare_expected_regressed = bool(primary_query_status.get("baseline_compare_expected")) and not bool(
        primary_query_status.get("candidate_compare_expected")
    )
    regressed_query_tabs = ordered_unique(
        [
            choose_text(change.get("candidate_tab_id")) or choose_text(change.get("baseline_tab_id"))
            for change in query_changes
            if change["impact"] == "regression"
        ]
    )
    primary_query_regressed = (
        primary_query_missing or primary_query_scope_narrowed or primary_query_compare_expected_regressed
    )
    primary_tab_id = choose_text(primary_query_status.get("baseline_tab_id")) or choose_text(
        primary_query_status.get("candidate_tab_id")
    )
    affected_tab_ids = ordered_unique(
        removed_query_tab_ids
        + lost_compare_expected_tab_ids
        + regressed_query_tabs
        + ([primary_tab_id] if primary_query_regressed and primary_tab_id else [])
    )

    narratives: list[str] = []
    if removed_query_tab_ids:
        narratives.append("candidate landing no longer carries query plan(s) for tab(s): `{0}`".format("`, `".join(removed_query_tab_ids)))
    if primary_query_missing:
        narratives.append("candidate landing no longer exposes a primary explain opening query")
    if primary_query_scope_narrowed:
        narratives.append(
            "primary explain opening narrowed `{0}` -> `{1}`".format(
                choose_text(primary_query_status.get("baseline_scope")),
                choose_text(primary_query_status.get("candidate_scope")),
            )
        )
    if primary_query_compare_expected_regressed:
        narratives.append("primary explain opening no longer advertises compare-aware overview semantics")
    if lost_compare_expected_tab_ids:
        narratives.append(
            "query plan(s) lost compare-aware semantics for tab(s): `{0}`".format(
                "`, `".join(lost_compare_expected_tab_ids)
            )
        )
    for change in [item for item in query_changes if item["impact"] == "regression"][:3]:
        narratives.append(
            "query opening for `{0}` regressed with `{1}`".format(
                choose_text(change.get("candidate_tab_id")) or choose_text(change.get("baseline_tab_id")),
                "; ".join(change.get("change_notes", [])) or "changed explain opening semantics",
            )
        )

    return OrderedDict(
        [
            (
                "changed",
                bool(
                    removed_query_tab_ids
                    or lost_compare_expected_tab_ids
                    or primary_query_regressed
                    or regressed_query_tabs
                ),
            ),
            ("removed_query_tab_ids", removed_query_tab_ids),
            ("lost_compare_expected_tab_ids", lost_compare_expected_tab_ids),
            ("primary_query_missing", primary_query_missing),
            ("primary_query_scope_narrowed", primary_query_scope_narrowed),
            ("primary_query_compare_expected_regressed", primary_query_compare_expected_regressed),
            ("regressed_query_tabs", regressed_query_tabs),
            ("affected_tab_ids", affected_tab_ids),
            ("narratives", narratives),
        ]
    )


def determine_landing_verdict(
    candidate_summary: dict[str, Any],
    landing_status: dict[str, Any],
    landing_changes: dict[str, Any],
    query_plan_changes: dict[str, Any],
    tab_summary: dict[str, int],
    query_summary: dict[str, int],
    landing_regression_surface: dict[str, Any],
    query_regression_surface: dict[str, Any],
) -> str:
    if choose_text(candidate_summary.get("result")) != "ok":
        return "collapsed"
    if (
        landing_regression_surface["changed"]
        or query_regression_surface["changed"]
        or tab_summary["regression_count"] > 0
        or query_summary["regression_count"] > 0
    ):
        return "drifted"

    baseline_tier_rank = ENTRY_TIER_RANK.get(choose_text(landing_status.get("baseline_entry_tier")), -1)
    candidate_tier_rank = ENTRY_TIER_RANK.get(choose_text(landing_status.get("candidate_entry_tier")), -1)
    if candidate_tier_rank > baseline_tier_rank:
        return "improved"
    if tab_summary["improvement_count"] > 0:
        return "improved"
    if query_summary["improvement_count"] > 0:
        return "improved"
    if landing_changes["available_tab_changes"]["added"]:
        return "improved"
    if landing_changes["direct_capability_changes"]["added"]:
        return "improved"
    if landing_changes["provenance_root_changes"]["added"]:
        return "improved"
    if query_plan_changes["available_query_tab_changes"]["added"]:
        return "improved"
    if landing_changes["changed"] or query_plan_changes["changed"] or tab_summary["neutral_change_count"] > 0 or query_summary["neutral_change_count"] > 0:
        return "drifted"
    return "standing"


def build_questions(
    landing_status: dict[str, Any],
    landing_changes: dict[str, Any],
    query_plan_changes: dict[str, Any],
    query_summary: dict[str, int],
    landing_verdict: str,
    query_regression_surface: dict[str, Any],
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    if bool(landing_changes.get("primary_tab_changed")):
        compare_questions.append("Did the default explain landing tab change?")
    if bool(query_plan_changes.get("primary_query_changed")):
        compare_questions.append("Did the default explain opening query change?")
    if has_array_changes(landing_changes["direct_capability_changes"]):
        compare_questions.append("Did the landing gain or lose any direct explain mode?")
    if has_array_changes(landing_changes["provenance_root_changes"]):
        compare_questions.append("Did the landing expose new provenance roots?")
    if landing_changes["provenance_root_detail_changes"]:
        compare_questions.append("Did any provenance root keep the same id but change source detail?")
    if has_array_changes(query_plan_changes["available_query_tab_changes"]) or query_summary["changed_query_count"] > 0:
        compare_questions.append("Did any landing tab change its explain opening plan?")
    if not compare_questions:
        compare_questions.append("Does the candidate landing still match the baseline open plan?")

    next_questions: list[str] = []
    for mode in landing_changes["direct_capability_changes"]["removed"][:2]:
        next_questions.append(f"How do we restore direct `{mode}` entry on the candidate landing?")
    for tab_id in landing_changes["available_tab_changes"]["removed"][:2]:
        next_questions.append(f"Which landing should bring back tab `{tab_id}`?")
    for tab_id in query_regression_surface["removed_query_tab_ids"][:2]:
        next_questions.append(f"Which explain opening should replace removed query plan `{tab_id}`?")
    for tab_id in query_regression_surface["regressed_query_tabs"][:2]:
        next_questions.append(f"Which query should open tab `{tab_id}` by default now?")
    if query_regression_surface["primary_query_compare_expected_regressed"]:
        next_questions.append("Should the candidate primary opening recover compare-aware overview semantics?")
    for root_id in landing_changes["provenance_root_changes"]["added"][:2]:
        next_questions.append(f"Should provenance root `{root_id}` become a first-class explain follow-on world?")
    for change in landing_changes["provenance_root_detail_changes"][:2]:
        root_id = choose_text(change.get("root_id"))
        if root_id:
            next_questions.append(f"Should provenance root `{root_id}` still be treated as the same follow-on world?")
    if not next_questions and landing_verdict == "improved":
        next_questions.append("Which richer landing should become the next default consumer baseline?")
    if not next_questions:
        next_tab_id = (
            choose_text(landing_status.get("candidate_primary_tab_id"))
            or choose_text(landing_status.get("baseline_primary_tab_id"))
            or "this entry"
        )
        next_questions.append(f"What query should open `{next_tab_id}` by default?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_compare_summary_model(
    baseline_landing_path: Path,
    candidate_landing_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_summary = load_landing_summary(baseline_landing_path)
    candidate_summary = load_landing_summary(candidate_landing_path)
    baseline_tabs, baseline_anchor_order = normalize_landing_tabs(baseline_summary)
    candidate_tabs, candidate_anchor_order = normalize_landing_tabs(candidate_summary)
    _, baseline_queries, baseline_query_anchor_order = normalize_query_hints(baseline_summary)
    _, candidate_queries, candidate_query_anchor_order = normalize_query_hints(candidate_summary)
    landing_status = build_landing_status(baseline_summary, candidate_summary, baseline_tabs, candidate_tabs)
    landing_changes = build_landing_changes(baseline_summary, candidate_summary, landing_status)
    primary_query_status = build_primary_query_status(baseline_summary, candidate_summary)
    query_plan_changes = build_query_plan_changes(
        baseline_summary,
        candidate_summary,
        primary_query_status,
    )
    tab_changes, tab_summary = compare_landing_tabs(
        baseline_tabs,
        candidate_tabs,
        baseline_anchor_order,
        candidate_anchor_order,
    )
    query_changes, query_summary = compare_query_hints(
        baseline_queries,
        candidate_queries,
        baseline_query_anchor_order,
        candidate_query_anchor_order,
    )
    query_summary["primary_query_change_count"] = 1 if query_plan_changes["primary_query_changed"] else 0
    landing_regression_surface = build_landing_regression_surface(landing_status, landing_changes, tab_changes)
    query_regression_surface = build_query_regression_surface(
        primary_query_status,
        query_plan_changes,
        query_changes,
    )
    landing_verdict = determine_landing_verdict(
        candidate_summary,
        landing_status,
        landing_changes,
        query_plan_changes,
        tab_summary,
        query_summary,
        landing_regression_surface,
        query_regression_surface,
    )
    questions = build_questions(
        landing_status,
        landing_changes,
        query_plan_changes,
        query_summary,
        landing_verdict,
        query_regression_surface,
    )

    supporting_surfaces = [
        build_front_page_surface(
            landing_summary=baseline_summary,
            landing_summary_path=baseline_landing_path,
            surface_id="baseline_landing",
            role="baseline_landing",
        ),
        build_front_page_surface(
            landing_summary=candidate_summary,
            landing_summary_path=candidate_landing_path,
            surface_id="candidate_landing",
            role="candidate_landing",
        ),
    ]
    landing_provenance = [
        build_landing_provenance_entry(
            landing_summary=baseline_summary,
            landing_summary_path=baseline_landing_path,
            provenance_id="baseline_landing",
            landing_role="baseline_landing",
        ),
        build_landing_provenance_entry(
            landing_summary=candidate_summary,
            landing_summary_path=candidate_landing_path,
            provenance_id="candidate_landing",
            landing_role="candidate_landing",
        ),
    ]

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_entry_landing_compare/v0"),
            ("kind", "system_compiler.front_page_entry_landing_compare"),
            ("generator", "scripts/compare_system_compiler_front_page_entry_landing.py"),
            ("result", "ok"),
            (
                "landing_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Landing Compare"),
                        (
                            "summary",
                            "A consumer-side comparison that asks how the default explain landing plan and opening query plan change between two landing summaries.",
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
            ("landing_provenance", landing_provenance),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_landing_summary_path", normalize_path(baseline_landing_path)),
                        ("candidate_landing_summary_path", normalize_path(candidate_landing_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("landing_verdict", landing_verdict),
            ("landing_status", landing_status),
            ("primary_query_status", primary_query_status),
            ("landing_changes", landing_changes),
            ("query_plan_changes", query_plan_changes),
            ("query_summary", query_summary),
            ("query_changes", query_changes),
            ("tab_summary", tab_summary),
            ("tab_changes", tab_changes),
            ("landing_regression_surface", landing_regression_surface),
            ("query_regression_surface", query_regression_surface),
            ("questions", questions),
            ("violations", []),
        ]
    )
