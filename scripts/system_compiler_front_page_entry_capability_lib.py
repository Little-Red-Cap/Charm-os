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

CAPABILITY_IDS = [
    "delivery_biography",
    "counterfactual_verdict",
    "grouped_review",
    "supporting_evidence",
    "supporting_testimony",
    "shelf_compare",
    "candidate_shelf",
    "baseline_shelf",
    "route_provenance",
]

ROLE_TO_CAPABILITY = {
    "delivery_biography": "delivery_biography",
    "counterfactual_verdict": "counterfactual_verdict",
    "grouped_review": "grouped_review",
    "supporting_evidence": "supporting_evidence",
    "supporting_testimony": "supporting_testimony",
    "shelf_compare": "shelf_compare",
    "candidate_shelf": "candidate_shelf",
    "baseline_shelf": "baseline_shelf",
}

ROOT_KIND_TO_CAPABILITY = {
    "system_compiler.biography": "delivery_biography",
    "system_compiler.world_compare": "counterfactual_verdict",
    "system_compiler.world_shelf_review": "grouped_review",
    "system_compiler.witness_bundle": "supporting_testimony",
    "system_compiler.biography_index_compare": "shelf_compare",
    "system_compiler.biography_index": "candidate_shelf",
}

ROOT_SCHEMA_TO_CAPABILITY = {
    "minimal_kernel.runtime_evidence_bundle.summary/v1": "supporting_evidence",
}


