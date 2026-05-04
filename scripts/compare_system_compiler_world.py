import argparse
import json
from datetime import datetime
from pathlib import Path


WITNESS_BUNDLE_SCHEMA = "system_compiler.witness_bundle/v0"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def resolve_output_path(explicit: str, output_root: Path, default_name: str) -> Path:
    if explicit:
        return Path(explicit).resolve()
    return (output_root / default_name).resolve()


def ordered_unique(values):
    seen = set()
    result = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        result.append(value)
    return result


def string_array_changes(left, right):
    left_values = ordered_unique(str(value) for value in (left or []))
    right_values = ordered_unique(str(value) for value in (right or []))
    left_set = set(left_values)
    right_set = set(right_values)
    return {
        "added": [value for value in right_values if value not in left_set],
        "removed": [value for value in left_values if value not in right_set],
    }


def has_array_changes(change):
    return bool(change["added"] or change["removed"])


def nullable_text(value):
    if value is None:
        return None
    text = str(value)
    return text if text else None


def status_rank(status: str) -> int:
    return {
        "absent": -1,
        "ok": 0,
        "missing": 1,
        "fail": 2,
    }[status]


def load_witness_bundle(path: Path):
    data = load_json(path)
    if data.get("schema") != WITNESS_BUNDLE_SCHEMA:
        raise ValueError(f"unsupported witness bundle schema: {path}")
    if data.get("kind") != "system_compiler.witness_bundle":
        raise ValueError(f"unsupported witness bundle kind: {path}")
    return data


def world_state(bundle: dict) -> str:
    if bundle.get("result") == "ok":
        return "standing"
    return "collapsed"


def make_world_view(bundle: dict):
    world = bundle["world"]
    return {
        "name": str(world["name"]),
        "title": str(world["title"]),
        "summary": str(world["summary"]),
        "subject": {
            "profile": nullable_text(world.get("subject", {}).get("profile")),
            "board": nullable_text(world.get("subject", {}).get("board")),
            "active_facets": ordered_unique(str(value) for value in world.get("subject", {}).get("active_facets", [])),
        },
        "first_class_terms": ordered_unique(str(value) for value in world.get("first_class_terms", [])),
        "core_questions": ordered_unique(str(value) for value in world.get("core_questions", [])),
        "compare_questions": ordered_unique(str(value) for value in world.get("compare_questions", [])),
        "contract_refs": ordered_unique(str(value) for value in world.get("contract_refs", [])),
    }


def compare_worlds(baseline: dict, candidate: dict):
    left_world = baseline["world"]
    right_world = candidate["world"]

    left_subject = left_world.get("subject", {})
    right_subject = right_world.get("subject", {})

    subject_changes = []
    left_profile = nullable_text(left_subject.get("profile"))
    right_profile = nullable_text(right_subject.get("profile"))
    if left_profile != right_profile:
        subject_changes.append(f"profile:{left_profile}->{right_profile}")

    left_board = nullable_text(left_subject.get("board"))
    right_board = nullable_text(right_subject.get("board"))
    if left_board != right_board:
        subject_changes.append(f"board:{left_board}->{right_board}")

    active_facet_changes = string_array_changes(
        left_subject.get("active_facets", []),
        right_subject.get("active_facets", []),
    )
    if has_array_changes(active_facet_changes):
        subject_changes.append(
            "active_facets:+[{0}] -[{1}]".format(
                ", ".join(active_facet_changes["added"]),
                ", ".join(active_facet_changes["removed"]),
            )
        )

    world_changes = {
        "changed": False,
        "summary_changed": str(left_world.get("summary", "")) != str(right_world.get("summary", "")),
        "subject_changes": subject_changes,
        "first_class_term_changes": string_array_changes(
            left_world.get("first_class_terms", []),
            right_world.get("first_class_terms", []),
        ),
        "core_question_changes": string_array_changes(
            left_world.get("core_questions", []),
            right_world.get("core_questions", []),
        ),
        "compare_question_changes": string_array_changes(
            left_world.get("compare_questions", []),
            right_world.get("compare_questions", []),
        ),
        "contract_ref_changes": string_array_changes(
            left_world.get("contract_refs", []),
            right_world.get("contract_refs", []),
        ),
        "witness_plan_changes": string_array_changes(
            [entry.get("id", "") for entry in left_world.get("witness_plan", [])],
            [entry.get("id", "") for entry in right_world.get("witness_plan", [])],
        ),
    }

    world_changes["changed"] = (
        world_changes["summary_changed"]
        or bool(world_changes["subject_changes"])
        or has_array_changes(world_changes["first_class_term_changes"])
        or has_array_changes(world_changes["core_question_changes"])
        or has_array_changes(world_changes["compare_question_changes"])
        or has_array_changes(world_changes["contract_ref_changes"])
        or has_array_changes(world_changes["witness_plan_changes"])
    )

    return world_changes


