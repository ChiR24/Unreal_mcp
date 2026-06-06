#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
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

bool HandleSetPerceptionTeam(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_perception_team");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("set_perception_team"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        int32 TeamId = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("teamId"), 0));

        // CRITICAL: Explicitly check if asset exists before LoadObject
        // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
        if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        bool bAppliedToGenericTeamAgent = false;
        if (Blueprint->GeneratedClass)
        {
            if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
            {
                if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(CDO))
                {
                    TeamAgent->SetGenericTeamId(FGenericTeamId(static_cast<uint8>(FMath::Clamp(TeamId, 0, 255))));
                    bAppliedToGenericTeamAgent = true;
                }
            }
        }

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

        if (bStoredBlueprintVariable)
        {
            SetBPVarDefaultValueAI(Blueprint, TeamVarName, FString::FromInt(TeamId));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
        McpSafeAssetSave(Blueprint);
        Result->SetNumberField(TEXT("teamId"), TeamId);
        Result->SetBoolField(TEXT("appliedToGenericTeamAgent"), bAppliedToGenericTeamAgent);
        Result->SetBoolField(TEXT("storedBlueprintVariable"), bStoredBlueprintVariable);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Team ID set to %d"), TeamId));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Team set"), Result);
        return true;
    }

    // =========================================================================
    // 16.6 State Trees - UE5.3+ (4 actions)
    // =========================================================================

    return true;
}
}
#endif
