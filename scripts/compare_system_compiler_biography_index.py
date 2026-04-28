import argparse
import json
from datetime import datetime
from pathlib import Path


BIOGRAPHY_INDEX_SCHEMA = "system_compiler.biography_index/v0"
BIOGRAPHY_INDEX_KIND = "system_compiler.biography_index"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def resolve_output_path(explicit: str, output_root: Path, default_name: str) -> Path:
    if explicit:
        return Path(explicit).resolve()
    return (output_root / default_name).resolve()


def build_surface_ref(
    surface_id: str,
    summary_schema: str,
    label: str,
    role: str,
    summary_path: str,
    report_markdown_path: str,
    check_text_path: str,
):
    return {
        "id": surface_id,
        "label": label,
        "role": role,
        "summary_schema": summary_schema,
        "summary_path": str(Path(summary_path).resolve()),
        "report_markdown_path": str(Path(report_markdown_path).resolve()),
        "check_text_path": str(Path(check_text_path).resolve()),
    }


def build_front_page(summary_path: Path, report_path: Path, check_path: Path, supporting_surfaces: list[dict]):
    return {
        "summary_path": str(summary_path.resolve()),
        "report_markdown_path": str(report_path.resolve()),
        "check_text_path": str(check_path.resolve()),
        "supporting_surfaces": supporting_surfaces,
    }


def build_route_provenance_entry(index_summary: dict, index_path: Path, route_id: str):
    front_page = index_summary.get("front_page", {})
    delivery = index_summary.get("delivery", {})
    supporting_surface_ids = ordered_unique(
        str(surface.get("id", "")).strip()
        for surface in front_page.get("supporting_surfaces", [])
        if isinstance(surface, dict) and str(surface.get("id", "")).strip()
    )
    return {
        "id": route_id,
        "route_kind": "front_page_root",
        "source_summary_schema": BIOGRAPHY_INDEX_SCHEMA,
        "source_summary_path": str(index_path.resolve()),
        "source_front_page_summary_path": str(
            Path(front_page.get("summary_path") or delivery.get("summary_path") or index_path).resolve()
        ),
        "source_front_page_report_markdown_path": str(
            Path(front_page.get("report_markdown_path") or delivery["report_markdown_path"]).resolve()
        ),
        "source_front_page_check_text_path": str(
            Path(front_page.get("check_text_path") or delivery["check_text_path"]).resolve()
        ),
        "available_supporting_surface_ids": supporting_surface_ids,
    }


def ordered_unique(values):
    seen = set()
    result = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        result.append(value)
    return result


def string_list(values):
    return ordered_unique(
        str(value)
        for value in (values or [])
        if value is not None and str(value).strip()
    )


def string_array_changes(left, right):
    left_values = string_list(left)
    right_values = string_list(right)
    left_set = set(left_values)
    right_set = set(right_values)
    return {
        "added": [value for value in right_values if value not in left_set],
        "removed": [value for value in left_values if value not in right_set],
    }


def has_array_changes(change: dict):
    return bool(change["added"] or change["removed"])


def nullable_text(value):
    if value is None:
        return None
    text = str(value)
    return text if text else None


def verdict_label(value):
    return nullable_text(value) or "not-attached"


def load_biography_index(path: Path):
    data = load_json(path)
    if data.get("schema") != BIOGRAPHY_INDEX_SCHEMA:
        raise ValueError(f"unsupported biography index schema: {path}")
    if data.get("kind") != BIOGRAPHY_INDEX_KIND:
        raise ValueError(f"unsupported biography index kind: {path}")
    return data


