#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR
namespace McpBlueprintUtils
{
MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);
MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindOutputPin(UEdGraphNode* Node, const FName& PinName = NAME_None);
MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindInputPin(UEdGraphNode* Node, const FName& PinName);
MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindDataPin(
    UEdGraphNode* Node,
    EEdGraphPinDirection Direction,
    const FName& PreferredName = NAME_None);
MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindPreferredEventExec(UEdGraph* Graph);

/**
 * Structured result of a type-string resolution.  When bSuccess is false,
 * OutError contains a human-readable diagnostic that identifies which part
 * of a (potentially nested) type specification failed (e.g.
 * "Unsupported map key type 'MyStruct' in 'Map:MyStruct,Int'").
 *
 * The legacy MakePinType() wrapper calls ResolvePinType() and, on failure,
 * returns a PC_Wildcard pin (preserving the previous fallback behaviour for
 * existing call-sites that have not yet been upgraded to surface errors).
 */
struct FTypeResolutionResult
{
    bool bSuccess = false;
    FEdGraphPinType PinType;
    FString OutError;
};

/**
 * Canonical type-string -> FEdGraphPinType resolver used by struct members,
 * Blueprint variables, function/event parameters, Make/Break Struct nodes,
 * typed container nodes and DataTable row-struct workflows.
 *
 * Supported syntax (both colon and angle-bracket forms are accepted for
 * backward compatibility; the serialised canonical form is the colon form):
 *
 *   Primitives :  Bool Byte Int Int64 Float Double String Name Text
 *   Structs    :  Vector Vector2D Vector4 Rotator Transform Color
 *                 LinearColor  (case-insensitive short names)
 *                 Struct:/Game/Path/ST_X.ST_X        (full object path)
 *                 Struct:/Script/Engine.FVector      (native UScriptStruct)
 *   Enums      :  Enum:/Game/Path/E_X.E_X
 *                 Enum:/Script/Engine.EMyEnum
 *   References:  Object   Object:/Script/Engine.Actor
 *                 Class    Class:/Script/Engine.Actor
 *                 SoftObject  SoftObject:/Script/Engine.Texture2D
 *                 SoftClass   SoftClass:/Script/Engine.Actor
 *   Containers :  Array:<Type>
 *                 Set:<Type>
 *                 Map:<KeyType>,<ValueType>
 *
 * Nesting is recursive (e.g. Array:Map:Name,Struct:/Game/Structs/ST_X.ST_X).
 *
 * Validation rules:
 *   - Unknown base type names are rejected (error).
 *   - Unresolved structs/enums/classes/objects are rejected (error).
 *   - Recursive *by-value* struct references are rejected (the caller may
 *     pass the owning struct path via InSelfStructPath to enable the check;
 *     pass NAME_None to skip the self-reference guard).
 *   - Unsupported map key types are rejected (only hashable primitives are
 *     valid map keys: Byte, Int, Int64, Name, String, and enums).
 *   - Malformed container forms are rejected (empty Array:/Set:/Map: body,
 *     Map with three or more comma-separated parts).
 *   - A malformed type is NEVER silently coerced to Int or any other valid
 *     type; on failure PinType is left as a PC_Wildcard placeholder.
 *
 * Round-trips with DescribePinType: resolving DescribePinType(result.PinType)
 * yields the canonical colon-form string of the resolved type.
 */
MCPAUTOMATIONBRIDGE_API FTypeResolutionResult ResolvePinType(
    const FString& TypeSpec,
    const FName& InSelfStructPath = NAME_None);

/** Legacy wrapper: resolves the type and falls back to PC_Wildcard on error
 *  (logging the diagnostic).  Prefer ResolvePinType() in new code. */
MCPAUTOMATIONBRIDGE_API FEdGraphPinType MakePinType(const FString& TypeName);
MCPAUTOMATIONBRIDGE_API FString DescribePinType(const FEdGraphPinType& PinType);
MCPAUTOMATIONBRIDGE_API UK2Node_VariableGet* CreateVariableGetter(
    UEdGraph* Graph,
    const FMemberReference& VarRef,
    float NodePosX,
    float NodePosY);
MCPAUTOMATIONBRIDGE_API void LogConnectionFailure(
    const TCHAR* Context,
    UEdGraphPin* SourcePin,
    UEdGraphPin* TargetPin,
    const FPinConnectionResponse& Response);
MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintVariables(UBlueprint* Blueprint);
MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintFunctions(UBlueprint* Blueprint);
MCPAUTOMATIONBRIDGE_API FProperty* FindBlueprintProperty(UBlueprint* Blueprint, const FString& PropertyName);
MCPAUTOMATIONBRIDGE_API UFunction* FindBlueprintFunction(UBlueprint* Blueprint, const FString& FunctionName);
}
#endif
