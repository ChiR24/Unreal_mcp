# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 🚧 [Unreleased]

> [!IMPORTANT]
> ### 🔄 Major Architecture Migration
> This release marks the **complete migration** from Unreal's built-in Remote Plugin to a native C++ **McpAutomationBridge** plugin. This provides:
> - ⚡ Better performance
> - 🔗 Tighter editor integration  
> - 🚫 No dependency on Unreal's Remote API

### 🏗️ Architecture

| Change | Description |
|--------|-------------|
| 🆕 **Native C++ Plugin** | Introduced `McpAutomationBridge` - a native UE5 editor plugin replacing the Remote API |
| 🔌 **Direct Editor Integration** | Commands execute directly in the editor context via automation bridge subsystem |
| 🌐 **WebSocket Communication** | Implemented `McpBridgeWebSocket` for real-time bidirectional communication |
| 🎯 **Bridge-First Architecture** | All operations route through the native C++ bridge (`fe65968`) |

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
<summary><b>🛠️ New Handlers & Tools</b></summary>

- Blueprint graph management and Niagara functionalities (`aff4d55`)
- Physics simulation setup in AnimationTools (`83a6f5d`)
- **New Asset Actions:**
  - `generate_lods`
  - `add_material_parameter`
  - `list_instances`
  - `reset_instance_parameters`
  - `get_material_stats`
- World partition and rendering tool handlers (`83a6f5d`)

</details>

<details>
<summary><b>🧪 Test Suites</b></summary>

**50+ new test cases** covering:
- Animation, Assets, Materials
- Sequences, World Partition
- Blueprints, Niagara
- And more! (`31c6db9`, `85817c9`, `fc47839`)

</details>

### 🔄 Changed

#### Core Refactors
| Component | Change | Commit |
|-----------|--------|--------|
| `SequenceTools` | Migrated to Automation Bridge | `c2fb15a` |
| `UnrealBridge` | Refactored for bridge connection | `7bd48d8` |
| Automation Dispatch | Editor-native handlers modernization | `c9db1a4` |
| Test Runner | Timeout expectations & content extraction | `c9766b0` |

#### Tool Improvements
- 🚗 **PhysicsTools** - Vehicle config logic updated, deprecated checks removed (`6dba9f7`)
- 🎬 **AnimationTools** - Logging and response normalization (`7666c31`)
- ⚠️ **Error Handling** - Utilities refactored, INI file reader added (`f5444e4`)
- 📐 **Blueprint Actions** - Timeout handling enhancements (`65d2738`)

#### Other Changes
- 📡 **Connection & Logging** - Improved error messages for clarity (`41350b3`)
- 📚 **Documentation** - README updated with UE 5.7, WASM docs, architecture overview (`8d72f28`, `4d77b7e`)

### 🐛 Fixed

- `McpAutomationBridgeSubsystem` - Header removal, logging category, heartbeat methods (`498f644`)
- `McpBridgeWebSocket` - Reliable WebSocket communication (`861ad91`)
- **AutomationBridge** - Heartbeat handling and server metadata retrieval (`0da54f7`)

### 🗑️ Removed

| Removed | Reason |
|---------|--------|
| 🔌 Remote API Dependency | Replaced by native C++ plugin |
| 🐍 Python Fallbacks | Native C++ automation preferred (`fe65968`) |
| 📦 Unused HTTP Client | Cleanup from error-handler (`f5444e4`) |

---

## 🏷️ [0.4.7] - 2025-11-16

### ✨ Added
- 🆕 Initial `McpAutomationBridge` plugin with foundational implementation (`30e62f9`)
- 🧪 Comprehensive test suites for various Unreal Engine tools (`31c6db9`)

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

- Cleaned up deprecated environment variable definitions
- Updated tool count display from 13 to correct consolidated count

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

###  Links

[![GitHub](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/ChiR24/Unreal_mcp)
[![npm](https://img.shields.io/badge/npm-Package-CB3837?style=for-the-badge&logo=npm)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![UE5](https://img.shields.io/badge/Unreal-5.6%20|%205.7-0E1128?style=for-the-badge&logo=unrealengine)](https://www.unrealengine.com/)

</div>