def build_index_surface(index_summary: dict, index_path: Path, surface_id: str, role: str):
    front_page = index_summary.get("front_page", {})
    delivery = index_summary.get("delivery", {})
    shelf = index_summary.get("shelf", {})
    shelf_title = str(shelf.get("title", "")).strip() or "System Compiler World Shelf"
    return build_surface_ref(
        surface_id=surface_id,
        summary_schema=BIOGRAPHY_INDEX_SCHEMA,
        label=f"{role.replace('_', ' ')}: {shelf_title}",
        role=role,
        summary_path=front_page.get("summary_path") or delivery.get("summary_path") or str(index_path),
        report_markdown_path=front_page.get("report_markdown_path") or delivery["report_markdown_path"],
        check_text_path=front_page.get("check_text_path") or delivery["check_text_path"],
    )


def entry_anchor_base(entry: dict):
    facets = ",".join(string_list(entry.get("active_facets", [])))
    compare_label = "compare" if bool(entry.get("compare_attached")) else "witness"
    return "|".join(
        [
            str(entry.get("world_name", "")),
            str(entry.get("profile", "")),
            nullable_text(entry.get("board")) or "",
            facets,
            compare_label,
        ]
    )


def build_entry_maps(index_summary: dict):
    seen: dict[str, int] = {}
    mapping: dict[str, dict] = {}
    order: list[str] = []
    for entry in index_summary.get("entries", []):
        base = entry_anchor_base(entry)
        seen[base] = seen.get(base, 0) + 1
        anchor = base if seen[base] == 1 else f"{base}#{seen[base]}"
        mapping[anchor] = entry
        order.append(anchor)
    return mapping, order


def entry_health_rank(entry: dict | None):
    if entry is None:
        return -1

    if str(entry.get("result", "fail")) != "ok":
        return 0

    verdict = verdict_label(entry.get("world_verdict"))
    if verdict == "collapsed":
        return 0
    if verdict == "drifted":
        return 1
    if verdict == "not-attached":
        return 2
    if verdict == "standing":
        return 3
    if verdict == "improved":
        return 4
    return 2


def append_scalar_change(changes: list[str], label: str, left_value, right_value):
    if left_value != right_value:
        changes.append(f"{label}:{left_value}->{right_value}")


def append_array_change(changes: list[str], label: str, left_values, right_values):
    change = string_array_changes(left_values, right_values)
    if has_array_changes(change):
        changes.append(
            "{0}:+[{1}] -[{2}]".format(
                label,
                ", ".join(change["added"]),
                ", ".join(change["removed"]),
            )
        )


def collect_path_change(changes: list[str], label: str, left_value, right_value):
    left_text = nullable_text(left_value)
    right_text = nullable_text(right_value)
    if left_text != right_text:
        changes.append(f"{label}:{left_text}->{right_text}")


def make_added_or_removed_change(anchor_id: str, left_entry: dict | None, right_entry: dict | None):
    anchor = right_entry if right_entry is not None else left_entry
    compare_attached = bool(anchor.get("compare_attached"))
    world_verdict = verdict_label(anchor.get("world_verdict"))
    result = str(anchor.get("result", "fail"))

    if left_entry is None and right_entry is not None:
        change_kind = "added"
        if result != "ok" or world_verdict in ("drifted", "collapsed"):
            impact = "regression"
        elif compare_attached or world_verdict in ("standing", "improved"):
            impact = "improvement"
        else:
            impact = "neutral"
    else:
        change_kind = "removed"
        if result != "ok" or world_verdict in ("drifted", "collapsed"):
            impact = "improvement"
        else:
            impact = "regression"

    return {
        "anchor_id": anchor_id,
        "baseline_entry_id": nullable_text(left_entry.get("id")) if left_entry is not None else None,
        "candidate_entry_id": nullable_text(right_entry.get("id")) if right_entry is not None else None,
        "change_kind": change_kind,
        "impact": impact,
        "world_name": str(anchor.get("world_name", "")),
        "profile": str(anchor.get("profile", "")),
        "compare_attached": compare_attached,
        "left_result": str(left_entry.get("result", "absent")) if left_entry is not None else "absent",
        "right_result": str(right_entry.get("result", "absent")) if right_entry is not None else "absent",
        "left_world_verdict": left_entry.get("world_verdict") if left_entry is not None else None,
        "right_world_verdict": right_entry.get("world_verdict") if right_entry is not None else None,
        "left_summary_path": nullable_text(left_entry.get("summary_path")) if left_entry is not None else None,
        "right_summary_path": nullable_text(right_entry.get("summary_path")) if right_entry is not None else None,
        "metadata_changes": [],
        "path_changes": [],
        "evidence_path_added": [],
        "evidence_path_removed": [],
        "next_questions_added": [],
        "next_questions_removed": [],
    }


