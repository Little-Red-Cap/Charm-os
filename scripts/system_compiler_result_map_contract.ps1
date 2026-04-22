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

function Test-ResultMapContractPathExists {
    param(
        $Object,
        [string]$Path
    )

    if ($null -eq $Object -or [string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $current = $Object
    foreach ($segment in ($Path -split '\.')) {
        if ([string]::IsNullOrWhiteSpace($segment)) {
            continue
        }

        if (-not (Test-ResultMapContractHasDirectProperty -Object $current -PropertyName $segment)) {
            return $false
        }

        $current = Get-ResultMapContractDirectPropertyValue -Object $current -PropertyName $segment
    }

    return $true
}

function New-ResultMapContractFieldRelationDescriptor {
    param(
        [string]$RootField,
        [string]$BlockFieldPath,
        [string]$BlockRelation = 'none',
        [string]$SummaryFieldPath,
        [string]$SummaryRelation = 'none'
    )

    return [ordered]@{
        root_field = [string]$RootField
        block_field_path = if ([string]::IsNullOrWhiteSpace($BlockFieldPath)) { $null } else { [string]$BlockFieldPath }
        block_relation = [string]$BlockRelation
        summary_field_path = if ([string]::IsNullOrWhiteSpace($SummaryFieldPath)) { $null } else { [string]$SummaryFieldPath }
        summary_relation = [string]$SummaryRelation
    }
}

function New-ResultMapContractCaseProjectionSourceCandidateDescriptor {
    param(
        [string]$Stage,
        [string]$FieldPath,
        [string]$Relation = 'same_field'
    )

    return [ordered]@{
        stage = [string]$Stage
        field_path = [string]$FieldPath
        relation = [string]$Relation
    }
}

function New-ResultMapContractCaseProjectionFieldRelationDescriptor {
    param(
        [string]$ProjectionField,
        [object[]]$SourceCandidates
    )

    return [ordered]@{
        projection_field = [string]$ProjectionField
        source_candidates = @($SourceCandidates)
    }
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
                field_relations = @(
                    New-ResultMapContractFieldRelationDescriptor -RootField 'system_spec_change_matrix' -SummaryFieldPath 'system_spec_change_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'resolved_input_change_matrix' -SummaryFieldPath 'resolved_input_change_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'declared_fact_change_matrix' -SummaryFieldPath 'declared_fact_change_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'declared_contract_change_matrix' -SummaryFieldPath 'declared_contract_change_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'subject_fact_change_matrix' -SummaryFieldPath 'subject_fact_change_matrix' -SummaryRelation 'same_field'
                )
            }
            case_projection_fields = [ordered]@{
                formation = 'cases[*].formation_basis_changes'
                binding = 'cases[*].binding_summary_changes'
                bringup = 'cases[*].bringup_summary_changes'
            }
            case_projection_field_relations = [ordered]@{
                formation = @(
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'system_spec_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'system_spec_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'resolved_input_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'resolved_input_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'declared_fact_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'declared_fact_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'declared_contract_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'declared_contract_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'subject_fact_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'subject_fact_changes'
                    )
                )
                binding = @(
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'summary_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'summary_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'binding_change_count' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'binding_change_count'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'capabilities_changed' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'capabilities_changed'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'resolved_capability_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'resolved_capability_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'unresolved_capability_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'unresolved_capability_changes'
                    )
                )
                bringup = @(
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'summary_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'summary_changes'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'entry_change_count' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'entry_change_count'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'nodes_changed' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'nodes_changed'
                    )
                    New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'blocked_node_changes' -SourceCandidates @(
                        New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'blocked_node_changes'
                    )
                )
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
                    field_relations = @(
                        New-ResultMapContractFieldRelationDescriptor -RootField 'status_change_matrix' -BlockFieldPath 'status_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'status_change_matrix' -SummaryRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'declared_fact_change_matrix' -BlockFieldPath 'declared_fact_change_matrix' -BlockRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'declared_contract_change_matrix' -BlockFieldPath 'declared_contract_change_matrix' -BlockRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'subject_fact_change_matrix' -BlockFieldPath 'subject_fact_change_matrix' -BlockRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'unresolved_capability_change_matrix' -BlockFieldPath 'unresolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_change_matrix' -SummaryRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocked_node_change_matrix' -BlockFieldPath 'blocked_node_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_change_matrix' -SummaryRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_change_matrix' -BlockFieldPath 'blocker_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocker_change_matrix' -SummaryRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_reason_change_matrix' -BlockFieldPath 'blocker_reason_change_matrix' -BlockRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_missing_requires_change_matrix' -BlockFieldPath 'blocker_missing_requires_change_matrix' -BlockRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_depends_on_change_matrix' -BlockFieldPath 'blocker_depends_on_change_matrix' -BlockRelation 'same_field'
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
                    field_relations = @(
                        New-ResultMapContractFieldRelationDescriptor -RootField 'binding_reason_change_matrix' -BlockFieldPath 'reason_change_matrix' -BlockRelation 'field_alias'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'resolved_capability_change_matrix' -BlockFieldPath 'resolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'resolved_capability_change_matrix' -SummaryRelation 'same_field'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'unresolved_capability_change_matrix' -BlockFieldPath 'unresolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_change_matrix' -SummaryRelation 'same_field'
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
                    field_relations = @(
                        New-ResultMapContractFieldRelationDescriptor -RootField 'bringup_phase_change_matrix' -BlockFieldPath 'phase_change_matrix' -BlockRelation 'field_alias'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'bringup_dependency_change_matrix' -BlockFieldPath 'dependency_change_matrix' -BlockRelation 'field_alias'
                        New-ResultMapContractFieldRelationDescriptor -RootField 'blocked_node_change_matrix' -BlockFieldPath 'blocked_node_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_change_matrix' -SummaryRelation 'same_field'
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
            field_relations = @(
                New-ResultMapContractFieldRelationDescriptor -RootField 'case_kind_matrix' -SummaryFieldPath 'case_kind_matrix' -SummaryRelation 'same_field'
                New-ResultMapContractFieldRelationDescriptor -RootField 'resolved_profile_matrix' -SummaryFieldPath 'resolved_profile_matrix' -SummaryRelation 'same_field'
                New-ResultMapContractFieldRelationDescriptor -RootField 'resolved_board_matrix' -SummaryFieldPath 'resolved_board_matrix' -SummaryRelation 'same_field'
                New-ResultMapContractFieldRelationDescriptor -RootField 'resolved_active_facet_matrix' -SummaryFieldPath 'resolved_active_facet_matrix' -SummaryRelation 'same_field'
            )
        }
        case_projection_fields = [ordered]@{
            formation = 'cases[*].formation_basis'
            binding = 'cases[*].binding_summary'
            bringup = 'cases[*].bringup_summary'
        }
        case_projection_field_relations = [ordered]@{
            formation = @(
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'case_kind' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'case_kind'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'case_kind'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'declared_fact_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'declared_fact_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'declared_fact_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'declared_contract_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'declared_contract_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'declared_contract_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'subject_fact_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'subject_fact_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_input' -FieldPath 'subject_fact_count'
                )
            )
            binding = @(
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'required_binding_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'required_binding_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'required_binding_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'resolved_binding_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'resolved_binding_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'resolved_binding_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'unresolved_binding_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'unresolved_binding_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'unresolved_binding_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'resolved_capabilities' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'resolved_capabilities'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'unresolved_capabilities' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'unresolved_capabilities'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'binding_result' -FieldPath 'unresolved_capabilities'
                )
            )
            bringup = @(
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'ordered_node_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'ordered_node_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'ordered_node_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'blocked_node_count' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'blocked_node_count'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'blocked_node_count'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'blocked_nodes' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'system_formation' -FieldPath 'blocked_nodes'
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'blocked_nodes'
                )
                New-ResultMapContractCaseProjectionFieldRelationDescriptor -ProjectionField 'phase_counts' -SourceCandidates @(
                    New-ResultMapContractCaseProjectionSourceCandidateDescriptor -Stage 'bringup_order' -FieldPath 'phase_counts'
                )
            )
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
                field_relations = @(
                    New-ResultMapContractFieldRelationDescriptor -RootField 'status_counts' -BlockFieldPath 'status_counts' -BlockRelation 'same_field' -SummaryFieldPath 'status_counts' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'formed_case_count' -BlockFieldPath 'formed_case_count' -BlockRelation 'same_field' -SummaryFieldPath 'formed_case_count' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocked_case_count' -BlockFieldPath 'blocked_case_count' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_case_count' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'unresolved_capability_matrix' -BlockFieldPath 'unresolved_capability_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocked_node_matrix' -BlockFieldPath 'blocked_node_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_matrix' -BlockFieldPath 'blocker_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocker_matrix' -SummaryRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_reason_matrix' -BlockFieldPath 'blocker_reason_matrix' -BlockRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_missing_requires_matrix' -BlockFieldPath 'blocker_missing_requires_matrix' -BlockRelation 'same_field'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocker_depends_on_matrix' -BlockFieldPath 'blocker_depends_on_matrix' -BlockRelation 'same_field'
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
                field_relations = @(
                    New-ResultMapContractFieldRelationDescriptor -RootField 'binding_reason_matrix' -BlockFieldPath 'reason_matrix' -BlockRelation 'field_alias'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'unresolved_capability_matrix' -BlockFieldPath 'unresolved_capability_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_matrix' -SummaryRelation 'same_field'
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
                field_relations = @(
                    New-ResultMapContractFieldRelationDescriptor -RootField 'bringup_phase_matrix' -BlockFieldPath 'phase_matrix' -BlockRelation 'field_alias'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'bringup_dependency_matrix' -BlockFieldPath 'dependency_matrix' -BlockRelation 'field_alias'
                    New-ResultMapContractFieldRelationDescriptor -RootField 'blocked_node_matrix' -BlockFieldPath 'blocked_node_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_matrix' -SummaryRelation 'same_field'
                )
            }
        )
    }
}

