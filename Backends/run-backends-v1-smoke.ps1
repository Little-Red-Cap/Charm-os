$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

$smokes = @(
    @{
        Name = 'backends_contract_topology_header_smoke'
        Source = 'Backends/contract/topology_header_smoke'
        Build = 'Backends/contract/topology_header_smoke/cmake-build-backends-contract-topology-header-smoke'
    },
    @{
        Name = 'backends_contract_evidence_header_smoke'
        Source = 'Backends/contract/evidence_header_smoke'
        Build = 'Backends/contract/evidence_header_smoke/cmake-build-backends-contract-evidence-header-smoke'
    },
    @{
        Name = 'backends_contract_console_output_header_smoke'
        Source = 'Backends/contract/console_output_header_smoke'
        Build = 'Backends/contract/console_output_header_smoke/cmake-build-backends-contract-console-output-header-smoke'
    },
    @{
        Name = 'backends_contract_block_storage_header_smoke'
        Source = 'Backends/contract/block_storage_header_smoke'
        Build = 'Backends/contract/block_storage_header_smoke/cmake-build-backends-contract-block-storage-header-smoke'
    },
    @{
        Name = 'backends_contract_raster_display_header_smoke'
        Source = 'Backends/contract/raster_display_header_smoke'
        Build = 'Backends/contract/raster_display_header_smoke/cmake-build-backends-contract-raster-display-header-smoke'
    },
    @{
        Name = 'backends_host_reference_smoke'
        Source = 'Backends/host/reference_smoke'
        Build = 'Backends/host/reference_smoke/cmake-build-backends-host-reference-smoke'
    },
    @{
        Name = 'backends_qemu_reference_smoke'
        Source = 'Backends/qemu/reference_smoke'
        Build = 'Backends/qemu/reference_smoke/cmake-build-backends-qemu-reference-smoke'
    },
    @{
        Name = 'backends_board_reference_smoke'
        Source = 'Backends/board/reference_smoke'
        Build = 'Backends/board/reference_smoke/cmake-build-backends-board-reference-smoke'
    },
    @{
        Name = 'capability_topology_bridge_smoke'
        Source = 'Examples/system/capability_topology_bridge_smoke'
        Build = 'Examples/system/capability_topology_bridge_smoke/cmake-build-capability-topology-bridge-smoke'
    },
    @{
        Name = 'console_output_provider_smoke'
        Source = 'Examples/system/console_output_provider_smoke'
        Build = 'Examples/system/console_output_provider_smoke/cmake-build-console-output-provider-smoke'
    },
    @{
        Name = 'block_storage_provider_smoke'
        Source = 'Examples/system/block_storage_provider_smoke'
        Build = 'Examples/system/block_storage_provider_smoke/cmake-build-block-storage-provider-smoke'
    }
)

foreach ($smoke in $smokes) {
    $sourceDir = Join-Path $repoRoot $smoke.Source
    $buildDir = Join-Path $repoRoot $smoke.Build

    Write-Host "[backends-v1] configure $($smoke.Name)"
    cmake -S $sourceDir -B $buildDir -G Ninja
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "[backends-v1] build $($smoke.Name)"
    cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "[backends-v1] test $($smoke.Name)"
    ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host '[backends-v1] ok'