def compare_entry(anchor_id: str, left_entry: dict | None, right_entry: dict | None):
    if left_entry is None or right_entry is None:
        return make_added_or_removed_change(anchor_id, left_entry, right_entry)

    metadata_changes: list[str] = []
    path_changes: list[str] = []

    append_scalar_change(metadata_changes, "result", str(left_entry.get("result", "fail")), str(right_entry.get("result", "fail")))
    append_scalar_change(metadata_changes, "world_verdict", verdict_label(left_entry.get("world_verdict")), verdict_label(right_entry.get("world_verdict")))
    append_scalar_change(metadata_changes, "world_title", str(left_entry.get("world_title", "")), str(right_entry.get("world_title", "")))
    append_scalar_change(metadata_changes, "board", nullable_text(left_entry.get("board")), nullable_text(right_entry.get("board")))
    append_scalar_change(metadata_changes, "identity", str(left_entry.get("identity", "")), str(right_entry.get("identity", "")))
    append_scalar_change(metadata_changes, "thesis", str(left_entry.get("thesis", "")), str(right_entry.get("thesis", "")))
    append_array_change(metadata_changes, "active_facets", left_entry.get("active_facets", []), right_entry.get("active_facets", []))

    collect_path_change(path_changes, "summary_path", left_entry.get("summary_path"), right_entry.get("summary_path"))
    collect_path_change(path_changes, "report_markdown_path", left_entry.get("report_markdown_path"), right_entry.get("report_markdown_path"))
    collect_path_change(path_changes, "check_text_path", left_entry.get("check_text_path"), right_entry.get("check_text_path"))
    collect_path_change(path_changes, "runtime_evidence_summary", left_entry.get("runtime_evidence_summary"), right_entry.get("runtime_evidence_summary"))
    collect_path_change(path_changes, "witness_bundle_summary", left_entry.get("witness_bundle_summary"), right_entry.get("witness_bundle_summary"))
    collect_path_change(path_changes, "world_compare_summary", left_entry.get("world_compare_summary"), right_entry.get("world_compare_summary"))

    evidence_path_change = string_array_changes(left_entry.get("evidence_path", []), right_entry.get("evidence_path", []))
    next_question_change = string_array_changes(left_entry.get("next_questions", []), right_entry.get("next_questions", []))

    left_rank = entry_health_rank(left_entry)
    right_rank = entry_health_rank(right_entry)
    if right_rank > left_rank:
        impact = "improvement"
    elif right_rank < left_rank:
        impact = "regression"
    else:
        impact = "neutral"

    if (
        impact == "neutral"
        and not metadata_changes
        and not path_changes
        and not has_array_changes(evidence_path_change)
        and not has_array_changes(next_question_change)
    ):
        return None

    return {
        "anchor_id": anchor_id,
        "baseline_entry_id": nullable_text(left_entry.get("id")),
        "candidate_entry_id": nullable_text(right_entry.get("id")),
        "change_kind": "changed",
        "impact": impact,
        "world_name": str(right_entry.get("world_name", left_entry.get("world_name", ""))),
        "profile": str(right_entry.get("profile", left_entry.get("profile", ""))),
        "compare_attached": bool(right_entry.get("compare_attached", left_entry.get("compare_attached", False))),
        "left_result": str(left_entry.get("result", "fail")),
        "right_result": str(right_entry.get("result", "fail")),
        "left_world_verdict": left_entry.get("world_verdict"),
        "right_world_verdict": right_entry.get("world_verdict"),
        "left_summary_path": nullable_text(left_entry.get("summary_path")),
        "right_summary_path": nullable_text(right_entry.get("summary_path")),
        "metadata_changes": metadata_changes,
        "path_changes": path_changes,
        "evidence_path_added": evidence_path_change["added"],
        "evidence_path_removed": evidence_path_change["removed"],
        "next_questions_added": next_question_change["added"],
        "next_questions_removed": next_question_change["removed"],
    }


