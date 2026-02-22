# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 🏷️ [0.5.18] - 2026-02-21

> [!IMPORTANT]
> ### 🔧 Installation, Documentation & Dependency Updates
> This release fixes npm install failures when downloading from GitHub releases, adds first-time project setup guidance, and updates dependencies.

### 🛠️ Fixed

<details>
<summary><b>🐛 npm install failure from release archives</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/215">#215</a>)</summary>

| Issue | Root Cause | Fix |
|-------|------------|-----|
| `npm install` fails with ESLint config error | Release archives excluded `eslint.config.mjs` and other build files | Added `-source` archives with complete build files |
| `prepare` script runs build unnecessarily | Checked only `dist/` existence, not build artifacts | Now verifies `dist/cli.js` and `dist/index.js` exist |
| Deprecated `--ext .ts` flag in lint | ESLint 9.x removed support for `--ext` flag | Removed flag, extensions configured in `eslint.config.mjs` |

**Files Modified:**
- `package.json` (prepare script, lint scripts, removed prebuild)
- `.github/workflows/release.yml` (added source archives, fixed plugin path)
- `README.md` (added Rust/wasm-pack prerequisites)

</details>

### 📚 Documentation

<details>
<summary><b>📖 First-time project open instructions</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/112df08">112df08</a>)</summary>

Added guidance for users opening Unreal projects for the first time:
- Explains UE prompt to rebuild missing modules
- Documents expected plugin load failure after first rebuild
- Recommends closing and reopening project to resolve

</details>

