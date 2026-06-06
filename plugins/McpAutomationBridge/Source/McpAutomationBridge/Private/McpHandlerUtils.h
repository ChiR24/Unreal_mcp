#pragma once

#include "McpHandlerUtilsActionsPaths.h"
#include "McpHandlerUtilsBlueprintGraph.h"
#include "McpHandlerUtilsJson.h"
#include "McpHandlerUtilsResponses.h"

#define MCP_DISPATCH_ACTION(ActionVar, ActionName, HandlerCall) \
    if (McpHandlerUtils::ActionMatches(ActionVar, TEXT(ActionName))) \
    { \
        return HandlerCall; \
    }

#define MCP_DISPATCH_SUBACTION(ActionVar, Payload, SubActionName, HandlerCall) \
    { \
        FString SubAction = McpHandlerUtils::NormalizeAction(ActionVar, Payload); \
        if (SubAction.Equals(TEXT(SubActionName), ESearchCase::IgnoreCase)) \
        { \
            return HandlerCall; \
        } \
    }

#define MCP_ERROR_INVALID_PAYLOAD(SendError, RequestId, Message) \
    SendError(nullptr, RequestId, Message, TEXT("INVALID_PAYLOAD"))

#define MCP_ERROR_MISSING_PARAM(SendError, RequestId, ParamName) \
    SendError(nullptr, RequestId, FString::Printf(TEXT("Missing required parameter: %s"), *ParamName), TEXT("MISSING_PARAMETER"))

#define MCP_ERROR_NOT_FOUND(SendError, RequestId, ItemName, ItemId) \
    SendError(nullptr, RequestId, FString::Printf(TEXT("%s not found: %s"), *ItemName, *ItemId), TEXT("NOT_FOUND"))

#define MCP_SUCCESS_RESPONSE(SendResponse, RequestId, Message, ResultObj) \
    SendResponse(nullptr, RequestId, true, Message, ResultObj)
