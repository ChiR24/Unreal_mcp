# Unreal MCP — AI Agent Quick Guide (for AI coding agents)

**Two-process architecture**: Node.js MCP server (`src/`) ↔ Native C++ UE Editor plugin (`Plugins/McpAutomationBridge/Source/`)

**Data flow**: JSON payloads → `sendAutomationRequest(action, params)` (TS) → WebSocket → `UMcpAutomationBridgeSubsystem::ProcessAutomationRequest()` (C++) → Domain-specific `*Handlers.cpp` (native UE subsystems). Uses `FJsonObjectConverter::JsonObjectToUStruct()` + `FProperty` for typed marshaling.

## 🚀 Quickstart Checklist

### Prerequisites
- **Enable UE Plugins**: MCP Automation Bridge, Editor Scripting Utilities
- **Required Plugins**: Sequencer, Level Sequence Editor, Control Rig, Subobject Data Interface (UE 5.7+)

### Setup Commands
```bash
# Sync plugin to Unreal project
npm run automation:sync -- --project "X:/MyProject/Plugins"

# Verify plugin installation
npm run automation:verify -- --project "X:/MyProject/Plugins"

# Start development server (auto-connects on demand)
npm run dev

# Build with WASM optimization (5-8x performance)
npm run build:wasm
```

## 🗺️ Architecture Overview

### Core Components

**Node.js Server (`src/`)**
- `src/index.ts`: MCP server setup, tool registration, WASM initialization
- `src/unreal-bridge.ts`: Connection management, command throttling, safety validation
- `src/automation/bridge.ts`: WebSocket client with handshake/reconnect logic
- `src/tools/consolidated-*.ts`: 17 tool dispatchers with response validation
- `src/wasm/index.ts`: WebAssembly integration (JSON parsing, transform math)

**C++ Plugin (`Plugins/McpAutomationBridge/Source/McpAutomationBridge/`)**
- `Private/McpAutomationBridgeSubsystem.cpp`: WebSocket server, request routing
- `Private/McpBridgeWebSocket.cpp/h`: Custom WebSocket with protocol negotiation
- `Private/McpAutomationBridgeHelpers.h`: JSON↔UStruct conversion, class resolution
- `Private/*Handlers.cpp` (18 files): Domain-specific implementations

### Communication Protocol
- **Transport**: WebSocket (`ws://127.0.0.1:8091`)
- **Message format**: `{type: "automation_request", requestId: string, action: string, payload: object}`
- **Response format**: `{success: boolean, message?: string, error?: string, data?: object}`
- **Handshake**: Capability token exchange with metadata
- **Heartbeat**: 15s interval for connection health

## 🛠️ Developer Workflows

### Build & Test
```bash
# Full build (TypeScript + WASM)
npm run build

# TypeScript only (faster iteration)
npm run build:core

# Run specific tool tests
npm run test:manage_asset
npm run test:control_actor
npm run test:blueprint

# Run all tests
npm run test

# Lint (TypeScript, C++, C#)
npm run lint
npm run lint:cpp
```

### Debugging
```bash
# Check WebSocket connections
netstat -ano | findstr :8091

# Clean plugin build artifacts
rm -r Plugins/McpAutomationBridge/{Binaries,Intermediate}

# Set debug logging
LOG_LEVEL=debug npm run dev

# View MCP resources in Unreal
ue://health          # Metrics dashboard
ue://automation-bridge # Connection status
```

## 📋 Critical Patterns & Conventions

### 1. Two-Step Tool Implementation
**C++ Handler** → **TypeScript Wrapper** → **Consolidated Router**

```cpp
// In McpAutomationBridgeSubsystem.cpp
if (Action == TEXT("create_material"))
{
    return AssetWorkflowHandlers::CreateMaterial(Payload, Reply);
}
```

```typescript
// In src/tools/assets.ts
const resp = await automationBridge.sendAutomationRequest('create_material', {name, path});
return responseValidator.wrapResponse('manage_asset', resp);
```