def compare_entry_subject(left_subject: dict, right_subject: dict):
    changes = []
    left_case = nullable_text((left_subject or {}).get("case"))
    right_case = nullable_text((right_subject or {}).get("case"))
    if left_case != right_case:
        changes.append(f"case:{left_case}->{right_case}")

    left_profile = nullable_text((left_subject or {}).get("profile"))
    right_profile = nullable_text((right_subject or {}).get("profile"))
    if left_profile != right_profile:
        changes.append(f"profile:{left_profile}->{right_profile}")

    left_board = nullable_text((left_subject or {}).get("board"))
    right_board = nullable_text((right_subject or {}).get("board"))
    if left_board != right_board:
        changes.append(f"board:{left_board}->{right_board}")

    facet_changes = string_array_changes(
        (left_subject or {}).get("active_facets", []),
        (right_subject or {}).get("active_facets", []),
    )
    if has_array_changes(facet_changes):
        changes.append(
            "active_facets:+[{0}] -[{1}]".format(
                ", ".join(facet_changes["added"]),
                ", ".join(facet_changes["removed"]),
            )
        )
    return changes


def compare_witness_entry(left_entry: dict | None, right_entry: dict | None):
    left_present = left_entry is not None
    right_present = right_entry is not None

    anchor = right_entry if right_present else left_entry
    required = bool(anchor.get("required", False))
    kind = str(anchor.get("kind", ""))
    layer = str(anchor.get("layer", ""))
    label = str(anchor.get("label", ""))
    role = str(anchor.get("role", ""))
    witness_focus = ordered_unique(str(value) for value in anchor.get("witness_focus", []))

    left_status = str(left_entry["status"]) if left_present else "absent"
    right_status = str(right_entry["status"]) if right_present else "absent"

    if not left_present and right_present:
        change_kind = "added"
        impact = "regression" if required and right_status != "ok" else "neutral"
    elif left_present and not right_present:
        change_kind = "removed"
        impact = "regression" if required else "neutral"
    else:
        left_rank = status_rank(left_status)
        right_rank = status_rank(right_status)
        if right_rank > left_rank:
            change_kind = "changed"
            impact = "regression"
        elif right_rank < left_rank:
            change_kind = "changed"
            impact = "improvement"
        else:
            change_kind = "unchanged"
            impact = "none"

    metadata_changes = []
    for field in ("kind", "label", "role", "layer"):
        left_value = nullable_text(left_entry.get(field)) if left_present else None
        right_value = nullable_text(right_entry.get(field)) if right_present else None
        if left_value != right_value:
            metadata_changes.append(f"{field}:{left_value}->{right_value}")

    left_focus = left_entry.get("witness_focus", []) if left_present else []
    right_focus = right_entry.get("witness_focus", []) if right_present else []
    focus_changes = string_array_changes(left_focus, right_focus)
    if has_array_changes(focus_changes):
        metadata_changes.append(
            "witness_focus:+[{0}] -[{1}]".format(
                ", ".join(focus_changes["added"]),
                ", ".join(focus_changes["removed"]),
            )
        )

    left_source_path = nullable_text(left_entry.get("source_path")) if left_present else None
    right_source_path = nullable_text(right_entry.get("source_path")) if right_present else None
    if left_source_path != right_source_path:
        metadata_changes.append(f"source_path:{left_source_path}->{right_source_path}")

    subject_changes = compare_entry_subject(
        left_entry.get("subject", {}) if left_present else {},
        right_entry.get("subject", {}) if right_present else {},
    )

    observation_changes = string_array_changes(
        left_entry.get("observations", []) if left_present else [],
        right_entry.get("observations", []) if right_present else [],
    )
    artifact_ref_changes = string_array_changes(
        left_entry.get("artifact_refs", []) if left_present else [],
        right_entry.get("artifact_refs", []) if right_present else [],
    )

    if change_kind == "unchanged" and (
        metadata_changes
        or subject_changes
        or has_array_changes(observation_changes)
        or has_array_changes(artifact_ref_changes)
    ):
        change_kind = "changed"
        impact = "neutral"

    if change_kind == "unchanged":
        return None

    return {
        "id": str(anchor["id"]),
        "change_kind": change_kind,
        "impact": impact,
        "required": required,
        "kind": kind,
        "label": label,
        "layer": layer,
        "role": role,
        "witness_focus": witness_focus,
        "left_status": left_status,
        "right_status": right_status,
        "left_source_path": left_source_path,
        "right_source_path": right_source_path,
        "subject_changes": subject_changes,
        "observations_added": observation_changes["added"],
        "observations_removed": observation_changes["removed"],
        "artifact_refs_added": artifact_ref_changes["added"],
        "artifact_refs_removed": artifact_ref_changes["removed"],
        "metadata_changes": metadata_changes,
    }


