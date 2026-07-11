# console_output_provider_smoke

This host-only smoke verifies the first console/output provider candidate on top
of the Capability-Oriented Runtime Topology bridge.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
candidate provider proof, not a promoted module, backend contract, service
locator, manifest, DSL, or public API.

The smoke checks:

- app code consumes `TextSink.log` and `LineSource.shell` requirements, not a
  provider identity;
- profile binding targets `host.buffered_console` as a provider instance;
- provider type, adapter, backend, transport, and HAL/API tokens cannot be
  binding targets;
- one provider instance may provide both output and input-line roles, but each
  role is declared and bound explicitly;
- `Transfer.bytes` means bytes accepted by the provider in this v1 smoke;
- overflow/drop/busy/fallback counters are provider evidence, not app API;
- evidence projection is collected outside the app context and remains
  structured data rather than formatted log text.
- host provider evidence and H747-style `console_tx=<started>/<done>/<bytes>/<fallback>/<dropped>/<busy>/<used>/<size>`
  presentation can be projected into a comparable evidence view;
- H747 status text remains evidence presentation, not a public schema.

Build:

```powershell
cmake -S Examples/system/console_output_provider_smoke `
  -B Examples/system/console_output_provider_smoke/cmake-build-console-output-provider-smoke `
  -G Ninja
cmake --build Examples/system/console_output_provider_smoke/cmake-build-console-output-provider-smoke
ctest --test-dir Examples/system/console_output_provider_smoke/cmake-build-console-output-provider-smoke --output-on-failure
```
