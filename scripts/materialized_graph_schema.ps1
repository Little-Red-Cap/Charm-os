$script:SupportedMaterializedGraphSampleSchemas = @(
    'materialized_graph.sample/v2'
)

function Assert-MaterializedGraphSampleSchema {
    param(
        $Graph,
        [string]$Context = 'graph'
    )

    if ($null -eq $Graph) {
        throw "$Context graph payload is null"
    }

    $schema = [string]$Graph.schema
    if ([string]::IsNullOrWhiteSpace($schema)) {
        throw "$Context graph schema is missing"
    }

    if ($script:SupportedMaterializedGraphSampleSchemas -notcontains $schema) {
        throw "$Context graph schema not supported: $schema"
    }
}

function Assert-MaterializedGraphSampleShape {
    param(
        $Graph,
        [string]$Context = 'graph'
    )

    Assert-MaterializedGraphSampleSchema -Graph $Graph -Context $Context

    $requiredFields = @(
        'effective_max_phase',
        'effective_runlevel_mask',
        'effective_runlevel_text',
        'node_count',
        'edge_count',
        'nodes',
        'edges'
    )

    foreach ($field in $requiredFields) {
        if ($null -eq $Graph.PSObject.Properties[$field]) {
            throw "$Context graph field missing: $field"
        }
    }
}
