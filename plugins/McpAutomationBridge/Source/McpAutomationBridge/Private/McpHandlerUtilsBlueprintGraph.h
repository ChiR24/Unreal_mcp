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
