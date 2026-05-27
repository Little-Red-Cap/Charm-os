# Dev Loader Packet Console Tool

Host-only tool that converts a raw `.packetstream` file into H747 `dev_loader`
console commands:

```text
dev packet ingest <hex>
```

Usage:

```powershell
dev-loader-packet-console <input.packetstream> <output.commands> [--bytes-per-line N]
```

The default `--bytes-per-line` is `48`, matching the current H747 console
line-buffer recommendation. The tool does not add `dev packet reset`, `status`,
or `abort`; state control stays explicit at the monitor.
