# EngineWire Runtime

Reads of a running game for an MCP client, in Development and Test builds. It is a separate plugin from EngineWire (which is editor only), has one `DeveloperTool` module (never part of a Shipping build), and does nothing until the game is started with `-EngineWireRuntime`.

## Use

1. Copy `EngineWireRuntime` into the project's `Plugins/` folder and enable it (it is off by default).
2. Start the game (a Development or Test build, or `UnrealEditor.exe <project> -game`) with `-EngineWireRuntime`; `-EngineWireRuntimePort=N` picks the port (3100 by default).
3. Point the client at `http://127.0.0.1:<port>/mcp` with the header `X-MCP-Capability-Token`: the token in the project's `Saved/MCP/capability-token` (made there when the editor plugin has not written one).

One tool, `unreal`, with `operation: "execute"` and one of these capabilities:

| Capability | Params | Answers |
|---|---|---|
| `runtime.list_actors` | `classFilter?`, `limit?` (default 200, at most 1000) | each actor's label, name, class and location, and `totalCount` |
| `runtime.get_transform` | `actorName` | location, rotation, scale and velocity |
| `runtime.frame_cost` | none | frame, game thread, render thread and GPU milliseconds, average FPS |
| `runtime.screenshot` | none | the path of the PNG the next frame is saved as |

## Security

- It listens on 127.0.0.1 only, through its own loop: the engine's HTTP server takes its bind address from the engine config, where `any` binds every interface.
- Every request carries the token, compared in constant time; a request without it is answered 401.
- Reads only: nothing it serves changes the game.
