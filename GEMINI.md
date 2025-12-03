# Unreal Engine MCP Server

## Project Overview

The **Unreal Engine MCP Server** (`unreal-engine-mcp-server`) is a comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine via a native automation bridge. It facilitates bidirectional communication between an AI agent (running via MCP) and an active Unreal Engine Editor session.

### Architecture

The system is composed of three main components:

1.  **MCP Server (Node.js/TypeScript):**
    *   **Role:** The core server that implements the MCP protocol, handles client requests, and orchestrates communication.
    *   **Location:** `src/`
    *   **Key File:** `src/index.ts` (Entry point), `src/unreal-bridge.ts` (Connection management).
    *   **Tools:** Defines MCP tools for Actor manipulation, Asset management, Blueprints, etc. (found in `src/tools/`).

2.  **Unreal Engine Plugin (C++):**
    *   **Role:** Acts as the receiver and executor within the Unreal Engine process. It runs a WebSocket server to listen for commands from the Node.js server.
    *   **Location:** `Plugins/McpAutomationBridge/`
    *   **Key Functionality:** Handles `execute_console_command`, `set_object_property`, and other engine-level operations on the Game Thread.
    *   **Communication:** Listens on `ws://localhost:8091` (default).

3.  **WebAssembly Module (Rust):**
    *   **Role:** Provides high-performance utilities for the Node.js server, such as complex property parsing and vector math.
    *   **Location:** `wasm/`
    *   **Key File:** `wasm/src/lib.rs`.

## Building and Running

### Prerequisites
*   Node.js (v18 or higher)
*   Unreal Engine (5.0 or higher)
*   Rust (optional, if modifying WASM components)

### Setup

1.  **Install Dependencies:**
    ```bash
    npm install
    ```

2.  **Build the Server:**
    ```bash
    npm run build
    ```

3.  **Build WebAssembly (Optional):**
    If you are working on the Rust module:
    ```bash
    npm run build:wasm
    ```

4.  **Unreal Plugin Installation:**
    *   Copy the `Plugins/McpAutomationBridge` folder into your Unreal Project's `Plugins` directory (e.g., `YourProject/Plugins/`).
    *   Regenerate project files and recompile your Unreal project.
    *   Enable the plugin in the Unreal Editor if it's not automatically enabled.
    *   Ensure the plugin is running (it should log "MCP Automation Bridge listening" to the Output Log).

### Running the Server
To start the MCP server:
```bash
npm start
```
Or for development with `ts-node`:
```bash
npm run dev
```

### Testing
The project includes a suite of integration tests located in the `tests/` directory. These scripts act as "fake" MCP clients to verify functionality.

*   **Run All Tests:**
    ```bash
    npm test
    ```
*   **Run Specific Test:**
    ```bash
    npm run test:control_actor
    npm run test:manage_asset
    npm run test:wasm
    ```

## Development Conventions

*   **Language:**
    *   Server logic is written in **TypeScript**.
    *   Unreal integration is written in **C++**.
    *   Performance modules are written in **Rust**.
*   **Safety:**
    *   The `UnrealBridge` class (`src/unreal-bridge.ts`) includes safety mechanisms like command throttling and "Safe ViewMode" switching to prevent crashing the engine with invalid commands.
    *   **Do not** bypass these safety checks when adding new functionality.
*   **Linting:**
    *   TypeScript: `npm run lint`
    *   C++/C#: Scripts provided (`npm run lint:cpp`, `npm run lint:csharp`) rely on external tools like `cpplint` and `dotnet-format`.

## Directory Structure

*   `src/`: Source code for the Node.js MCP server.
    *   `tools/`: Definitions for MCP tools (Actors, Assets, etc.).
    *   `resources/`: Definitions for MCP resources.
    *   `wasm/`: Compiled WebAssembly bindings.
*   `Plugins/`: Source code for the Unreal Engine C++ plugin.
*   `wasm/`: Source code for the Rust WebAssembly module.
*   `tests/`: Integration tests and test runners.
*   `scripts/`: Utility scripts for build and maintenance.

# Project Rules Agent

- The user explicitly wants real, working code only. I must never add mocks, placeholders, false-passed success code, or any non-functional code. All solutions should be directly applicable to a live editor and pass all relevant tests genuinely.
- The user explicitly requires real, working code only. I must never add mocks, placeholders, false-passed success codes, or any non-functional code. All solutions must be directly applicable to a live editor and pass all relevant tests genuinely.
- Always read full file content.
- Engine path: X:\Unreal_Engine\UE_5.6\Engine
- Before editing codes in plugin make sure to check before in engine's code so that all code are correct.