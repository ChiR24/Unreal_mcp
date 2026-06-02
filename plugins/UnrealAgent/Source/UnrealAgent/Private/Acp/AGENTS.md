# Private/Acp

OpenCode ACP client for the Unreal Agent panel. This subtree owns process lifecycle, pipe IO, JSON-RPC framing, model/agent config parsing, transcript events, and permission responses.

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Public client API | `McpOpenCodeAcpClient.h` | `Start`, `Stop`, `Tick`, prompt/model/agent/permission methods |
| Process launch/lookup | `McpOpenCodeAcpClient.cpp` | `ResolveOpenCodeExecutable`, safe arg checks, pipe setup |
| ACP handshake/session | `McpOpenCodeAcpClient.cpp` | `SendInitialize`, `SendNewSession`, request IDs and timeouts |
| Studio Kit generation | `UnrealAgentStudioKit.*` | Managed `.opencode/` agents, skills, commands, config, and guardrail plugin |
| Editor context envelope | `UnrealAgentEditorContext.*` | Redacted project/map/PIE/selection/dirty/evidence prompt context |
| Evidence ledger | `UnrealAgentEvidenceLedger.*` | `Saved/UnrealAgent/state.json`, decisions, evidence event files |
| Validation runner | `UnrealAgentValidationRunner.*` | Lightweight Studio Kit/context/evidence checks for panel use |
| MCP server injection | `McpOpenCodeAcpClient.cpp` | `EnsureProjectUnrealAgentConfig`, `AddConfiguredMcpServers`, context attachment |
| Model/agent selectors | `McpOpenCodeAcpClient.cpp` | Config option parsing and fallback lists |
| Permissions/tool activity | `McpOpenCodeAcpClient.cpp` | Pending permission ID/options, transcript formatting |

## PROTOCOL CONVENTIONS
- ACP protocol version is `1`.
- Client-owned request timeouts: lifecycle `120s`, config `30s`, shutdown wait `2s`.
- Output buffer cap is `1 MB`; recent diagnostics cap is `4096` chars; permission/tool details are capped before display.
- Start in the Unreal project directory, but never resolve `opencode` from a project-relative path.
- `Start()` writes the versioned OpenCode Studio Kit before `session/new`; `session/new` includes the configured `unreal-engine` MCP server when bridge settings provide one.
- The generated prompt, skills, commands, and specialist agents should name the MCP production playbook clearly enough for a complete-game task: discover tools, inspect editor state, implement through the right MCP domains, and validate before reporting success.
- `SendPrompt()` attaches a bounded editor context envelope by default, but the visible transcript keeps the user's original prompt.
- Model config IDs may arrive as `id`, `configId`, or `configOptionId`; values may arrive as `currentValue`, `value`, or `optionValue`.
- Prompt cancellation sends ACP cancellation for the active prompt and keeps the process/session alive.
- Permission approval prefers the ACP-provided option IDs/kinds; allow-always is only enabled when ACP exposes a matching option.
- Generated Studio Kit files must preserve unmarked user-authored files. Only overwrite managed files with `unreal_agent_studio_kit_version: 1` or legacy managed prompts.
- Redact capability tokens, authorization headers, passwords, API keys, and secrets before prompt injection or evidence writing.

## ANTI-PATTERNS
- Blocking Slate/UI code while waiting for ACP responses; `Tick()` drains output and enforces timeouts.
- Treating stdout/stderr process noise as conversation content.
- Logging full unbounded JSON-RPC payloads, permission details, or tool output.
- Spawning unsafe executable paths, quoted arguments, newlines, or control characters.
- Claiming hidden role routing, autonomous swarm behavior, or live editor facts without implemented MCP/tool evidence.
- Killing the ACP process for normal prompt cancellation.
- Writing secrets into `.opencode/`, `Saved/UnrealAgent/`, transcripts, or validation evidence.
