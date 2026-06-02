// McpToolDefinition.h — Base class for self-describing MCP tool definitions

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Descriptor for one MCP tool. Pure metadata — no execution logic.
 *
 * Each tool .cpp defines a static subclass and registers it via MCP_REGISTER_TOOL.
 * The registry queries these at startup to build tools/list responses and
 * resolve dispatch actions for tools/call.
 */
class FMcpToolDefinition
{
public:
	virtual ~FMcpToolDefinition() = default;

	/** MCP tool name (e.g. "build_environment"). Used as the "name" field in tools/list. */
	virtual FString GetName() const = 0;

	/** Human-readable description shown to the LLM. */
	virtual FString GetDescription() const = 0;

	/** Category for dynamic tool management (core, world, gameplay, utility). */
	virtual FString GetCategory() const = 0;

	/**
	 * Build the inputSchema JSON object for this tool.
	 * Called once at startup; result is cached by the registry.
	 */
	virtual TSharedPtr<FJsonObject> BuildInputSchema() const = 0;

	/**
	 * The Action string to pass to ProcessAutomationRequest.
	 *
	 * Pattern A (default): Returns the tool name. The handler checks
	 *   Action == "tool_name" and reads the sub-action from payload internally.
	 *
	 * Pattern B: Returns empty string. The transport extracts the sub-action
	 *   from the arguments field (see GetActionFieldName) and passes that
	 *   as the Action parameter instead of the tool name.
	 */
	virtual FString GetDispatchAction() const { return GetName(); }

	/**
	 * The field name in arguments that contains the sub-action for Pattern B tools.
	 * Most tools use "action"; some older tools use "subAction".
	 * Only consulted when GetDispatchAction() returns empty.
	 */
	virtual FString GetActionFieldName() const { return TEXT("action"); }

	/** True if this tool uses tool-name dispatch (Pattern A). */
	bool UsesToolNameDispatch() const { return !GetDispatchAction().IsEmpty(); }
};