def compare_entries(baseline_index: dict, candidate_index: dict):
    baseline_entries, baseline_order = build_entry_maps(baseline_index)
    candidate_entries, candidate_order = build_entry_maps(candidate_index)
    anchor_order = ordered_unique(candidate_order + baseline_order)

    changes = []
    regression_count = 0
    improvement_count = 0
    neutral_change_count = 0
    added_entry_count = 0
    removed_entry_count = 0

    for anchor_id in anchor_order:
        change = compare_entry(anchor_id, baseline_entries.get(anchor_id), candidate_entries.get(anchor_id))
        if change is None:
            continue

        changes.append(change)
        if change["change_kind"] == "added":
            added_entry_count += 1
        elif change["change_kind"] == "removed":
            removed_entry_count += 1

        if change["impact"] == "regression":
            regression_count += 1
        elif change["impact"] == "improvement":
            improvement_count += 1
        else:
            neutral_change_count += 1

    summary = {
        "baseline_entry_count": len(baseline_order),
        "candidate_entry_count": len(candidate_order),
        "changed_entry_count": len(changes),
        "added_entry_count": added_entry_count,
        "removed_entry_count": removed_entry_count,
        "unchanged_entry_count": max(len(anchor_order) - len(changes), 0),
        "regression_count": regression_count,
        "improvement_count": improvement_count,
        "neutral_change_count": neutral_change_count,
    }

    return changes, summary


def build_shelf_view(candidate_index: dict):
    shelf = candidate_index.get("shelf", {})
    return {
        "title": str(shelf.get("title", "")),
        "summary": str(shelf.get("summary", "")),
    }


def build_shelf_status(baseline_index: dict, candidate_index: dict):
    baseline_summary = baseline_index.get("summary", {})
    candidate_summary = candidate_index.get("summary", {})
    return {
        "baseline_result": str(baseline_index.get("result", "fail")),
        "candidate_result": str(candidate_index.get("result", "fail")),
        "baseline_profile": str(baseline_index.get("profile", "")),
        "candidate_profile": str(candidate_index.get("profile", "")),
        "baseline_biography_count": int(baseline_summary.get("biography_count", 0)),
        "candidate_biography_count": int(candidate_summary.get("biography_count", 0)),
        "baseline_unique_world_count": int(baseline_summary.get("unique_world_count", 0)),
        "candidate_unique_world_count": int(candidate_summary.get("unique_world_count", 0)),
        "baseline_compare_attached_count": int(baseline_summary.get("compare_attached_count", 0)),
        "candidate_compare_attached_count": int(candidate_summary.get("compare_attached_count", 0)),
        "baseline_not_attached_count": int(baseline_summary.get("not_attached_count", 0)),
        "candidate_not_attached_count": int(candidate_summary.get("not_attached_count", 0)),
    }


