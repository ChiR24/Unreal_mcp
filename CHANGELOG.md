# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- **WebAssembly Integration**: WASM acceleration for high-performance operations (5-8x faster JSON parsing, 5-10x faster transform math, 3-5x faster dependency resolution)
- WASM automatically initializes on server startup with graceful TypeScript fallbacks
- Performance monitoring and metrics tracking for WASM operations

### Changed
- Integrated WASM into automation bridge for JSON parsing
- Integrated WASM into response validator for faster schema validation
- Streamlined and cleaned up codebase by removing deprecated Python-related comments

### Documentation
- Updated README.md with WASM integration documentation
- Added WebAssembly Performance section with setup instructions

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
