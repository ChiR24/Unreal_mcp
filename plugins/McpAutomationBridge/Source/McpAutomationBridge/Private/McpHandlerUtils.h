// =============================================================================
// McpHandlerUtils.h
// =============================================================================
// Centralized utility functions and macros for MCP Automation Bridge handlers.
// 
// This file provides:
// - Standardized JSON parsing helpers
// - Common response building utilities
// - Action dispatch macros
// - Blueprint graph manipulation utilities
// - Pin type conversion helpers
//
// REFACTORING NOTES:
// - Functions extracted from McpAutomationBridge_BlueprintHandlers.cpp (900+ lines)
// - Response building standardized across all 56 handler files
// - JSON parsing patterns consolidated from duplicated implementations
// 
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

// Forward declarations
class UMcpAutomationBridgeSubsystem;
class FMcpBridgeWebSocket;

// =============================================================================
// MCP Namespace for Handler Utilities
// =============================================================================
namespace McpHandlerUtils
{
    // =========================================================================
    // JSON Response Building
    // =========================================================================
    
    /**
     * Build a standardized success response object.
     * @param Message Human-readable success message
     * @param Data Optional result data object
     * @return JSON object ready for SendAutomationResponse
     */
    inline TSharedPtr<FJsonObject> BuildSuccessResponse(
        const FString& Message,
        const TSharedPtr<FJsonObject>& Data = nullptr)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), Message);
        if (Data.IsValid())
        {
            Response->SetObjectField(TEXT("data"), Data);
        }
        return Response;
    }

    /**
     * Build a standardized error response object.
     * @param ErrorCode Short error code (e.g., "INVALID_PARAM", "NOT_FOUND")
     * @param Message Human-readable error message
     * @param Details Optional additional error details
     * @return JSON object ready for SendAutomationResponse
     */
    inline TSharedPtr<FJsonObject> BuildErrorResponse(
        const FString& ErrorCode,
        const FString& Message,
        const TSharedPtr<FJsonObject>& Details = nullptr)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), Message);
        Response->SetStringField(TEXT("code"), ErrorCode);
        if (Details.IsValid())
        {
            Response->SetObjectField(TEXT("details"), Details);
        }
        return Response;
    }

    // =========================================================================
    // JSON Field Extraction Helpers (with validation)
    // =========================================================================

    /**
     * Extract a required string field with validation and error response.
     * @param Payload The JSON payload to extract from
     * @param FieldName The field name to extract
     * @param OutValue Output value if found
     * @param OutError Error message if extraction failed
     * @return true if field was found and extracted
     */
    inline bool TryGetRequiredString(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        FString& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        if (!Payload->TryGetStringField(FieldName, OutValue))
        {
            OutError = FString::Printf(TEXT("Missing required field '%s'"), *FieldName);
            return false;
        }
        
        if (OutValue.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Field '%s' is empty"), *FieldName);
            return false;
        }
        
        return true;
    }

    /**
     * Extract an optional string field with default value.
     */
    inline FString GetOptionalString(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        const FString& DefaultValue = FString())
    {
        FString Value;
        if (Payload.IsValid() && Payload->TryGetStringField(FieldName, Value))
        {
            return Value;
        }
        return DefaultValue;
    }

    /**
     * Extract a required integer field with validation.
     */
    inline bool TryGetRequiredInt(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        int32& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        if (!Payload->TryGetNumberField(FieldName, OutValue))
        {
            OutError = FString::Printf(TEXT("Missing required integer field '%s'"), *FieldName);
            return false;
        }
        
        return true;
    }

    /**
     * Extract an optional integer field with default value.
     */
    inline int32 GetOptionalInt(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        int32 DefaultValue = 0)
    {
        int32 Value = DefaultValue;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(FieldName, Value);
        }
        return Value;
    }

    /**
     * Extract a required float/double field with validation.
     */
    inline bool TryGetRequiredFloat(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        double& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        if (!Payload->TryGetNumberField(FieldName, OutValue))
        {
            OutError = FString::Printf(TEXT("Missing required number field '%s'"), *FieldName);
            return false;
        }
        
        return true;
    }

    /**
     * Extract a required boolean field with validation.
     */
    inline bool TryGetRequiredBool(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        bool& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        if (!Payload->TryGetBoolField(FieldName, OutValue))
        {
            OutError = FString::Printf(TEXT("Missing required boolean field '%s'"), *FieldName);
            return false;
        }
        
        return true;
    }

    /**
     * Extract an optional boolean field with default value.
     */
    inline bool GetOptionalBool(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        bool DefaultValue = false)
    {
        bool Value = DefaultValue;
        if (Payload.IsValid())
        {
            Payload->TryGetBoolField(FieldName, Value);
        }
        return Value;
    }

    /**
     * Extract a required object field with validation.
     */
    inline bool TryGetRequiredObject(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        TSharedPtr<FJsonObject>& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        const TSharedPtr<FJsonObject>* ObjectPtr;
        if (!Payload->TryGetObjectField(FieldName, ObjectPtr))
        {
            OutError = FString::Printf(TEXT("Missing required object field '%s'"), *FieldName);
            return false;
        }
        
        OutValue = *ObjectPtr;
        return OutValue.IsValid();
    }

    /**
     * Extract a required array field with validation.
     */
    inline bool TryGetRequiredArray(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        TArray<TSharedPtr<FJsonValue>>& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
        if (!Payload->TryGetArrayField(FieldName, ArrayPtr))
        {
            OutError = FString::Printf(TEXT("Missing required array field '%s'"), *FieldName);
            return false;
        }
        
        OutValue = *ArrayPtr;
        return true;
    }

    // =========================================================================
    // JSON Value Conversion
    // =========================================================================

    /**
     * Convert a JSON value to its string representation.
     * Handles all JSON types including objects and arrays.
     */
    MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);

    /**
     * Convert a FVector to a JSON object.
     */
    inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), Vector.X);
        Obj->SetNumberField(TEXT("y"), Vector.Y);
        Obj->SetNumberField(TEXT("z"), Vector.Z);
        return Obj;
    }

    /**
     * Convert a FRotator to a JSON object.
     */
    inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
        return Obj;
    }

    /**
     * Convert a FTransform to a JSON object.
     */
    inline TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetTranslation()));
        Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
        Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
        return Obj;
    }

    /**
     * Parse a JSON object to FVector.
     */
    inline bool JsonToVector(const TSharedPtr<FJsonObject>& Obj, FVector& OutVector)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double X = 0.0, Y = 0.0, Z = 0.0;
        Obj->TryGetNumberField(TEXT("x"), X);
        Obj->TryGetNumberField(TEXT("y"), Y);
        Obj->TryGetNumberField(TEXT("z"), Z);
        
        // Also try uppercase keys
        if (!Obj->HasField(TEXT("x")))
        {
            Obj->TryGetNumberField(TEXT("X"), X);
            Obj->TryGetNumberField(TEXT("Y"), Y);
            Obj->TryGetNumberField(TEXT("Z"), Z);
        }
        
        OutVector = FVector(X, Y, Z);
        return true;
    }

    /**
     * Parse a JSON object to FRotator.
     */
    inline bool JsonToRotator(const TSharedPtr<FJsonObject>& Obj, FRotator& OutRotator)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
        Obj->TryGetNumberField(TEXT("pitch"), Pitch);
        Obj->TryGetNumberField(TEXT("yaw"), Yaw);
        Obj->TryGetNumberField(TEXT("roll"), Roll);
        
        // Also try uppercase keys
        if (!Obj->HasField(TEXT("pitch")))
        {
            Obj->TryGetNumberField(TEXT("Pitch"), Pitch);
            Obj->TryGetNumberField(TEXT("Yaw"), Yaw);
            Obj->TryGetNumberField(TEXT("Roll"), Roll);
        }
        
        OutRotator = FRotator(Pitch, Yaw, Roll);
        return true;
    }

    /**
     * Parse a JSON object or array to FLinearColor.
     * Accepts both {r, g, b, a} object and [r, g, b, a] array formats.
     */
    inline bool JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, FLinearColor& OutColor)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
        Obj->TryGetNumberField(TEXT("r"), R);
        Obj->TryGetNumberField(TEXT("g"), G);
        Obj->TryGetNumberField(TEXT("b"), B);
        Obj->TryGetNumberField(TEXT("a"), A);
        
        OutColor = FLinearColor(R, G, B, A);
        return true;
    }

    /**
     * Convert FLinearColor to a JSON object.
     */
    inline TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("r"), Color.R);
        Obj->SetNumberField(TEXT("g"), Color.G);
        Obj->SetNumberField(TEXT("b"), Color.B);
        Obj->SetNumberField(TEXT("a"), Color.A);
        return Obj;
    }

    // =========================================================================
    // Action Dispatch Helpers
    // =========================================================================

    /**
     * Normalize an action string for case-insensitive comparison.
     * Also extracts sub-action from payload if present.
     * 
     * @param Action The action string to normalize
     * @param Payload Optional payload to check for subAction field
     * @return Normalized lowercase action string
     */
    inline FString NormalizeAction(const FString& Action, const TSharedPtr<FJsonObject>& Payload = nullptr)
    {
        FString Normalized = Action.ToLower();
        
        // Check for subAction in payload
        if (Payload.IsValid())
        {
            FString SubAction;
            if (Payload->TryGetStringField(TEXT("subAction"), SubAction) && !SubAction.IsEmpty())
            {
                Normalized = SubAction.ToLower();
            }
        }
        
        return Normalized;
    }

    /**
     * Check if an action matches a pattern (case-insensitive).
     */
    inline bool ActionMatches(const FString& Action, const FString& Pattern)
    {
        return Action.ToLower().Equals(Pattern.ToLower());
    }

    /**
     * Check if action matches any of multiple patterns.
     */
    inline bool ActionMatchesAny(const FString& Action, const TArray<FString>& Patterns)
    {
        const FString LowerAction = Action.ToLower();
        for (const FString& Pattern : Patterns)
        {
            if (LowerAction.Equals(Pattern.ToLower()))
            {
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // Asset Path Utilities
    // =========================================================================

    /**
     * Validate and normalize an asset path.
     * Returns empty string if invalid.
     */
    MCPAUTOMATIONBRIDGE_API FString ValidateAssetPath(const FString& Path);

    /**
     * Extract asset name from a full path.
     */
    inline FString ExtractAssetName(const FString& Path)
    {
        int32 LastSlash;
        if (Path.FindLastChar('/', LastSlash))
        {
            return Path.Mid(LastSlash + 1);
        }
        return Path;
    }

    /**
     * Extract package path from a full asset path.
     * e.g., /Game/MyFolder/MyAsset -> /Game/MyFolder
     */
    inline FString ExtractPackagePath(const FString& AssetPath)
    {
        int32 LastSlash;
        if (AssetPath.FindLastChar('/', LastSlash) && LastSlash > 0)
        {
            return AssetPath.Left(LastSlash);
        }
        return AssetPath;
    }

    // =========================================================================
    // Actor/Component Utilities
    // =========================================================================

#if WITH_EDITOR
    /**
     * Find an actor by name in the current world.
     * @param ActorName Actor name to search for
     * @param bExactMatch If true, requires exact name match; otherwise partial match
     * @return Found actor or nullptr
     */
    MCPAUTOMATIONBRIDGE_API class AActor* FindActorByName(const FString& ActorName, bool bExactMatch = true);

    /**
     * Find a component by name on an actor.
     * @param Actor The actor to search
     * @param ComponentName Component name to search for
     * @return Found component or nullptr
     */
    MCPAUTOMATIONBRIDGE_API class UActorComponent* FindActorComponentByName(
        class AActor* Actor, 
        const FString& ComponentName);
#endif

    // =========================================================================
    // String Utilities
    // =========================================================================

    /**
     * Convert a string to a safe asset name (remove invalid characters).
     */
    MCPAUTOMATIONBRIDGE_API FString ToSafeAssetName(const FString& Input);

    /**
     * Generate a unique asset name by appending a number if necessary.
     * @param BaseName The base name to make unique
     * @param PackagePath The package path to check for existing assets
     * @return A unique asset name
     */
    MCPAUTOMATIONBRIDGE_API FString MakeUniqueAssetName(const FString& BaseName, const FString& PackagePath);

    // =========================================================================
    // Logging Utilities
    // =========================================================================

    /**
     * Log an automation request (for debugging).
     */
    inline void LogAutomationRequest(const FString& RequestId, const FString& Action, const FString& PayloadPreview)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[MCP] Request %s: Action='%s' Payload='%s'"),
            *RequestId, *Action, *PayloadPreview.Left(200));
    }

    // =========================================================================
    // Object and Property Resolution Helpers
    // =========================================================================
    
#if WITH_EDITOR
    /**
     * Resolve an object from various path formats.
     * Supports: Actor names, component paths (Actor.Component), asset paths (/Game/...)
     * @param ObjectPath The path to resolve
     * @param OutResolvedPath Optional output for the normalized path
     * @return The resolved UObject, or nullptr if not found
     */
    MCPAUTOMATIONBRIDGE_API UObject* ResolveObjectFromPath(
        const FString& ObjectPath,
        FString* OutResolvedPath = nullptr);
#endif
    
    /**
     * Result struct for property resolution.
     */
    struct FPropertyResolveResult
    {
        FProperty* Property = nullptr;
        void* Container = nullptr;
        FString Error;
        bool IsValid() const { return Property != nullptr && Container != nullptr; }
    };
    
    /**
     * Resolve a property from an object, handling both simple and nested paths.
     * @param Object The object to resolve the property on
     * @param PropertyName The property name (can be "Property" or "Component.Property")
     * @return Result containing Property, Container, and any error message
     */
    MCPAUTOMATIONBRIDGE_API FPropertyResolveResult ResolveProperty(
        UObject* Object,
        const FString& PropertyName);

    /**
     * Truncate a string for logging (prevent log spam).
     */
    inline FString TruncateForLog(const FString& Input, int32 MaxLength = 256)
    {
        if (Input.Len() <= MaxLength)
        {
            return Input;
        }
        return Input.Left(MaxLength) + TEXT("...");
    }

    // =========================================================================
    // Standard Response Builders (reduces MakeShared<FJsonObject> duplication)
    // =========================================================================
    
    /**
     * Create a result object with common fields.
     * Pattern: Result->SetStringField(TEXT("name"), Name);
     */
    inline TSharedPtr<FJsonObject> CreateResultObject()
    {
        return MakeShared<FJsonObject>();
    }
    
    /**
     * Create a result object with a single string field.
     */
    inline TSharedPtr<FJsonObject> CreateResultObject(const FString& Key, const FString& Value)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(Key, Value);
        return Result;
    }
    
    /**
     * Create a result object with name and path fields (common pattern).
     */
    inline TSharedPtr<FJsonObject> CreateNamedResult(const FString& Name, const FString& Path)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("path"), Path);
        return Result;
    }
    
    /**
     * Add common verification fields to a result object.
     * Determines actor vs asset automatically.
     */
    MCPAUTOMATIONBRIDGE_API void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);
}

