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


CAPABILITY_SCHEMA = "system_compiler.front_page_entry_capability/v0"
CAPABILITY_KIND = "system_compiler.front_page_entry_capability"

MODE_TO_CAPABILITY_ORDER = {
    "review": [
        "grouped_review",
        "shelf_compare",
        "candidate_shelf",
        "baseline_shelf",
        "delivery_biography",
        "counterfactual_verdict",
        "supporting_evidence",
        "runtime_session",
        "supporting_testimony",
        "route_provenance",
    ],
    "compare": [
        "counterfactual_verdict",
        "delivery_biography",
        "supporting_evidence",
        "runtime_session",
        "supporting_testimony",
        "shelf_compare",
        "candidate_shelf",
        "baseline_shelf",
        "route_provenance",
    ],
    "biography": [
        "delivery_biography",
        "supporting_evidence",
        "runtime_session",
        "supporting_testimony",
        "counterfactual_verdict",
        "shelf_compare",
        "candidate_shelf",
        "baseline_shelf",
        "route_provenance",
    ],
    "evidence": [
        "supporting_evidence",
        "runtime_session",
        "delivery_biography",
        "supporting_testimony",
        "counterfactual_verdict",
        "shelf_compare",
        "candidate_shelf",
        "baseline_shelf",
        "route_provenance",
    ],
    "route": [
        "route_provenance",
        "candidate_shelf",
        "shelf_compare",
        "baseline_shelf",
        "delivery_biography",
        "counterfactual_verdict",
        "supporting_evidence",
        "runtime_session",
        "supporting_testimony",
    ],
}

CAPABILITY_LABELS = {
    "grouped_review": "grouped review",
    "shelf_compare": "shelf compare",
    "candidate_shelf": "candidate shelf",
    "baseline_shelf": "baseline shelf",
    "delivery_biography": "delivery biography",
    "counterfactual_verdict": "counterfactual verdict",
    "supporting_evidence": "supporting evidence",
    "runtime_session": "runtime session",
    "supporting_testimony": "supporting testimony",
    "route_provenance": "route provenance",
}

MODE_FALLBACK_ORDER = {
    "review": ["review", "compare", "biography", "evidence", "route"],
    "compare": ["compare", "biography", "evidence", "route"],
    "biography": ["biography", "evidence", "route"],
    "evidence": ["evidence", "route"],
    "route": ["route"],
}

ARTIFACT_ROOT_DEFAULT_QUERY_KINDS = [
    "default_overview",
    "resource_summary",
    "bringup_evidence",
    "cap_list",
]

REPORT_DEFAULT_QUERY_KINDS = [
    "default_overview",
    "bringup_evidence",
    "resource_summary",
    "cap_list",
]


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


def load_capability_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != CAPABILITY_SCHEMA:
        raise ValueError(f"unsupported front page entry capability schema: {path}")
    if choose_text(summary.get("kind")) != CAPABILITY_KIND:
        raise ValueError(f"unsupported front page entry capability kind: {path}")
    return summary