def compare_witness_entries(baseline: dict, candidate: dict):
    left_entries = {
        str(entry["id"]): entry
        for entry in baseline.get("witness_entries", [])
    }
    right_entries = {
        str(entry["id"]): entry
        for entry in candidate.get("witness_entries", [])
    }

    order = ordered_unique(
        [str(entry["id"]) for entry in candidate.get("witness_entries", [])]
        + [str(entry["id"]) for entry in baseline.get("witness_entries", [])]
    )

    changes = []
    regression_count = 0
    improvement_count = 0
    neutral_change_count = 0
    added_count = 0
    removed_count = 0
    required_regression_count = 0

    for entry_id in order:
        change = compare_witness_entry(left_entries.get(entry_id), right_entries.get(entry_id))
        if change is None:
            continue
        changes.append(change)
        if change["change_kind"] == "added":
            added_count += 1
        if change["change_kind"] == "removed":
            removed_count += 1
        if change["impact"] == "regression":
            regression_count += 1
            if change["required"]:
                required_regression_count += 1
        elif change["impact"] == "improvement":
            improvement_count += 1
        else:
            neutral_change_count += 1

    left_entry_count = int(baseline.get("witness_summary", {}).get("entry_count", len(left_entries)))
    right_entry_count = int(candidate.get("witness_summary", {}).get("entry_count", len(right_entries)))
    comparable_count = len(order)

    summary = {
        "baseline_entry_count": left_entry_count,
        "candidate_entry_count": right_entry_count,
        "changed_entry_count": len(changes),
        "added_entry_count": added_count,
        "removed_entry_count": removed_count,
        "unchanged_entry_count": max(comparable_count - len(changes), 0),
        "regression_count": regression_count,
        "improvement_count": improvement_count,
        "neutral_change_count": neutral_change_count,
        "required_regression_count": required_regression_count,
    }

    return changes, summary


SESSION_OBSERVATION_DOMAINS = {
    "session_status": "verdict",
    "failure": "verdict",
    "semantic_host": "semantic",
    "machine_qemu": "machine",
    "trap_ingress": "machine",
    "runtime_loop": "runtime",
    "handoff_continuity": "handoff",
    "entry_status": "source",
    "source_path": "source",
}