// =============================================================================
// Action Dispatch Macros
// =============================================================================

/**
 * Macro for dispatching actions based on string matching.
 * Usage:
 *   MCP_DISPATCH_ACTION(Action, "spawn", HandleSpawn(RequestId, Payload, Socket))
 *   MCP_DISPATCH_ACTION(Action, "delete", HandleDelete(RequestId, Payload, Socket))
 */
#define MCP_DISPATCH_ACTION(ActionVar, ActionName, HandlerCall) \
    if (McpHandlerUtils::ActionMatches(ActionVar, TEXT(ActionName))) \
    { \
        return HandlerCall; \
    }

/**
 * Macro for dispatching actions with subAction extraction.
 * Automatically extracts subAction from payload if present.
 */
#define MCP_DISPATCH_SUBACTION(ActionVar, Payload, SubActionName, HandlerCall) \
    { \
        FString SubAction = McpHandlerUtils::NormalizeAction(ActionVar, Payload); \
        if (SubAction.Equals(TEXT(SubActionName), ESearchCase::IgnoreCase)) \
        { \
            return HandlerCall; \
        } \
    }

/**
 * Macro for standard error response when payload is invalid.
 */
#define MCP_ERROR_INVALID_PAYLOAD(SendError, RequestId, Message) \
    SendError(nullptr, RequestId, Message, TEXT("INVALID_PAYLOAD"))

