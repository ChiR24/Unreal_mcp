#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "EdGraph/EdGraph.h"

// Mirror the inventory handlers' JSON payload accessors (GetJsonStringField /
// GetJsonBoolField / GetJsonNumberField live in McpAutomationBridgeHelpersJsonFields.h).
#define GetPayloadString GetJsonStringField
#define GetPayloadBool GetJsonBoolField
#define GetPayloadNumber GetJsonNumberField

FGuid ResolveMemberGuid(UUserDefinedStruct* S, const FString& VarGuidStr, const FString& MemberName);
FEdGraphPinType ParseMemberType(const FString& TypeStr);
FString PinTypeToSummary(const FEdGraphPinType& Pin);
FString UserDefinedStructureStatusToString(EUserDefinedStructureStatus Status);
TSharedPtr<FJsonObject> VariableDescriptionToJson(const FStructVariableDescription& Var);
FString BuildDefaultExportText(UUserDefinedStruct* S, FProperty* Prop, const TSharedPtr<FJsonValue>& JsonValue);
void ForEachReferencingBlueprint(UUserDefinedStruct* S, TFunction<void(UBlueprint*)> Callback);

bool HandleStructAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructLifecycleActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberAddRemoveActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberEditActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructSerializationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructImportActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAssetActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructPropertyAction(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
