from __future__ import annotations

import json
from collections import Counter, OrderedDict
from pathlib import Path
from typing import Any


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


def normalize_optional_path(value: Any) -> str:
    text = choose_text(value)
    if not text:
        return ""
    return normalize_path(text)


def choose_text(value: Any) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    return text


def get_mapping(value: Any) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    return {}


def get_front_page(summary: dict[str, Any]) -> dict[str, Any]:
    return get_mapping(summary.get("front_page"))


def get_artifact_context(summary: dict[str, Any]) -> dict[str, Any]:
    return get_mapping(summary.get("artifact_context"))


def build_artifact_report_index_provenance(summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    artifact_context = get_artifact_context(summary)
    artifact_report_index = normalize_optional_path(artifact_context.get("artifact_report_index"))
    if not artifact_report_index:
        return []

    return [
        OrderedDict(
            [
                ("id", "artifact_report_index"),
                ("route_kind", "artifact_report_index"),
                ("source_summary_schema", "system_compiler.artifact_report_index/v0"),
                ("source_summary_path", artifact_report_index),
                ("source_front_page_summary_path", ""),
                ("source_front_page_report_markdown_path", ""),
                ("source_front_page_check_text_path", ""),
                ("available_supporting_surface_ids", []),
            ]
        )
    ]


def get_route_provenance(summary: dict[str, Any]) -> list[Any]:
    route_provenance = summary.get("route_provenance", [])
    explicit_provenance = route_provenance if isinstance(route_provenance, list) else []
    return [*explicit_provenance, *build_artifact_report_index_provenance(summary)]


def is_front_page_provenance(entry: dict[str, Any]) -> bool:
    return choose_text(entry.get("provenance_route_kind")) != "artifact_report_index"


def build_root_label(summary: dict[str, Any]) -> str:
    kind = choose_text(summary.get("kind"))
    if kind == "system_compiler.witness_bundle":
        world = get_mapping(summary.get("world"))
        title = choose_text(world.get("title")) or choose_text(world.get("name")) or "witness world"
        return f"witness bundle: {title}"
    if kind == "system_compiler.biography":
        title = choose_text(summary.get("world_title")) or choose_text(summary.get("world_name")) or "world biography"
        return f"biography: {title}"
    if kind == "system_compiler.biography_index":
        shelf = get_mapping(summary.get("shelf"))
        title = choose_text(shelf.get("title")) or "System Compiler World Shelf"
        return f"world shelf: {title}"
    if kind == "system_compiler.biography_index_compare":
        shelf = get_mapping(summary.get("shelf"))
        title = choose_text(shelf.get("title")) or "System Compiler World Shelf"
        return f"shelf compare: {title}"
    if kind == "system_compiler.world_shelf_review":
        review = get_mapping(summary.get("review"))
        title = choose_text(review.get("title")) or "System Compiler World Shelf Review"
        return f"world shelf review: {title}"
    return choose_text(summary.get("title")) or choose_text(summary.get("schema")) or "root summary"


def build_root_surface(summary_path: Path, summary: dict[str, Any]) -> OrderedDict[str, Any]:
    front_page = get_front_page(summary)
    artifact_context = get_artifact_context(summary)
    summary_path_text = normalize_path(summary_path)
    report_markdown_path = (
        normalize_optional_path(front_page.get("report_markdown_path"))
        or normalize_optional_path(artifact_context.get("report_markdown_path"))
    )
    check_text_path = (
        normalize_optional_path(front_page.get("check_text_path"))
        or normalize_optional_path(artifact_context.get("check_text_path"))
    )
    actual_schema = choose_text(summary.get("schema"))
    actual_kind = choose_text(summary.get("kind"))
    return OrderedDict(
        [
            ("surface_id", "root"),
            ("label", build_root_label(summary)),
            ("role", "root"),
            ("declared_summary_schema", actual_schema),
            ("summary_schema", actual_schema),
            ("summary_kind", actual_kind),
            ("summary_path", summary_path_text),
            ("report_markdown_path", report_markdown_path),
            ("check_text_path", check_text_path),
        ]
    )


def build_child_surface(surface: dict[str, Any]) -> OrderedDict[str, Any]:
    summary_path = choose_text(surface.get("summary_path"))
    if not summary_path:
        raise ValueError("front_page supporting surface is missing summary_path")

    report_markdown_path = choose_text(surface.get("report_markdown_path"))
    if not report_markdown_path:
        raise ValueError("front_page supporting surface is missing report_markdown_path")

    check_text_path = choose_text(surface.get("check_text_path"))
    if not check_text_path:
        raise ValueError("front_page supporting surface is missing check_text_path")

    declared_schema = choose_text(surface.get("summary_schema"))
    surface_id = choose_text(surface.get("id")) or "supporting_surface"
    label = choose_text(surface.get("label")) or surface_id
    role = choose_text(surface.get("role")) or "supporting_surface"

    return OrderedDict(
        [
            ("surface_id", surface_id),
            ("label", label),
            ("role", role),
            ("declared_summary_schema", declared_schema),
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_markdown_path)),
            ("check_text_path", normalize_path(check_text_path)),
        ]
    )


