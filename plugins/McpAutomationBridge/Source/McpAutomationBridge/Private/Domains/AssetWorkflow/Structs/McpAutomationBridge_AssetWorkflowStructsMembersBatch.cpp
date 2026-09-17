#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

#if WITH_EDITOR

bool AddStructMembersFromArray(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const TArray<TSharedPtr<FJsonValue>>* MembersArr = nullptr;
    if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("members"), MembersArr) ||
        !MembersArr || MembersArr->Num() == 0)
    {
        return false;
    }
    const FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
    UUserDefinedStruct* S = StructPath.IsEmpty()
        ? nullptr
        : LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
    if (!S)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    TArray<FParsedMember> Parsed;
    TArray<FString> Failures;
    // Refuse the whole batch on a bad entry rather than half-building a struct:
    // a partially applied member list is worse than none, because the caller
    // cannot tell which half landed.
    if (!ValidateStructMembers(*MembersArr, FName(*StructPath), Parsed, Failures))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No member was added; %d of %d entries are invalid: %s"),
                Failures.Num(), MembersArr->Num(), *FString::Join(Failures, TEXT("; "))),
            TEXT("TYPE_RESOLUTION_FAILED"));
        return true;
    }

    const int32 Applied = ApplyParsedStructMembers(S, Parsed, Failures);
    FStructureEditorUtils::CompileStructure(S);
    S->GetOutermost()->MarkPackageDirty();
    if (GetPayloadBool(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(S);
    }

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("structPath"), StructPath);
    Data->SetNumberField(TEXT("addedCount"), Applied);
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const FParsedMember& M : Parsed)
    {
        Names.Add(MakeShared<FJsonValueString>(M.Name));
    }
    Data->SetArrayField(TEXT("memberNames"), Names);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Added %d struct member(s)"), Applied), Data);
    return true;
}

#endif // WITH_EDITOR
