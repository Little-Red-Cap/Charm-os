from __future__ import annotations

import json
import re
from dataclasses import dataclass, asdict, field
from pathlib import Path

EXPORT_MODULE_RE = re.compile(
    r'^\s*export\s+module\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;',
    re.MULTILINE,
)

IMPORT_RE = re.compile(
    r'^\s*import\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;',
    re.MULTILINE,
)

# const char* cap_name = "system.reactor_pump";
CONST_STR_RE = re.compile(
    r'\b(?:const\s+char\s*\*|constexpr\s+auto)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*"([a-z0-9]+(?:[._][a-z0-9]+)+)"\s*;'
)

# provides[0] = init::cap_id(cap_name);
PROVIDES_ASSIGN_RE = re.compile(
    r'provides\s*\[[^\]]+\]\s*=\s*init::cap_id\s*\(\s*([A-Za-z_][A-Za-z0-9_]*|"[a-z0-9]+(?:[._][a-z0-9]+)+")\s*\)'
)

# requires_caps[0] = init::cap_id(eda_cap_name);
REQUIRES_ASSIGN_RE = re.compile(
    r'requires_caps\s*\[[^\]]+\]\s*=\s*init::cap_id\s*\(\s*([A-Za-z_][A-Za-z0-9_]*|"[a-z0-9]+(?:[._][a-z0-9]+)+")\s*\)'
)


@dataclass
class ModuleInfo:
    name: str
    path: str
    group: str
    imports: list[str] = field(default_factory=list)
    provides: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)


@dataclass
class CapabilityInfo:
    name: str
    group: str
    providers: list[str] = field(default_factory=list)
    required_by: list[str] = field(default_factory=list)
    module_paths: list[str] = field(default_factory=list)


def infer_group(path: str) -> str:
    p = path.replace("\\", "/").lower()
    if "/modules/system/" in p:
        return "System"
    if "/modules/io/" in p:
        return "IO"
    if "/modules/ui/" in p:
        return "UI"
    if "/modules/media/" in p:
        return "Media"
    if "/modules/platform/" in p:
        return "Platform"
    if "/modules/core/" in p:
        return "Core"
    return "Unknown"


def resolve_cap(token: str, const_map: dict[str, str]) -> str | None:
    token = token.strip()
    if token.startswith('"') and token.endswith('"'):
        return token.strip('"')
    return const_map.get(token)


def parse_cppm(path: Path) -> ModuleInfo | None:
    text = path.read_text(encoding="utf-8", errors="ignore")

    m = EXPORT_MODULE_RE.search(text)
    if not m:
        return None

    module_name = m.group(1)
    group = infer_group(str(path))

    imports = sorted(set(IMPORT_RE.findall(text)))

    const_map: dict[str, str] = {}
    for name, value in CONST_STR_RE.findall(text):
        const_map[name] = value

    provides: list[str] = []
    requires: list[str] = []

    for token in PROVIDES_ASSIGN_RE.findall(text):
        cap = resolve_cap(token, const_map)
        if cap:
            provides.append(cap)

    for token in REQUIRES_ASSIGN_RE.findall(text):
        cap = resolve_cap(token, const_map)
        if cap:
            requires.append(cap)

    return ModuleInfo(
        name=module_name,
        path=str(path),
        group=group,
        imports=sorted(set(imports)),
        provides=sorted(set(provides)),
        requires=sorted(set(requires)),
    )


def build_capabilities(modules: list[ModuleInfo]) -> dict[str, CapabilityInfo]:
    caps: dict[str, CapabilityInfo] = {}

    for mod in modules:
        for cap in mod.provides:
            entry = caps.setdefault(cap, CapabilityInfo(name=cap, group=mod.group))
            entry.group = entry.group if entry.group != "Unknown" else mod.group
            if mod.name not in entry.providers:
                entry.providers.append(mod.name)
            if mod.path not in entry.module_paths:
                entry.module_paths.append(mod.path)

        for dep in mod.requires:
            entry = caps.setdefault(dep, CapabilityInfo(name=dep, group="Unknown"))
            if mod.name not in entry.required_by:
                entry.required_by.append(mod.name)

    return caps


def write_json(modules: list[ModuleInfo], caps: dict[str, CapabilityInfo], out_path: Path) -> None:
    data = {
        "modules": [asdict(m) for m in modules],
        "capabilities": [asdict(c) for c in sorted(caps.values(), key=lambda x: x.name)],
    }
    out_path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def write_markdown(modules: list[ModuleInfo], caps: dict[str, CapabilityInfo], out_path: Path) -> None:
    lines: list[str] = []
    lines.append("# Generated Capability Map")
    lines.append("")
    lines.append("> Auto-generated from `Modules/**/*.cppm`.")
    lines.append("")

    lines.append("## Capabilities")
    lines.append("")
    lines.append("| Capability | Group | Provider Modules | Requires By |")
    lines.append("|---|---|---|---|")

    for cap in sorted(caps.values(), key=lambda x: x.name):
        providers = "<br>".join(sorted(cap.providers)) if cap.providers else "-"
        required_by = "<br>".join(sorted(cap.required_by)) if cap.required_by else "-"
        lines.append(f"| `{cap.name}` | {cap.group} | {providers} | {required_by} |")

    lines.append("")
    lines.append("## Modules")
    lines.append("")
    lines.append("| Module | Group | Provides | Requires |")
    lines.append("|---|---|---|---|")

    for mod in sorted(modules, key=lambda x: x.name):
        provides = "<br>".join(f"`{x}`" for x in mod.provides) if mod.provides else "-"
        requires = "<br>".join(f"`{x}`" for x in mod.requires) if mod.requires else "-"
        lines.append(f"| `{mod.name}` | {mod.group} | {provides} | {requires} |")

    out_path.write_text("\n".join(lines), encoding="utf-8")


def sanitize_mermaid_id(name: str) -> str:
    return name.replace(".", "_").replace("-", "_")


def write_mermaid(modules: list[ModuleInfo], out_path: Path) -> None:
    lines: list[str] = []
    lines.append("graph LR")

    seen: set[tuple[str, str]] = set()

    for mod in modules:
        for provided in mod.provides:
            for required in mod.requires:
                edge = (provided, required)
                if edge in seen:
                    continue
                seen.add(edge)
                a = sanitize_mermaid_id(provided)
                b = sanitize_mermaid_id(required)
                lines.append(f'    {a}["{provided}"] --> {b}["{required}"]')

    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    root = Path(".")
    module_files = sorted(root.glob("Modules/**/*.cppm"))

    modules = []
    for path in module_files:
        info = parse_cppm(path)
        if info is not None:
            modules.append(info)

    caps = build_capabilities(modules)

    out_dir = Path("docs/generated")
    out_dir.mkdir(parents=True, exist_ok=True)

    write_json(modules, caps, out_dir / "capability_data.generated.json")
    write_markdown(modules, caps, out_dir / "capability_map.generated.md")
    write_mermaid(modules, out_dir / "capability_graph.generated.mmd")

    print(f"Scanned modules: {len(modules)}")
    print(f"Capabilities: {len(caps)}")
    print(f"Output: {out_dir}")


if __name__ == "__main__":
    main()
