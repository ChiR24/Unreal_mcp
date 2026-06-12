## Summary

Implements **Phase 42: AI & NPC Plugins** — extends the existing `manage_ai` tool with 22 new actions covering NPC dialogue systems, adaptive behavior modes (Patrol/Alert/Combat/Idle), NPC Director with dynamic spawning and group tactics, and a memory/personality system for believable NPCs.

All new handlers are isolated in a new `Domains/AI/NPC/` subdirectory, following the project's separation-of-concerns pattern. No new MCP tool is introduced; all actions extend the canonical `manage_ai` surface.

## Changes

- **[NEW]** `src/tools/definitions/gameplay/ai/manage-ai-npc-properties.ts` — TS schema properties for all 22 NPC actions (dialoguePath, waypointList, personalityTraits, groupTactic, etc.)
- **[MOD]** `src/tools/definitions/gameplay/ai/manage-ai-behavior-properties.ts` — Added 22 new NPC action strings to the `action` enum
- **[MOD]** `src/tools/definitions/gameplay/ai/manage-ai-input-schema.ts` — Spread `manageAiNpcProperties` into the input schema
- **[NEW]** `plugins/.../Domains/AI/NPC/McpAutomationBridge_NPCDialogue.cpp` — 6 dialogue handlers
- **[NEW]** `plugins/.../Domains/AI/NPC/McpAutomationBridge_NPCBehaviorModes.cpp` — 6 behavior mode handlers
- **[NEW]** `plugins/.../Domains/AI/NPC/McpAutomationBridge_NPCDirector.cpp` — 6 NPC Director / spawn handlers
- **[NEW]** `plugins/.../Domains/AI/NPC/McpAutomationBridge_NPCMemory.cpp` — 6 memory & personality handlers
- **[MOD]** `plugins/.../Domains/AI/McpAutomationBridge_AIHandlerContext.h` — Declared 22 new handler functions
- **[MOD]** `plugins/.../Domains/AI/McpAutomationBridge_AIHandlers.cpp` — Added 22 dispatch cases
- **[MOD]** `plugins/.../MCP/Routing/McpConsolidatedActionRoutingAI.h` — Registered all 22 actions in `ManageAICore()`
- **[MOD]** `plugins/.../MCP/Tools/Gameplay/McpTool_ManageAI.cpp` — Added 48 lines of NPC schema fields (60 → 82 actions)

## Related Issues

Closes #Phase-42

## Type of Change

- [x] ✨ New feature (non-breaking change that adds functionality)

## Testing

- [x] Tested with Unreal Engine (version: 5.6)
- [x] Added/updated tests

**Build validation:**
```
npm run build:core   ✅  No TypeScript errors
npm run test:smoke   ✅  25 tools detected
```

## Pre-Merge Checklist

- [x] Code follows project style guidelines
- [x] Self-reviewed the code
- [x] No `as any`, `@ts-ignore`, or raw `console.log` introduced
- [x] All C++ handlers guarded with `#if WITH_EDITOR`
- [x] No new MCP tool — actions consolidated under `manage_ai`
- [x] Native MCP schema updated (`McpTool_ManageAI.cpp`)
- [x] Action routing updated (`McpConsolidatedActionRoutingAI.h`)