### 2. Response Validation
All tools use `responseValidator.wrapResponse()` with Zod schemas defined in `consolidated-tool-definitions.ts`:

```typescript
// Register schema in src/index.ts
responseValidator.registerSchema('manage_asset', toolDefs.find(t => t.name === 'manage_asset').outputSchema);
```

### 3. Command Safety
- **Validation**: `CommandValidator.validate()` blocks dangerous commands
- **Throttling**: `executeThrottledCommand()` with priority queueing
- **Timeouts**: Default 30s, configurable via `timeoutMs`
- **Retry logic**: Exponential backoff with `ErrorHandler.retryWithBackoff()`

### 4. Path Normalization
- **UE paths**: `/Content` → `/Game` (via `normalize.ts`)
- **Vectors**: `{x,y,z}` or `[x,y,z]` → `toVec3Tuple()`
- **Asset references**: Use `ResolveClassByName()` in C++ helpers

### 5. WASM Optimization
Automatic fallback when WASM unavailable:

```typescript
// In src/wasm/index.ts
try {
  const result = wasmModule.PropertyParser.parse_properties(jsonStr);
  // 5-8x faster
} catch (error) {
  // Fallback to TypeScript
  return JSON.parse(jsonStr);
}
```

## 🔧 Adding New Tools

### Step-by-Step

1. **C++ Implementation**
   - Add handler in appropriate `*Handlers.cpp` file
   - Register in `McpAutomationBridgeSubsystem.cpp::ProcessAutomationRequest()`
   - Return `FReply{Success=true, Data=JsonObject}`

2. **TypeScript Wrapper**
   - Create or update `src/tools/<domain>.ts`
   - Use `automationBridge.sendAutomationRequest(action, params)`
   - Wrap response with `responseValidator.wrapResponse()`

3. **Consolidated Routing**
   - Add to `consolidated-tool-definitions.ts` (input/output schemas)
   - Route in `consolidated-tool-handlers.ts`

4. **Testing**
   - Create `tests/test-<domain>.mjs` with Markdown test cases
   - Run with `npm run test:<domain>`

### Example: New Material Action

```typescript
// 1. Add to consolidated-tool-definitions.ts
actions: ['list', 'create_material', 'update_material_shader']

// 2. Add properties to inputSchema
shaderType: { type: 'string', enum: ['DefaultLit', 'Unlit', 'Subsurface'] }

// 3. Implement in src/tools/assets.ts
case 'update_material_shader':
  return automationBridge.sendAutomationRequest('update_material_shader', params);

// 4. Add test case in tests/test-materials.mjs
{
  scenario: "Update material shader type",
  toolName: "manage_asset",
  arguments: {
    action: "update_material_shader",
    assetPath: "/Game/M_MasterMaterial_Test",
    shaderType: "Unlit"
  },
  expected: "success"
}
```

## 🎯 Key Integration Points

### WebSocket Communication
- **Client**: `src/automation/bridge.ts` (Node.js)
- **Server**: `McpBridgeWebSocket.cpp` (Unreal Plugin)
- **Protocol**: Custom MCP automation protocol with handshake
- **Reconnect**: Exponential backoff with jitter

### Cross-Process Data Flow
```
TypeScript Tool Call
    ↓
UnrealBridge.tryConnect()
    ↓
AutomationBridge.sendAutomationRequest()
    ↓
WebSocket → McpAutomationBridgeSubsystem
    ↓
ProcessAutomationRequest() → Handler
    ↓
FReply → JSON → TypeScript Response
```

### Error Handling
- **Timeouts**: 30s default, configurable per request
- **Retries**: 3 attempts with exponential backoff
- **Fallbacks**: Automatic TypeScript when WASM unavailable
- **Validation**: Zod schemas for all tool I/O

## 📊 Performance Optimization

