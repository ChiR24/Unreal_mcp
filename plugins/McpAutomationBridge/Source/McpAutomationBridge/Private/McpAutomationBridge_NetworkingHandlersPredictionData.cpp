#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleAddNetworkPredictionData(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString DataType = GetStringField(Payload, TEXT("dataType"));
    FString VariableName = GetStringField(Payload, TEXT("variableName"));

    if (BlueprintPath.IsEmpty() || DataType.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    FString VarName = VariableName.IsEmpty() ? FString::Printf(TEXT("PredictionData_%s"), *DataType) : VariableName;
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;

    if (DataType == TEXT("Transform"))
    {
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (DataType == TEXT("Vector"))
    {
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (DataType == TEXT("Rotator"))
    {
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }

    bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    if (bSuccess)
    {
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*VarName))
            {
                VarDesc.PropertyFlags |= CPF_Net;
                VarDesc.ReplicationCondition = COND_AutonomousOnly;
                break;
            }
        }
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), bSuccess);
    ResultJson->SetStringField(TEXT("variableName"), VarName);
    ResultJson->SetStringField(TEXT("dataType"), DataType);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Network prediction data variable '%s' of type '%s' %s"), *VarName, *DataType, bSuccess ? TEXT("added") : TEXT("could not be added (may already exist)")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, bSuccess, TEXT("Network prediction data added"), ResultJson);
    return true;
}
}