def find_kernel_runtime_session_entry(bundle: dict):
    for entry in bundle.get("witness_entries", []):
        if str(entry.get("id", "")) == "kernel_runtime_session":
            return entry
        if str(entry.get("kind", "")) == "kernel_runtime_session":
            return entry
    return None


def parse_key_value_observations(entry: dict | None):
    values = {}
    if entry is None:
        return values

    for observation in entry.get("observations", []):
        text = str(observation)
        if "=" not in text:
            continue
        key, value = text.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key:
            values[key] = value
    return values


def normalize_bool_text(value: str | None):
    if value is None:
        return None
    lowered = str(value).lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    return None


def classify_session_fact_impact(key: str, left_value: str | None, right_value: str | None):
    if key in ("entry_status",):
        left_status = left_value or "absent"
        right_status = right_value or "absent"
        if left_status == "absent" and right_status != "absent":
            return "neutral" if right_status == "ok" else "regression"
        if left_status != "absent" and right_status == "absent":
            return "regression"
        left_rank = status_rank(left_status)
        right_rank = status_rank(right_status)
        if right_rank > left_rank:
            return "regression"
        if right_rank < left_rank:
            return "improvement"
        return "neutral"

    if key == "session_status":
        if left_value == "standing" and right_value != "standing":
            return "regression"
        if left_value != "standing" and right_value == "standing":
            return "improvement"
        return "neutral"

    if key == "failure":
        if left_value is None and right_value is not None:
            return "regression"
        if left_value is not None and right_value is None:
            return "improvement"
        return "neutral"

    left_bool = normalize_bool_text(left_value)
    right_bool = normalize_bool_text(right_value)
    if left_bool is True and right_bool is False:
        return "regression"
    if left_bool is False and right_bool is True:
        return "improvement"
    return "neutral"


def build_session_drift(baseline: dict, candidate: dict):
    left_entry = find_kernel_runtime_session_entry(baseline)
    right_entry = find_kernel_runtime_session_entry(candidate)
    left_present = left_entry is not None
    right_present = right_entry is not None

    left_status = str(left_entry.get("status", "absent")) if left_present else "absent"
    right_status = str(right_entry.get("status", "absent")) if right_present else "absent"
    left_observations = parse_key_value_observations(left_entry)
    right_observations = parse_key_value_observations(right_entry)

    fact_keys = ordered_unique(
        ["entry_status", "source_path"]
        + list(left_observations.keys())
        + list(right_observations.keys())
    )
    fact_changes = []
    for key in fact_keys:
        if key == "entry_status":
            left_value = left_status
            right_value = right_status
        elif key == "source_path":
            left_value = nullable_text(left_entry.get("source_path")) if left_present else None
            right_value = nullable_text(right_entry.get("source_path")) if right_present else None
        else:
            left_value = left_observations.get(key)
            right_value = right_observations.get(key)

        if left_value == right_value:
            continue

        domain = SESSION_OBSERVATION_DOMAINS.get(key, "session")
        fact_changes.append(
            {
                "key": key,
                "domain": domain,
                "baseline": left_value,
                "candidate": right_value,
                "impact": classify_session_fact_impact(key, left_value, right_value),
            }
        )

    drift_domains = ordered_unique(change["domain"] for change in fact_changes)
    narratives = []
    if not left_present and not right_present:
        narratives.append("no kernel_runtime_session witness appears in either bundle")
    elif left_present and not right_present:
        narratives.append("kernel_runtime_session witness was removed from the candidate bundle")
    elif not left_present and right_present:
        narratives.append("kernel_runtime_session witness was added to the candidate bundle")

    for change in fact_changes[:5]:
        narratives.append(
            "session `{0}` changed `{1}` -> `{2}` in domain `{3}`".format(
                change["key"],
                change["baseline"],
                change["candidate"],
                change["domain"],
            )
        )

    candidate_session_status = right_observations.get("session_status")
    if right_present and candidate_session_status and candidate_session_status != "standing":
        narratives.append(f"candidate session status is `{candidate_session_status}`")
    if right_present and right_status != "ok":
        narratives.append(f"candidate session witness status is `{right_status}`")
    if left_present and right_present and not fact_changes:
        narratives.append("kernel_runtime_session witness remains stable")

    return {
        "present": left_present or right_present,
        "changed": bool(fact_changes),
        "baseline_entry_status": left_status,
        "candidate_entry_status": right_status,
        "baseline_session_status": left_observations.get("session_status"),
        "candidate_session_status": right_observations.get("session_status"),
        "drift_domains": drift_domains,
        "fact_changes": fact_changes,
        "narratives": narratives,
    }


