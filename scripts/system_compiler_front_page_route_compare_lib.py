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


ROUTE_SCHEMA = "system_compiler.front_page_route/v0"
ROUTE_KIND = "system_compiler.front_page_route"


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


def build_count_change_map(
    baseline_counts: dict[str, int], candidate_counts: dict[str, int]
) -> OrderedDict[str, OrderedDict[str, int]]:
    result: OrderedDict[str, OrderedDict[str, int]] = OrderedDict()
    keys = sorted(set(baseline_counts) | set(candidate_counts))
    for key in keys:
        baseline_value = int(baseline_counts.get(key, 0))
        candidate_value = int(candidate_counts.get(key, 0))
        if baseline_value == candidate_value:
            continue
        result[key] = OrderedDict(
            [
                ("baseline", baseline_value),
                ("candidate", candidate_value),
                ("delta", candidate_value - baseline_value),
            ]
        )
    return result


def ensure_route_summary(route_summary: dict[str, Any], route_summary_path: Path) -> dict[str, Any]:
    if choose_text(route_summary.get("schema")) != ROUTE_SCHEMA:
        raise ValueError(f"unsupported front page route schema: {route_summary_path}")
    if choose_text(route_summary.get("kind")) != ROUTE_KIND:
        raise ValueError(f"unsupported front page route kind: {route_summary_path}")
    return route_summary


def load_route_summary(route_summary_path: Path) -> dict[str, Any]:
    return ensure_route_summary(load_json(route_summary_path), route_summary_path)


def build_front_page_surface(
    route_summary: dict[str, Any],
    route_summary_path: Path,
    surface_id: str,
    role: str,
) -> OrderedDict[str, str]:
    root_surface = get_mapping(route_summary.get("root_surface"))
    artifact_context = get_mapping(route_summary.get("artifact_context"))
    root_label = choose_text(root_surface.get("label")) or "front page route"
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", f"{role.replace('_', ' ')}: {root_label}"),
            ("role", role),
            ("summary_schema", ROUTE_SCHEMA),
            ("summary_path", normalize_path(route_summary_path)),
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


def build_route_provenance_entry(
    route_summary: dict[str, Any],
    route_summary_path: Path,
    route_id: str,
) -> OrderedDict[str, Any]:
    artifact_context = get_mapping(route_summary.get("artifact_context"))
    root_surface = get_mapping(route_summary.get("root_surface"))
    route_entries = route_summary.get("route_entries", [])
    level1_surface_ids = ordered_unique(
        [
            choose_text(entry.get("surface_id"))
            for entry in route_entries
            if isinstance(entry, dict) and int(entry.get("depth", 0)) == 1
        ]
    )
    return OrderedDict(
        [
            ("id", route_id),
            ("route_kind", "front_page_route_root"),
            ("source_summary_schema", ROUTE_SCHEMA),
            ("source_summary_path", normalize_path(route_summary_path)),
            ("source_input_summary_path", normalize_path(artifact_context.get("input_summary_path", ""))),
            ("source_root_summary_path", normalize_path(root_surface.get("summary_path", ""))),
            ("source_report_markdown_path", normalize_path(artifact_context.get("report_markdown_path", ""))),
            ("source_check_text_path", normalize_path(artifact_context.get("check_text_path", ""))),
            ("level1_surface_ids", level1_surface_ids),
        ]
    )


