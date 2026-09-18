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
    const bool bSaved = GetPayloadBool(Payload, TEXT("save"), false);
    if (bSaved)
    {
        McpSafeAssetSave(S);
    }

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("structPath"), StructPath);
    Data->SetNumberField(TEXT("addedCount"), Applied);
    Data->SetBoolField(TEXT("saved"), bSaved);
    if (!bSaved && Applied > 0)
    {
        // A UserDefinedStruct left dirty is a silent time bomb: the members are
        // live for the rest of the session -- readable, connectable, writable from
        // a DataTable -- and then an editor restart reverts the struct and every
        // value stored in those columns is gone. save_all does not rescue it
        // either, because nothing marks the package dirty for that path.
        Data->SetStringField(
            TEXT("persistenceWarning"),
            TEXT("These members exist in memory only. They will survive this session "
                 "but revert on the next editor restart, silently discarding any "
                 "DataTable values written to them. Pass save:true to persist."));
    }
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