/**
 * Macro for standard error response when a required parameter is missing.
 */
#define MCP_ERROR_MISSING_PARAM(SendError, RequestId, ParamName) \
    SendError(nullptr, RequestId, FString::Printf(TEXT("Missing required parameter: %s"), *ParamName), TEXT("MISSING_PARAMETER"))

/**
 * Macro for standard error response when an operation is not found.
 */
#define MCP_ERROR_NOT_FOUND(SendError, RequestId, ItemName, ItemId) \
    SendError(nullptr, RequestId, FString::Printf(TEXT("%s not found: %s"), *ItemName, *ItemId), TEXT("NOT_FOUND"))

/**
 * Macro for standard success response with optional message.
 */
#define MCP_SUCCESS_RESPONSE(SendResponse, RequestId, Message, ResultObj) \
    SendResponse(nullptr, RequestId, true, Message, ResultObj)

// =============================================================================
// Blueprint Graph Utilities (Editor-only)
// =============================================================================
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

#include "EdGraphSchema_K2.h"

namespace McpBlueprintUtils
{
    // -------------------------------------------------------------------------
    // Pin Finding Utilities
    // -------------------------------------------------------------------------

    /**
     * Find an execution pin on a node by direction.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);

    /**
     * Find an output pin on a node by name.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindOutputPin(UEdGraphNode* Node, const FName& PinName = NAME_None);

    /**
     * Find an input pin on a node by name.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindInputPin(UEdGraphNode* Node, const FName& PinName);

    /**
     * Find a data pin on a node by direction and optionally by name.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName& PreferredName = NAME_None);

    /**
     * Find the preferred event execution output pin in a graph.
     * Prefers custom events, falls back to first available event.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindPreferredEventExec(UEdGraph* Graph);

    // -------------------------------------------------------------------------
    // Pin Type Conversion
    // -------------------------------------------------------------------------

    /**
     * Convert a string type name to an FEdGraphPinType.
     * Supports: float, int, int64, bool, string, name, text, byte, vector, rotator, transform, object, class
     * Also resolves class/struct/enum paths.
     */
    MCPAUTOMATIONBRIDGE_API FEdGraphPinType MakePinType(const FString& TypeName);

