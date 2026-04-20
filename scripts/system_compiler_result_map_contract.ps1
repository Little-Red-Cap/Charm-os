function Assert-ResultMapContractCondition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-ResultMapContractDirectPropertyValue {
    param(
        $Object,
        [string]$PropertyName
    )

    if ($null -eq $Object) {
        return $null
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($PropertyName)) {
            return $Object[$PropertyName]
        }

        return $null
    }

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Test-ResultMapContractHasDirectProperty {
    param(
        $Object,
        [string]$PropertyName
    )

    if ($null -eq $Object) {
        return $false
    }

    if ($Object -is [System.Collections.IDictionary]) {
        return $Object.Contains($PropertyName)
    }

    return ($null -ne $Object.PSObject.Properties[$PropertyName])
}

function Get-ResultMapContractPathValue {
    param(
        $Object,
        [string]$Path
    )

    if ($null -eq $Object -or [string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $current = $Object
    foreach ($segment in ($Path -split '\.')) {
        if ([string]::IsNullOrWhiteSpace($segment)) {
            continue
        }

        $current = Get-ResultMapContractDirectPropertyValue -Object $current -PropertyName $segment
        if ($null -eq $current) {
            return $null
        }
    }

    return $current
}

function Get-ResultMapContractExpectedDescriptor {
    param(
        [switch]$Comparison
    )

    if ($Comparison) {
        return [ordered]@{
            mode = 'comparison'
            summary_path = 'comparison.system_compiler_summary'
            input_bridge = [ordered]@{
                summary_field = 'comparison.system_input_summary'
                root_fields = @(
                    'system_spec_change_matrix'
                    'resolved_input_change_matrix'
                    'declared_fact_change_matrix'
                    'declared_contract_change_matrix'
                    'subject_fact_change_matrix'
                )
            }
            case_projection_fields = [ordered]@{
                formation = 'cases[*].formation_basis_changes'
                binding = 'cases[*].binding_summary_changes'
                bringup = 'cases[*].bringup_summary_changes'
            }
            stage_blocks = @(
                [ordered]@{
                    stage = 'formation'
                    block_field = 'formation_drift'
                    summary_field = 'comparison.system_formation_summary'
                    root_fields = @(
                        'status_change_matrix'
                        'declared_fact_change_matrix'
                        'declared_contract_change_matrix'
                        'subject_fact_change_matrix'
                        'unresolved_capability_change_matrix'
                        'blocked_node_change_matrix'
                        'blocker_change_matrix'
                        'blocker_reason_change_matrix'
                        'blocker_missing_requires_change_matrix'
                        'blocker_depends_on_change_matrix'
                    )
                }
                [ordered]@{
                    stage = 'binding'
                    block_field = 'binding_drift'
                    summary_field = 'comparison.binding_result_summary'
                    root_fields = @(
                        'binding_reason_change_matrix'
                        'resolved_capability_change_matrix'
                        'unresolved_capability_change_matrix'
                    )
                }
                [ordered]@{
                    stage = 'bringup'
                    block_field = 'bringup_drift'
                    summary_field = 'comparison.bringup_order_summary'
                    root_fields = @(
                        'bringup_phase_change_matrix'
                        'bringup_dependency_change_matrix'
                        'blocked_node_change_matrix'
                    )
                }
            )
        }
    }

    return [ordered]@{
        mode = 'summary'
        summary_path = 'system_compiler_summary'
        input_bridge = [ordered]@{
            summary_field = 'system_input_summary'
            root_fields = @(
                'case_kind_matrix'
                'resolved_profile_matrix'
                'resolved_board_matrix'
                'resolved_active_facet_matrix'
            )
        }
        case_projection_fields = [ordered]@{
            formation = 'cases[*].formation_basis'
            binding = 'cases[*].binding_summary'
            bringup = 'cases[*].bringup_summary'
        }
        stage_blocks = @(
            [ordered]@{
                stage = 'formation'
                block_field = 'formation_basis'
                summary_field = 'system_formation_summary'
                root_fields = @(
                    'status_counts'
                    'formed_case_count'
                    'blocked_case_count'
                    'unresolved_capability_matrix'
                    'blocked_node_matrix'
                    'blocker_matrix'
                    'blocker_reason_matrix'
                    'blocker_missing_requires_matrix'
                    'blocker_depends_on_matrix'
                )
            }
            [ordered]@{
                stage = 'binding'
                block_field = 'binding_basis'
                summary_field = 'binding_result_summary'
                root_fields = @(
                    'binding_reason_matrix'
                    'unresolved_capability_matrix'
                )
            }
            [ordered]@{
                stage = 'bringup'
                block_field = 'bringup_basis'
                summary_field = 'bringup_order_summary'
                root_fields = @(
                    'bringup_phase_matrix'
                    'bringup_dependency_matrix'
                    'blocked_node_matrix'
                )
            }
        )
    }
}

function Get-ResultMapCaseProjectionPropertyName {
    param(
        [string]$ProjectionPath
    )

    if ([string]::IsNullOrWhiteSpace($ProjectionPath)) {
        return ''
    }

    if ($ProjectionPath -match '^cases\[\*\]\.(.+)$') {
        return [string]$Matches[1]
    }

    return ''
}

function Assert-SystemCompilerResultMapContract {
    param(
        $RootSummary,
        [switch]$Comparison,
        [string]$Context = 'artifact_root'
    )

    $expected = Get-ResultMapContractExpectedDescriptor -Comparison:$Comparison
    $summaryObject = Get-ResultMapContractPathValue -Object $RootSummary -Path ([string]$expected.summary_path)
    Assert-ResultMapContractCondition ($null -ne $summaryObject) "$Context missing $($expected.summary_path)"

    $resultMap = Get-ResultMapContractDirectPropertyValue -Object $summaryObject -PropertyName 'result_map'
    Assert-ResultMapContractCondition ($null -ne $resultMap) "$Context missing $($expected.summary_path).result_map"
    Assert-ResultMapContractCondition ([string]$resultMap.kind -eq 'system_compiler_result_map/v0') "$Context result_map.kind mismatch"
    Assert-ResultMapContractCondition ([string]$resultMap.mode -eq [string]$expected.mode) "$Context result_map.mode mismatch"

    $inputBridge = Get-ResultMapContractDirectPropertyValue -Object $resultMap -PropertyName 'input_bridge'
    Assert-ResultMapContractCondition ($null -ne $inputBridge) "$Context result_map.input_bridge missing"
    Assert-ResultMapContractCondition ([string]$inputBridge.summary_field -eq [string]$expected.input_bridge.summary_field) "$Context result_map.input_bridge.summary_field mismatch"

    $inputSummary = Get-ResultMapContractPathValue -Object $RootSummary -Path ([string]$expected.input_bridge.summary_field)
    Assert-ResultMapContractCondition ($null -ne $inputSummary) "$Context input bridge target missing: $($expected.input_bridge.summary_field)"

    $actualInputRootFields = @(@($inputBridge.root_fields) | ForEach-Object { [string]$_ })
    Assert-ResultMapContractCondition ($actualInputRootFields.Count -eq @($expected.input_bridge.root_fields).Count) "$Context result_map.input_bridge.root_fields count mismatch"
    foreach ($field in @($expected.input_bridge.root_fields)) {
        Assert-ResultMapContractCondition (($actualInputRootFields -contains [string]$field)) "$Context result_map.input_bridge.root_fields missing $field"
        Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $summaryObject -PropertyName ([string]$field)) "$Context summary missing input bridge field $field"
        Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $inputSummary -PropertyName ([string]$field)) "$Context input summary missing field $field"
    }

    $caseProjectionFields = Get-ResultMapContractDirectPropertyValue -Object $resultMap -PropertyName 'case_projection_fields'
    Assert-ResultMapContractCondition ($null -ne $caseProjectionFields) "$Context result_map.case_projection_fields missing"

    $caseEntries = @(
        @(Get-ResultMapContractDirectPropertyValue -Object $summaryObject -PropertyName 'cases') |
            Where-Object { $null -ne $_ }
    )
    foreach ($stageName in @('formation', 'binding', 'bringup')) {
        $expectedProjection = [string](Get-ResultMapContractDirectPropertyValue -Object $expected.case_projection_fields -PropertyName $stageName)
        $actualProjection = [string](Get-ResultMapContractDirectPropertyValue -Object $caseProjectionFields -PropertyName $stageName)
        Assert-ResultMapContractCondition ($actualProjection -eq $expectedProjection) "$Context result_map.case_projection_fields.$stageName mismatch"

        $caseProjectionField = Get-ResultMapCaseProjectionPropertyName -ProjectionPath $actualProjection
        if (-not [string]::IsNullOrWhiteSpace($caseProjectionField) -and $caseEntries.Count -gt 0) {
            $projectedCase = @(
                @($caseEntries) |
                    Where-Object { Test-ResultMapContractHasDirectProperty -Object $_ -PropertyName $caseProjectionField } |
                    Select-Object -First 1
            ) | Select-Object -First 1
            Assert-ResultMapContractCondition ($null -ne $projectedCase) "$Context cases do not expose $caseProjectionField"
        }
    }

    $stageBlocks = @(
        @(Get-ResultMapContractDirectPropertyValue -Object $resultMap -PropertyName 'stage_blocks') |
            Where-Object { $null -ne $_ }
    )
    Assert-ResultMapContractCondition ($stageBlocks.Count -eq @($expected.stage_blocks).Count) "$Context result_map.stage_blocks count mismatch"

    foreach ($expectedStageBlock in @($expected.stage_blocks)) {
        $stageName = [string]$expectedStageBlock.stage
        $actualStageBlock = @(
            @($stageBlocks) |
                Where-Object { [string]$_.stage -eq $stageName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        Assert-ResultMapContractCondition ($null -ne $actualStageBlock) "$Context result_map.stage_blocks missing $stageName stage"
        Assert-ResultMapContractCondition ([string]$actualStageBlock.block_field -eq [string]$expectedStageBlock.block_field) "$Context result_map $stageName block_field mismatch"
        Assert-ResultMapContractCondition ([string]$actualStageBlock.summary_field -eq [string]$expectedStageBlock.summary_field) "$Context result_map $stageName summary_field mismatch"
        Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $summaryObject -PropertyName ([string]$expectedStageBlock.block_field)) "$Context summary missing stage block $($expectedStageBlock.block_field)"

        $stageSummary = Get-ResultMapContractPathValue -Object $RootSummary -Path ([string]$expectedStageBlock.summary_field)
        Assert-ResultMapContractCondition ($null -ne $stageSummary) "$Context stage summary missing: $($expectedStageBlock.summary_field)"

        $actualRootFields = @(@($actualStageBlock.root_fields) | ForEach-Object { [string]$_ })
        Assert-ResultMapContractCondition ($actualRootFields.Count -eq @($expectedStageBlock.root_fields).Count) "$Context result_map $stageName root_fields count mismatch"
        foreach ($field in @($expectedStageBlock.root_fields)) {
            Assert-ResultMapContractCondition (($actualRootFields -contains [string]$field)) "$Context result_map $stageName root_fields missing $field"
            Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $summaryObject -PropertyName ([string]$field)) "$Context summary missing $stageName root field $field"
        }
    }
}