def build_contract_drift(baseline: dict, candidate: dict):
    baseline_contract_status = baseline.get("contract_status", {})
    candidate_contract_status = candidate.get("contract_status", {})

    present_ref_changes = string_array_changes(
        baseline_contract_status.get("present_refs", []),
        candidate_contract_status.get("present_refs", []),
    )
    missing_ref_changes = string_array_changes(
        baseline_contract_status.get("missing_refs", []),
        candidate_contract_status.get("missing_refs", []),
    )
    return {
        "changed": has_array_changes(present_ref_changes) or has_array_changes(missing_ref_changes),
        "present_ref_changes": present_ref_changes,
        "missing_ref_changes": missing_ref_changes,
    }


def build_bundle_status(baseline: dict, candidate: dict):
    baseline_contract_status = baseline.get("contract_status", {})
    candidate_contract_status = candidate.get("contract_status", {})
    baseline_witness_summary = baseline.get("witness_summary", {})
    candidate_witness_summary = candidate.get("witness_summary", {})

    return {
        "baseline_result": str(baseline.get("result", "fail")),
        "candidate_result": str(candidate.get("result", "fail")),
        "baseline_state": world_state(baseline),
        "candidate_state": world_state(candidate),
        "baseline_required_missing_count": int(baseline_witness_summary.get("required_missing_count", 0)),
        "candidate_required_missing_count": int(candidate_witness_summary.get("required_missing_count", 0)),
        "baseline_missing_contract_count": int(baseline_contract_status.get("missing_count", 0)),
        "candidate_missing_contract_count": int(candidate_contract_status.get("missing_count", 0)),
    }


def build_collapse_surface(witness_changes, contract_drift, candidate, session_drift=None):
    regressed_changes = [change for change in witness_changes if change["impact"] == "regression"]
    required_regressed = [change for change in regressed_changes if change["required"]]

    candidate_entries = candidate.get("witness_entries", [])
    failing_candidate_witnesses = [
        str(entry["id"])
        for entry in candidate_entries
        if str(entry.get("status", "")) == "fail"
    ]
    missing_candidate_witnesses = [
        str(entry["id"])
        for entry in candidate_entries
        if str(entry.get("status", "")) == "missing"
    ]

    affected_layers = ordered_unique(change["layer"] for change in regressed_changes if change["layer"])
    affected_focus = ordered_unique(
        focus
        for change in regressed_changes
        for focus in change.get("witness_focus", [])
    )

    narratives = []
    for change in regressed_changes[:3]:
        narratives.append(
            "witness `{0}` regressed `{1}` -> `{2}` on layer `{3}`".format(
                change["id"],
                change["left_status"],
                change["right_status"],
                change["layer"],
            )
        )
    for ref in contract_drift["missing_ref_changes"]["added"][:3]:
        narratives.append(f"candidate is missing contract ref `{ref}`")
    if candidate.get("result") != "ok":
        narratives.append("candidate witness bundle no longer stands as `ok`")
    if session_drift and session_drift.get("changed"):
        for narrative in session_drift.get("narratives", [])[:3]:
            narratives.append(narrative)

    return {
        "changed": bool(regressed_changes or contract_drift["missing_ref_changes"]["added"] or (session_drift or {}).get("changed")),
        "regressed_witnesses": [change["id"] for change in regressed_changes],
        "required_regressed_witnesses": [change["id"] for change in required_regressed],
        "failing_candidate_witnesses": failing_candidate_witnesses,
        "missing_candidate_witnesses": missing_candidate_witnesses,
        "added_missing_contract_refs": contract_drift["missing_ref_changes"]["added"],
        "affected_layers": affected_layers,
        "affected_focus": ordered_unique(
            affected_focus + list((session_drift or {}).get("drift_domains", []))
        ),
        "narratives": narratives,
    }