def normalize_route_entries(route_summary: dict[str, Any]) -> tuple[list[OrderedDict[str, Any]], list[str]]:
    route_entries = route_summary.get("route_entries", [])
    anchor_by_route_id: dict[str, str] = {}
    sibling_occurrence_count: dict[tuple[str, str], int] = {}
    normalized_entries: list[OrderedDict[str, Any]] = []
    anchor_order: list[str] = []

    for entry_value in route_entries:
        entry = get_mapping(entry_value)
        route_id = choose_text(entry.get("route_id"))
        parent_route_id = nullable_text(entry.get("parent_route_id"))
        if not route_id:
            continue

        if parent_route_id is None:
            anchor_id = "root"
            parent_anchor_id = None
        else:
            parent_anchor_id = anchor_by_route_id.get(parent_route_id)
            if parent_anchor_id is None:
                raise ValueError(f"parent route id not found: {parent_route_id}")
            segment = "|".join(
                [
                    choose_text(entry.get("surface_id")),
                    choose_text(entry.get("role")),
                    choose_text(entry.get("declared_summary_schema")),
                ]
            )
            occurrence_key = (parent_anchor_id, segment)
            occurrence = sibling_occurrence_count.get(occurrence_key, 0) + 1
            sibling_occurrence_count[occurrence_key] = occurrence
            segment_token = segment if occurrence == 1 else f"{segment}#{occurrence}"
            anchor_id = f"{parent_anchor_id} > {segment_token}"

        anchor_by_route_id[route_id] = anchor_id
        anchor_order.append(anchor_id)

        normalized_entries.append(
            OrderedDict(
                [
                    ("anchor_id", anchor_id),
                    ("route_id", route_id),
                    ("parent_anchor_id", parent_anchor_id),
                    ("parent_route_id", parent_route_id),
                    ("depth", int(entry.get("depth", 0))),
                    ("surface_id", choose_text(entry.get("surface_id"))),
                    ("label", choose_text(entry.get("label"))),
                    ("role", choose_text(entry.get("role"))),
                    ("declared_summary_schema", choose_text(entry.get("declared_summary_schema"))),
                    ("summary_schema", choose_text(entry.get("summary_schema"))),
                    ("summary_kind", choose_text(entry.get("summary_kind"))),
                    ("summary_path", normalize_path(entry.get("summary_path", ""))),
                    ("report_markdown_path", normalize_path(entry.get("report_markdown_path", ""))),
                    ("check_text_path", normalize_path(entry.get("check_text_path", ""))),
                    ("supporting_surface_count", int(entry.get("supporting_surface_count", 0))),
                    ("revisit", bool(entry.get("revisit"))),
                    ("cycle", bool(entry.get("cycle"))),
                    ("expanded", bool(entry.get("expanded"))),
                    ("first_route_id", nullable_text(entry.get("first_route_id"))),
                ]
            )
        )

    for normalized_entry in normalized_entries:
        first_route_id = normalized_entry.pop("first_route_id")
        normalized_entry["first_entry_anchor"] = anchor_by_route_id.get(first_route_id) if first_route_id else None

    return normalized_entries, anchor_order