def build_route_provenance_entry(
    owner_route_id: str,
    owner_surface: OrderedDict[str, Any],
    owner_depth: int,
    owner_summary_schema: str,
    owner_summary_kind: str,
    provenance_index: int,
    provenance_value: Any,
) -> OrderedDict[str, Any]:
    provenance = get_mapping(provenance_value)
    available_supporting_surface_ids: list[str] = []
    for surface_id_value in provenance.get("available_supporting_surface_ids", []):
        surface_id = choose_text(surface_id_value)
        if not surface_id or surface_id in available_supporting_surface_ids:
            continue
        available_supporting_surface_ids.append(surface_id)

    return OrderedDict(
        [
            ("owner_route_id", owner_route_id),
            ("owner_surface_id", owner_surface["surface_id"]),
            ("owner_surface_role", owner_surface["role"]),
            ("owner_depth", owner_depth),
            ("owner_summary_schema", owner_summary_schema),
            ("owner_summary_kind", owner_summary_kind),
            ("owner_summary_path", owner_surface["summary_path"]),
            ("provenance_index", provenance_index),
            ("provenance_id", choose_text(provenance.get("id")) or f"{owner_route_id}:{provenance_index}"),
            ("provenance_route_kind", choose_text(provenance.get("route_kind"))),
            ("source_summary_schema", choose_text(provenance.get("source_summary_schema"))),
            ("source_summary_path", normalize_optional_path(provenance.get("source_summary_path"))),
            (
                "source_front_page_summary_path",
                normalize_optional_path(provenance.get("source_front_page_summary_path")),
            ),
            (
                "source_front_page_report_markdown_path",
                normalize_optional_path(provenance.get("source_front_page_report_markdown_path")),
            ),
            (
                "source_front_page_check_text_path",
                normalize_optional_path(provenance.get("source_front_page_check_text_path")),
            ),
            ("available_supporting_surface_ids", available_supporting_surface_ids),
        ]
    )


class _RouteState:
    def __init__(self) -> None:
        self.summary_cache: dict[str, dict[str, Any]] = {}
        self.first_route_id_by_path: dict[str, str] = {}
        self.route_entries: list[OrderedDict[str, Any]] = []
        self.route_provenance_entries: list[OrderedDict[str, Any]] = []
        self.active_path_set: set[str] = set()

    def load_summary(self, summary_path: str) -> dict[str, Any]:
        cached = self.summary_cache.get(summary_path)
        if cached is not None:
            return cached

        summary = load_json(Path(summary_path))
        self.summary_cache[summary_path] = summary
        return summary