    /**
     * Convert an FEdGraphPinType to a human-readable string.
     * Handles containers (Array, Set, Map) and complex types.
     */
    MCPAUTOMATIONBRIDGE_API FString DescribePinType(const FEdGraphPinType& PinType);

    // -------------------------------------------------------------------------
    // Node Creation Utilities
    // -------------------------------------------------------------------------

    /**
     * Create a variable getter node in a graph.
     */
    MCPAUTOMATIONBRIDGE_API UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY);

    /**
     * Log a pin connection failure for debugging.
     */
    MCPAUTOMATIONBRIDGE_API void LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response);

    // -------------------------------------------------------------------------
    // Blueprint Introspection
    // -------------------------------------------------------------------------

    /**
     * Collect all variables from a blueprint as JSON.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintVariables(UBlueprint* Blueprint);

    /**
     * Collect all functions from a blueprint as JSON.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintFunctions(UBlueprint* Blueprint);

    /**
     * Find a property in a blueprint by name.
     */
    MCPAUTOMATIONBRIDGE_API FProperty* FindBlueprintProperty(UBlueprint* Blueprint, const FString& PropertyName);

    /**
     * Find a function in a blueprint by name.
     */
    MCPAUTOMATIONBRIDGE_API UFunction* FindBlueprintFunction(UBlueprint* Blueprint, const FString& FunctionName);
}

#endif // WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2