def build_next_questions(world_view, world_changes, witness_changes, contract_drift, collapse_surface, witness_summary):
    next_questions = []

    for change in [entry for entry in witness_changes if entry["impact"] == "regression"][:3]:
        next_questions.append(
            "Why did witness `{0}` regress from `{1}` to `{2}`?".format(
                change["id"],
                change["left_status"],
                change["right_status"],
            )
        )

    for contract_ref in contract_drift["missing_ref_changes"]["added"][:2]:
        next_questions.append(f"Which route should restore missing contract ref `{contract_ref}`?")

    if has_array_changes(world_changes["witness_plan_changes"]):
        next_questions.append(
            "Does witness plan drift reflect a real semantic change, or only an evidence-routing move?"
        )

    if not next_questions and witness_summary["improvement_count"] > 0:
        next_questions.append("Which improved witness should be promoted into a stronger canonical case next?")

    if not next_questions:
        next_questions.extend(world_view["compare_questions"][:1] or ["What counterfactual should this world face next?"])

    if collapse_surface["changed"] and collapse_surface["affected_layers"]:
        next_questions.append(
            "How do we restore the smallest collapse surface across layers `{0}`?".format(
                ", ".join(collapse_surface["affected_layers"])
            )
        )

    return ordered_unique(next_questions)


def determine_world_verdict(bundle_status, world_changes, contract_drift, witness_summary):
    if bundle_status["candidate_result"] != "ok" or bundle_status["candidate_state"] == "collapsed":
        return "collapsed"
    if witness_summary["regression_count"] > 0:
        return "drifted"
    if world_changes["changed"] or contract_drift["changed"]:
        if witness_summary["improvement_count"] > 0 and witness_summary["neutral_change_count"] == 0:
            return "improved"
        return "drifted"
    if witness_summary["improvement_count"] > 0:
        return "improved"
    return "standing"


