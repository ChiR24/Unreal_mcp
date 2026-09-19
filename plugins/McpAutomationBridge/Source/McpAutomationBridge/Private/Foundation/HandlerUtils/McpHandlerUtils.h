#pragma once

#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsPropertyResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsResponses.h"

#define MCP_DISPATCH_SUBACTION(ActionVar, Payload, SubActionName, HandlerCall) \
    { \
        FString SubAction = McpHandlerUtils::NormalizeAction(ActionVar, Payload); \
        if (SubAction.Equals(TEXT(SubActionName), ESearchCase::IgnoreCase)) \
        { \
            return HandlerCall; \
        } \
    }