def build_shelf_changes(baseline_index: dict, candidate_index: dict):
    baseline_shelf = baseline_index.get("shelf", {})
    candidate_shelf = candidate_index.get("shelf", {})
    baseline_questions = baseline_index.get("questions", {})
    candidate_questions = candidate_index.get("questions", {})

    question_changes = {
        "core_question_changes": string_array_changes(
            baseline_questions.get("core_questions", []),
            candidate_questions.get("core_questions", []),
        ),
        "compare_question_changes": string_array_changes(
            baseline_questions.get("compare_questions", []),
            candidate_questions.get("compare_questions", []),
        ),
        "next_question_changes": string_array_changes(
            baseline_questions.get("next_questions", []),
            candidate_questions.get("next_questions", []),
        ),
    }

    world_name_changes = string_array_changes(
        [entry.get("world_name", "") for entry in baseline_index.get("entries", [])],
        [entry.get("world_name", "") for entry in candidate_index.get("entries", [])],
    )

    changed = (
        str(baseline_shelf.get("title", "")) != str(candidate_shelf.get("title", ""))
        or str(baseline_shelf.get("summary", "")) != str(candidate_shelf.get("summary", ""))
        or str(baseline_index.get("profile", "")) != str(candidate_index.get("profile", ""))
        or has_array_changes(world_name_changes)
        or any(has_array_changes(change) for change in question_changes.values())
    )

    return {
        "changed": changed,
        "title_changed": str(baseline_shelf.get("title", "")) != str(candidate_shelf.get("title", "")),
        "summary_changed": str(baseline_shelf.get("summary", "")) != str(candidate_shelf.get("summary", "")),
        "profile_changed": str(baseline_index.get("profile", "")) != str(candidate_index.get("profile", "")),
        "question_changes": question_changes,
        "world_name_changes": world_name_changes,
    }


def build_collapse_surface(entry_changes, shelf_changes, candidate_index: dict):
    regressed_changes = [change for change in entry_changes if change["impact"] == "regression"]
    removed_worlds = shelf_changes["world_name_changes"]["removed"]
    added_failed_entries = [
        change["anchor_id"]
        for change in entry_changes
        if change["change_kind"] == "added" and change["right_result"] != "ok"
    ]

    affected_worlds = ordered_unique(
        [change["world_name"] for change in regressed_changes]
        + [change["world_name"] for change in entry_changes if change["anchor_id"] in added_failed_entries]
        + removed_worlds
    )
    affected_profiles = ordered_unique(
        change["profile"]
        for change in regressed_changes
        if change["profile"]
    )

    narratives = []
    for change in regressed_changes[:3]:
        narratives.append(
            "shelf entry `{0}` regressed `{1}` -> `{2}`".format(
                change["anchor_id"],
                verdict_label(change["left_world_verdict"]),
                verdict_label(change["right_world_verdict"]),
            )
        )
    for world_name in removed_worlds[:3]:
        narratives.append(f"world `{world_name}` disappeared from the candidate shelf")
    for anchor_id in added_failed_entries[:3]:
        narratives.append(f"candidate added failing shelf entry `{anchor_id}`")
    if str(candidate_index.get("result", "fail")) != "ok":
        narratives.append("candidate shelf no longer stands as `ok`")

    return {
        "changed": bool(regressed_changes or removed_worlds or added_failed_entries or str(candidate_index.get("result", "fail")) != "ok"),
        "regressed_entries": [change["anchor_id"] for change in regressed_changes],
        "removed_worlds": removed_worlds,
        "added_failed_entries": added_failed_entries,
        "affected_worlds": affected_worlds,
        "affected_profiles": affected_profiles,
        "narratives": narratives,
    }


def determine_shelf_verdict(candidate_index: dict, shelf_changes: dict, entry_summary: dict):
    if str(candidate_index.get("result", "fail")) != "ok":
        return "collapsed"
    if entry_summary["regression_count"] > 0:
        return "drifted"
    if entry_summary["improvement_count"] > 0:
        return "improved"
    if (
        shelf_changes["changed"]
        or entry_summary["neutral_change_count"] > 0
        or entry_summary["added_entry_count"] > 0
        or entry_summary["removed_entry_count"] > 0
    ):
        return "drifted"
    return "standing"