def build_summary(args):
    baseline_path = Path(args.baseline).resolve()
    candidate_path = Path(args.candidate).resolve()
    output_root = Path(args.output_root or "out/system-compiler-world-compare").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    summary_path = resolve_output_path(args.summary, output_root, "summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "report.md")
    check_path = resolve_output_path(args.check_text, output_root, "check.txt")

    baseline = load_witness_bundle(baseline_path)
    candidate = load_witness_bundle(candidate_path)

    baseline_world_name = str(baseline["world"]["name"])
    candidate_world_name = str(candidate["world"]["name"])
    if baseline_world_name != candidate_world_name:
        raise ValueError(
            f"world mismatch: baseline={baseline_world_name} candidate={candidate_world_name}"
        )

    world_view = make_world_view(candidate)
    world_changes = compare_worlds(baseline, candidate)
    contract_drift = build_contract_drift(baseline, candidate)
    witness_changes, witness_summary = compare_witness_entries(baseline, candidate)
    bundle_status = build_bundle_status(baseline, candidate)
    session_drift = build_session_drift(baseline, candidate)
    collapse_surface = build_collapse_surface(witness_changes, contract_drift, candidate, session_drift)
    next_questions = build_next_questions(
        world_view,
        world_changes,
        witness_changes,
        contract_drift,
        collapse_surface,
        witness_summary,
    )

    summary = {
        "schema": "system_compiler.world_compare/v0",
        "kind": "system_compiler.world_compare",
        "generated_at_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "generator": "scripts/compare_system_compiler_world.py",
        "result": "ok",
        "world_verdict": determine_world_verdict(
            bundle_status,
            world_changes,
            contract_drift,
            witness_summary,
        ),
        "world": world_view,
        "artifact_context": {
            "baseline_witness_bundle": str(baseline_path),
            "candidate_witness_bundle": str(candidate_path),
            "output_root": str(output_root),
            "report_markdown_path": str(report_path),
            "check_text_path": str(check_path),
        },
        "bundle_status": bundle_status,
        "world_changes": world_changes,
        "contract_drift": contract_drift,
        "witness_summary": witness_summary,
        "witness_changes": witness_changes,
        "session_drift": session_drift,
        "collapse_surface": collapse_surface,
        "questions": {
            "compare_questions": world_view["compare_questions"],
            "next_questions": next_questions,
        },
        "violations": [],
    }

    write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")

    report_lines = [
        "# System Compiler World Compare",
        "",
        f"- Result: `{summary['result']}`",
        f"- World verdict: `{summary['world_verdict']}`",
        f"- World: `{world_view['name']}` (`{world_view['title']}`)",
        f"- Baseline witness bundle: `{baseline_path}`",
        f"- Candidate witness bundle: `{candidate_path}`",
        f"- Summary JSON: `{summary_path}`",
        "",
        "## World",
        f"- Summary: {world_view['summary']}",
    ]
    if world_view["subject"]["profile"]:
        report_lines.append(f"- Profile: `{world_view['subject']['profile']}`")
    if world_view["subject"]["board"]:
        report_lines.append(f"- Board: `{world_view['subject']['board']}`")
    if world_view["subject"]["active_facets"]:
        report_lines.append(
            "- Active facets: `{0}`".format("`, `".join(world_view["subject"]["active_facets"]))
        )

    report_lines.extend(
        [
            "",
            "## Verdict",
            f"- Baseline state: `{bundle_status['baseline_state']}` ({bundle_status['baseline_result']})",
            f"- Candidate state: `{bundle_status['candidate_state']}` ({bundle_status['candidate_result']})",
            f"- Witness changes: `changed={witness_summary['changed_entry_count']} regressions={witness_summary['regression_count']} improvements={witness_summary['improvement_count']}`",
            f"- Contract drift: `missing_added={len(contract_drift['missing_ref_changes']['added'])} missing_removed={len(contract_drift['missing_ref_changes']['removed'])}`",
        ]
    )

    if world_changes["changed"]:
        report_lines.extend(["", "## World Drift"])
        if world_changes["summary_changed"]:
            report_lines.append("- Summary text changed")
        for entry in world_changes["subject_changes"]:
            report_lines.append(f"- Subject: `{entry}`")
        if has_array_changes(world_changes["contract_ref_changes"]):
            report_lines.append(
                "- Contract refs: `+[{0}] -[{1}]`".format(
                    ", ".join(world_changes["contract_ref_changes"]["added"]),
                    ", ".join(world_changes["contract_ref_changes"]["removed"]),
                )
            )
        if has_array_changes(world_changes["witness_plan_changes"]):
            report_lines.append(
                "- Witness plan ids: `+[{0}] -[{1}]`".format(
                    ", ".join(world_changes["witness_plan_changes"]["added"]),
                    ", ".join(world_changes["witness_plan_changes"]["removed"]),
                )
            )

    report_lines.extend(["", "## Collapse Surface"])
    if collapse_surface["changed"]:
        report_lines.append(
            "- Regressed witnesses: `{0}`".format(
                "`, `".join(collapse_surface["regressed_witnesses"])
            )
        )
        if collapse_surface["required_regressed_witnesses"]:
            report_lines.append(
                "- Required regressions: `{0}`".format(
                    "`, `".join(collapse_surface["required_regressed_witnesses"])
                )
            )
        if collapse_surface["added_missing_contract_refs"]:
            report_lines.append(
                "- Missing contract refs added: `{0}`".format(
                    "`, `".join(collapse_surface["added_missing_contract_refs"])
                )
            )
        if collapse_surface["affected_layers"]:
            report_lines.append(
                "- Affected layers: `{0}`".format(
                    "`, `".join(collapse_surface["affected_layers"])
                )
            )
        if collapse_surface["affected_focus"]:
            report_lines.append(
                "- Affected focus: `{0}`".format(
                    "`, `".join(collapse_surface["affected_focus"])
                )
            )
        for narrative in collapse_surface["narratives"]:
            report_lines.append(f"- {narrative}")
    else:
        report_lines.append("- No collapse-surface drift detected")

    report_lines.extend(["", "## Kernel Runtime Session Drift"])
    if session_drift["present"]:
        report_lines.append(
            "- Entry status: `{0}` -> `{1}`".format(
                session_drift["baseline_entry_status"],
                session_drift["candidate_entry_status"],
            )
        )
        report_lines.append(
            "- Session status: `{0}` -> `{1}`".format(
                session_drift["baseline_session_status"],
                session_drift["candidate_session_status"],
            )
        )
        if session_drift["drift_domains"]:
            report_lines.append(
                "- Drift domains: `{0}`".format("`, `".join(session_drift["drift_domains"]))
            )
        for narrative in session_drift["narratives"]:
            report_lines.append(f"- {narrative}")
    else:
        report_lines.append("- No kernel_runtime_session witness is present in either bundle")

    if witness_changes:
        report_lines.extend(["", "## Witness Changes", "Id | Change | Impact | Status | Layer", "--- | --- | --- | --- | ---"])
        for change in witness_changes:
            report_lines.append(
                "{0} | {1} | {2} | {3}->{4} | {5}".format(
                    change["id"],
                    change["change_kind"],
                    change["impact"],
                    change["left_status"],
                    change["right_status"],
                    change["layer"],
                )
            )
    else:
        report_lines.extend(["", "## Witness Changes", "- No witness drift detected"])

    report_lines.extend(["", "## Questions"])
    for question in summary["questions"]["compare_questions"]:
        report_lines.append(f"- compare: {question}")
    for question in summary["questions"]["next_questions"]:
        report_lines.append(f"- next: {question}")

    write_text(report_path, "\n".join(report_lines) + "\n")

    check_lines = [
        f"summary: {summary_path}",
        f"world: {world_view['name']}",
        f"result: {summary['result']}",
        f"world_verdict: {summary['world_verdict']}",
        f"baseline_state: {bundle_status['baseline_state']}",
        f"candidate_state: {bundle_status['candidate_state']}",
        "witness_changes: regressions={0} improvements={1} neutral={2}".format(
            witness_summary["regression_count"],
            witness_summary["improvement_count"],
            witness_summary["neutral_change_count"],
        ),
        "collapse_surface: regressed={0} missing_contract_refs_added={1}".format(
            len(collapse_surface["regressed_witnesses"]),
            len(collapse_surface["added_missing_contract_refs"]),
        ),
        "session_drift: present={0} changed={1} domains={2}".format(
            session_drift["present"],
            session_drift["changed"],
            ",".join(session_drift["drift_domains"]),
        ),
    ]
    write_text(check_path, "\n".join(check_lines) + "\n")

    return summary_path, summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two witness bundles as one canonical world drift object."
    )
    parser.add_argument("--baseline", required=True, help="Baseline witness bundle summary path.")
    parser.add_argument("--candidate", required=True, help="Candidate witness bundle summary path.")
    parser.add_argument("--output-root", default="", help="Output root for world compare artifacts.")
    parser.add_argument("--summary", default="", help="Explicit summary.json output path.")
    parser.add_argument("--report-markdown", default="", help="Explicit report.md output path.")
    parser.add_argument("--check-text", default="", help="Explicit check.txt output path.")
    args = parser.parse_args()

    try:
        summary_path, summary = build_summary(args)
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[WORLD-COMPARE] summary={summary_path}")
    print(f"[WORLD-COMPARE] verdict={summary['world_verdict']}")
    print(f"[WORLD-COMPARE] regressions={summary['witness_summary']['regression_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