### ⬆️ Dependencies

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.11.7 | 4.12.0 | [#213](https://github.com/ChiR24/Unreal_mcp/pull/213) |
| ajv | 8.17.1 | 8.18.0 | [#210](https://github.com/ChiR24/Unreal_mcp/pull/210) |
| actions/stale | 10.1.1 | 10.2.0 | [#208](https://github.com/ChiR24/Unreal_mcp/pull/208) |
| actions/dependency-review-action | 4.8.2 | 4.8.3 | [#212](https://github.com/ChiR24/Unreal_mcp/pull/212) |

---

## 🏷️ [0.5.17] - 2026-02-16

> [!IMPORTANT]
> ### 🔧 World Tools Category Fixes & Security Hardening
> This release includes critical bug fixes, security hardening, and UE 5.7 compatibility improvements across all world-building tools (landscape, foliage, geometry, volumes, navigation).

### 🛡️ Security

<details>
<summary><b>🔒 Path Validation & Input Sanitization</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/207">#207</a>)</summary>

| Component | Change |
|-----------|--------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots (`/Game`, `/Engine`, `/Script`) |
| **SanitizeProjectFilePath** | File operations with path traversal protection |
| **ValidateAssetCreationPath** | Combines folder + name validation for asset creation |
| **Actor/Volume Name Validation** | Blocks invalid characters, enforces length checks |
| **Snapshot Path Validation** | Prevents directory traversal attacks via snapshot paths |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+326 lines of security helpers)
- `src/tools/environment.ts` (snapshot path security)
- `src/utils/path-security.ts` (path normalization)

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Landscape Handler Silent Fallback Bug</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Bug | Root Cause | Fix |
|-----|------------|-----|
| False positives on non-existent landscapes | Path matching compared `GetPathName()` (internal path) with asset path | Normalized both paths with `.uasset` stripping |
| Silent fallback to single landscape | `if (!Landscape && LandscapeCount == 1)` used any available landscape | Removed fallback, now returns `LANDSCAPE_NOT_FOUND` error |
| Wrong response path | Returned requested path instead of actual path | Now returns `Landscape->GetPackage()->GetPathName()` |

**Affected Handlers:** `HandleModifyHeightmap`, `HandlePaintLandscapeLayer`, `HandleSculptLandscape`, `HandleSetLandscapeMaterial`

</details>

<details>
<summary><b>🐛 Rotation Yaw Bug</b> (McpAutomationBridge_LightingHandlers.cpp:200)</summary>

| Bug | Fix |
|-----|-----|
| `Rotation.Yaw` read from `LocPtr` instead of `RotPtr` | Changed to `GetJsonNumberField((*RotPtr), TEXT("yaw"))` |

**Impact:** Incorrect rotation when spawning lights with rotation parameters.

</details>

<details>
<summary><b>🐛 Integer Overflow in Heightmap Operations</b> (McpAutomationBridge_LandscapeHandlers.cpp:631-635)</summary>

| Bug | Fix |
|-----|-----|
| `static_cast<int16>(CurrentHeights[i])` overflows for values > 32767 | Changed to `static_cast<int32>` |

**Impact:** Heightmap raise/lower operations now produce correct results for heights above midpoint.

</details>

<details>
<summary><b>🐛 set_curve_key Success Reporting</b> (McpAutomationBridge_AnimationHandlers.cpp:2139)</summary>

| Bug | Fix |
|-----|-----|
| `bSuccess` initialized `false`, only set `true` inside `if (bSuccess)` block (unreachable) | Moved success logic before the condition check |

**Impact:** `set_curve_key` now correctly reports success.

</details>

<details>
<summary><b>🐛 CraftingSpeed Truncation</b> (McpAutomationBridge_InventoryHandlers.cpp:2716)</summary>

| Bug | Fix |
|-----|-----|
| `int32 CraftingSpeed` truncated fractional multipliers (1.5 → 1) | Changed to `double` |

</details>

<details>
<summary><b>🐛 Invalid Color Fallback Not Applied</b> (McpAutomationBridge_LightingHandlers.cpp:277)</summary>

| Bug | Fix |
|-----|-----|
| `SetLightColor()` only called when `bColorValid == true`, but `bColorValid = false` for invalid colors | Removed guard, always call `SetLightColor()` after correcting invalid colors to white |

</details>

<details>
<summary><b>🐛 Double-Validation in Snapshot Path</b> (src/tools/environment.ts:253, 322)</summary>

| Bug | Fix |
|-----|-----|
| Redundant second `validateSnapshotPath()` call on already-resolved absolute paths | Removed redundant call |

</details>

<details>
<summary><b>🐛 Intel GPU Driver Crash Prevention</b> (McpAutomationBridgeHelpers.h)</summary>

| Bug | Fix |
|-----|-----|
| `MONZA DdiThreadingContext` exceptions on Intel GPUs during level save | Added `McpSafeLevelSave` helper with `FlushRenderingCommands` and retry logic |

</details>

### ✨ Added

<details>
<summary><b>🛤️ LOD Generation Enhancements</b> (McpAutomationBridge_GeometryHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **landscapePath support** | LOD generation now accepts single `landscapePath` or array `assetPaths` |
| **lodCount parameter** | Alternative to `numLODs` for specifying LOD count |
| **Path sanitization** | All LOD operations use `SanitizeProjectRelativePath` |

</details>

<details>
<summary><b>🌿 FoliageType Auto-Creation</b> (McpAutomationBridge_FoliageHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create FoliageType** | When painting/adding foliage, FoliageType is automatically created from StaticMesh if missing |
| **Path validation** | All foliage operations use path sanitization |

</details>

<details>
<summary><b>🏔️ Landscape Layer Auto-Creation</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create layers** | When painting, landscape layers are auto-created if they don't exist (matches UE editor behavior) |

</details>

<details>
<summary><b>📊 Handler Verification</b> (Multiple Handler Files)</summary>

| Pattern | Description |
|---------|-------------|
| **AddActorVerification** | Returns `actorPath`, `actorName`, `actorGuid`, `existsAfter`, `actorClass` |
| **AddComponentVerification** | Returns `componentName`, `componentClass`, `ownerActorPath` |
| **AddAssetVerification** | Returns `assetPath`, `assetName`, `existsAfter`, `assetClass` |
| **VerifyAssetExists** | Verifies asset exists at path |

**Files Updated:** PropertyHandlers, LevelHandlers, EffectHandlers, GASHandlers, SequenceHandlers, SkeletonHandlers, and 30+ additional handler files

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket Protocol** | `GetProtocolType()` (FName) replaces deprecated `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` replaces `SaveLoadedAssetThrottled` to prevent recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in UE 5.7) |
| **Niagara Graph** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |
| **Landscape Edit** | `FLandscapeEditDataInterface` for UE 5.5+, deprecation suppression for 5.0-5.4 |
| **WorldPartition** | Support `RuntimeHashSet` in addition to `RuntimeSpatialHash` for UE 5.7+ |

</details>

<details>
<summary><b>📈 Performance Improvements</b></summary>

| Component | Change |
|-----------|--------|
| **Heightmap Modification** | Pass `false` to `FLandscapeEditDataInterface` to prevent 60+ second GPU sync delays |
| **Landscape Updates** | Use `MarkPackageDirty` instead of `PostEditChange` to avoid unnecessary rebuilds |
| **Geometry Operations** | Memory pressure checks and triangle limits to prevent OOM crashes |

</details>

### 📊 Statistics

- **Files Changed:** 70 files
- **Lines Added:** ~7,200
- **Lines Removed:** ~1,400
- **Bug Fixes:** 8 critical bugs
- **New Verification Helpers:** 4

---

## 🏷️ [0.5.16] - 2026-02-12

> [!IMPORTANT]
> ### 🚀 Major Feature Release: 200+ Action Handlers
> This release adds ~200 new C++ automation sub-actions across all domains, introduces progress heartbeat protocol for long-running operations, dynamic tool management, IPv6 support, and comprehensive security hardening.

### ✨ Added

<details>
<summary><b>🎮 200+ MCP Action Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/200">#200</a>)</summary>

| Domain | New Actions |
|--------|-------------|
| **AI** | 50+ actions for EQS, Perception, State Trees, Smart Objects |
| **Combat** | Weapons, projectiles, damage, melee combat |
| **Character** | Character creation, movement, advanced locomotion |
| **Inventory** | Items, equipment, loot tables, crafting |
| **GAS** | Gameplay Ability System: abilities, effects, attributes |
| **Audio** | MetaSounds, sound classes, dialogue |
| **Materials** | Material expressions, landscape layers |
| **Textures** | Texture creation, compression, virtual texturing |
| **Levels** | 15+ new sub-actions for level management |
| **Volumes** | 18 volume types |
| **Performance** | Profiling, optimization, scalability |
| **Input** | Enhanced Input Actions & Contexts |
| **Interaction** | Interactables, destructibles, triggers |
| **Misc** | System control, tests, logs |

**New Handler Files:**
- `McpAutomationBridge_CharacterHandlers.cpp` (337 lines)
- `McpAutomationBridge_CombatHandlers.cpp` (398 lines)
- `McpAutomationBridge_SystemControlHandlers.cpp` (324 lines)
- `McpAutomationBridge_MiscHandlers.cpp` (1010 lines)
- `McpAutomationBridge_WidgetAuthoringHandlers.cpp` (2404 lines)

</details>

<details>
<summary><b>💓 Progress Heartbeat Protocol</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/201">#201</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Progress Updates** | C++ sends `progress_update` WebSocket messages during long-running operations |
| **Deadline Extensions** | TS extends request deadlines on each update with deadlock safeguards |
| **Stale Detection** | Detects same percentage for 3 consecutive updates |
| **Absolute Cap** | 5-minute maximum extension limit |
| **Max Extensions** | 10 extensions per request |

**Timeout Changes:**
- Default request timeout: 60s → 30s (extensions handle slow ops)

</details>

<details>
<summary><b>🔧 Dynamic Tool Management</b></summary>

| Feature | Description |
|---------|-------------|
| **manage_tools MCP Tool** | Enables AI to enable/disable tools at runtime |
| **Protected Tools** | `manage_tools`, `inspect`, and core category cannot be disabled |
| **list_changed Notifications** | Tool registry sends MCP notifications when tools change |
| **Category Filtering** | Filter tools by category (core, world, authoring, gameplay, utility) |

</details>

<details>
<summary><b>🌐 IPv6 Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/194">#194</a>)</summary>

| Feature | Description |
|---------|-------------|
| **IPv6 Addresses** | Full support for IPv6 addresses in automation bridge |
| **Hostname Resolution** | DNS resolution via `GetAddressInfo` instead of fallback to 127.0.0.1 |
| **Address Family Detection** | Auto-detect IPv6 by checking for colons in address |
| **Zone ID Handling** | Strip zone IDs from IPv6 addresses for Node.js compatibility |
| **Fallback Support** | Re-create socket as IPv4 when IPv6 not available |

</details>

### 🛡️ Security

<details>
<summary><b>🔒 Security Hardening</b></summary>

| Function | Description |
|----------|-------------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots |
| **SanitizeAssetName** | Strips SQL injection patterns, invalid characters, enforces 64-char limit |
| **ValidateAssetCreationPath** | Combines folder + name validation |
| **IsValidAssetPath** | Rejects `:` (Windows drive letters) and consecutive slashes |

**TypeScript Security:**
- `src/utils/path-security.ts`: Collapse `//` normalization
- `src/utils/validation.ts`: SQL injection detection

</details>

<details>
<summary><b>🔒 String Escaping Fix</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/202">#202</a>)</summary>

| Issue | Fix |
|-------|-----|
| Incomplete string escaping in path handling | Added proper escaping for special characters |

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility Fixes</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket** | `GetProtocolType()` (FName) replaces `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` prevents recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in 5.7) |
| **Niagara** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |

</details>

<details>
<summary><b>⚡ Performance & Infrastructure</b></summary>

| Change | Description |
|--------|-------------|
| **Memory Detection** | Windows `GlobalMemoryStatusEx` replaces heuristic detection |
| **Rate Limit** | `MaxAutomationRequestsPerMinute` raised 120 → 600 |
| **Logging** | Improved request/response logging with action name and filtered payload preview |
| **Blueprint Handler** | Variable name collision generates unique suffix, type validation before loading |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Various Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| **~30 handlers** | Handlers that returned `nullptr` now return structured JSON |
| **Blueprint** | Unknown actions return explicit error instead of silent failure |
| **Level tools** | File existence checked before load, post-load path validation |
| **Eject handler** | Changed from stopping PIE to ejecting from possessed pawn |

</details>

### 📊 Statistics

- **Files Changed:** 83 files
- **Lines Added:** ~23,000
- **Lines Removed:** ~2,700
- **New Action Handlers:** ~200
- **New Handler Files:** 5

---

## 🏷️ [0.5.15] - 2026-02-06

> [!NOTE]
> ### 🌐 Network Configuration Release
> This release adds support for non-loopback binding in automation bridge settings, enabling LAN access configuration.

### ✨ Added

<details>
<summary><b>🌐 Non-Loopback Binding Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/193">#193</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Non-Loopback Binding** | Automation bridge can now bind to non-loopback addresses (e.g., `0.0.0.0`) for LAN access |
| **Allow Non-Loopback Setting** | New `bAllowNonLoopback` setting in plugin configuration |
| **TypeScript Support** | Added `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable |
| **Host Validation Tests** | New test suite for bridge host validation |

**Configuration:**
```env
# Enable LAN access
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Security Note:** Only enable on trusted networks with appropriate firewall rules.

</details>

### 🔄 Dependencies

<details>
<summary><b>Dependabot Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.1 → 4.32.2 | [#189](https://github.com/ChiR24/Unreal_mcp/pull/189) |
| Dependencies group | 2 updates | [#190](https://github.com/ChiR24/Unreal_mcp/pull/190) |

</details>

### 📊 Statistics

- **Files Changed:** 8 files
- **Lines Added:** ~270
- **Lines Removed:** ~10

---

## 🏷️ [0.5.14] - 2026-02-05

> [!IMPORTANT]
> ### 🔐 TLS & Network Security Release
> This release introduces TLS/SSL support for secure WebSocket connections (`wss://`), per-connection rate limiting, loopback-only network binding enforcement, and authentication state tracking for the Automation Bridge.

### 🛡️ Security

<details>
<summary><b>🔒 Loopback-Only Binding & Handshake Enforcement</b> (<code>70c2745</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Loopback Binding** | Automation Bridge now only binds to loopback addresses (127.0.0.1 or ::1) |
| **Handshake Required** | Automation requests require completed `bridge_hello` handshake |

**C++ Plugin:**
- Rejects `0.0.0.0` and `::` bind attempts, falls back to `127.0.0.1` with warning
- Added `AuthenticatedSockets` tracking set in `McpConnectionManager`
- Unauthenticated sockets receive `HANDSHAKE_REQUIRED` error and connection close (code 4004)

**TypeScript Bridge:**
- Added `normalizeLoopbackHost()` to validate and enforce loopback addresses
- Non-loopback host values rejected with warning and fallback

</details>

### ✨ Added

<details>
<summary><b>🔐 TLS/SSL, Rate Limiting & Schema Validation</b> (<code>d2a94cf</code>)</summary>

| Feature | Description |
|---------|-------------|
| **TLS/SSL Support** | Full `wss://` WebSocket support with OpenSSL/TLS integration (TLS 1.2+) |
| **Rate Limiting** | Per-connection limits: configurable, defaults to disabled (0) for development |
| **Schema Validation** | New Zod schemas in `src/automation/message-schema.ts` for type-safe message parsing |

**New Plugin Settings:**
- `bEnableTls`, `TlsCertificatePath`, `TlsPrivateKeyPath` - TLS configuration
- `MaxMessagesPerMinute`, `MaxAutomationRequestsPerMinute` - Rate limit configuration

**C++ Implementation:**
- `InitializeTlsContext()`, `EstablishTls()`, `SendRaw()`, `RecvRaw()` - TLS-aware I/O
- Requires UE 5.7+ for native socket release; graceful fallback on older versions

**TypeScript Integration:**
- Added `rateLimitState` tracking with cleanup on connection close

</details>

### 🛠️ Fixed

<details>
<summary><b>🔧 TLS Memory Management</b> (<code>321206e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Struct Initialization** | Fixed `FParsedWebSocketUrl` member initialization order (Port using uninitialized `bUseTls`) |
| **SSL Context Ownership** | Added `bOwnsSslContext` to prevent double-free of client contexts owned by `ISslManager` |

</details>

<details>
<summary><b>🔧 Thread Safety & TLS Error Handling</b> (<code>6fd1553</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Mutex Protection** | Added `SocketRateLimits` cleanup in `ForceReconnect` with proper mutex locking |
| **Declaration** | Moved `ShutdownTls()` declaration outside `WITH_SSL` guard for compilation compatibility |

</details>

<details>
<summary><b>🔧 Review Feedback Fixes</b> (<code>8987a3e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Duplicate Call** | Fixed duplicate `ActiveSockets.Empty()` call in connection manager |
| **TypeScript Cleanup** | Added `rateLimitState` cleanup in `closeAll()` method |

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.3 → 1.26.0 | [#187](https://github.com/ChiR24/Unreal_mcp/pull/187) |
| `mcp-client-capabilities` | Latest | [#186](https://github.com/ChiR24/Unreal_mcp/pull/186) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.0 → 4.32.1 | [#185](https://github.com/ChiR24/Unreal_mcp/pull/185) |
| `actions/github-script` | 7.0.1 → 8.0.0 | [#184](https://github.com/ChiR24/Unreal_mcp/pull/184) |

</details>

---

## 🏷️ [0.5.13] - 2026-02-02

> [!IMPORTANT]
> ### 🛡️ Security & Compatibility Release
> This release includes multiple critical security fixes for command injection and path traversal vulnerabilities, along with full Unreal Engine 5.0 backward compatibility and WebSocket stability improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection in UITools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/144">#144</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via unsanitized user input in widget creation |
| **Fix** | Added `sanitizeConsoleString()` and applied `sanitizeAssetName()` to all user-provided identifiers |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Command Injection in LevelTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/179">#179</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via level names, event types, and game mode parameters |
| **Fix** | Added `sanitizeCommandArgument()` and applied to all console command parameters |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Path Traversal in Asset Listing</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/163">#163</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in `listAssets` via `filter.pathStartsWith` parameter |
| **Fix** | Applied `normalizeAndSanitizePath()` to GraphQL `listAssets` and asset handler `list` action |
| **Contributors** | @google-labs-jules[bot] |

</details>

### ✨ Added

<details>
<summary><b>🎮 Unreal Engine 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/183">#183</a>)</summary>

| Component | Description |
|-----------|-------------|
| **API Abstractions** | Version-guarded macros for Material, Niagara, AssetRegistry, Animation, and World Partition APIs |
| **Build System** | Made plugin dependencies optional with dynamic memory-based configuration |
| **Coverage** | 41 handler files updated with UE 5.0-5.7 compatibility |

**Compatibility Macros Added:**
- `MCP_GET_MATERIAL_EXPRESSIONS()` - Abstracts material expression access
- `MCP_DATALAYER_TYPE` / `MCP_DATALAYER_ASSET_TYPE` - Data layer type abstraction
- `MCP_ASSET_FILTER_CLASS_PATHS` - Asset registry filter abstraction
- `MCP_ASSET_DATA_GET_CLASS_PATH()` - FAssetData abstraction
- `MCP_NIAGARA_EMITTER_DATA_TYPE` - Niagara emitter abstraction

</details>

### 🛠️ Fixed

<details>
<summary><b>🔌 WebSocket Stability</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/180">#180</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/181">#181</a>)</summary>

| Fix | Description |
|-----|-------------|
| **TOCTOU Race** | Fixed Time-of-Check-Time-of-Use race condition in ListenSocket shutdown |
| **Shutdown Hang** | Fixed WebSocket server blocking cook/package builds |
| **Version Compatibility** | Fixed `PendingReceived.RemoveAt()` API differences for UE 5.4+ |

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Resource Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/165">#165</a>)</summary>

- Fixed broken actors and level resource handlers
- Added missing actors and level resources to MCP resource list

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Other Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| UE 5.7 | Resolved macro handling and ControlRig dynamic loading issues |
| UE 5.5 | Fixed API compatibility issues in handlers |
| UE 5.1 | Fixed `MaterialDomain.h` inclusion path |
| JSON | Refactored JSON handling in McpAutomationBridge |

</details>

### 🧪 Testing

- Added security regression tests for UITools, LevelTools, and asset handlers

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.1 → 6.2.0 | [#160](https://github.com/ChiR24/Unreal_mcp/pull/160) |
| `actions/checkout` | 6.0.1 → 6.0.2 | [#161](https://github.com/ChiR24/Unreal_mcp/pull/161) |
| `github/codeql-action` | 4.31.10 → 4.32.0 | [#168](https://github.com/ChiR24/Unreal_mcp/pull/168), [#170](https://github.com/ChiR24/Unreal_mcp/pull/170) |
| `google-github-actions/run-gemini-cli` | Latest | [#177](https://github.com/ChiR24/Unreal_mcp/pull/177) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | Latest | [#154](https://github.com/ChiR24/Unreal_mcp/pull/154) |
| `hono` | 4.11.4 → 4.11.7 | [#173](https://github.com/ChiR24/Unreal_mcp/pull/173) |
| `@types/node` | Various updates | [#158](https://github.com/ChiR24/Unreal_mcp/pull/158), [#162](https://github.com/ChiR24/Unreal_mcp/pull/162), [#175](https://github.com/ChiR24/Unreal_mcp/pull/175) |

</details>

---

## 🏷️ [0.5.12] - 2026-01-15

> [!NOTE]
> ### 🔧 Handler Synchronization Release
> This release focuses on synchronizing TypeScript handler parameters with C++ handlers and dependency updates.

### 🛠️ Fixed

<details>
<summary><b>🔧 TS Handler Parameter Sync</b> (<code>5953232</code>)</summary>

- Synchronized TypeScript handler parameters with C++ handlers for consistency
- Fixed parameter mapping issues between TS and C++ layers

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.0 → 6.1.1 | [#141](https://github.com/ChiR24/Unreal_mcp/pull/141) |
| `google-github-actions/run-gemini-cli` | Latest | [#142](https://github.com/ChiR24/Unreal_mcp/pull/142) |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/143">#143</a>)</summary>

| Package | Update |
|---------|--------|
| `@types/node` | Various dev dependency updates |

</details>

---

## 🏷️ [0.5.11] - 2026-01-12

> [!IMPORTANT]
> ### 🛡️ Security Hardening & UE 5.7 Compatibility
> This release includes multiple critical security fixes for path traversal and command injection vulnerabilities, along with UE 5.7 Interchange compatibility fixes.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in Asset Import</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/125">#125</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in asset import functionality |
| **Fix** | Added path sanitization and validation |

</details>

<details>
<summary><b>🔒 Command Injection Bypass</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/122">#122</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Command injection bypass via flexible whitespace |
| **Fix** | Enhanced command validation to detect and block bypass attempts |

</details>

<details>
<summary><b>🔒 Path Traversal in Screenshots</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/120">#120</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in screenshot filenames |
| **Fix** | Implemented filename sanitization and path validation |

</details>

<details>
<summary><b>🔒 Path Traversal in GraphQL</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/135">#135</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in GraphQL resolvers |
| **Fix** | Added input sanitization for GraphQL resolver paths |

</details>

<details>
<summary><b>🔒 GraphQL CORS Configuration</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/118">#118</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 MEDIUM |
| **Vulnerability** | Insecure GraphQL CORS configuration |
| **Fix** | Implemented secure CORS policy |

</details>

<details>
<summary><b>🔒 Enhanced Command Validation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/113">#113</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection bypasses |
| **Fix** | Enhanced validation patterns to prevent injection bypasses |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 UE 5.7 Asset Import Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/138">#138</a>)</summary>

| Fix | Description |
|-----|-------------|
| **Interchange Compatibility** | Deferred asset import to next tick for UE 5.7 Interchange compatibility |
| **Name Sanitization** | Improved asset import robustness and name sanitization |

**Closes [#137](https://github.com/ChiR24/Unreal_mcp/issues/137)**

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.1 → 1.25.2 | [#119](https://github.com/ChiR24/Unreal_mcp/pull/119) |
| `hono` | 4.11.1 → 4.11.4 | [#129](https://github.com/ChiR24/Unreal_mcp/pull/129) |
| `@types/node` | Various updates | [#130](https://github.com/ChiR24/Unreal_mcp/pull/130), [#133](https://github.com/ChiR24/Unreal_mcp/pull/133), [#134](https://github.com/ChiR24/Unreal_mcp/pull/134) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.31.9 → 4.31.10 | [#126](https://github.com/ChiR24/Unreal_mcp/pull/126) |
| `actions/setup-node` | 6.1.0 → 6.2.0 | [#133](https://github.com/ChiR24/Unreal_mcp/pull/133) |
| `dependabot/fetch-metadata` | 2.4.0 → 2.5.0 | [#114](https://github.com/ChiR24/Unreal_mcp/pull/114) |

</details>

---

## 🏷️ [0.5.10] - 2026-01-04

> [!IMPORTANT]
> ### 🚀 Context Reduction Initiative & Spline System
> This release implements the **Context Reduction Initiative** (Phases 48-53), reducing AI context overhead from ~78,000 to ~25,000 tokens, and adds a complete **Spline System** (Phase 26) with 21 new actions. ([#107](https://github.com/ChiR24/Unreal_mcp/pull/107), [#105](https://github.com/ChiR24/Unreal_mcp/pull/105))

### ✨ Added

<details>
<summary><b>🛤️ Spline System (Phase 26)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/105">#105</a>)</summary>

New `manage_splines` tool with 21 actions for spline-based content creation:

| Category | Actions |
|----------|---------|
| **Creation** | `create_spline_actor`, `add_spline_point`, `remove_spline_point`, `set_spline_point` |
| **Properties** | `set_closed_loop`, `set_spline_type`, `set_tangent`, `get_spline_info` |
| **Mesh Components** | `create_spline_mesh`, `set_mesh_asset`, `set_spline_mesh_axis`, `set_spline_mesh_material` |
| **Scattering** | `create_mesh_along_spline`, `set_scatter_spacing`, `randomize_scatter` |
| **Quick Templates** | `create_road_spline`, `create_river_spline`, `create_fence_spline`, `create_wall_spline`, `create_cable_spline`, `create_pipe_spline` |
| **Utility** | `get_splines_info` |

**C++ Implementation:**
- `McpAutomationBridge_SplineHandlers.cpp` (1,512 lines)
- Full UE5 Spline API integration with `USplineComponent` and `USplineMeshComponent`

</details>

<details>
<summary><b>🔧 Pipeline Management Tool</b></summary>

New `manage_pipeline` tool for dynamic tool category management:

| Action | Description |
|--------|-------------|
| `set_categories` | Enable specific tool categories (core, world, authoring, gameplay, utility, all) |
| `list_categories` | Show available categories and their tools |
| `get_status` | View current state and tool counts |

**MCP Capability:**
- Server advertises `capabilities.tools.listChanged: true`
- Client capability detection via `mcp-client-capabilities` package
- Backward compatible: clients without `listChanged` support get ALL tools

</details>

### 🔧 Changed

<details>
<summary><b>📉 Context Reduction Initiative (Phases 48-53)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/107">#107</a>)</summary>

| Phase | Description | Token Reduction |
|-------|-------------|-----------------|
| **Phase 48** | Schema Pruning - Condensed all 35+ tool descriptions to 1-2 sentences | ~23,000 |
| **Phase 49** | Common Schema Extraction - Shared schemas for paths, names, locations | ~8,000 |
| **Phase 50** | Dynamic Tool Loading - Category-based filtering | ~50,000 (when using filtering) |
| **Phase 53** | Strategic Tool Merging - Consolidated 4 tools | ~10,000 |

**Total Potential Reduction: ~91,000 tokens**

**Common Schemas Added:**
- `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- `standardResponse` for consistent output formatting
- Helper functions: `createOutputSchema()`, `actionDescription()`

</details>

<details>
<summary><b>🔀 Tool Consolidation (Phase 53)</b></summary>

| Deprecated Tool | Merged Into | Actions Moved |
|-----------------|-------------|---------------|
| `manage_blueprint_graph` | `manage_blueprint` | 11 graph actions |
| `manage_audio_authoring` | `manage_audio` | 30 authoring actions |
| `manage_niagara_authoring` | `manage_effect` | 36 authoring actions |
| `manage_animation_authoring` | `animation_physics` | 45 authoring actions |

**Benefits:**
- Reduced tool count: 38 → 35
- Simplified tool discovery for AI assistants
- Backward compatible: deprecated tools still work with once-per-session warnings
- Action routing uses parameter sniffing to resolve conflicts

</details>

### ⚠️ Deprecated

- `manage_blueprint_graph` - Use `manage_blueprint` with graph actions instead
- `manage_audio_authoring` - Use `manage_audio` with authoring actions instead
- `manage_niagara_authoring` - Use `manage_effect` with authoring actions instead
- `manage_animation_authoring` - Use `animation_physics` with authoring actions instead

### 📊 Statistics

- **Files Changed:** 20
- **Lines Added:** 4,541
- **Lines Removed:** 3,555
- **Net Change:** +986 lines
- **New C++ Handler:** 1,512 lines (`McpAutomationBridge_SplineHandlers.cpp`)
- **New TS Handler:** 169 lines (`spline-handlers.ts`)
- **Common Schemas Added:** 50+ reusable schema definitions

### 🔗 Related Issues

Closes [#104](https://github.com/ChiR24/Unreal_mcp/issues/104), [#106](https://github.com/ChiR24/Unreal_mcp/issues/106), [#108](https://github.com/ChiR24/Unreal_mcp/issues/108), [#109](https://github.com/ChiR24/Unreal_mcp/issues/109), [#111](https://github.com/ChiR24/Unreal_mcp/issues/111)

---

## 🏷️ [0.5.9] - 2026-01-03

> [!IMPORTANT]
> ### 🎮 Major Feature Release
> This release introduces **15+ new automation tools** with comprehensive handlers for Navigation, Volumes, Level Structure, Sessions, Game Framework, and complete game development systems. ([#53](https://github.com/ChiR24/Unreal_mcp/pull/53))

### 🛡️ Security

<details>
<summary><b>🔒 Fix Arbitrary File Read in LogTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/103">#103</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Arbitrary file read via `logPath` parameter |
| **Impact** | Attackers could read any file on the system by manipulating the `logPath` override |
| **Fix** | Validated that `logPath` ends with `.log` and is within `Saved/Logs` directory |

**Protections Added:**
- Enforced `.log` extension requirement
- Restricted to `Saved/Logs` directory (CWD or UE_PROJECT_PATH)
- Added path traversal and sibling directory attack protection

</details>

### ✨ Added

<details>
<summary><b>🛠️ New Automation Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_navigation` | NavMesh configuration, Nav Modifiers, Nav Links, pathfinding control |
| `manage_volumes` | 18 volume types (Trigger, Blocking, Audio, Physics, Navigation, Streaming) |
| `manage_level_structure` | World Partition, HLOD, Data Layers, Level Blueprints |
| `manage_sessions` | Split-screen, LAN play, Voice Chat configuration |
| `manage_game_framework` | GameMode, GameState, PlayerController, match flow |
| `manage_skeleton` | Bone manipulation, sockets, physics assets |
| `manage_material_authoring` | Material expressions, landscape materials |
| `manage_texture` | Texture creation, compression, virtual texturing |
| `manage_animation_authoring` | AnimBP, Control Rig, IK Rig, Retargeter |
| `manage_niagara_authoring` | Niagara systems, modules, parameters |
| `manage_gas` | Gameplay Ability System (Abilities, Effects, Attributes) |
| `manage_character` | Character creation, movement, locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | EQS, Perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widgets, layout, styling |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_audio_authoring` | MetaSounds, sound classes, dialogue |

</details>

### 🔧 Changed

<details>
<summary><b>Build & Infrastructure Improvements</b></summary>

| Change | Description |
|--------|-------------|
| Bounded Directory Search | Replaced unbounded recursive search with bounded depth search (3-4 levels) |
| Property Management | Enhanced property management across all automation handlers |
| Connection Manager | Added `IsReconnectPending()` method to McpConnectionManager |
| State Machine | Improved state machine creation with enhanced error handling |

</details>

### 📊 Statistics

- **New Tools:** 15+
- **New C++ Handler Files:** 20+

---

## 🏷️ [0.5.8] - 2026-01-02

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for path traversal vulnerability and material graph parameter improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Path Traversal in INI Reader</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/48">#48</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `getProjectSetting()` |
| **Impact** | Attackers could access arbitrary files by injecting `../` sequences into the category parameter |
| **Fix** | Added strict regex validation `^[a-zA-Z0-9_-]+$` to `cleanCategory` in `src/utils/ini-reader.ts` |

</details>

### 🛠️ Fixed

<details>
<summary><b>Material Graph Parameter Mapping</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/50">#50</a>)</summary>

| Schema Parameter | C++ Handler Expected | Status |
|------------------|---------------------|--------|
| `fromNodeId` | `sourceNodeId` | ✅ Auto-mapped |
| `toNodeId` | `targetNodeId` | ✅ Auto-mapped |
| `toPin` | `inputName` | ✅ Auto-mapped |

Closes [#49](https://github.com/ChiR24/Unreal_mcp/issues/49)

</details>

---

## 🏷️ [0.5.7] - 2026-01-01

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for Python execution bypass vulnerability.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Python Execution Bypass</b> (<code>e16dab0</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Python execution restriction bypass |
| **Impact** | Attackers could execute arbitrary Python code by using tabs instead of spaces after the `py` command |
| **Fix** | Updated `CommandValidator` to use regex `^py(?:\s|$)` which correctly matches `py` followed by any whitespace |

</details>

### 🔧 Changed

<details>
<summary><b>Release Process Improvements</b></summary>

- Removed automatic git tag creation from release workflow
- Updated release summary instructions for manual tag management

</details>

### 🔄 Dependencies

<details>
<summary><b>Package Updates</b></summary>

| Package | Update | Type |
|---------|--------|------|
| `zod` | 4.2.1 → 4.3.4 | Minor |
| `qs` | 6.14.0 → 6.14.1 | Patch (indirect) |
| `github/codeql-action` | 3.28.1 → 4.31.9 | Major |

</details>

---

## 🏷️ [0.5.6] - 2025-12-30

> [!IMPORTANT]
> ### 🛡️ Type Safety Milestone
> This release achieves **near-zero `any` type usage** across the entire codebase. All tool interfaces, handlers, automation bridge, GraphQL resolvers, and WASM integration now use strict TypeScript types with `unknown` and proper type guards.

### ✨ Added

<details>
<summary><b>📐 New Zod Schema Infrastructure</b></summary>

| File | Description |
|------|-------------|
| `src/schemas/primitives.ts` | 261 lines of Zod schemas for Vector3, Rotator, Transform, Color, etc. |
| `src/schemas/responses.ts` | 380 lines of response validation schemas |
| `src/schemas/parser.ts` | 167 lines of safe parsing utilities with type guards |
| `src/schemas/index.ts` | 173 lines of unified schema exports |

**Total:** 981 lines of new type-safe schema infrastructure

</details>

<details>
<summary><b>🔧 Type-Safe Argument Helpers</b> (<code>d5e6d1e</code>)</summary>

New extraction functions in `argument-helper.ts`:

| Function | Description |
|----------|-------------|
| `extractString(params, key)` | Extract required string with assertion |
| `extractOptionalString(params, key)` | Extract optional string |
| `extractNumber(params, key)` | Extract required number with assertion |
| `extractOptionalNumber(params, key)` | Extract optional number |
| `extractBoolean(params, key)` | Extract required boolean with assertion |
| `extractOptionalBoolean(params, key)` | Extract optional boolean |
| `extractArray<T>(params, key, validator?)` | Extract typed array with optional validation |
| `extractOptionalArray<T>(params, key, validator?)` | Extract optional array |
| `normalizeArgsTyped(args, configs)` | Returns `NormalizedArgs` interface with accessor methods |

**NormalizedArgs Interface:**
- `getString(key)`, `getOptionalString(key)`
- `getNumber(key)`, `getOptionalNumber(key)`
- `getBoolean(key)`, `getOptionalBoolean(key)`
- `get(key)` for raw `unknown` access
- `raw()` for full object access

</details>

<details>
<summary><b>🔌 WASM Module Interface</b> (<code>d5e6d1e</code>)</summary>

Defined structured `WASMModule` interface replacing `any`:

```typescript
interface WASMModule {
  PropertyParser?: new () => { parse_properties(json, maxDepth) };
  TransformCalculator?: new () => { composeTransform, decomposeMatrix };
  Vector?: new (x, y, z) => { x, y, z, add(other) };
  DependencyResolver?: new () => { analyzeDependencies, calculateDepth, ... };
}
```

</details>

<details>
<summary><b>📝 Automation Bridge Types</b> (<code>f97b008</code>)</summary>

| Type | Location | Description |
|------|----------|-------------|
| `QueuedRequestItem` | `automation/types.ts` | Typed interface for queued request items |
| `ASTFieldNode` | `graphql/resolvers.ts` | GraphQL AST node types for parseLiteral |
| `ASTNode` | `graphql/resolvers.ts` | Typed AST parsing |

</details>

### 🔧 Changed

<details>
<summary><b>🎯 Tool Interfaces Refactored</b> (<code>d5e6d1e</code>)</summary>

**ITools Interface - Replaced all `any` with concrete types:**

| Property | Before | After |
|----------|--------|-------|
| `materialTools` | `any` | `MaterialTools` |
| `niagaraTools` | `any` | `NiagaraTools` |
| `animationTools` | `any` | `AnimationTools` |
| `physicsTools` | `any` | `PhysicsTools` |
| `lightingTools` | `any` | `LightingTools` |
| `debugTools` | `any` | `DebugVisualizationTools` |
| `performanceTools` | `any` | `PerformanceTools` |
| `audioTools` | `any` | `AudioTools` |
| `uiTools` | `any` | `UITools` |
| `introspectionTools` | `any` | `IntrospectionTools` |
| `engineTools` | `any` | `EngineTools` |
| `behaviorTreeTools` | `any` | `BehaviorTreeTools` |
| `logTools` | `any` | `LogTools` |
| `inputTools` | `any` | `InputTools` |
| Index signature | `[key: string]: any` | `[key: string]: unknown` |

**StandardActionResponse:**
- Changed `StandardActionResponse<T = any>` → `StandardActionResponse<T = unknown>`

**IBlueprintTools:**
- `operations: any[]` → `operations: Array<Record<string, unknown>>`
- `defaultValue?: any` → `defaultValue?: unknown`
- `propertyValue: any` → `propertyValue: unknown`

**IAssetResources:**
- `list(): Promise<any>` → `list(): Promise<Record<string, unknown>>`

</details>

<details>
<summary><b>🔷 GraphQL Resolvers Type Safety</b> (<code>f97b008</code>, <code>fa4dddc</code>)</summary>

All scalar resolvers now use typed parameters:

| Scalar | Before | After |
|--------|--------|-------|
| `Vector.serialize` | `(value: any)` | `(value: unknown)` |
| `Rotator.serialize` | `(value: any)` | `(value: unknown)` |
| `Transform.parseLiteral` | `(ast: any)` | `(ast: ASTNode)` |
| `JSON.parseLiteral` | `(ast: any)` | `(ast: ASTNode): unknown` |

**Internal interfaces typed:**
- `Asset.metadata?: Record<string, any>` → `Record<string, unknown>`
- `Actor.properties?: Record<string, any>` → `Record<string, unknown>`
- `Blueprint.defaultValue?: any` → `unknown`

</details>

<details>
<summary><b>🌐 Automation Bridge Type Safety</b> (<code>f97b008</code>)</summary>

| Location | Before | After |
|----------|--------|-------|
| `onError` callback | `(err: any)` | `(err: unknown)` |
| `onHandshakeFail` callback | `(err: any)` | `(err: Record<string, unknown>)` |
| `catch` block | `catch (err: any)` | `catch (err: unknown)` with type guard |
| `onMessage` handler | `(data: any)` | `(data: Buffer \| string)` |
| `queuedRequestItems` | inline type with `any` | `QueuedRequestItem[]` |

</details>

<details>
<summary><b>🔌 WASM Integration Type Safety</b> (<code>d5e6d1e</code>)</summary>

| Method | Before | After |
|--------|--------|-------|
| `parseProperties()` | `Promise<any>` | `Promise<unknown>` |
| `analyzeDependencies()` | `Promise<any>` | `Promise<unknown>` |
| `fallbackParseProperties()` | `any` | `unknown` |
| `fallbackAnalyzeDependencies()` | `any` | `Record<string, unknown>` |
| `globalThis.fetch` patch | `(globalThis as any).fetch` | Typed with `GlobalThisWithFetch` |
| Error handling | `(error as any)?.code` | `(error as Record<string, unknown>)?.code` |

</details>

<details>
<summary><b>📊 Handler Types Expanded</b> (<code>d5e6d1e</code>)</summary>

`src/types/handler-types.ts` expanded with 147+ lines of new typed interfaces for all handler argument types.

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ extractOptionalArray Behavior</b> (<code>f97b008</code>)</summary>

- Now returns `undefined` (instead of throwing) when value is not an array
- Documented behavior: graceful fallback for type mismatches
- Allows handlers to use default behavior when optional arrays are invalid

</details>

### 📊 Statistics

- **Files Changed:** 70 source files
- **Lines Added:** 3,806
- **Lines Removed:** 1,816
- **Net Change:** +1,990 lines (mostly type definitions)
- **New Schema Files:** 4 (981 lines total)
- **`any` → `unknown` Replacements:** 100+ occurrences

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `actions/first-interaction` | 1.3.0 → 3.1.0 | [#38](https://github.com/ChiR24/Unreal_mcp/pull/38) |
| `actions/labeler` | 5.0.0 → 6.0.1 | Dependabot |
| `github/codeql-action` | SHA update | Dependabot |
| `release-drafter/release-drafter` | SHA update | Dependabot |
| Dev dependencies group | 2 updates | Dependabot |

</details>

---

## 🏷️ [0.5.5] - 2025-12-29

> [!NOTE]
> ### 📝 Quality & Validation Release
> This release focuses on **input validation**, **structured logging**, and **developer experience** improvements. WebSocket connections now enforce message size limits, Blueprint graph editing supports user-friendly node names, and all tools use structured logging.

### ✨ Added

<details>
<summary><b>🔌 WebSocket Message Size Limits</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Max Message Size** | 5MB limit for WebSocket frames and accumulated messages |
| **Close Code 1009** | Connections close with standard "Message Too Big" code when exceeded |
| **Fragment Accumulation** | Size checks applied during fragmented message assembly |

**C++ Changes:**
- Added `MaxWebSocketMessageBytes` (5MB) and `MaxWebSocketFramePayloadBytes` constants
- Implemented size validation at frame receive, fragment accumulation, and initial payload
- Proper teardown with `WebSocketCloseCodeMessageTooBig` (1009)

</details>

<details>
<summary><b>🔷 Blueprint Node Type Aliases</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

User-friendly node names now map to internal K2Node classes:

| Alias | K2Node Class |
|-------|-------------|
| `Branch` | `K2Node_IfThenElse` |
| `Sequence` | `K2Node_ExecutionSequence` |
| `ForLoop` | `K2Node_ForLoop` |
| `ForLoopWithBreak` | `K2Node_ForLoopWithBreak` |
| `WhileLoop` | `K2Node_WhileLoop` |
| `Switch` | `K2Node_SwitchInteger` |
| `Select` | `K2Node_Select` |
| `DoOnce`, `DoN`, `FlipFlop`, `Gate`, `MultiGate` | Flow control nodes |
| `SpawnActorFromClass`, `GetAllActorsOfClass` | Actor manipulation |
| `Timeline`, `MakeArray`, `MakeStruct`, `BreakStruct` | Data/utility nodes |

**C++ & TypeScript Sync:**
- `BLUEPRINT_NODE_ALIASES` map in `graph-handlers.ts`
- `NodeTypeAliases` map in `McpAutomationBridge_BlueprintGraphHandlers.cpp`

</details>

<details>
<summary><b>🌳 Behavior Tree Generic Node Types</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Node Type | Default Class | Category |
|-----------|---------------|----------|
| `Task` | `BTTask_Wait` | task |
| `Decorator` / `Blackboard` | `BTDecorator_Blackboard` | decorator |
| `Service` / `DefaultFocus` | `BTService_DefaultFocus` | service |
| `Composite` | `BTComposite_Sequence` | composite |

Aliases for common BT nodes: `Wait`, `MoveTo`, `PlaySound`, `Cooldown`, `Loop`, `TimeLimit`, `Selector`, etc.

</details>

<details>
<summary><b>📊 show_stats Action</b></summary>

New `show_stats` action in `system_control` tool:
- Toggle engine stats display (`stat Unit`, `stat FPS`, etc.)
- Parameters: `category` (string), `enabled` (boolean)

</details>

### 🔧 Changed

<details>
<summary><b>📋 Structured Logging</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

Replaced `console.error`/`console.warn` with structured `Logger` across all tools:

| File | Change |
|------|--------|
| `actors.ts` | WASM debug logging |
| `debug.ts` | Viewmode stability warnings |
| `dynamic-handler-registry.ts` | Handler overwrite warnings |
| `editor.ts` | Removed commented debug logs |
| `physics.ts` | Improved error handling with fallback mesh resolution |

</details>

<details>
<summary><b>🎯 Handler Response Improvements</b></summary>

| Handler | Change |
|---------|--------|
| `actor-handlers.ts` | Returns clean responses without `ResponseFactory.success()` wrapping |
| `blueprint-handlers.ts` | Includes `blueprintPath` in responses |
| `environment.ts` | Changed default snapshot path to `./tmp/unreal-mcp/` |

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ Input Validation Enhancements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Handler | Validation Added |
|---------|------------------|
| `editor-handlers.ts` | Viewport resolution requires positive numbers |
| `asset-handlers.ts` | Folder paths must start with `/` |
| `lighting-handlers.ts` | Valid light types: `point`, `directional`, `spot`, `rect`, `sky` |
| `lighting-handlers.ts` | Valid GI methods: `lumen`, `screenspace`, `none`, `raytraced`, `ssgi` |
| `performance-handlers.ts` | Valid profiling types with clear error messages |
| `performance-handlers.ts` | Scalability levels clamped to 0-4 range |
| `system-handlers.ts` | Quality level clamped to 0-4 range |

</details>

<details>
<summary><b>🔧 WASM Binding Patching</b> (<code>7cc602a</code>)</summary>

- Fixed TOCTOU (Time-of-Check-Time-of-Use) race condition in `patch-wasm.js`
- Uses atomic file operations with file descriptors (`openSync`, `ftruncateSync`, `writeSync`)
- Proper error handling for missing WASM files

</details>

### 🗑️ Removed

<details>
<summary><b>🧹 Code Cleanup</b></summary>

| Removed | Lines | Reason |
|---------|-------|--------|
| `src/types/responses.ts` content | 355 | Obsolete response type definitions |
| `scripts/validate-server.js` | 46 | Unused validation script |
| `scripts/verify-automation-bridge.js` | 177 | Unused functions and broken code |

</details>

### 📊 Statistics

- **Files Changed:** 28+ source files
- **Lines Removed:** 436 (cleanup)
- **Lines Added:** 283 (validation + features)
- **New Node Aliases:** 30+ Blueprint, 20+ Behavior Tree

---

## 🏷️ [0.5.4] - 2025-12-27

> [!IMPORTANT]
> ### 🛡️ Security Release
> This release focuses on **security hardening** and **defensive improvements** across the entire stack, including command injection prevention, network isolation, and resource management.

### 🛡️ Security & Command Hardening

<details>
<summary><b>UBT Validation & Safe Execution</b></summary>

| Feature | Description |
|---------|-------------|
| **UBT Argument Validation** | Added `validateUbtArgumentsString` and `tokenizeArgs` to block dangerous characters (`;`, `|`, backticks) |
| **Safe Process Spawning** | Updated child process spawning to use `shell: false`, preventing shell injection attacks |
| **Console Command Validation** | Implemented strict input validation for the Unreal Automation Bridge to block chained or multi-line commands |
| **Argument Quoting** | Improved logging and execution logic to correctly quote arguments containing spaces |

</details>

### 🌐 Network & Host Binding

<details>
<summary><b>Localhost Default & Remote Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **Localhost Default** | WebSocket, Metrics, and GraphQL servers now bind to `127.0.0.1` by default |
| **Remote Exposure Prevention** | Prevents accidental remote exposure of services |
| **GRAPHQL_ALLOW_REMOTE** | Added environment variable check for explicit remote binding configuration |
| **Security Warnings** | Warnings logged for unsafe/permissive network settings |

</details>

### 🚦 Resource Management

<details>
<summary><b>Rate Limiting & Queue Management</b></summary>

| Feature | Description |
|---------|-------------|
| **IP-Based Rate Limiting** | Implemented rate limiting on the metrics server |
| **Queue Limits** | Introduced `maxQueuedRequests` to automation bridge to prevent memory exhaustion |
| **Message Size Enforcement** | Enforced `MAX_WS_MESSAGE_SIZE_BYTES` for WebSocket connections to reject oversized payloads |

</details>

### 🧪 Testing & Cleanup

<details>
<summary><b>Test Updates & File Cleanup</b></summary>

| Change | Description |
|--------|-------------|
| **Path Sanitization Tests** | Modified validation tests to verify path sanitization and expect errors for traversal attempts |
| **Removed Legacy Tests** | Removed outdated test files (`run-unreal-tool-tests.mjs`, `test-asset-errors.mjs`) |
| **Response Logging** | Implemented better response logging in the test runner |

</details>

### 🔄 Dependencies

- **dependencies group**: Bumped 2 updates via @dependabot ([#33](https://github.com/ChiR24/Unreal_mcp/pull/33))

---

## 🏷️ [0.5.3] - 2025-12-21

> [!IMPORTANT]
> ### 🔄 Major Enhancements
> - **Dynamic Type Discovery** - New runtime introspection for lights, debug shapes, and sequencer tracks
> - **Metrics Rate Limiting** - Per-IP rate limiting (60 req/min) on Prometheus endpoint
> - **Centralized Class Configuration** - Unified Unreal Engine class aliases
> - **Enhanced Type Safety** - Comprehensive TypeScript interfaces replacing `any` types

### ✨ Added

<details>
<summary><b>🔍 Dynamic Discovery & Engine Handlers</b></summary>

| Feature | Description |
|---------|-------------|
| **list_light_types** | Discovers all available light class types at runtime |
| **list_debug_shapes** | Enumerates supported debug shape types |
| **list_track_types** | Lists all sequencer track types available in the engine |
| **Heuristic Resolution** | Improved C++ handlers use multiple naming conventions and inheritance validation |
| **Vehicle Type Support** | Expanded vehicle type from union to string for flexibility |

**C++ Changes:**
- `McpAutomationBridge_LightingHandlers.cpp` - Runtime `ResolveUClass` for lights
- `McpAutomationBridge_SequenceHandlers.cpp` - Runtime resolution for tracks
- Added `UObjectIterator.h` for dynamic type scanning
- Unified spawn/track-creation flows
- Removed editor/PIE branching logic

</details>

<details>
<summary><b>⚙️ Tooling & Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **class-aliases.ts** | Centralized Unreal Engine class name mappings |
| **handler-types.ts** | Comprehensive TypeScript interfaces (ActorArgs, EditorArgs, LightingArgs, etc.) |
| **timeout constants** | Command-specific operation timeouts in constants.ts |
| **listDebugShapes()** | Programmatic access in DebugVisualizationTools |

**Type System:**
- Geometry types: Vector3, Rotator, Transform
- Required-component lookups
- Centralized class-alias mappings

</details>

<details>
<summary><b>📈 Metrics Server Enhancements</b></summary>

| Feature | Description |
|---------|-------------|
| **Rate Limiting** | Per-IP limit of 60 requests/minute |
| **Server Lifecycle** | Returns instance for better management |
| **Error Handling** | Improved internal error handling |

</details>

<details>
<summary><b>📚 Documentation & DX</b></summary>

| Feature | Description |
|---------|-------------|
| **handler-mapping.md** | Updated with new discovery actions |
| **README.md** | Clarified WASM build instructions |
| **Tool Definitions** | Synchronized with new discovery actions |

</details>

### 🔧 Changed

<details>
<summary><b>Handler Type Safety & Logic</b></summary>

**src/tools/handlers/common-handlers.ts:**
- Replaced `any` typings with strict `HandlerArgs`/`LocationInput`/`RotationInput`
- Added automation-bridge connectivity validation
- Enhanced location/rotation normalization with type guards

**Specialized Handlers:**
- `actor-handlers.ts` - Applied typed handler-args
- `asset-handlers.ts` - Improved argument normalization
- `blueprint-handlers.ts` - Added new action cases
- `editor-handlers.ts` - Enhanced default handling
- `effect-handlers.ts` - Added `list_debug_shapes`
- `graph-handlers.ts` - Improved validation
- `level-handlers.ts` - Type-safe operations
- `lighting-handlers.ts` - Added `list_light_types`
- `pipeline-handlers.ts` - Enhanced error handling

</details>

<details>
<summary><b>Infrastructure & Utilities</b></summary>

**Security & Validation:**
- `command-validator.ts` - Blocks semicolons, pipes, backticks
- `error-handler.ts` - Enhanced error logging
- `response-validator.ts` - Improved Ajv typing
- `safe-json.ts` - Generic typing for cleanObject
- `validation.ts` - Expanded path-traversal protection

**Performance:**
- `unreal-command-queue.ts` - Optimized queue processing (250ms interval)
- `unreal-bridge.ts` - Centralized timeout constants

</details>

### 🛠️ Fixed

- **Command Injection Prevention** - Additional dangerous command patterns blocked
- **Path Security** - Enhanced asset-name validation
- **Type Safety** - Eliminated `any` types across handler functions
- **Error Messages** - Clearer error messages for class resolution failures

### 📊 Statistics

- **Files Changed:** 20+
- **New Interfaces:** 15+ handler type definitions
- **Discovery Actions:** 3 new runtime introspection methods
- **Security Enhancements:** 5+ new validation patterns

### 🔄 Dependencies

- **graphql-yoga**: Bumped from 5.17.1 to 5.18.0 (#31)

---

## 🏷️ [0.5.2] - 2025-12-18

> [!IMPORTANT]
> ### 🔄 Breaking Changes
> - **Standardized Tools & Type Safety** - All tool handlers now use consistent interfaces with improved type safety. Some internal API signatures have changed. (`079e3c2`)

### ✨ Added

<details>
<summary><b>🛠️ Blueprint Enhancements</b> (<code>e710751</code>)</summary>

| Feature | Description |
|---------|-------------|
| **Dynamic Node Creation** | Support for creating nodes dynamically in Blueprint graphs |
| **Struct Property Support** | Added ability to set and get struct properties on Blueprint components |

</details>

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/28">#28</a>)</summary>

| Component | Change |
|-----------|--------|
| Tool Handlers | Optimized bridge communication and standardized response handling |
| Type Safety | Hardened type definitions across all tool interfaces |
| Bridge Optimization | Improved performance and reliability of automation bridge |

</details>

### 🔧 CI/CD

- 🔗 **MCP Publisher** - Fixed download URL format in workflow steps (`0d452e7`)
- 🧹 **Workflow Cleanup** - Removed unnecessary success conditions from MCP workflow steps (`82bd575`)

---

## 🏷️ [0.5.1] - 2025-12-17

> [!WARNING]
> ### ⚠️ Breaking Changes
> - **Standardized Return Types** - All tool methods now return `StandardActionResponse` type instead of generic objects. Consumers must update their code to handle the new response structure with `success`, `data`, `warnings`, and `error` fields. (`5e615c5`)
> - **Test Suite Structure** - New test files added and existing tests enhanced with comprehensive coverage.

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<code>5e615c5</code>)</summary>

| Component | Change |
|-----------|--------|
| Tool Methods | Updated all tool methods to return `StandardActionResponse` type for consistency |
| Tool Interfaces | Modified interfaces (assets, blueprint, editor, environment, foliage, landscape, level, sequence) to use standardized response format |
| Type System | Added proper type imports and exports for `StandardActionResponse` |
| Handler Files | Updated to work with new standardized response types |
| Response Structure | All implementations return correct structure with `success`/`error` fields |

</details>

### ✨ Added

<details>
<summary><b>🧪 Comprehensive Test Suite</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/25">#25</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Test Coverage** | Added comprehensive test files with success, error, and edge cases |
| **GraphQL DataLoader** | Implemented N+1 query optimization with batching and caching |
| **Type-Safe Interfaces** | Added type-safe automation response interfaces for better error handling |
| **Utility Tests** | Added tests for core utilities (normalize, safe-json, validation) |
| **Real-World Scenarios** | Enhanced coverage with real-world scenarios and cleanup procedures |
| **New Test Suites** | Audio, lighting, performance, input, and asset graph management |
| **Enhanced Logging** | Improved diagnostic logging throughout tools |
| **Documentation** | Updated supported Unreal Engine versions (5.0-5.7) in testing documentation |

</details>

### 🧹 Maintenance

- 🗑️ **Prompts Module Cleanup** - Removed prompts module and related GraphQL prompt functionality ([#26](https://github.com/ChiR24/Unreal_mcp/pull/26))
- 🔒 **Security Updates** - Removed unused dependencies (axios, json5, yargs) from package.json for security (`5e615c5`)
- 📐 **Tool Interfaces** - Enhanced asset and level tools with security validation and timeout handling (`5e615c5`)

### 📦 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR | Commit |
|---------|--------|-----|--------|
| `actions/checkout` | v4 → v6 | [#23](https://github.com/ChiR24/Unreal_mcp/pull/23) | `4c6b3b5` |
| `actions/setup-node` | v4 → v6 | [#22](https://github.com/ChiR24/Unreal_mcp/pull/22) | `71aa35c` |
| `softprops/action-gh-release` | 2.0.8 → 2.5.0 | [#21](https://github.com/ChiR24/Unreal_mcp/pull/21) | `b6c8a46` |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/24">#24</a>, <code>5e615c5</code>)</summary>

| Package | Update |
|---------|--------|
| `@modelcontextprotocol/sdk` | 1.25.0 → 1.25.1 |
| `@types/node` | 25.0.2 → 25.0.3 |

</details>

---

## 🏷️ [0.5.0] - 2025-12-16

> [!IMPORTANT]
> ### 🔄 Major Architecture Migration
> This release marks the **complete migration** from Unreal's built-in Remote Plugin to a native C++ **McpAutomationBridge** plugin. This provides:
> - ⚡ Better performance
> - 🔗 Tighter editor integration  
> - 🚫 No dependency on Unreal's Remote API
>
> **BREAKING CHANGE:** Response format has been standardized across all automation tools. Clients should expect responses to follow the new `StandardActionResponse` format with `success`, `data`, `warnings`, and `error` fields.

### 🏗️ Architecture

| Change | Description |
|--------|-------------|
| 🆕 **Native C++ Plugin** | Introduced `McpAutomationBridge` - a native UE5 editor plugin replacing the Remote API |
| 🔌 **Direct Editor Integration** | Commands execute directly in the editor context via automation bridge subsystem |
| 🌐 **WebSocket Communication** | Implemented `McpBridgeWebSocket` for real-time bidirectional communication |
| 🎯 **Bridge-First Architecture** | All operations route through the native C++ bridge (`fe65968`) |
| 📐 **Standardized Responses** | All tools now return `StandardActionResponse` format (`0a8999b`) |

### ✨ Added

<details>
<summary><b>🎮 Engine Compatibility</b></summary>

- **UE 5.7 Support** - Updated McpAutomationBridge with ControlRig dynamic loading and improved sequence handling (`ec5409b`)

</details>

<details>
<summary><b>🔧 New APIs & Integrations</b></summary>

- **GraphQL API** - Broadened automation bridge with GraphQL support, WASM integration, UI/editor integrations (`ffdd814`)
- **WebAssembly Integration** - High-performance JSON parsing with 5-8x performance gains (`23f63c7`)

</details>

<details>
<summary><b>🌉 Automation Bridge Features</b></summary>

| Feature | Commit |
|---------|--------|
| Server mode on port `8091` | `267aa42` |
| Client mode with enhanced connection handling | `bf0fa56` |
| Heartbeat tracking and output capturing | `28242e1` |
| Event handling and asset management | `d10e1e2` |

</details>

<details>
<summary><b>🎛️ New Tool Systems (0a8999b, 0ac82ac)</b></summary>

| Tool | Description |
|------|-------------|
| 🎮 **Input Management** | New `manage_input` tool with EnhancedInput support for Input Actions and Mapping Contexts |
| 💡 **Lighting Manager** | Full lighting configuration via `manage_lighting` including spawn, GI setup, shadow config, build lighting |
| 📊 **Performance Manager** | `manage_performance` with profiling (CPU/GPU/Memory), optimization, scalability, Nanite/Lumen config |
| 🌳 **Behavior Tree Editing** | Full behavior tree creation and node editing via `manage_behavior_tree` |
| 🎬 **Enhanced Sequencer** | Track operations (add/remove tracks, set muted/solo/locked), display rate, tick resolution |
| 🌍 **World Partition** | Cell management, data layer toggling via `manage_level` |
| 🖼️ **Widget Management** | UI widget creation, visibility controls, child widget adding |

</details>

<details>
<summary><b>📊 Graph Editing Capabilities (0a8999b)</b></summary>

- **Blueprint Graph** - Direct node manipulation with `manage_blueprint_graph` (create_node, delete_node, connect_pins, etc.)
- **Material Graph** - Node operations via `manage_asset` (add_material_node, connect_material_pins, etc.)
- **Niagara Graph** - Module and parameter editing (add_niagara_module, set_niagara_parameter, etc.)

</details>

<details>
<summary><b>🛠️ New Handlers & Actions</b></summary>

- Blueprint graph management and Niagara functionalities (`aff4d55`)
- Physics simulation setup in AnimationTools (`83a6f5d`)
- **New Asset Actions:**
  - `generate_lods`, `add_material_parameter`, `list_instances`
  - `reset_instance_parameters`, `get_material_stats`, `exists`
  - `nanite_rebuild_mesh`
- World partition and rendering tool handlers (`83a6f5d`)
- Screenshot with base64 image encoding (`bb4f6a8`)

</details>

<details>
<summary><b>🧪 Test Suites</b></summary>

**50+ new test cases** covering:
- Animation, Assets, Materials
- Sequences, World Partition
- Blueprints, Niagara, Behavior Trees
- Audio, Input Actions
- And more! (`31c6db9`, `85817c9`, `fc47839`, `02fd2af`)

</details>

### 🔄 Changed

#### Core Refactors
| Component | Change | Commit |
|-----------|--------|--------|
| `SequenceTools` | Migrated to Automation Bridge | `c2fb15a` |
| `UnrealBridge` | Refactored for bridge connection | `7bd48d8` |
| Automation Dispatch | Editor-native handlers modernization | `c9db1a4` |
| Test Runner | Timeout expectations & content extraction | `c9766b0` |
| UI Handlers | Improved readability and organization | `bb4f6a8` |
| Connection Manager | Streamlined connection handling | `0ac82ac` |

#### Tool Improvements
- 🚗 **PhysicsTools** - Vehicle config logic updated, deprecated checks removed (`6dba9f7`)
- 🎬 **AnimationTools** - Logging and response normalization (`7666c31`)
- ⚠️ **Error Handling** - Utilities refactored, INI file reader added (`f5444e4`)
- 📐 **Blueprint Actions** - Timeout handling enhancements (`65d2738`)
- 🎨 **Materials** - Enhanced material graph editing capabilities (`0a8999b`)
- 🔊 **Audio** - Improved sound component management (`0a8999b`)

#### Other Changes
- 📡 **Connection & Logging** - Improved error messages for clarity (`41350b3`)
- 📚 **Documentation** - README updated with UE 5.7, WASM docs, architecture overview, 17 tools (`8d72f28`, `4d77b7e`)
- 🔄 **Dependencies** - Updated to latest versions (`08eede5`)
- 📝 **Type Definitions** - Enhanced tool interfaces and type coverage (`0a8999b`)

### 🐛 Fixed

- `McpAutomationBridgeSubsystem` - Header removal, logging category, heartbeat methods (`498f644`)
- `McpBridgeWebSocket` - Reliable WebSocket communication (`861ad91`)
- **AutomationBridge** - Heartbeat handling and server metadata retrieval (`0da54f7`)
- **UI Handlers** - Missing payload and invalid widget path error handling (`bb4f6a8`)
- **Screenshot** - Clearer error messages and flow (`bb4f6a8`)

### 🗑️ Removed

| Removed | Reason |
|---------|--------|
| 🔌 Remote API Dependency | Replaced by native C++ plugin |
| 🐍 Python Fallbacks | Native C++ automation preferred (`fe65968`) |
| 📦 Unused HTTP Client | Cleanup from error-handler (`f5444e4`) |

---

## 🏷️ [0.4.7] - 2025-11-16

### ✨ Added
- Output Log reading via `system_control` tool with `read_log` action. filtering by category, level, line count.
- New `src/tools/logs.ts` implementing robust log tailing.
- 🆕 Initial `McpAutomationBridge` plugin with foundational implementation (`30e62f9`)
- 🧪 Comprehensive test suites for various Unreal Engine tools (`31c6db9`)

### 🔄 Changed
- `system_control` tool schema: Added `read_log` action.
- Updated tool handlers to route `read_log` to LogTools.
- Version bumped to 0.4.7.

### 📚 Documentation
- Updated README.md with initial bridge documentation (`a24dafd`)

---

## 🏷️ [0.4.6] - 2025-10-04

### 🐛 Fixed
- Fixed duplicate response output issue where tool responses were displayed twice in MCP content
- Response validator now emits concise summaries instead of duplicating full JSON payloads
- Structured content preserved for validation while user-facing output is streamlined

---

## 🏷️ [0.4.5] - 2025-10-03

### ✨ Added
- 🔧 Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client configs
- 📁 Added `projectPath` to runtime `configSchema` for Smithery's session UI

### 🔄 Changed
- ⚡ Made `createServer` synchronous factory (removed `async`)
- 🏠 Default for `ueHost` in exported `configSchema`

### 📚 Documentation
- Updated `README.md`, config examples to include `UE_PROJECT_PATH`
- Updated `smithery.yaml` and `server.json` manifests

### 🔨 Build
- Rebuilt Smithery bundle and TypeScript output

### 🐛 Fixed
- Smithery UI blank `ueHost` field by defining default in runtime schema

---

## 🏷️ [0.4.4] - 2025-09-28

### ✨ Improvements

- 🤝 **Client Elicitation Helper** - Added support for Cursor, VS Code, Claude Desktop, and other MCP clients
- 📊 **Consistent RESULT Parsing** - Handles JSON5 and legacy Python literals across all tools
- 🔒 **Safe Output Stringification** - Robust handling of circular references and complex objects
- 🔍 **Enhanced Logging** - Improved validation messages for easier debugging

---

## 🏷️ [0.4.0] - 2025-09-20

> **Major Release** - Consolidated Tools Mode

### ✨ Improvements

- 🎯 **Consolidated Tools Mode Exclusively** - Removed legacy mode, all tools now use unified handler system
- 🧹 **Simplified Tool Handlers** - Removed deprecated code paths and inline plugin validation
- 📝 **Enhanced Error Handling** - Better error messages and recovery mechanisms

### 🔧 Quality & Maintenance

- ⚡ Reduced resource usage by optimizing tool handlers
- 🧹 Cleanup of deprecated environment variables

---

## 🏷️ [0.3.1] - 2025-09-19

> **BREAKING:** Connection behavior is now on-demand

### 🏗️ Architecture

- 🔄 **On-Demand Connection** - Shifted to intelligent on-demand connection model
- 🚫 **No Background Processes** - Eliminated persistent background connections

### ⚡ Performance

- Reduced resource usage and eliminated background processes
- Optimized connection state management

### 🛡️ Reliability

- Improved error handling and connection state management
- Better recovery from connection failures

---

## 🏷️ [0.3.0] - 2025-09-17

> 🎉 **Initial Public Release**

### ✨ Features

- 🎮 **13 Consolidated Tools** - Full suite of Unreal Engine automation tools
- 📁 **Normalized Asset Listing** - Auto-map `/Content` and `/Game` paths
- 🏔️ **Landscape Creation** - Returns real UE/Python response data
- 📝 **Action-Oriented Descriptions** - Enhanced tool documentation with usage examples

### 🔧 Quality & Maintenance

- Server version 0.3.0 with clarified 13-tool mode
- Comprehensive documentation and examples
- Lint error fixes and code style cleanup

---

<div align="center">

### 🔗 Links

[![GitHub](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/ChiR24/Unreal_mcp)
[![npm](https://img.shields.io/badge/npm-Package-CB3837?style=for-the-badge&logo=npm)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![UE5](https://img.shields.io/badge/Unreal-5.6%20|%205.7-0E1128?style=for-the-badge&logo=unrealengine)](https://www.unrealengine.com/)

</div>