def build_next_questions(candidate_index: dict, entry_changes, shelf_changes: dict, entry_summary: dict, collapse_surface: dict):
    compare_questions = string_list(candidate_index.get("questions", {}).get("compare_questions", []))
    next_questions = []

    for change in [item for item in entry_changes if item["impact"] == "regression"][:3]:
        next_questions.append(
            "Why did shelf entry `{0}` regress from `{1}` to `{2}`?".format(
                change["anchor_id"],
                verdict_label(change["left_world_verdict"]),
                verdict_label(change["right_world_verdict"]),
            )
        )

    for world_name in shelf_changes["world_name_changes"]["removed"][:2]:
        next_questions.append(f"Which route should restore removed world `{world_name}` to the shelf?")

    for change in [
        item
        for item in entry_changes
        if item["impact"] == "improvement" and item["change_kind"] == "added" and item["compare_attached"]
    ][:2]:
        next_questions.append(
            "Should added compare-attached shelf entry `{0}` become the default review front page?".format(
                change["anchor_id"]
            )
        )

    if not next_questions and entry_summary["improvement_count"] > 0:
        next_questions.append("Which improved shelf entry should become the next default review front page?")

    if collapse_surface["changed"] and collapse_surface["affected_worlds"]:
        next_questions.append(
            "How do we restore the smallest shelf collapse surface across worlds `{0}`?".format(
                ", ".join(collapse_surface["affected_worlds"])
            )
        )

    if not next_questions:
        next_questions.extend(compare_questions[:1] or ["What world shelf should this delivery be compared against next?"])

    return {
        "compare_questions": compare_questions,
        "next_questions": ordered_unique(next_questions),
    }


