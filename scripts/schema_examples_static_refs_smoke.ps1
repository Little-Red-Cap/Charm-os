param(
    [string]$SampleRoot = "schemas/examples",
    [string]$PythonExe = "python"
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Resolve-RepoPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$sampleRootPath = Resolve-RepoPath -Path $SampleRoot

if (-not (Test-Path -LiteralPath $sampleRootPath -PathType Container)) {
    throw "sample root not found: $sampleRootPath"
}

$pythonScript = @'
import json
import re
import sys
from pathlib import Path

repo = Path(sys.argv[1]).resolve()
sample_root = Path(sys.argv[2]).resolve()

static_prefixes = ("docs/", "schemas/", "Examples/", "scripts/", ".github/")
ignored_prefixes = ("out/", "cmake-build-")
url_pattern = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*://")
drive_pattern = re.compile(r"^[A-Za-z]:/")

missing = []
invalid_json = []
style_violations = []
checked = 0
static_refs = 0


def repo_label(path: Path) -> str:
    try:
        return path.relative_to(repo).as_posix()
    except ValueError:
        return path.as_posix()


def normalize_static_ref(value, location):
    raw = value.strip()
    text = raw.replace("\\", "/")
    if not text:
        return None
    if url_pattern.match(text) or text.startswith("/") or drive_pattern.match(text):
        return None
    if text.startswith(ignored_prefixes):
        return None
    if not text.startswith(static_prefixes):
        return None

    if "\\" in raw:
        style_violations.append((location, raw, text))

    # JSON samples may carry Markdown-style fragments for human navigation.
    return text.split("#", 1)[0].split("?", 1)[0]


def walk(value, location: str):
    global static_refs
    if isinstance(value, dict):
        for key, child in value.items():
            walk(child, f"{location}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            walk(child, f"{location}[{index}]")
    elif isinstance(value, str):
        candidate = normalize_static_ref(value, location)
        if candidate is None:
            return
        static_refs += 1
        if not (repo / candidate).exists():
            missing.append((location, candidate))


for sample in sorted(sample_root.glob("*.json")):
    checked += 1
    try:
        data = json.loads(sample.read_text(encoding="utf-8-sig"))
    except Exception as exc:  # Keep the smoke useful for malformed fixtures.
        invalid_json.append((repo_label(sample), str(exc)))
        continue
    walk(data, repo_label(sample))

if invalid_json:
    for sample, error in invalid_json:
        print(f"[SCHEMA-EXAMPLES-STATIC-REFS-SMOKE][INVALID-JSON] {sample}: {error}")

if missing:
    for location, candidate in missing:
        print(f"[SCHEMA-EXAMPLES-STATIC-REFS-SMOKE][MISSING] {location}: {candidate}")

if style_violations:
    for location, raw, normalized in style_violations:
        print(
            "[SCHEMA-EXAMPLES-STATIC-REFS-SMOKE][STYLE] "
            f"{location}: {raw} -> {normalized}"
        )

print(
    "[SCHEMA-EXAMPLES-STATIC-REFS-SMOKE] "
    f"checked={checked} static_refs={static_refs} "
    f"invalid_json={len(invalid_json)} missing={len(missing)} "
    f"style_violations={len(style_violations)}"
)

if invalid_json or missing or style_violations:
    raise SystemExit(1)
'@

$pythonScript | & $PythonExe - $repoRoot $sampleRootPath
if ($LASTEXITCODE -ne 0) {
    throw ("schema examples static refs smoke failed with exit code {0}" -f $LASTEXITCODE)
}