def build_route_status(
    baseline_route: dict[str, Any],
    candidate_route: dict[str, Any],
    baseline_entries: list[OrderedDict[str, Any]],
    candidate_entries: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    baseline_route_summary = get_mapping(baseline_route.get("route_summary"))
    candidate_route_summary = get_mapping(candidate_route.get("route_summary"))
    baseline_root = get_mapping(baseline_route.get("root_surface"))
    candidate_root = get_mapping(candidate_route.get("root_surface"))

    baseline_level1_surface_ids = [
        choose_text(entry.get("surface_id"))
        for entry in baseline_entries
        if int(entry.get("depth", 0)) == 1
    ]
    candidate_level1_surface_ids = [
        choose_text(entry.get("surface_id"))
        for entry in candidate_entries
        if int(entry.get("depth", 0)) == 1
    ]
    baseline_level1_roles = [choose_text(entry.get("role")) for entry in baseline_entries if int(entry.get("depth", 0)) == 1]
    candidate_level1_roles = [choose_text(entry.get("role")) for entry in candidate_entries if int(entry.get("depth", 0)) == 1]

    return OrderedDict(
        [
            ("baseline_result", choose_text(baseline_route.get("result"))),
            ("candidate_result", choose_text(candidate_route.get("result"))),
            ("baseline_root_label", choose_text(baseline_root.get("label"))),
            ("candidate_root_label", choose_text(candidate_root.get("label"))),
            ("baseline_root_summary_schema", choose_text(baseline_root.get("summary_schema"))),
            ("candidate_root_summary_schema", choose_text(candidate_root.get("summary_schema"))),
            ("baseline_root_summary_kind", choose_text(baseline_root.get("summary_kind"))),
            ("candidate_root_summary_kind", choose_text(candidate_root.get("summary_kind"))),
            ("baseline_level1_surface_ids", baseline_level1_surface_ids),
            ("candidate_level1_surface_ids", candidate_level1_surface_ids),
            ("baseline_level1_roles", baseline_level1_roles),
            ("candidate_level1_roles", candidate_level1_roles),
            ("baseline_entry_count", int(baseline_route_summary.get("entry_count", 0))),
            ("candidate_entry_count", int(candidate_route_summary.get("entry_count", 0))),
            ("baseline_unique_summary_count", int(baseline_route_summary.get("unique_summary_count", 0))),
            ("candidate_unique_summary_count", int(candidate_route_summary.get("unique_summary_count", 0))),
            ("baseline_repeated_entry_count", int(baseline_route_summary.get("repeated_entry_count", 0))),
            ("candidate_repeated_entry_count", int(candidate_route_summary.get("repeated_entry_count", 0))),
            ("baseline_cycle_entry_count", int(baseline_route_summary.get("cycle_entry_count", 0))),
            ("candidate_cycle_entry_count", int(candidate_route_summary.get("cycle_entry_count", 0))),
            ("baseline_leaf_entry_count", int(baseline_route_summary.get("leaf_entry_count", 0))),
            ("candidate_leaf_entry_count", int(candidate_route_summary.get("leaf_entry_count", 0))),
            ("baseline_expanded_entry_count", int(baseline_route_summary.get("expanded_entry_count", 0))),
            ("candidate_expanded_entry_count", int(candidate_route_summary.get("expanded_entry_count", 0))),
            ("baseline_max_depth", int(baseline_route_summary.get("max_depth", 0))),
            ("candidate_max_depth", int(candidate_route_summary.get("max_depth", 0))),
        ]
    )


def build_route_changes(
    baseline_route: dict[str, Any],
    candidate_route: dict[str, Any],
    baseline_entries: list[OrderedDict[str, Any]],
    candidate_entries: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    baseline_root = get_mapping(baseline_route.get("root_surface"))
    candidate_root = get_mapping(candidate_route.get("root_surface"))
    baseline_level1_surface_ids = [entry["surface_id"] for entry in baseline_entries if int(entry["depth"]) == 1]
    candidate_level1_surface_ids = [entry["surface_id"] for entry in candidate_entries if int(entry["depth"]) == 1]
    baseline_level1_roles = [entry["role"] for entry in baseline_entries if int(entry["depth"]) == 1]
    candidate_level1_roles = [entry["role"] for entry in candidate_entries if int(entry["depth"]) == 1]

    baseline_surface_ids = ordered_unique([entry["surface_id"] for entry in baseline_entries if entry["surface_id"] != "root"])
    candidate_surface_ids = ordered_unique([entry["surface_id"] for entry in candidate_entries if entry["surface_id"] != "root"])
    baseline_role_ids = ordered_unique([entry["role"] for entry in baseline_entries if entry["role"] != "root"])
    candidate_role_ids = ordered_unique([entry["role"] for entry in candidate_entries if entry["role"] != "root"])

    baseline_cycle_anchors = [entry["anchor_id"] for entry in baseline_entries if bool(entry["cycle"])]
    candidate_cycle_anchors = [entry["anchor_id"] for entry in candidate_entries if bool(entry["cycle"])]
    baseline_revisit_anchors = [entry["anchor_id"] for entry in baseline_entries if bool(entry["revisit"])]
    candidate_revisit_anchors = [entry["anchor_id"] for entry in candidate_entries if bool(entry["revisit"])]
    baseline_expanded_anchors = [entry["anchor_id"] for entry in baseline_entries if bool(entry["expanded"])]
    candidate_expanded_anchors = [entry["anchor_id"] for entry in candidate_entries if bool(entry["expanded"])]

    key_surface_ids = ["runtime_evidence", "witness_bundle", "biography", "world_compare", "world_shelf_review"]
    baseline_key_surfaces = [surface_id for surface_id in key_surface_ids if surface_id in baseline_surface_ids]
    candidate_key_surfaces = [surface_id for surface_id in key_surface_ids if surface_id in candidate_surface_ids]

    route_changes = OrderedDict(
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
            ("level1_surface_changes", string_array_changes(baseline_level1_surface_ids, candidate_level1_surface_ids)),
            ("level1_role_changes", string_array_changes(baseline_level1_roles, candidate_level1_roles)),
            ("level1_order_changed", baseline_level1_surface_ids != candidate_level1_surface_ids),
            ("reachable_surface_changes", string_array_changes(baseline_surface_ids, candidate_surface_ids)),
            ("reachable_role_changes", string_array_changes(baseline_role_ids, candidate_role_ids)),
            ("key_surface_changes", string_array_changes(baseline_key_surfaces, candidate_key_surfaces)),
            (
                "schema_count_changes",
                build_count_change_map(
                    get_mapping(baseline_route.get("schema_counts")),
                    get_mapping(candidate_route.get("schema_counts")),
                ),
            ),
            (
                "role_count_changes",
                build_count_change_map(
                    get_mapping(baseline_route.get("role_counts")),
                    get_mapping(candidate_route.get("role_counts")),
                ),
            ),
            ("cycle_anchor_changes", string_array_changes(baseline_cycle_anchors, candidate_cycle_anchors)),
            ("revisit_anchor_changes", string_array_changes(baseline_revisit_anchors, candidate_revisit_anchors)),
            ("expanded_anchor_changes", string_array_changes(baseline_expanded_anchors, candidate_expanded_anchors)),
        ]
    )

    route_changes["changed"] = any(
        [
            bool(route_changes["root_label_changed"]),
            bool(route_changes["root_schema_changed"]),
            bool(route_changes["root_kind_changed"]),
            has_array_changes(route_changes["level1_surface_changes"]),
            has_array_changes(route_changes["level1_role_changes"]),
            bool(route_changes["level1_order_changed"]),
            has_array_changes(route_changes["reachable_surface_changes"]),
            has_array_changes(route_changes["reachable_role_changes"]),
            has_array_changes(route_changes["key_surface_changes"]),
            bool(route_changes["schema_count_changes"]),
            bool(route_changes["role_count_changes"]),
            has_array_changes(route_changes["cycle_anchor_changes"]),
            has_array_changes(route_changes["revisit_anchor_changes"]),
            has_array_changes(route_changes["expanded_anchor_changes"]),
        ]
    )
    return route_changes


def append_scalar_change(changes: list[str], label: str, baseline_value: Any, candidate_value: Any) -> None:
    if baseline_value != candidate_value:
        changes.append(f"{label}:{baseline_value}->{candidate_value}")


def compare_route_entry(
    anchor_id: str,
    baseline_entry: dict[str, Any] | None,
    candidate_entry: dict[str, Any] | None,
) -> OrderedDict[str, Any] | None:
    if baseline_entry is None and candidate_entry is None:
        return None

    if baseline_entry is None:
        surface_id = choose_text(candidate_entry.get("surface_id"))
        role = choose_text(candidate_entry.get("role"))
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_route_id", None),
                ("candidate_route_id", choose_text(candidate_entry.get("route_id"))),
                ("change_kind", "added"),
                ("impact", "improvement"),
                ("surface_id", surface_id),
                ("role", role),
                ("baseline_label", None),
                ("candidate_label", choose_text(candidate_entry.get("label"))),
                ("baseline_summary_schema", None),
                ("candidate_summary_schema", choose_text(candidate_entry.get("summary_schema"))),
                ("baseline_summary_kind", None),
                ("candidate_summary_kind", choose_text(candidate_entry.get("summary_kind"))),
                ("baseline_summary_path", None),
                ("candidate_summary_path", normalize_path(candidate_entry.get("summary_path", ""))),
                ("baseline_depth", None),
                ("candidate_depth", int(candidate_entry.get("depth", 0))),
                ("baseline_revisit", None),
                ("candidate_revisit", bool(candidate_entry.get("revisit"))),
                ("baseline_cycle", None),
                ("candidate_cycle", bool(candidate_entry.get("cycle"))),
                ("baseline_expanded", None),
                ("candidate_expanded", bool(candidate_entry.get("expanded"))),
                ("baseline_supporting_surface_count", None),
                ("candidate_supporting_surface_count", int(candidate_entry.get("supporting_surface_count", 0))),
                ("baseline_first_entry_anchor", None),
                ("candidate_first_entry_anchor", nullable_text(candidate_entry.get("first_entry_anchor"))),
                ("metadata_changes", []),
                ("path_changes", []),
            ]
        )

    if candidate_entry is None:
        surface_id = choose_text(baseline_entry.get("surface_id"))
        role = choose_text(baseline_entry.get("role"))
        return OrderedDict(
            [
                ("anchor_id", anchor_id),
                ("baseline_route_id", choose_text(baseline_entry.get("route_id"))),
                ("candidate_route_id", None),
                ("change_kind", "removed"),
                ("impact", "regression"),
                ("surface_id", surface_id),
                ("role", role),
                ("baseline_label", choose_text(baseline_entry.get("label"))),
                ("candidate_label", None),
                ("baseline_summary_schema", choose_text(baseline_entry.get("summary_schema"))),
                ("candidate_summary_schema", None),
                ("baseline_summary_kind", choose_text(baseline_entry.get("summary_kind"))),
                ("candidate_summary_kind", None),
                ("baseline_summary_path", normalize_path(baseline_entry.get("summary_path", ""))),
                ("candidate_summary_path", None),
                ("baseline_depth", int(baseline_entry.get("depth", 0))),
                ("candidate_depth", None),
                ("baseline_revisit", bool(baseline_entry.get("revisit"))),
                ("candidate_revisit", None),
                ("baseline_cycle", bool(baseline_entry.get("cycle"))),
                ("candidate_cycle", None),
                ("baseline_expanded", bool(baseline_entry.get("expanded"))),
                ("candidate_expanded", None),
                ("baseline_supporting_surface_count", int(baseline_entry.get("supporting_surface_count", 0))),
                ("candidate_supporting_surface_count", None),
                ("baseline_first_entry_anchor", nullable_text(baseline_entry.get("first_entry_anchor"))),
                ("candidate_first_entry_anchor", None),
                ("metadata_changes", []),
                ("path_changes", []),
            ]
        )

    metadata_changes: list[str] = []
    path_changes: list[str] = []
    append_scalar_change(
        metadata_changes,
        "label",
        choose_text(baseline_entry.get("label")),
        choose_text(candidate_entry.get("label")),
    )
    append_scalar_change(
        metadata_changes,
        "summary_schema",
        choose_text(baseline_entry.get("summary_schema")),
        choose_text(candidate_entry.get("summary_schema")),
    )
    append_scalar_change(
        metadata_changes,
        "summary_kind",
        choose_text(baseline_entry.get("summary_kind")),
        choose_text(candidate_entry.get("summary_kind")),
    )
    append_scalar_change(metadata_changes, "depth", int(baseline_entry.get("depth", 0)), int(candidate_entry.get("depth", 0)))
    append_scalar_change(
        metadata_changes,
        "revisit",
        bool(baseline_entry.get("revisit")),
        bool(candidate_entry.get("revisit")),
    )
    append_scalar_change(metadata_changes, "cycle", bool(baseline_entry.get("cycle")), bool(candidate_entry.get("cycle")))
    append_scalar_change(
        metadata_changes,
        "expanded",
        bool(baseline_entry.get("expanded")),
        bool(candidate_entry.get("expanded")),
    )
    append_scalar_change(
        metadata_changes,
        "supporting_surface_count",
        int(baseline_entry.get("supporting_surface_count", 0)),
        int(candidate_entry.get("supporting_surface_count", 0)),
    )
    append_scalar_change(
        metadata_changes,
        "first_entry_anchor",
        nullable_text(baseline_entry.get("first_entry_anchor")),
        nullable_text(candidate_entry.get("first_entry_anchor")),
    )
    append_scalar_change(
        path_changes,
        "summary_path",
        normalize_path(baseline_entry.get("summary_path", "")),
        normalize_path(candidate_entry.get("summary_path", "")),
    )
    append_scalar_change(
        path_changes,
        "report_markdown_path",
        normalize_path(baseline_entry.get("report_markdown_path", "")),
        normalize_path(candidate_entry.get("report_markdown_path", "")),
    )
    append_scalar_change(
        path_changes,
        "check_text_path",
        normalize_path(baseline_entry.get("check_text_path", "")),
        normalize_path(candidate_entry.get("check_text_path", "")),
    )

    if not metadata_changes and not path_changes:
        return None

    score = 0
    baseline_depth = int(baseline_entry.get("depth", 0))
    candidate_depth = int(candidate_entry.get("depth", 0))
    if candidate_depth > baseline_depth:
        score += 1
    elif candidate_depth < baseline_depth:
        score -= 1

    baseline_expanded = bool(baseline_entry.get("expanded"))
    candidate_expanded = bool(candidate_entry.get("expanded"))
    if candidate_expanded and not baseline_expanded:
        score += 2
    elif baseline_expanded and not candidate_expanded:
        score -= 2

    baseline_supporting_surface_count = int(baseline_entry.get("supporting_surface_count", 0))
    candidate_supporting_surface_count = int(candidate_entry.get("supporting_surface_count", 0))
    if candidate_supporting_surface_count > baseline_supporting_surface_count:
        score += 1
    elif candidate_supporting_surface_count < baseline_supporting_surface_count:
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
            ("baseline_route_id", choose_text(baseline_entry.get("route_id"))),
            ("candidate_route_id", choose_text(candidate_entry.get("route_id"))),
            ("change_kind", "changed"),
            ("impact", impact),
            ("surface_id", choose_text(candidate_entry.get("surface_id")) or choose_text(baseline_entry.get("surface_id"))),
            ("role", choose_text(candidate_entry.get("role")) or choose_text(baseline_entry.get("role"))),
            ("baseline_label", choose_text(baseline_entry.get("label"))),
            ("candidate_label", choose_text(candidate_entry.get("label"))),
            ("baseline_summary_schema", choose_text(baseline_entry.get("summary_schema"))),
            ("candidate_summary_schema", choose_text(candidate_entry.get("summary_schema"))),
            ("baseline_summary_kind", choose_text(baseline_entry.get("summary_kind"))),
            ("candidate_summary_kind", choose_text(candidate_entry.get("summary_kind"))),
            ("baseline_summary_path", normalize_path(baseline_entry.get("summary_path", ""))),
            ("candidate_summary_path", normalize_path(candidate_entry.get("summary_path", ""))),
            ("baseline_depth", baseline_depth),
            ("candidate_depth", candidate_depth),
            ("baseline_revisit", bool(baseline_entry.get("revisit"))),
            ("candidate_revisit", bool(candidate_entry.get("revisit"))),
            ("baseline_cycle", bool(baseline_entry.get("cycle"))),
            ("candidate_cycle", bool(candidate_entry.get("cycle"))),
            ("baseline_expanded", baseline_expanded),
            ("candidate_expanded", candidate_expanded),
            ("baseline_supporting_surface_count", baseline_supporting_surface_count),
            ("candidate_supporting_surface_count", candidate_supporting_surface_count),
            ("baseline_first_entry_anchor", nullable_text(baseline_entry.get("first_entry_anchor"))),
            ("candidate_first_entry_anchor", nullable_text(candidate_entry.get("first_entry_anchor"))),
            ("metadata_changes", metadata_changes),
            ("path_changes", path_changes),
        ]
    )