def build_summary(args):
    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-biography-index-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "report.md")
    check_path = resolve_output_path(args.check_text, output_root, "check.txt")

    baseline_index = load_biography_index(baseline_path)
    candidate_index = load_biography_index(candidate_path)

    shelf_view = build_shelf_view(candidate_index)
    shelf_status = build_shelf_status(baseline_index, candidate_index)
    entry_changes, entry_summary = compare_entries(baseline_index, candidate_index)
    shelf_changes = build_shelf_changes(baseline_index, candidate_index)
    collapse_surface = build_collapse_surface(entry_changes, shelf_changes, candidate_index)
    questions = build_next_questions(candidate_index, entry_changes, shelf_changes, entry_summary, collapse_surface)
    supporting_surfaces = [
        build_index_surface(
            index_summary=baseline_index,
            index_path=baseline_path,
            surface_id="baseline_shelf",
            role="baseline_shelf",
        ),
        build_index_surface(
            index_summary=candidate_index,
            index_path=candidate_path,
            surface_id="candidate_shelf",
            role="candidate_shelf",
        ),
    ]
    route_provenance = [
        build_route_provenance_entry(
            index_summary=baseline_index,
            index_path=baseline_path,
            route_id="baseline_shelf",
        ),
        build_route_provenance_entry(
            index_summary=candidate_index,
            index_path=candidate_path,
            route_id="candidate_shelf",
        ),
    ]

    summary = {
        "schema": "system_compiler.biography_index_compare/v0",
        "kind": "system_compiler.biography_index_compare",
        "generated_at_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "generator": "scripts/compare_system_compiler_biography_index.py",
        "result": "ok",
        "shelf_verdict": determine_shelf_verdict(candidate_index, shelf_changes, entry_summary),
        "shelf": shelf_view,
        "front_page": build_front_page(
            summary_path=summary_path,
            report_path=report_path,
            check_path=check_path,
            supporting_surfaces=supporting_surfaces,
        ),
        "route_provenance": route_provenance,
        "artifact_context": {
            "baseline_biography_index": str(baseline_path),
            "candidate_biography_index": str(candidate_path),
            "output_root": str(output_root),
            "report_markdown_path": str(report_path),
            "check_text_path": str(check_path),
        },
        "shelf_status": shelf_status,
        "shelf_changes": shelf_changes,
        "entry_summary": entry_summary,
        "entry_changes": entry_changes,
        "collapse_surface": collapse_surface,
        "questions": questions,
        "violations": [],
    }

    write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")

    report_lines = [
        "# System Compiler Biography Index Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- Shelf verdict: `{summary['shelf_verdict']}`",
        f"- Shelf: `{shelf_view['title']}`",
        f"- Baseline shelf: `{baseline_path}`",
        f"- Candidate shelf: `{candidate_path}`",
        f"- Summary JSON: `{summary_path}`",
        "",
        "## Shelf Status",
        "- Baseline: `result={0} profile={1} biographies={2} worlds={3} compare_attached={4} not_attached={5}`".format(
            shelf_status["baseline_result"],
            shelf_status["baseline_profile"],
            shelf_status["baseline_biography_count"],
            shelf_status["baseline_unique_world_count"],
            shelf_status["baseline_compare_attached_count"],
            shelf_status["baseline_not_attached_count"],
        ),
        "- Candidate: `result={0} profile={1} biographies={2} worlds={3} compare_attached={4} not_attached={5}`".format(
            shelf_status["candidate_result"],
            shelf_status["candidate_profile"],
            shelf_status["candidate_biography_count"],
            shelf_status["candidate_unique_world_count"],
            shelf_status["candidate_compare_attached_count"],
            shelf_status["candidate_not_attached_count"],
        ),
        "- Entry drift: `changed={0} added={1} removed={2} regressions={3} improvements={4} neutral={5}`".format(
            entry_summary["changed_entry_count"],
            entry_summary["added_entry_count"],
            entry_summary["removed_entry_count"],
            entry_summary["regression_count"],
            entry_summary["improvement_count"],
            entry_summary["neutral_change_count"],
        ),
    ]

    report_lines.extend(["", "## Route Provenance"])
    for route in route_provenance:
        report_lines.append(
            "- `{0}` via `{1}` -> `{2}`".format(
                route["id"],
                route["route_kind"],
                route["source_front_page_summary_path"],
            )
        )
        if route["available_supporting_surface_ids"]:
            report_lines.append(
                "  - supporting surfaces: `{0}`".format(
                    "`, `".join(route["available_supporting_surface_ids"])
                )
            )
        else:
            report_lines.append("  - supporting surfaces: none")

    if shelf_changes["changed"]:
        report_lines.extend(["", "## Shelf Drift"])
        if shelf_changes["title_changed"]:
            report_lines.append("- Shelf title changed")
        if shelf_changes["summary_changed"]:
            report_lines.append("- Shelf summary text changed")
        if shelf_changes["profile_changed"]:
            report_lines.append(
                "- Profile: `{0}` -> `{1}`".format(
                    shelf_status["baseline_profile"],
                    shelf_status["candidate_profile"],
                )
            )
        if has_array_changes(shelf_changes["world_name_changes"]):
            report_lines.append(
                "- Worlds: `+[{0}] -[{1}]`".format(
                    ", ".join(shelf_changes["world_name_changes"]["added"]),
                    ", ".join(shelf_changes["world_name_changes"]["removed"]),
                )
            )
        for label, change in (
            ("Core questions", shelf_changes["question_changes"]["core_question_changes"]),
            ("Compare questions", shelf_changes["question_changes"]["compare_question_changes"]),
            ("Next questions", shelf_changes["question_changes"]["next_question_changes"]),
        ):
            if has_array_changes(change):
                report_lines.append(
                    "- {0}: `+[{1}] -[{2}]`".format(
                        label,
                        ", ".join(change["added"]),
                        ", ".join(change["removed"]),
                    )
                )

    report_lines.extend(["", "## Collapse Surface"])
    if collapse_surface["changed"]:
        if collapse_surface["regressed_entries"]:
            report_lines.append(
                "- Regressed entries: `{0}`".format(
                    "`, `".join(collapse_surface["regressed_entries"])
                )
            )
        if collapse_surface["removed_worlds"]:
            report_lines.append(
                "- Removed worlds: `{0}`".format(
                    "`, `".join(collapse_surface["removed_worlds"])
                )
            )
        if collapse_surface["added_failed_entries"]:
            report_lines.append(
                "- Added failing entries: `{0}`".format(
                    "`, `".join(collapse_surface["added_failed_entries"])
                )
            )
        if collapse_surface["affected_worlds"]:
            report_lines.append(
                "- Affected worlds: `{0}`".format(
                    "`, `".join(collapse_surface["affected_worlds"])
                )
            )
        if collapse_surface["affected_profiles"]:
            report_lines.append(
                "- Affected profiles: `{0}`".format(
                    "`, `".join(collapse_surface["affected_profiles"])
                )
            )
        for narrative in collapse_surface["narratives"]:
            report_lines.append(f"- {narrative}")
    else:
        report_lines.append("- No shelf collapse-surface drift detected")

    report_lines.extend(
        [
            "",
            "## Entry Changes",
            "Anchor | Change | Impact | Verdict | World | Profile",
            "--- | --- | --- | --- | --- | ---",
        ]
    )
    if entry_changes:
        for change in entry_changes:
            report_lines.append(
                "{0} | {1} | {2} | {3}->{4} | {5} | {6}".format(
                    change["anchor_id"],
                    change["change_kind"],
                    change["impact"],
                    verdict_label(change["left_world_verdict"]),
                    verdict_label(change["right_world_verdict"]),
                    change["world_name"],
                    change["profile"],
                )
            )
    else:
        report_lines.append("none | unchanged | none | none->none | none | none")

    report_lines.extend(["", "## Questions"])
    for question in questions["compare_questions"]:
        report_lines.append(f"- compare: {question}")
    for question in questions["next_questions"]:
        report_lines.append(f"- next: {question}")

    write_text(report_path, "\n".join(report_lines) + "\n")

    check_lines = [
        f"summary: {summary_path}",
        f"result: {summary['result']}",
        f"shelf_verdict: {summary['shelf_verdict']}",
        "entry_changes: changed={0} added={1} removed={2}".format(
            entry_summary["changed_entry_count"],
            entry_summary["added_entry_count"],
            entry_summary["removed_entry_count"],
        ),
        "entry_impact: regressions={0} improvements={1} neutral={2}".format(
            entry_summary["regression_count"],
            entry_summary["improvement_count"],
            entry_summary["neutral_change_count"],
        ),
        "world_drift: added={0} removed={1}".format(
            len(shelf_changes["world_name_changes"]["added"]),
            len(shelf_changes["world_name_changes"]["removed"]),
        ),
        "collapse_surface: changed={0} regressed={1}".format(
            collapse_surface["changed"],
            len(collapse_surface["regressed_entries"]),
        ),
    ]
    write_text(check_path, "\n".join(check_lines) + "\n")

    return summary_path, summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two system compiler world shelves as one biography-index compare object."
    )
    parser.add_argument("--baseline", required=True, help="Baseline biography index summary path.")
    parser.add_argument("--candidate", required=True, help="Candidate biography index summary path.")
    parser.add_argument("--output-root", default="", help="Output root for shelf compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit summary.json output path.")
    parser.add_argument("--report-markdown", default="", help="Explicit report.md output path.")
    parser.add_argument("--check-text", default="", help="Explicit check.txt output path.")
    args = parser.parse_args()

    try:
        summary_path, summary = build_summary(args)
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[BIOGRAPHY-INDEX-COMPARE] summary={summary_path}")
    print(f"[BIOGRAPHY-INDEX-COMPARE] verdict={summary['shelf_verdict']}")
    print(f"[BIOGRAPHY-INDEX-COMPARE] regressions={summary['entry_summary']['regression_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
