module;
export module out.print;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.api (compat shim)
// Forbidden out.* imports: everything else
// Rationale: backwards compatibility only; all behavior lives in logger.
// If you need functionality from a higher layer, add an extension point in this layer instead.

// Compatibility shim: prefer importing out.api directly.
export import out.api;