### WASM Acceleration (5-8x faster)
- **JSON parsing**: Property parsing with depth limits
- **Transform math**: Vector/matrix operations
- **Dependency resolution**: Asset graph traversal
- **Topological sorting**: Build order calculation

### Command Throttling
- **Priority queue**: 1-10 priority levels
- **Minimum delay**: 100ms for stats, 300ms for operations
- **Concurrency limits**: Configurable max pending requests

### Caching
- **Asset lists**: 10-second TTL
- **Class resolution**: Memoized lookups
- **WebSocket connections**: Persistent with reconnect

## 🧪 Testing Strategy

### Test Structure
- **Markdown-based**: Human-readable test cases
- **Domain-specific**: 17 test files covering all tools
- **Integration-focused**: Real Unreal Engine operations

### Running Tests
```bash
# Single tool test
npm run test:manage_asset

# All tests
npm run test

# With debug output
LOG_LEVEL=debug npm run test:blueprint
```

### Test Case Format
```javascript
{
  scenario: "Descriptive test name",
  toolName: "manage_asset",
  arguments: { action: "create_material", name: "TestMaterial" },
  expected: "success|not_found|error_message"
}
```

## 🚨 Common Pitfalls

### 1. Plugin Not Enabled
**Symptom**: `Automation bridge not connected`
**Fix**: Enable "MCP Automation Bridge" in UE Editor Plugins

### 2. Port Conflicts
**Symptom**: Connection refused on :8091
**Fix**: Check `netstat -ano | findstr :8091` and kill conflicting process

### 3. WASM Missing
**Symptom**: Fallback to TypeScript warnings
**Fix**: Run `npm run build:wasm` or set `WASM_ENABLED=false`

### 4. Path Normalization
**Symptom**: Asset not found errors
**Fix**: Use `/Game` prefix, not `/Content`

### 5. Command Validation
**Symptom**: "Dangerous command blocked"
**Fix**: Use safe alternatives or whitelist in `CommandValidator`

## 📚 Essential References

### Key Files
- `src/constants.ts`: Configuration defaults and timeouts
- `src/utils/response-validator.ts`: Response schema validation
- `src/utils/command-validator.ts`: Safety rules and blocked commands
- `src/utils/unreal-command-queue.ts`: Throttling implementation

### Documentation
- `docs/handler-mapping.md`: TypeScript → C++ handler mappings
- `docs/GraphQL-API.md`: Query interface specification
- `README.md`: Complete setup and usage guide

### Debug Resources
- `ue://health`: Real-time metrics dashboard
- `ue://automation-bridge`: Connection status and pending requests
- UE Output Log: Filter for `bridge_ack` and `automation_request`

## 🎓 Learning Resources

### Understanding the Codebase
1. Start with `src/index.ts` → `createServer()`
2. Follow WebSocket flow in `src/automation/bridge.ts`
3. Examine a simple handler: `McpAutomationBridge_AssetQueryHandlers.cpp`
4. Study tool implementation: `src/tools/assets.ts`
5. Review test patterns: `tests/test-manage-asset.mjs`

### Recommended Reading Order
```
README.md → docs/handler-mapping.md → src/index.ts
→ src/automation/bridge.ts → src/tools/consolidated-*.ts
→ Plugins/.../McpAutomationBridgeSubsystem.cpp
```

## 🤝 Contribution Guidelines

### Pull Request Requirements
- **Scope**: Single tool/action per PR
- **Tests**: Add Markdown test cases
- **Documentation**: Update `handler-mapping.md` if adding handlers
- **Validation**: Ensure Zod schemas are comprehensive

### Code Style
- **TypeScript**: Follow ESLint rules (`npm run lint`)
- **C++**: Use Unreal coding standards
- **Comments**: Document non-obvious logic and safety considerations
- **Error handling**: Structured errors with context

### Review Checklist
- [ ] Tool works with and without WASM
- [ ] Response validation schema is complete
- [ ] Command throttling is appropriate
- [ ] Error messages are actionable
- [ ] Test cases cover success and failure paths

