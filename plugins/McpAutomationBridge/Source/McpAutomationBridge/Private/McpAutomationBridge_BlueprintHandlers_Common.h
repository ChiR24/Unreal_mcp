#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#if defined(__has_include)
#if __has_include("BlueprintGraph/K2Node_CallFunction.h")
#include "BlueprintGraph/K2Node_CallFunction.h"
#include "BlueprintGraph/K2Node_CustomEvent.h"
#include "BlueprintGraph/K2Node_Event.h"
#include "BlueprintGraph/K2Node_FunctionEntry.h"
#include "BlueprintGraph/K2Node_FunctionResult.h"
#include "BlueprintGraph/K2Node_VariableGet.h"
#include "BlueprintGraph/K2Node_VariableSet.h"
#elif __has_include("BlueprintGraph/Classes/K2Node_CallFunction.h")
#include "BlueprintGraph/Classes/K2Node_CallFunction.h"
#include "BlueprintGraph/Classes/K2Node_CustomEvent.h"
#include "BlueprintGraph/Classes/K2Node_Event.h"
#include "BlueprintGraph/Classes/K2Node_FunctionEntry.h"
#include "BlueprintGraph/Classes/K2Node_FunctionResult.h"
#include "BlueprintGraph/Classes/K2Node_VariableGet.h"
#include "BlueprintGraph/Classes/K2Node_VariableSet.h"
#else
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#endif
#else
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#endif
#endif

// Shared Blueprint busy set for reentrancy protection
extern TSet<FString> GBlueprintBusySet;

// Macro aliases for Pin Categories
#define MCP_PC_Exec UEdGraphSchema_K2::PC_Exec
#define MCP_PC_Boolean UEdGraphSchema_K2::PC_Boolean
#define MCP_PC_Byte UEdGraphSchema_K2::PC_Byte
#define MCP_PC_Class UEdGraphSchema_K2::PC_Class
#define MCP_PC_Int UEdGraphSchema_K2::PC_Int
#define MCP_PC_Int64 UEdGraphSchema_K2::PC_Int64
#define MCP_PC_Float UEdGraphSchema_K2::PC_Float
#define MCP_PC_Double UEdGraphSchema_K2::PC_Double
#define MCP_PC_Name UEdGraphSchema_K2::PC_Name
#define MCP_PC_Object UEdGraphSchema_K2::PC_Object
#define MCP_PC_String UEdGraphSchema_K2::PC_String
#define MCP_PC_Text UEdGraphSchema_K2::PC_Text
#define MCP_PC_Struct UEdGraphSchema_K2::PC_Struct
#define MCP_PC_Wildcard UEdGraphSchema_K2::PC_Wildcard

// Helper functions (implemented in McpAutomationBridge_BlueprintHandlers.cpp)
FEdGraphPinType FMcpAutomationBridge_MakePinType(const FString &InType);
FString FMcpAutomationBridge_DescribePinType(const FEdGraphPinType &PinType);
void FMcpAutomationBridge_AddUserDefinedPin(UK2Node *Node, const FString &PinName, const FString &PinType, EEdGraphPinDirection Direction);
TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildBlueprintSnapshot(UBlueprint *Blueprint, const FString &Path);
TSharedPtr<FJsonObject> FMcpAutomationBridge_EnsureBlueprintEntry(const FString &Path);
TSharedPtr<FJsonObject> FMcpAutomationBridge_FindNamedEntry(const TArray<TSharedPtr<FJsonValue>> &Array, const FString &Key, const FString &Value);
FString FMcpAutomationBridge_JsonValueToString(const TSharedPtr<FJsonValue> &Value);
FName FMcpAutomationBridge_ResolveMetadataKey(const FString &RawKey);
void FMcpAutomationBridge_AppendPinsJson(const TArray<TSharedPtr<struct FUserPinInfo>> &Pins, TArray<TSharedPtr<FJsonValue>> &Out);
bool FMcpAutomationBridge_CollectVariableMetadata(const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc, TSharedPtr<FJsonObject> &OutMetadata);
TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildVariableJson(const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintVariables(UBlueprint *Blueprint);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintFunctions(UBlueprint *Blueprint);
void FMcpAutomationBridge_CollectEventPins(UK2Node *Node, TArray<TSharedPtr<FJsonValue>> &Out);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintEvents(UBlueprint *Blueprint);

// Additional helpers for graph manipulation
UEdGraphPin *FMcpAutomationBridge_FindExecPin(UEdGraphNode *Node, EEdGraphPinDirection Direction);
UEdGraphPin *FMcpAutomationBridge_FindOutputPin(UEdGraphNode *Node, const FName &PinName = NAME_None);
UEdGraphPin *FMcpAutomationBridge_FindPreferredEventExec(UEdGraph *Graph);
void FMcpAutomationBridge_LogConnectionFailure(const TCHAR *Context, UEdGraphPin *SourcePin, UEdGraphPin *TargetPin, const struct FPinConnectionResponse &Response);
UEdGraphPin *FMcpAutomationBridge_FindInputPin(UEdGraphNode *Node, const FName &PinName);
UEdGraphPin *FMcpAutomationBridge_FindDataPin(UEdGraphNode *Node, EEdGraphPinDirection Direction, const FName &PreferredName = NAME_None);
UK2Node_VariableGet *FMcpAutomationBridge_CreateVariableGetter(UEdGraph *Graph, const struct FMemberReference &VarRef, float NodePosX, float NodePosY);
bool FMcpAutomationBridge_AttachValuePin(UK2Node_VariableSet *VarSet, UEdGraph *Graph, const class UEdGraphSchema_K2 *Schema, bool &bOutLinked);
bool FMcpAutomationBridge_EnsureExecLinked(UEdGraph *Graph);
UFunction *FMcpAutomationBridge_ResolveFunction(UBlueprint *Blueprint, const FString &FunctionName);
FProperty *FMcpAutomationBridge_FindProperty(UBlueprint *Blueprint, const FString &PropertyName);