function Get-ResultMapContractCaseStageSummaryPath {
    param(
        [string]$Stage,
        [switch]$Comparison
    )

    $prefix = if ($Comparison) { 'comparison.' } else { '' }
    switch ([string]$Stage) {
        'system_input' { return "${prefix}system_input_summary" }
        'binding_result' { return "${prefix}binding_result_summary" }
        'bringup_order' { return "${prefix}bringup_order_summary" }
        'system_formation' { return "${prefix}system_formation_summary" }
        default { return '' }
    }
}

function Assert-ResultMapContractCaseProjectionFieldRelations {
    param(
        $RootSummary,
        $CaseProjectionFieldRelations,
        $CaseProjectionFields,
        $ExpectedCaseProjectionFieldRelations,
        [switch]$Comparison,
        [string]$Context
    )

    $stageNames = @('formation', 'binding', 'bringup')
    foreach ($stageName in @($stageNames)) {
        $projectionPath = [string](Get-ResultMapContractDirectPropertyValue -Object $CaseProjectionFields -PropertyName $stageName)
        $projectionPropertyName = Get-ResultMapCaseProjectionPropertyName -ProjectionPath $projectionPath
        Assert-ResultMapContractCondition (-not [string]::IsNullOrWhiteSpace($projectionPropertyName)) "$Context $stageName projection path invalid"

        $expectedFieldRelations = @(
            @(Get-ResultMapContractDirectPropertyValue -Object $ExpectedCaseProjectionFieldRelations -PropertyName $stageName) |
                Where-Object { $null -ne $_ }
        )
        $actualFieldRelations = @(
            @(Get-ResultMapContractDirectPropertyValue -Object $CaseProjectionFieldRelations -PropertyName $stageName) |
                Where-Object { $null -ne $_ }
        )
        Assert-ResultMapContractCondition ($actualFieldRelations.Count -eq $expectedFieldRelations.Count) "$Context $stageName case_projection_field_relations count mismatch"

        $summaryObject = if ($Comparison) {
            Get-ResultMapContractPathValue -Object $RootSummary -Path 'comparison.system_compiler_summary'
        } else {
            Get-ResultMapContractPathValue -Object $RootSummary -Path 'system_compiler_summary'
        }
        $caseEntries = @(
            @(Get-ResultMapContractDirectPropertyValue -Object $summaryObject -PropertyName 'cases') |
                Where-Object { $null -ne $_ }
        )
        $projectionCaseEntry = @(
            @($caseEntries) |
                Where-Object { Test-ResultMapContractHasDirectProperty -Object $_ -PropertyName $projectionPropertyName } |
                Where-Object { $null -ne (Get-ResultMapContractDirectPropertyValue -Object $_ -PropertyName $projectionPropertyName) } |
                Select-Object -First 1
        ) | Select-Object -First 1
        Assert-ResultMapContractCondition ($null -ne $projectionCaseEntry) "$Context $stageName missing non-null case projection"

        $projectionObject = Get-ResultMapContractDirectPropertyValue -Object $projectionCaseEntry -PropertyName $projectionPropertyName
        foreach ($expectedFieldRelation in @($expectedFieldRelations)) {
            $projectionField = [string]$expectedFieldRelation.projection_field
            $actualFieldRelation = @(
                @($actualFieldRelations) |
                    Where-Object { [string]$_.projection_field -eq $projectionField } |
                    Select-Object -First 1
            ) | Select-Object -First 1

            Assert-ResultMapContractCondition ($null -ne $actualFieldRelation) "$Context $stageName missing projection field relation $projectionField"
            Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $projectionObject -PropertyName $projectionField) "$Context $stageName projection missing field $projectionField"

            $expectedSourceCandidates = @(
                @($expectedFieldRelation.source_candidates) |
                    Where-Object { $null -ne $_ }
            )
            $actualSourceCandidates = @(
                @(Get-ResultMapContractDirectPropertyValue -Object $actualFieldRelation -PropertyName 'source_candidates') |
                    Where-Object { $null -ne $_ }
            )
            Assert-ResultMapContractCondition ($actualSourceCandidates.Count -eq $expectedSourceCandidates.Count) "$Context $stageName $projectionField source_candidates count mismatch"

            foreach ($expectedSourceCandidate in @($expectedSourceCandidates)) {
                $expectedStage = [string]$expectedSourceCandidate.stage
                $expectedFieldPath = [string]$expectedSourceCandidate.field_path
                $expectedRelation = [string]$expectedSourceCandidate.relation

                $actualSourceCandidate = @(
                    @($actualSourceCandidates) |
                        Where-Object {
                            ([string]$_.stage -eq $expectedStage) -and
                            ([string]$_.field_path -eq $expectedFieldPath)
                        } |
                        Select-Object -First 1
                ) | Select-Object -First 1

                Assert-ResultMapContractCondition ($null -ne $actualSourceCandidate) "$Context $stageName $projectionField missing source candidate $expectedStage::$expectedFieldPath"
                Assert-ResultMapContractCondition ([string]$actualSourceCandidate.relation -eq $expectedRelation) "$Context $stageName $projectionField relation mismatch for $expectedStage::$expectedFieldPath"

                $stageSummaryPath = Get-ResultMapContractCaseStageSummaryPath -Stage $expectedStage -Comparison:$Comparison
                Assert-ResultMapContractCondition (-not [string]::IsNullOrWhiteSpace($stageSummaryPath)) "$Context $stageName $projectionField unknown stage summary path for $expectedStage"
                $stageSummary = Get-ResultMapContractPathValue -Object $RootSummary -Path $stageSummaryPath
                Assert-ResultMapContractCondition ($null -ne $stageSummary) "$Context $stageName $projectionField missing stage summary $stageSummaryPath"
                $stageCases = @(
                    @(Get-ResultMapContractDirectPropertyValue -Object $stageSummary -PropertyName 'cases') |
                        Where-Object { $null -ne $_ }
                )
                $stageCaseEntry = @(
                    @($stageCases) |
                        Where-Object { Test-ResultMapContractPathExists -Object $_ -Path $expectedFieldPath } |
                        Select-Object -First 1
                ) | Select-Object -First 1
                Assert-ResultMapContractCondition ($null -ne $stageCaseEntry) "$Context $stageName $projectionField missing stage case field path $expectedStage::$expectedFieldPath"
            }
        }
    }
}