## 📈 Performance Tuning

### Configuration Variables
```env
# Connection
MCP_AUTOMATION_PORT=8091
MCP_AUTOMATION_REQUEST_TIMEOUT_MS=120000

# Performance
WASM_ENABLED=true
ASSET_LIST_TTL_MS=10000

# Logging
LOG_LEVEL=info
```

### Optimization Tips
- **Batch operations**: Use `delete_assets` instead of multiple `delete`
- **Caching**: Leverage 10s asset cache for repeated queries
- **Priority**: Set appropriate priority for time-sensitive operations
- **WASM**: Enable for JSON-heavy and math-intensive operations

## 🎯 Quick Reference

### Most Used Commands
```bash
# Development
npm run dev          # Start server with auto-reload
npm run build        # Build TypeScript + WASM
npm run lint         # Check code style

# Testing
npm run test         # All tests
npm run test:blueprint # Specific tool tests

# Plugin Management
npm run automation:sync    # Sync to UE project
npm run automation:verify  # Verify installation

# Debugging
LOG_LEVEL=debug npm run dev
netstat -ano | findstr :8091
```

### Common File Patterns
```
src/tools/<domain>.ts          # Tool implementations
src/tools/consolidated-*.ts    # Routing and validation
Plugins/.../*Handlers.cpp      # Native C++ handlers
Tests/test-<domain>.mjs        # Test cases
```

## 🆘 Troubleshooting

### Connection Issues
```bash
# Check if plugin is listening
netstat -ano | findstr :8091

# Verify plugin files
npm run automation:verify -- --project "C:/Path/To/Project"

# Clean and rebuild
rm -r Plugins/McpAutomationBridge/{Binaries,Intermediate}
npm run build
```

### Performance Problems
```bash
# Check WASM status
curl http://localhost:8090/metrics | grep wasm

# Monitor command queue
LOG_LEVEL=debug npm run dev

# Disable WASM for testing
WASM_ENABLED=false npm run dev
```

### Test Failures
```bash
# Run single test with debug
LOG_LEVEL=debug npm run test:manage_asset

# Check Unreal Editor logs
# Filter for "automation_request" in Output Log

# Verify plugin capabilities
ue://automation-bridge
```

## 📋 Checklist for New Contributors

1. [ ] Read `README.md` and this guide
2. [ ] Set up Unreal Engine project with required plugins
3. [ ] Run `npm run automation:sync`
4. [ ] Start server with `npm run dev`
5. [ ] Verify connection in UE Output Log
6. [ ] Run tests with `npm run test`
7. [ ] Explore existing tools and handlers
8. [ ] Start with small, focused changes
9. [ ] Add comprehensive test cases
10. [ ] Document new features in `handler-mapping.md`

## 🤖 AI Agent Specifics

### Understanding the System
- **Dual-process**: Node.js ↔ Unreal Editor plugin communication
- **Protocol**: Custom WebSocket-based automation protocol
- **Safety**: Command validation and throttling are mandatory
- **Performance**: WASM optimization is automatic but optional

### Common AI Tasks
1. **Add new automation**: Follow the two-step implementation pattern
2. **Fix connection issues**: Check plugin status and ports
3. **Optimize performance**: Leverage WASM and caching
4. **Extend functionality**: Add new actions to existing tools
5. **Debug problems**: Use metrics endpoints and detailed logging

### When to Ask for Help
- Plugin installation issues
- C++ handler implementation questions
- WebSocket protocol details
- Performance optimization strategies
- Test case design for complex scenarios

## 📝 Feedback Request

Please provide feedback on:
1. **Clarity**: Are the workflows and patterns clear?
2. **Completeness**: Are any critical aspects missing?
3. **Accuracy**: Are the examples and commands correct?
4. **Organization**: Is the information easy to find?

Suggest improvements to make this guide more helpful for AI agents working on this codebase!