def compare_route_entries(
    baseline_entries: list[OrderedDict[str, Any]],
    candidate_entries: list[OrderedDict[str, Any]],
    baseline_anchor_order: list[str],
    candidate_anchor_order: list[str],
) -> tuple[list[OrderedDict[str, Any]], OrderedDict[str, int]]:
    baseline_map = {entry["anchor_id"]: entry for entry in baseline_entries}
    candidate_map = {entry["anchor_id"]: entry for entry in candidate_entries}
    anchor_order = ordered_unique(candidate_anchor_order + baseline_anchor_order)

    entry_changes: list[OrderedDict[str, Any]] = []
    added_count = 0
    removed_count = 0
    regression_count = 0
    improvement_count = 0
    neutral_change_count = 0
    depth_changed_count = 0
    revisit_changed_count = 0
    cycle_changed_count = 0
    expanded_changed_count = 0

    for anchor_id in anchor_order:
        baseline_entry = baseline_map.get(anchor_id)
        candidate_entry = candidate_map.get(anchor_id)
        change = compare_route_entry(anchor_id, baseline_entry, candidate_entry)
        if change is None:
            continue

        entry_changes.append(change)
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

        if change["baseline_depth"] != change["candidate_depth"]:
            depth_changed_count += 1
        if change["baseline_revisit"] != change["candidate_revisit"]:
            revisit_changed_count += 1
        if change["baseline_cycle"] != change["candidate_cycle"]:
            cycle_changed_count += 1
        if change["baseline_expanded"] != change["candidate_expanded"]:
            expanded_changed_count += 1

    entry_summary = OrderedDict(
        [
            ("baseline_entry_count", len(baseline_entries)),
            ("candidate_entry_count", len(candidate_entries)),
            ("changed_entry_count", len(entry_changes)),
            ("added_entry_count", added_count),
            ("removed_entry_count", removed_count),
            ("unchanged_entry_count", max(len(anchor_order) - len(entry_changes), 0)),
            ("regression_count", regression_count),
            ("improvement_count", improvement_count),
            ("neutral_change_count", neutral_change_count),
            ("depth_changed_count", depth_changed_count),
            ("revisit_changed_count", revisit_changed_count),
            ("cycle_changed_count", cycle_changed_count),
            ("expanded_changed_count", expanded_changed_count),
        ]
    )
    return entry_changes, entry_summary