def choose_text(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def nullable_text(value: Any) -> str | None:
    text = choose_text(value)
    return text or None


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


def load_route_summary(route_summary_path: Path) -> dict[str, Any]:
    route_summary = load_json(route_summary_path)
    if choose_text(route_summary.get("schema")) != ROUTE_SCHEMA:
        raise ValueError(f"unsupported front page route schema: {route_summary_path}")
    if choose_text(route_summary.get("kind")) != ROUTE_KIND:
        raise ValueError(f"unsupported front page route kind: {route_summary_path}")
    return route_summary


def build_front_page_surface(route_summary_path: Path, route_summary: dict[str, Any]) -> OrderedDict[str, str]:
    root_surface = get_mapping(route_summary.get("root_surface"))
    artifact_context = get_mapping(route_summary.get("artifact_context"))
    root_label = choose_text(root_surface.get("label")) or "front page route"
    return OrderedDict(
        [
            ("id", "source_route"),
            ("label", f"source route: {root_label}"),
            ("role", "source_route"),
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


def build_route_provenance(route_summary_path: Path, route_summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
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
    return [
        OrderedDict(
            [
                ("id", "source_route"),
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
    ]


def build_root_entry_candidate(route_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    root_surface = get_mapping(route_summary.get("root_surface"))
    route_summary_block = get_mapping(route_summary.get("route_summary"))
    route_provenance_summary = get_mapping(route_summary.get("route_provenance_summary"))
    return OrderedDict(
        [
            ("route_id", "root"),
            ("depth", 0),
            ("surface_id", choose_text(root_surface.get("surface_id")) or "root"),
            ("label", choose_text(root_surface.get("label"))),
            ("role", choose_text(root_surface.get("role")) or "root"),
            ("summary_schema", choose_text(root_surface.get("summary_schema"))),
            ("summary_kind", choose_text(root_surface.get("summary_kind"))),
            ("summary_path", normalize_path(root_surface.get("summary_path", ""))),
            ("report_markdown_path", normalize_path(root_surface.get("report_markdown_path", ""))),
            ("check_text_path", normalize_path(root_surface.get("check_text_path", ""))),
            ("revisit", False),
            ("cycle", False),
            ("expanded", bool(int(route_summary_block.get("expanded_entry_count", 0)) > 0)),
            ("route_provenance_count", int(route_provenance_summary.get("entry_count", 0))),
            ("supporting_surface_count", int(route_summary_block.get("entry_count", 0)) - 1 if int(route_summary_block.get("entry_count", 0)) > 0 else 0),
        ]
    )


def capability_ids_for_entry(entry: dict[str, Any]) -> list[str]:
    capability_ids: list[str] = []
    role = choose_text(entry.get("role"))
    mapped_role = ROLE_TO_CAPABILITY.get(role)
    if mapped_role:
        capability_ids.append(mapped_role)

    if choose_text(entry.get("route_id")) == "root":
        summary_kind = choose_text(entry.get("summary_kind"))
        summary_schema = choose_text(entry.get("summary_schema"))
        mapped_root_kind = ROOT_KIND_TO_CAPABILITY.get(summary_kind)
        mapped_root_schema = ROOT_SCHEMA_TO_CAPABILITY.get(summary_schema)
        if mapped_root_kind:
            capability_ids.append(mapped_root_kind)
        if mapped_root_schema:
            capability_ids.append(mapped_root_schema)

    return ordered_unique(capability_ids)


def build_entry_ref(entry: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
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


def select_preferred_entry(entries: list[dict[str, Any]]) -> OrderedDict[str, Any] | None:
    if not entries:
        return None

    def sort_key(entry: dict[str, Any]) -> tuple[int, int, int, int, str]:
        return (
            int(entry.get("depth", 0)),
            1 if bool(entry.get("cycle")) else 0,
            1 if bool(entry.get("revisit")) else 0,
            0 if bool(entry.get("expanded")) else 1,
            choose_text(entry.get("route_id")),
        )

    preferred = sorted(entries, key=sort_key)[0]
    return build_entry_ref(preferred)


def build_capability_entries(route_summary: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    capability_entries: dict[str, list[dict[str, Any]]] = {capability_id: [] for capability_id in CAPABILITY_IDS}
    root_candidate = build_root_entry_candidate(route_summary)
    for capability_id in capability_ids_for_entry(root_candidate):
        capability_entries[capability_id].append(root_candidate)

    for entry_value in route_summary.get("route_entries", []):
        entry = get_mapping(entry_value)
        if not entry:
            continue
        for capability_id in capability_ids_for_entry(entry):
            capability_entries[capability_id].append(entry)

    route_provenance_entries = route_summary.get("route_provenance_entries", [])
    if isinstance(route_provenance_entries, list) and route_provenance_entries:
        capability_entries["route_provenance"] = [root_candidate]

    return capability_entries


def build_capability_summary(capability_entries: dict[str, list[dict[str, Any]]]) -> OrderedDict[str, Any]:
    available_capability_ids = [capability_id for capability_id in CAPABILITY_IDS if capability_entries[capability_id]]
    missing_capability_ids = [capability_id for capability_id in CAPABILITY_IDS if not capability_entries[capability_id]]

    capability_flags = OrderedDict(
        (capability_id, bool(capability_entries[capability_id])) for capability_id in CAPABILITY_IDS
    )
    capability_counts = OrderedDict(
        (capability_id, len(capability_entries[capability_id])) for capability_id in CAPABILITY_IDS
    )
    preferred_entries = OrderedDict(
        (capability_id, select_preferred_entry(capability_entries[capability_id])) for capability_id in CAPABILITY_IDS
    )

    return OrderedDict(
        [
            ("available_capability_ids", available_capability_ids),
            ("missing_capability_ids", missing_capability_ids),
            ("capability_flags", capability_flags),
            ("capability_counts", capability_counts),
            ("preferred_entries", preferred_entries),
        ]
    )


def determine_recommended_entry_mode(capability_summary: dict[str, Any]) -> str:
    flags = get_mapping(capability_summary.get("capability_flags"))
    if bool(flags.get("grouped_review")):
        return "review"
    if bool(flags.get("counterfactual_verdict")):
        return "compare"
    if bool(flags.get("delivery_biography")):
        return "biography"
    if bool(flags.get("supporting_evidence")):
        return "evidence"
    return "route"


def determine_entry_tier(capability_summary: dict[str, Any]) -> str:
    flags = get_mapping(capability_summary.get("capability_flags"))
    has_review = bool(flags.get("grouped_review"))
    has_compare = bool(flags.get("counterfactual_verdict"))
    has_biography = bool(flags.get("delivery_biography"))
    has_evidence = bool(flags.get("supporting_evidence"))
    has_shelf = bool(flags.get("shelf_compare") or flags.get("candidate_shelf"))

    if has_review and has_shelf and has_biography and has_evidence:
        return "review_ready"
    if has_compare and has_biography and has_evidence:
        return "compare_ready"
    if has_biography and has_evidence:
        return "biography_ready"
    if has_evidence:
        return "evidence_only"
    return "route_only"


def build_entry_status(route_summary: dict[str, Any], capability_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    route_summary_block = get_mapping(route_summary.get("route_summary"))
    route_provenance_summary = get_mapping(route_summary.get("route_provenance_summary"))
    root_surface = get_mapping(route_summary.get("root_surface"))
    route_entries = route_summary.get("route_entries", [])

    level1_surface_ids = ordered_unique(
        [
            choose_text(entry.get("surface_id"))
            for entry in route_entries
            if isinstance(entry, dict) and int(entry.get("depth", 0)) == 1
        ]
    )
    level1_roles = ordered_unique(
        [
            choose_text(entry.get("role"))
            for entry in route_entries
            if isinstance(entry, dict) and int(entry.get("depth", 0)) == 1
        ]
    )
    unique_surface_ids = ordered_unique(
        [choose_text(entry.get("surface_id")) for entry in route_entries if isinstance(entry, dict)]
    )
    unique_roles = ordered_unique(
        [choose_text(entry.get("role")) for entry in route_entries if isinstance(entry, dict)]
    )

    return OrderedDict(
        [
            ("route_result", choose_text(route_summary.get("result"))),
            ("recommended_entry_mode", determine_recommended_entry_mode(capability_summary)),
            ("entry_tier", determine_entry_tier(capability_summary)),
            ("root_summary_schema", choose_text(root_surface.get("summary_schema"))),
            ("root_summary_kind", choose_text(root_surface.get("summary_kind"))),
            ("root_label", choose_text(root_surface.get("label"))),
            ("level1_surface_ids", level1_surface_ids),
            ("level1_roles", level1_roles),
            ("unique_surface_count", len(unique_surface_ids)),
            ("unique_role_count", len(unique_roles)),
            ("available_capability_count", len(capability_summary.get("available_capability_ids", []))),
            ("missing_capability_count", len(capability_summary.get("missing_capability_ids", []))),
            ("route_entry_count", int(route_summary_block.get("entry_count", 0))),
            ("route_provenance_entry_count", int(route_provenance_summary.get("entry_count", 0))),
            ("route_provenance_owner_count", int(route_provenance_summary.get("owner_count", 0))),
            ("repeated_entry_count", int(route_summary_block.get("repeated_entry_count", 0))),
            ("cycle_entry_count", int(route_summary_block.get("cycle_entry_count", 0))),
            ("expanded_entry_count", int(route_summary_block.get("expanded_entry_count", 0))),
            ("max_depth", int(route_summary_block.get("max_depth", 0))),
        ]
    )


def build_provenance_hints(route_summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    hints: list[OrderedDict[str, Any]] = []
    for entry_value in route_summary.get("route_provenance_entries", []):
        entry = get_mapping(entry_value)
        if not entry:
            continue
        hints.append(
            OrderedDict(
                [
                    ("owner_route_id", choose_text(entry.get("owner_route_id"))),
                    ("owner_surface_id", choose_text(entry.get("owner_surface_id"))),
                    ("owner_surface_role", choose_text(entry.get("owner_surface_role"))),
                    ("owner_depth", int(entry.get("owner_depth", 0))),
                    ("provenance_id", choose_text(entry.get("provenance_id"))),
                    ("provenance_route_kind", choose_text(entry.get("provenance_route_kind"))),
                    ("source_summary_schema", choose_text(entry.get("source_summary_schema"))),
                    ("source_summary_path", normalize_path(entry.get("source_summary_path", ""))),
                    (
                        "available_supporting_surface_ids",
                        ordered_unique(
                            [
                                choose_text(surface_id)
                                for surface_id in entry.get("available_supporting_surface_ids", [])
                                if choose_text(surface_id)
                            ]
                        ),
                    ),
                ]
            )
        )
    return hints


def build_questions(capability_summary: dict[str, Any], entry_status: dict[str, Any]) -> OrderedDict[str, list[str]]:
    flags = get_mapping(capability_summary.get("capability_flags"))
    missing_capability_ids = capability_summary.get("missing_capability_ids", [])
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if bool(flags.get("grouped_review")):
        compare_questions.append("Can this entry surface grouped review state without reopening producer internals?")
    if bool(flags.get("counterfactual_verdict")):
        compare_questions.append("Can this entry answer counterfactual world drift directly?")
    if bool(flags.get("delivery_biography")):
        compare_questions.append("Can this entry explain who the current world is?")
    if not compare_questions:
        compare_questions.append("What explain surface can this entry expose first?")

    if "delivery_biography" in missing_capability_ids:
        next_questions.append("Should this route expose a direct biography landing?")
    if "counterfactual_verdict" in missing_capability_ids:
        next_questions.append("Should this route expose a direct world-compare landing?")
    if "grouped_review" in missing_capability_ids:
        next_questions.append("Should this route publish a grouped review landing?")
    if "route_provenance" in missing_capability_ids:
        next_questions.append("Should this route publish route provenance for deeper explain consumers?")
    if not next_questions and choose_text(entry_status.get("recommended_entry_mode")) == "review":
        next_questions.append("Which review landing should become the default explain entry?")
    if not next_questions and choose_text(entry_status.get("recommended_entry_mode")) == "compare":
        next_questions.append("Which compare landing should become the default explain entry?")
    if not next_questions:
        next_questions.append("What capability should this front page entry gain next?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_summary_model(
    route_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    route_summary = load_route_summary(route_summary_path)
    root_surface = get_mapping(route_summary.get("root_surface"))
    capability_entries = build_capability_entries(route_summary)
    capability_summary = build_capability_summary(capability_entries)
    entry_status = build_entry_status(route_summary, capability_summary)
    provenance_hints = build_provenance_hints(route_summary)
    questions = build_questions(capability_summary, entry_status)

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_entry_capability/v0"),
            ("kind", "system_compiler.front_page_entry_capability"),
            ("generator", "scripts/export_system_compiler_front_page_entry_capability.py"),
            ("result", "ok"),
            (
                "entry_capability",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Capability"),
                        (
                            "summary",
                            "A consumer-side capability map that says which explain landings a front_page route can already provide.",
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
                    supporting_surfaces=[build_front_page_surface(route_summary_path, route_summary)],
                ),
            ),
            ("route_provenance", build_route_provenance(route_summary_path, route_summary)),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("input_route_summary_path", normalize_path(route_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("capability_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            (
                "root_surface",
                OrderedDict(
                    [
                        ("surface_id", choose_text(root_surface.get("surface_id"))),
                        ("label", choose_text(root_surface.get("label"))),
                        ("role", choose_text(root_surface.get("role"))),
                        ("summary_schema", choose_text(root_surface.get("summary_schema"))),
                        ("summary_kind", choose_text(root_surface.get("summary_kind"))),
                        ("summary_path", normalize_path(root_surface.get("summary_path", ""))),
                    ]
                ),
            ),
            ("entry_status", entry_status),
            ("capability_summary", capability_summary),
            ("provenance_hints", provenance_hints),
            ("questions", questions),
            ("violations", []),
        ]
    )