def build_front_page_surface(path: Path, summary: dict[str, Any]) -> OrderedDict[str, str]:
    root_surface = get_mapping(summary.get("root_surface"))
    artifact_context = get_mapping(summary.get("artifact_context"))
    root_label = choose_text(root_surface.get("label")) or "entry capability"
    return OrderedDict(
        [
            ("id", "source_capability"),
            ("label", f"source capability: {root_label}"),
            ("role", "source_capability"),
            ("summary_schema", CAPABILITY_SCHEMA),
            ("summary_path", normalize_path(path)),
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


def clone_entry_ref(entry_ref: dict[str, Any]) -> OrderedDict[str, Any]:
    return OrderedDict(
        [
            ("route_id", choose_text(entry_ref.get("route_id"))),
            ("depth", int(entry_ref.get("depth", 0))),
            ("surface_id", choose_text(entry_ref.get("surface_id"))),
            ("label", choose_text(entry_ref.get("label"))),
            ("role", choose_text(entry_ref.get("role"))),
            ("summary_schema", choose_text(entry_ref.get("summary_schema"))),
            ("summary_kind", choose_text(entry_ref.get("summary_kind"))),
            ("summary_path", normalize_path(entry_ref.get("summary_path", ""))),
            ("report_markdown_path", normalize_path(entry_ref.get("report_markdown_path", ""))),
            ("check_text_path", normalize_path(entry_ref.get("check_text_path", ""))),
            ("revisit", bool(entry_ref.get("revisit"))),
            ("cycle", bool(entry_ref.get("cycle"))),
            ("expanded", bool(entry_ref.get("expanded"))),
            ("route_provenance_count", int(entry_ref.get("route_provenance_count", 0))),
            ("supporting_surface_count", int(entry_ref.get("supporting_surface_count", 0))),
        ]
    )


def build_route_provenance(path: Path, summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    provenance_entries: list[OrderedDict[str, Any]] = []
    for entry_value in get_list(summary.get("route_provenance")):
        entry = get_mapping(entry_value)
        if not entry:
            continue
        provenance_entries.append(
            OrderedDict(
                [
                    ("id", choose_text(entry.get("id"))),
                    ("route_kind", choose_text(entry.get("route_kind"))),
                    ("source_summary_schema", choose_text(entry.get("source_summary_schema"))),
                    ("source_summary_path", normalize_path(entry.get("source_summary_path", ""))),
                    ("source_input_summary_path", normalize_path(entry.get("source_input_summary_path", ""))),
                    ("source_root_summary_path", normalize_path(entry.get("source_root_summary_path", ""))),
                    ("source_report_markdown_path", normalize_path(entry.get("source_report_markdown_path", ""))),
                    ("source_check_text_path", normalize_path(entry.get("source_check_text_path", ""))),
                    (
                        "level1_surface_ids",
                        ordered_unique(
                            [choose_text(surface_id) for surface_id in get_list(entry.get("level1_surface_ids"))]
                        ),
                    ),
                ]
            )
        )
    return provenance_entries


def build_landing_tabs(capability_summary: dict[str, Any], recommended_mode: str) -> list[OrderedDict[str, Any]]:
    preferred_entries = get_mapping(get_mapping(capability_summary.get("capability_summary")).get("preferred_entries"))
    capability_order = MODE_TO_CAPABILITY_ORDER.get(recommended_mode, MODE_TO_CAPABILITY_ORDER["route"])
    tabs: list[OrderedDict[str, Any]] = []
    index_by_key: dict[tuple[str, str], int] = {}

    for capability_id in capability_order:
        entry_value = preferred_entries.get(capability_id)
        if not isinstance(entry_value, dict):
            continue

        entry_ref = clone_entry_ref(entry_value)
        key = (entry_ref["route_id"], entry_ref["summary_path"])
        existing_index = index_by_key.get(key)
        if existing_index is None:
            tab = OrderedDict(
                [
                    ("tab_id", capability_id),
                    ("title", CAPABILITY_LABELS.get(capability_id, capability_id)),
                    ("capability_ids", [capability_id]),
                    ("entry", entry_ref),
                ]
            )
            index_by_key[key] = len(tabs)
            tabs.append(tab)
        else:
            capability_ids = tabs[existing_index]["capability_ids"]
            if capability_id not in capability_ids:
                capability_ids.append(capability_id)

    return tabs


def build_provenance_roots(capability_summary: dict[str, Any]) -> list[OrderedDict[str, Any]]:
    roots_by_path: dict[str, OrderedDict[str, Any]] = {}
    for hint_value in get_list(capability_summary.get("provenance_hints")):
        hint = get_mapping(hint_value)
        if not hint:
            continue
        source_summary_path = normalize_path(hint.get("source_summary_path", ""))
        if not source_summary_path:
            continue

        root = roots_by_path.get(source_summary_path)
        if root is None:
            root = OrderedDict(
                [
                    ("root_id", choose_text(hint.get("provenance_id"))),
                    ("root_kind", choose_text(hint.get("provenance_route_kind"))),
                    ("source_summary_schema", choose_text(hint.get("source_summary_schema"))),
                    ("source_summary_path", source_summary_path),
                    (
                        "source_front_page_summary_path",
                        normalize_path(hint.get("source_front_page_summary_path", ""))
                        if choose_text(hint.get("source_front_page_summary_path"))
                        else "",
                    ),
                    ("owner_route_ids", []),
                    ("owner_surface_ids", []),
                    ("available_supporting_surface_ids", []),
                ]
            )
            roots_by_path[source_summary_path] = root

        owner_route_id = choose_text(hint.get("owner_route_id"))
        owner_surface_id = choose_text(hint.get("owner_surface_id"))
        if owner_route_id and owner_route_id not in root["owner_route_ids"]:
            root["owner_route_ids"].append(owner_route_id)
        if owner_surface_id and owner_surface_id not in root["owner_surface_ids"]:
            root["owner_surface_ids"].append(owner_surface_id)

        for surface_id in get_list(hint.get("available_supporting_surface_ids")):
            text = choose_text(surface_id)
            if text and text not in root["available_supporting_surface_ids"]:
                root["available_supporting_surface_ids"].append(text)

    return list(roots_by_path.values())


def build_query_hint(tab: dict[str, Any]) -> OrderedDict[str, Any]:
    tab_id = choose_text(tab.get("tab_id"))
    title = choose_text(tab.get("title"))
    entry = get_mapping(tab.get("entry"))
    summary_kind = choose_text(entry.get("summary_kind"))
    summary_schema = choose_text(entry.get("summary_schema"))
    entry_role = choose_text(entry.get("role"))

    if tab_id in {"grouped_review", "shelf_compare", "counterfactual_verdict"}:
        scope = "artifact_root"
        selection_rule = "artifact_root_or_subset"
        query_kind = "default_overview"
        compare_expected = True
        followup_query_kinds = [
            "resource_summary",
            "bringup_evidence",
            "cap_list",
        ]
        rationale = "Open the artifact_root overview first so compare drift and aggregate evidence stay visible."
    elif tab_id in {"candidate_shelf", "baseline_shelf", "route_provenance"}:
        scope = "artifact_root"
        selection_rule = "artifact_root_or_subset"
        query_kind = "default_overview"
        compare_expected = False
        followup_query_kinds = [
            "cap_list",
            "resource_summary",
            "bringup_evidence",
        ]
        rationale = "Open the artifact_root overview first so shelf-style navigation can pivot into shared aggregate explain surfaces."
    elif tab_id in {"supporting_evidence", "runtime_session"}:
        scope = "report"
        selection_rule = "single_report"
        query_kind = "bringup_evidence"
        compare_expected = False
        followup_query_kinds = [
            "resource_summary",
            "default_overview",
            "cap_list",
        ]
        rationale = "Start from report-scoped bringup evidence when the landing is already evidence-oriented."
    else:
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = summary_kind in {
            "system_compiler.world_compare",
            "system_compiler.world_shelf_review",
            "system_compiler.biography_index_compare",
        }
        followup_query_kinds = list(REPORT_DEFAULT_QUERY_KINDS)
        rationale = "Open the report overview first so single-world summary, bringup, resource, and capability reads stay nearby."

    if scope == "artifact_root" and query_kind == "cap_list":
        selection_rule = "artifact_root_full"

    return OrderedDict(
        [
            ("tab_id", tab_id),
            ("tab_title", title),
            ("entry_role", entry_role),
            ("summary_schema", summary_schema),
            ("summary_kind", summary_kind),
            ("scope", scope),
            ("selection_rule", selection_rule),
            ("query_kind", query_kind),
            ("compare_expected", compare_expected),
            ("followup_query_kinds", followup_query_kinds),
            ("rationale", rationale),
        ]
    )


def build_query_hints(tabs: list[OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    tab_queries = [build_query_hint(tab) for tab in tabs]
    primary_query = get_mapping(tab_queries[0]) if tab_queries else None
    return OrderedDict(
        [
            ("primary_query", primary_query),
            ("tab_queries", tab_queries),
        ]
    )


def build_landing_status(
    summary: dict[str, Any],
    tabs: list[OrderedDict[str, Any]],
    provenance_roots: list[OrderedDict[str, Any]],
) -> OrderedDict[str, Any]:
    entry_status = get_mapping(summary.get("entry_status"))
    recommended_mode = choose_text(entry_status.get("recommended_entry_mode")) or "route"
    available_tab_ids = [choose_text(tab.get("tab_id")) for tab in tabs]
    primary_tab_id = available_tab_ids[0] if available_tab_ids else None
    fallback_tab_ids = available_tab_ids[1:]
    primary_entry = get_mapping(tabs[0]).get("entry") if tabs else None

    return OrderedDict(
        [
            ("landing_result", choose_text(summary.get("result"))),
            ("recommended_entry_mode", recommended_mode),
            ("entry_tier", choose_text(entry_status.get("entry_tier"))),
            ("opening_reason", get_mapping(entry_status.get("opening_reason"))),
            ("primary_tab_id", primary_tab_id),
            ("primary_summary_schema", choose_text(get_mapping(primary_entry).get("summary_schema")) if isinstance(primary_entry, dict) else ""),
            ("primary_summary_kind", choose_text(get_mapping(primary_entry).get("summary_kind")) if isinstance(primary_entry, dict) else ""),
            ("available_tab_ids", available_tab_ids),
            ("fallback_tab_ids", fallback_tab_ids),
            ("tab_count", len(tabs)),
            ("fallback_tab_count", len(fallback_tab_ids)),
            ("provenance_root_count", len(provenance_roots)),
            ("route_provenance_entry_count", int(entry_status.get("route_provenance_entry_count", 0))),
            ("direct_review_available", "grouped_review" in available_tab_ids),
            ("direct_compare_available", "counterfactual_verdict" in available_tab_ids),
            ("direct_biography_available", "delivery_biography" in available_tab_ids),
            ("direct_evidence_available", "supporting_evidence" in available_tab_ids),
            ("direct_runtime_session_available", "runtime_session" in available_tab_ids),
        ]
    )


def build_questions(summary: dict[str, Any], landing_status: dict[str, Any], tabs: list[OrderedDict[str, Any]], provenance_roots: list[OrderedDict[str, Any]]) -> OrderedDict[str, list[str]]:
    capability_summary = get_mapping(summary.get("capability_summary"))
    missing_capability_ids = get_list(capability_summary.get("missing_capability_ids"))
    compare_questions: list[str] = []
    next_questions: list[str] = []

    if choose_text(landing_status.get("primary_tab_id")):
        compare_questions.append(
            "Is `{0}` the right default explain landing for this world?".format(
                landing_status["primary_tab_id"]
            )
        )
    if int(landing_status.get("provenance_root_count", 0)) > 0:
        compare_questions.append("Should provenance roots become expandable secondary explain entries?")
    if not compare_questions:
        compare_questions.append("What landing should open first for this entry?")

    if "grouped_review" in missing_capability_ids:
        next_questions.append("Should this entry gain a grouped review landing?")
    if "counterfactual_verdict" in missing_capability_ids:
        next_questions.append("Should this entry gain a direct compare landing?")
    if "delivery_biography" in missing_capability_ids:
        next_questions.append("Should this entry gain a direct biography landing?")
    if not next_questions and not provenance_roots and choose_text(landing_status.get("recommended_entry_mode")) == "review":
        next_questions.append("Should review mode also publish provenance roots for deeper shelf navigation?")
    if not next_questions and len(tabs) > 1:
        next_questions.append(
            "Which secondary landing should follow `{0}` by default?".format(
                choose_text(landing_status.get("primary_tab_id"))
            )
        )
    if not next_questions:
        next_questions.append("What explain landing should this entry gain next?")

    return OrderedDict(
        [
            ("compare_questions", ordered_unique(compare_questions)),
            ("next_questions", ordered_unique(next_questions)),
        ]
    )


def build_summary_model(
    capability_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    capability_summary = load_capability_summary(capability_summary_path)
    entry_status = get_mapping(capability_summary.get("entry_status"))
    recommended_mode = choose_text(entry_status.get("recommended_entry_mode")) or "route"
    tabs = build_landing_tabs(capability_summary, recommended_mode)
    provenance_roots = build_provenance_roots(capability_summary)
    landing_status = build_landing_status(capability_summary, tabs, provenance_roots)
    query_hints = build_query_hints(tabs)
    questions = build_questions(capability_summary, landing_status, tabs, provenance_roots)
    front_page_surface = build_front_page_surface(capability_summary_path, capability_summary)

    primary_landing = get_mapping(tabs[0]) if tabs else None
    secondary_landings = tabs[1:] if len(tabs) > 1 else []

    return OrderedDict(
        [
            ("schema", "system_compiler.front_page_entry_landing/v0"),
            ("kind", "system_compiler.front_page_entry_landing"),
            ("generator", "scripts/export_system_compiler_front_page_entry_landing.py"),
            ("result", "ok"),
            (
                "entry_landing",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Landing"),
                        (
                            "summary",
                            "A consumer-side open plan that turns one entry capability map into concrete landing tabs and provenance roots.",
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
                    supporting_surfaces=[front_page_surface],
                ),
            ),
            ("route_provenance", build_route_provenance(capability_summary_path, capability_summary)),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("input_capability_summary_path", normalize_path(capability_summary_path)),
                        ("output_root", normalize_path(output_root)),
                        ("landing_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("root_surface", get_mapping(capability_summary.get("root_surface"))),
            ("landing_status", landing_status),
            ("fallback_mode_order", MODE_FALLBACK_ORDER.get(recommended_mode, ["route"])),
            ("primary_landing", primary_landing),
            ("secondary_landings", secondary_landings),
            ("landing_tabs", tabs),
            ("provenance_roots", provenance_roots),
            ("query_hints", query_hints),
            ("questions", questions),
            ("violations", []),
        ]
    )
