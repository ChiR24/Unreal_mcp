#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if WITH_EDITOR
namespace McpMiscHandlers
{
bool HandleSetReplication(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    bool bReplicates = GetBoolField(Payload, TEXT("replicates"), true);
    bool bReplicateMovement = GetBoolField(Payload, TEXT("replicateMovement"), true);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        CDO->SetReplicates(bReplicates);
        CDO->SetReplicateMovement(bReplicateMovement);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetBoolField(TEXT("replicates"), bReplicates);
    ResponseJson->SetBoolField(TEXT("replicateMovement"), bReplicateMovement);
    McpHandlerUtils::AddVerification(ResponseJson, Blueprint);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Replication settings configured for %s"), *BlueprintPath), ResponseJson);
    return true;
}

bool HandleCreateReplicatedVariable(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString VariableName = GetStringField(Payload, TEXT("variableName"), TEXT(""));
    FString VariableType = GetStringField(Payload, TEXT("variableType"), TEXT("Boolean"));

    if (BlueprintPath.IsEmpty() || VariableName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath and variableName are required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    if (VariableType.Equals(TEXT("Integer"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }

    bool bCreated = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);
    if (bCreated)
    {
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*VariableName))
            {
                VarDesc.PropertyFlags |= CPF_Net;
                break;
            }
        }
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetStringField(TEXT("variableName"), VariableName);
    ResponseJson->SetStringField(TEXT("variableType"), VariableType);
    ResponseJson->SetBoolField(TEXT("replicated"), true);
    if (bCreated)
    {
        McpHandlerUtils::AddVerification(ResponseJson, Blueprint);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, bCreated,
        bCreated ? FString::Printf(TEXT("Created replicated variable: %s"), *VariableName) : TEXT("Failed to create variable"),
        ResponseJson);
    return true;
}

bool HandleSetNetUpdateFrequency(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    double Frequency = GetNumberField(Payload, TEXT("frequency"), 100.0);
    double MinFrequency = GetNumberField(Payload, TEXT("minFrequency"), 2.0);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetUpdateFrequency(static_cast<float>(Frequency));
        CDO->SetMinNetUpdateFrequency(static_cast<float>(MinFrequency));
#else
        CDO->NetUpdateFrequency = static_cast<float>(Frequency);
        CDO->MinNetUpdateFrequency = static_cast<float>(MinFrequency);
#endif
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetNumberField(TEXT("frequency"), Frequency);
    ResponseJson->SetNumberField(TEXT("minFrequency"), MinFrequency);
    McpHandlerUtils::AddVerification(ResponseJson, Blueprint);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Net update frequency set to %.1f (min: %.1f)"), Frequency, MinFrequency), ResponseJson);
    return true;
}

}
#endif
