# Changelog

All notable changes to this project will be documented in this file.

## [0.4.7] - 2025-11-16
### Added
- Output Log reading via `system_control` tool with `read_log` action. Supports filtering by category (comma-separated or array), log level (Error, Warning, Log, Verbose, VeryVerbose, All), line count (up to 2000), specific log path, include prefixes, and exclude categories. Automatically resolves the latest project log under Saved/Logs.
- New `src/tools/logs.ts` implementing robust log tailing, parsing (timestamp/category/level/message), and UE-specific internal entry filtering (e.g., excludes LogPython RESULT: blocks unless requested).

### Changed
- `system_control` tool schema: Added `read_log` action with full filter parameters to inputSchema; extended outputSchema with `logPath`, `entries` array, and `filteredCount`.
- Updated `src/tools/consolidated-tool-handlers.ts` to route `read_log` to LogTools without requiring UE connection (file-based).
- `src/index.ts`: Instantiates and passes LogTools to consolidated handler.
- Version bumped to 0.4.7 in package.json, package-lock.json, server.json, .env.production, and runtime config.

## [0.4.6] - 2025-10-04
### Fixed
- Fixed duplicate response output issue where tool responses were being displayed twice in MCP content
- Response validator now emits concise summaries in text content instead of duplicating full JSON payloads
- Structured content is preserved for validation and tests while user-facing output is streamlined

## [0.4.5] - 2025-10-03
### Added
- Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client example configs. This allows tools that need an absolute .uproject path (e.g., engine_start) to work without additional manual configuration.
- Added `projectPath` to the runtime `configSchema` so Smithery's session UI can inject a project path into the server environment.

### Changed
- Make `createServer` a synchronous factory (removed `async`) and updated `createServerDefault` and `startStdioServer` to use the synchronous factory. This aligns the exported default with Smithery’s expectations and prevents auto-start mismatches in the bundled output.
- Provide a default for `ueHost` in the exported `configSchema` so the Smithery configuration dialog pre-fills the host input.

### Documentation
- Updated `README.md`, `claude_desktop_config_example.json`, and `mcp-config-example.json` to include `UE_PROJECT_PATH` and usage notes.
- Updated `smithery.yaml` and `server.json` manifest to declare `UE_PROJECT_PATH` and default values.

### Build
- Rebuilt the Smithery bundle and TypeScript output to ensure schema and defaults are exported in the distributed artifact.

### Fixes
- Fixes Smithery UI blank ueHost field by defining a default in the runtime schema.


## [0.4.4] - 2025-09-30
- Previous release notes retained in upstream repo.