def _walk_surface(
    surface: OrderedDict[str, Any],
    route_id: str,
    parent_route_id: str | None,
    depth: int,
    state: _RouteState,
) -> None:
    summary_path = surface["summary_path"]
    summary = state.load_summary(summary_path)
    front_page = get_front_page(summary)
    supporting_surfaces = front_page.get("supporting_surfaces", [])
    if not isinstance(supporting_surfaces, list):
        supporting_surfaces = []
    route_provenance = get_route_provenance(summary)

    actual_schema = choose_text(summary.get("schema")) or surface.get("declared_summary_schema", "")
    actual_kind = choose_text(summary.get("kind"))
    first_route_id = state.first_route_id_by_path.get(summary_path)
    revisit = first_route_id is not None
    cycle = summary_path in state.active_path_set

    if not revisit:
        state.first_route_id_by_path[summary_path] = route_id
        first_route_id = route_id

    expanded = bool(supporting_surfaces) and not revisit
    state.route_entries.append(
        OrderedDict(
            [
                ("route_id", route_id),
                ("parent_route_id", parent_route_id),
                ("depth", depth),
                ("surface_id", surface["surface_id"]),
                ("label", surface["label"]),
                ("role", surface["role"]),
                ("declared_summary_schema", surface.get("declared_summary_schema", "")),
                ("summary_schema", actual_schema),
                ("summary_kind", actual_kind),
                ("summary_path", summary_path),
                ("report_markdown_path", surface["report_markdown_path"]),
                ("check_text_path", surface["check_text_path"]),
                ("route_provenance_count", len(route_provenance)),
                ("supporting_surface_count", len(supporting_surfaces)),
                ("revisit", revisit),
                ("cycle", cycle),
                ("first_route_id", first_route_id),
                ("expanded", expanded),
            ]
        )
    )

    for provenance_index, provenance_value in enumerate(route_provenance):
        state.route_provenance_entries.append(
            build_route_provenance_entry(
                owner_route_id=route_id,
                owner_surface=surface,
                owner_depth=depth,
                owner_summary_schema=actual_schema,
                owner_summary_kind=actual_kind,
                provenance_index=provenance_index,
                provenance_value=provenance_value,
            )
        )

    if not expanded:
        return

    state.active_path_set.add(summary_path)
    try:
        for index, child_surface_value in enumerate(supporting_surfaces):
            child_surface = build_child_surface(get_mapping(child_surface_value))
            _walk_surface(
                surface=child_surface,
                route_id=f"{route_id}/{index}",
                parent_route_id=route_id,
                depth=depth + 1,
                state=state,
            )
    finally:
        state.active_path_set.remove(summary_path)


def build_route_model(root_summary_path: Path) -> tuple[
    OrderedDict[str, Any],
    OrderedDict[str, Any],
    OrderedDict[str, Any],
    OrderedDict[str, int],
    OrderedDict[str, int],
    list[OrderedDict[str, Any]],
    list[OrderedDict[str, Any]],
]:
    resolved_root_path = Path(root_summary_path).resolve()
    root_summary = load_json(resolved_root_path)
    root_surface = build_root_surface(resolved_root_path, root_summary)

    state = _RouteState()
    _walk_surface(
        surface=root_surface,
        route_id="root",
        parent_route_id=None,
        depth=0,
        state=state,
    )

    schema_counts = Counter(str(entry["summary_schema"]) for entry in state.route_entries)
    role_counts = Counter(str(entry["role"]) for entry in state.route_entries)
    leaf_entry_count = sum(1 for entry in state.route_entries if not bool(entry["expanded"]))
    repeated_entry_count = sum(1 for entry in state.route_entries if bool(entry["revisit"]))
    cycle_entry_count = sum(1 for entry in state.route_entries if bool(entry["cycle"]))
    expanded_entry_count = sum(1 for entry in state.route_entries if bool(entry["expanded"]))
    max_depth = max((int(entry["depth"]) for entry in state.route_entries), default=0)

    route_summary = OrderedDict(
        [
            ("entry_count", len(state.route_entries)),
            ("unique_summary_count", len(state.first_route_id_by_path)),
            ("repeated_entry_count", repeated_entry_count),
            ("cycle_entry_count", cycle_entry_count),
            ("leaf_entry_count", leaf_entry_count),
            ("expanded_entry_count", expanded_entry_count),
            ("max_depth", max_depth),
        ]
    )

    provenance_owner_count = len({entry["owner_route_id"] for entry in state.route_provenance_entries})
    unique_source_summary_count = len(
        {
            entry["source_summary_path"]
            for entry in state.route_provenance_entries
            if choose_text(entry["source_summary_path"])
        }
    )
    unique_front_page_summary_count = len(
        {
            entry["source_front_page_summary_path"]
            for entry in state.route_provenance_entries
            if is_front_page_provenance(entry) and choose_text(entry["source_front_page_summary_path"])
        }
    )
    route_provenance_summary = OrderedDict(
        [
            ("entry_count", len(state.route_provenance_entries)),
            ("owner_count", provenance_owner_count),
            ("unique_source_summary_count", unique_source_summary_count),
            ("unique_front_page_summary_count", unique_front_page_summary_count),
        ]
    )

    ordered_schema_counts = OrderedDict((key, int(schema_counts[key])) for key in sorted(schema_counts))
    ordered_role_counts = OrderedDict((key, int(role_counts[key])) for key in sorted(role_counts))
    return (
        route_summary,
        route_provenance_summary,
        ordered_schema_counts,
        ordered_role_counts,
        root_surface,
        state.route_entries,
        state.route_provenance_entries,
    )