function Assert-ResultMapContractFieldRelations {
    param(
        $FieldRelations,
        [object[]]$ExpectedFieldRelations,
        $StageBlockObject,
        $StageSummaryObject,
        [string]$Context
    )

    $actualFieldRelations = @(
        @($FieldRelations) |
            Where-Object { $null -ne $_ }
    )
    Assert-ResultMapContractCondition ($actualFieldRelations.Count -eq @($ExpectedFieldRelations).Count) "$Context field_relations count mismatch"

    foreach ($expectedRelation in @($ExpectedFieldRelations)) {
        $rootField = [string]$expectedRelation.root_field
        $actualRelation = @(
            @($actualFieldRelations) |
                Where-Object { [string]$_.root_field -eq $rootField } |
                Select-Object -First 1
        ) | Select-Object -First 1

        Assert-ResultMapContractCondition ($null -ne $actualRelation) "$Context field_relations missing $rootField"
        Assert-ResultMapContractCondition ([string]$actualRelation.block_relation -eq [string]$expectedRelation.block_relation) "$Context field_relations $rootField block_relation mismatch"
        Assert-ResultMapContractCondition ([string]$actualRelation.summary_relation -eq [string]$expectedRelation.summary_relation) "$Context field_relations $rootField summary_relation mismatch"

        $expectedBlockFieldPath = if ($null -eq $expectedRelation.block_field_path) { $null } else { [string]$expectedRelation.block_field_path }
        $actualBlockFieldPath = if ($null -eq $actualRelation.block_field_path) { $null } else { [string]$actualRelation.block_field_path }
        $expectedSummaryFieldPath = if ($null -eq $expectedRelation.summary_field_path) { $null } else { [string]$expectedRelation.summary_field_path }
        $actualSummaryFieldPath = if ($null -eq $actualRelation.summary_field_path) { $null } else { [string]$actualRelation.summary_field_path }

        Assert-ResultMapContractCondition ($actualBlockFieldPath -eq $expectedBlockFieldPath) "$Context field_relations $rootField block_field_path mismatch"
        Assert-ResultMapContractCondition ($actualSummaryFieldPath -eq $expectedSummaryFieldPath) "$Context field_relations $rootField summary_field_path mismatch"

        if ([string]$expectedRelation.block_relation -eq 'none') {
            Assert-ResultMapContractCondition ([string]::IsNullOrWhiteSpace($actualBlockFieldPath)) "$Context field_relations $rootField block_field_path must be empty when block_relation is none"
        } else {
            Assert-ResultMapContractCondition ($null -ne $StageBlockObject) "$Context field_relations $rootField missing stage block object"
            Assert-ResultMapContractCondition (-not [string]::IsNullOrWhiteSpace($actualBlockFieldPath)) "$Context field_relations $rootField block_field_path missing"
            Assert-ResultMapContractCondition (Test-ResultMapContractPathExists -Object $StageBlockObject -Path $actualBlockFieldPath) "$Context stage block missing field path $actualBlockFieldPath"
        }

        if ([string]$expectedRelation.summary_relation -eq 'none') {
            Assert-ResultMapContractCondition ([string]::IsNullOrWhiteSpace($actualSummaryFieldPath)) "$Context field_relations $rootField summary_field_path must be empty when summary_relation is none"
        } else {
            Assert-ResultMapContractCondition ($null -ne $StageSummaryObject) "$Context field_relations $rootField missing stage summary object"
            Assert-ResultMapContractCondition (-not [string]::IsNullOrWhiteSpace($actualSummaryFieldPath)) "$Context field_relations $rootField summary_field_path missing"
            Assert-ResultMapContractCondition (Test-ResultMapContractPathExists -Object $StageSummaryObject -Path $actualSummaryFieldPath) "$Context stage summary missing field path $actualSummaryFieldPath"
        }
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
    Assert-ResultMapContractFieldRelations -FieldRelations (Get-ResultMapContractDirectPropertyValue -Object $inputBridge -PropertyName 'field_relations') -ExpectedFieldRelations @($expected.input_bridge.field_relations) -StageSummaryObject $inputSummary -Context "$Context result_map.input_bridge"

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
    Assert-ResultMapContractCaseProjectionFieldRelations -RootSummary $RootSummary -CaseProjectionFieldRelations (Get-ResultMapContractDirectPropertyValue -Object $resultMap -PropertyName 'case_projection_field_relations') -CaseProjectionFields $caseProjectionFields -ExpectedCaseProjectionFieldRelations $expected.case_projection_field_relations -Comparison:$Comparison -Context "$Context result_map.case_projection"

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
        $stageBlockObject = Get-ResultMapContractDirectPropertyValue -Object $summaryObject -PropertyName ([string]$expectedStageBlock.block_field)
        Assert-ResultMapContractCondition ($null -ne $stageBlockObject) "$Context stage block missing: $($expectedStageBlock.block_field)"

        $actualRootFields = @(@($actualStageBlock.root_fields) | ForEach-Object { [string]$_ })
        Assert-ResultMapContractCondition ($actualRootFields.Count -eq @($expectedStageBlock.root_fields).Count) "$Context result_map $stageName root_fields count mismatch"
        foreach ($field in @($expectedStageBlock.root_fields)) {
            Assert-ResultMapContractCondition (($actualRootFields -contains [string]$field)) "$Context result_map $stageName root_fields missing $field"
            Assert-ResultMapContractCondition (Test-ResultMapContractHasDirectProperty -Object $summaryObject -PropertyName ([string]$field)) "$Context summary missing $stageName root field $field"
        }
        Assert-ResultMapContractFieldRelations -FieldRelations (Get-ResultMapContractDirectPropertyValue -Object $actualStageBlock -PropertyName 'field_relations') -ExpectedFieldRelations @($expectedStageBlock.field_relations) -StageBlockObject $stageBlockObject -StageSummaryObject $stageSummary -Context "$Context result_map.$stageName"
    }
}
