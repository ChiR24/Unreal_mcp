# Unreal MCP Debug Host

This extension keeps Microsoft's C++ debugger inside VS Code and exposes a small authenticated named-pipe API to the Unreal MCP sidecar. Open the Unreal project's workspace, configure `unrealMcpDebug.enginePath`, and run **Unreal MCP: Restart Debug Host** if the discovery file needs to be refreshed.

The discovery file is written to `<Project>/Saved/McpDebug/debug-host.json`. Native launch uses `cppvsdbg`, the project's DebugGame Editor symbols, and Unreal's installed `Unreal.natvis`.