def build_route_regression_surface(
    route_changes: dict[str, Any],
    entry_changes: list[OrderedDict[str, Any]],
    candidate_entries: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    candidate_surface_ids = ordered_unique([entry["surface_id"] for entry in candidate_entries if entry["surface_id"] != "root"])
    regressed_entries = [change["anchor_id"] for change in entry_changes if change["impact"] == "regression"]
    removed_level1_surfaces = route_changes["level1_surface_changes"]["removed"]
    missing_key_surface_ids = route_changes["key_surface_changes"]["removed"]
    affected_surface_ids = ordered_unique(
        [change["surface_id"] for change in entry_changes if change["impact"] == "regression"]
        + removed_level1_surfaces
        + missing_key_surface_ids
    )
    narratives: list[str] = []
    for surface_id in removed_level1_surfaces[:3]:
        narratives.append(f"level-1 surface `{surface_id}` disappeared from the candidate route")
    for surface_id in missing_key_surface_ids[:3]:
        narratives.append(f"key surface `{surface_id}` is no longer reachable in the candidate route")
    for change in [item for item in entry_changes if item["impact"] == "regression"][:3]:
        narratives.append(
            "route entry `{0}` shrank `depth {1} -> {2}` `expanded {3} -> {4}`".format(
                change["anchor_id"],
                change["baseline_depth"],
                change["candidate_depth"],
                change["baseline_expanded"],
                change["candidate_expanded"],
            )
        )

    return OrderedDict(
        [
            (
                "changed",
                bool(regressed_entries or removed_level1_surfaces or missing_key_surface_ids),
            ),
            ("regressed_entries", regressed_entries),
            ("removed_level1_surfaces", removed_level1_surfaces),
            ("missing_key_surface_ids", missing_key_surface_ids),
            ("candidate_surface_ids", candidate_surface_ids),
            ("affected_surface_ids", affected_surface_ids),
            ("narratives", narratives),
        ]
    )


def determine_route_verdict(
    candidate_route: dict[str, Any],
    route_changes: dict[str, Any],
    entry_summary: dict[str, int],
    route_regression_surface: dict[str, Any],
) -> str:
    if choose_text(candidate_route.get("result")) != "ok":
        return "collapsed"
    if route_regression_surface["changed"] or entry_summary["regression_count"] > 0:
        return "drifted"
    if entry_summary["improvement_count"] > 0:
        return "improved"
    if route_changes["changed"] or entry_summary["neutral_change_count"] > 0:
        return "drifted"
    return "standing"


def build_questions(
    route_changes: dict[str, Any],
    entry_changes: list[OrderedDict[str, Any]],
    route_verdict: str,
) -> OrderedDict[str, list[str]]:
    compare_questions: list[str] = []
    if has_array_changes(route_changes["level1_surface_changes"]):
        compare_questions.append("Did the level-1 front page route change?")
    if has_array_changes(route_changes["key_surface_changes"]):
        compare_questions.append("Did any key front page surface appear or disappear?")
    if has_array_changes(route_changes["cycle_anchor_changes"]):
        compare_questions.append("Did the route gain or lose any real cycles?")
    if not compare_questions:
        compare_questions.append("Does the candidate front page route still match the baseline consumer walk?")

    next_questions: list[str] = []
    for surface_id in route_changes["level1_surface_changes"]["removed"][:2]:
        next_questions.append(f"Which consumer route should restore missing level-1 surface `{surface_id}`?")
    for surface_id in route_changes["key_surface_changes"]["removed"][:2]:
        next_questions.append(f"Why is key surface `{surface_id}` no longer reachable from the candidate front page?")
    for surface_id in route_changes["level1_surface_changes"]["added"][:2]:
        next_questions.append(f"Should new level-1 surface `{surface_id}` become part of the default explain entry route?")
    for change in [item for item in entry_changes if item["impact"] == "regression"][:2]:
        next_questions.append(
            "How do we recover route entry `{0}` without losing `surface_id={1}`?".format(
                change["anchor_id"],
                change["surface_id"],
            )
        )
    if not next_questions and route_verdict == "improved":
        next_questions.append("Which richer candidate route should become the next default consumer baseline?")
    if not next_questions:
        next_questions.append("What front page route should we compare against next?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_compare_summary_model(
    baseline_route_path: Path,
    candidate_route_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    baseline_route = load_route_summary(baseline_route_path)
    candidate_route = load_route_summary(candidate_route_path)
    baseline_entries, baseline_anchor_order = normalize_route_entries(baseline_route)
    candidate_entries, candidate_anchor_order = normalize_route_entries(candidate_route)
    route_status = build_route_status(baseline_route, candidate_route, baseline_entries, candidate_entries)
    route_changes = build_route_changes(baseline_route, candidate_route, baseline_entries, candidate_entries)
    entry_changes, entry_summary = compare_route_entries(
        baseline_entries,
        candidate_entries,
        baseline_anchor_order,
        candidate_anchor_order,
    )
    route_regression_surface = build_route_regression_surface(route_changes, entry_changes, candidate_entries)
    route_verdict = determine_route_verdict(candidate_route, route_changes, entry_summary, route_regression_surface)
    questions = build_questions(route_changes, entry_changes, route_verdict)

    supporting_surfaces = [
        build_front_page_surface(
            route_summary=baseline_route,
            route_summary_path=baseline_route_path,
            surface_id="baseline_route",
            role="baseline_route",
        ),
        build_front_page_surface(
            route_summary=candidate_route,
            route_summary_path=candidate_route_path,
            surface_id="candidate_route",
            role="candidate_route",
        ),
    ]
    route_provenance = [
        build_route_provenance_entry(baseline_route, baseline_route_path, "baseline_route"),
        build_route_provenance_entry(candidate_route, candidate_route_path, "candidate_route"),
    ]

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_route_compare/v0"),
            ("kind", "system_compiler.front_page_route_compare"),
            ("generator", "scripts/compare_system_compiler_front_page_route.py"),
            ("result", "ok"),
            (
                "route_compare",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Route Compare"),
                        (
                            "summary",
                            "A consumer-side comparison that asks how the declared front_page route changes between two route summaries.",
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
            ("route_provenance", route_provenance),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("baseline_route_summary_path", normalize_path(baseline_route_path)),
                        ("candidate_route_summary_path", normalize_path(candidate_route_path)),
                        ("output_root", normalize_path(output_root)),
                        ("compare_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("route_verdict", route_verdict),
            ("route_status", route_status),
            ("route_changes", route_changes),
            ("entry_summary", entry_summary),
            ("entry_changes", entry_changes),
            ("route_regression_surface", route_regression_surface),
            ("questions", questions),
            ("violations", []),
        ]
    )

