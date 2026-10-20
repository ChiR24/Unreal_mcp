#include "Domains/AI/McpAIContext.h"

#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "GenericTeamAgentInterface.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
static void SetBPVarDefaultValueAI(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
    if (!Blueprint)
    {
        return;
    }

    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == VarName)
        {
            VarDesc.DefaultValue = DefaultValue;
            break;
        }
    }

    McpSafeCompileBlueprint(Blueprint);
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            if (FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName))
            {
                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Property->ImportText_Direct(*DefaultValue, ValuePtr, CDO, 0);
#else
                Property->ImportText(*DefaultValue, ValuePtr, PPF_None, CDO);
#endif
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
}

// Implements the "set_perception_team" action.
bool HandleSetPerceptionTeam(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    double TeamValue = 0.0;
    if (!Payload->TryGetNumberField(TEXT("teamId"), TeamValue) || TeamValue < 0.0 || TeamValue > 255.0)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("teamId is required: 0 to 254, 255 meaning no team."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const int32 TeamId = static_cast<int32>(TeamValue);

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // SetGenericTeamId on the class default object was reported as applied, but AAIController keeps its team in a
    // plain member: never saved, never copied to the controllers spawned from the class. Blueprints cannot set it, so
    // the team is stored in a GenericTeamId variable for the project's own logic to apply.
    const FName TeamVarName(TEXT("GenericTeamId"));
    bool bStoredBlueprintVariable = false;
    const bool bHasTeamVariable = Blueprint->GeneratedClass &&
        FindFProperty<FProperty>(Blueprint->GeneratedClass, TeamVarName) != nullptr;
    if (!bHasTeamVariable)
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        bStoredBlueprintVariable = FBlueprintEditorUtils::AddMemberVariable(Blueprint, TeamVarName, PinType);
        if (bStoredBlueprintVariable)
        {
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TeamVarName, nullptr, FText::FromString(TEXT("AI Perception")));
        }
    }
    else
    {
        bStoredBlueprintVariable = true;
    }

    if (!bStoredBlueprintVariable)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("The GenericTeamId variable could not be added to the Blueprint."), TEXT("VARIABLE_FAILED"));
        return true;
    }
    SetBPVarDefaultValueAI(Blueprint, TeamVarName, FString::FromInt(TeamId));

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
    Result->SetNumberField(TEXT("teamId"), TeamId);
    Result->SetStringField(TEXT("variable"), TeamVarName.ToString());
    Result->SetBoolField(TEXT("appliedToGenericTeamAgent"), false);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("GenericTeamId variable set to %d; an AI controller takes a team only through C++ SetGenericTeamId"), TeamId), Result);
    return true;
}
}
