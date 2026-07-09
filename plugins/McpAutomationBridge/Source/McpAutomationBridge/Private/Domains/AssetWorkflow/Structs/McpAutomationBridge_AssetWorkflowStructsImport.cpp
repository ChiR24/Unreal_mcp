#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR


bool HandleStructImportActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("import_struct"))
    {
        FString Name = GetPayloadString(Payload, TEXT("name"));
        FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Structs"));
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        const TArray<TSharedPtr<FJsonValue>>* MembersArr = nullptr;
        if (!Payload->TryGetArrayField(TEXT("members"), MembersArr) || !MembersArr)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: members (array)"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = nullptr;
        FString FinalName = Name;
        FString PackageName;

        if (!StructPath.IsEmpty())
        {
            S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
            if (!S)
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }
            PackageName = S->GetOutermost()->GetName();
            FinalName = S->GetName();
        }
        else
        {
            if (FinalName.IsEmpty())
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("Missing required parameter: name (or structPath)"), TEXT("MISSING_PARAMETER"));
                return true;
            }
            FString PathError;
            FString SanitizedName = SanitizeAssetName(FinalName);
            if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId, PathError, TEXT("PACKAGE_CREATE_FAILED"));
                return true;
            }
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
                return true;
            }
            S = FStructureEditorUtils::CreateUserDefinedStruct(
                Package, FName(*SanitizedName), RF_Public | RF_Standalone);
            if (!S)
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("Failed to create user defined struct"), TEXT("ASSET_CREATE_FAILED"));
                return true;
            }
            // Strip the engine-seeded default variable; import defines members explicitly.
            TArray<FGuid> SeededGuids;
            for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
            {
                SeededGuids.Add(Var.VarGuid);
            }
            for (const FGuid& G : SeededGuids)
            {
                FStructureEditorUtils::RemoveVariable(S, G);
            }
            FinalName = SanitizedName;
        }

        // Remove existing members so import is idempotent (replace semantics).
        TArray<FGuid> Existing;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Existing.Add(Var.VarGuid);
        }
        for (const FGuid& G : Existing)
        {
            FStructureEditorUtils::RemoveVariable(S, G);
        }

        int32 Imported = 0;
        for (const TSharedPtr<FJsonValue>& MemberVal : *MembersArr)
        {
            const TSharedPtr<FJsonObject>* MemberObj = nullptr;
            if (!MemberVal->TryGetObject(MemberObj) || !MemberObj || !MemberObj->IsValid())
            {
                continue;
            }
            const FString MemberName = (*MemberObj)->GetStringField(TEXT("name"));
            const FString MemberType = (*MemberObj)->GetStringField(TEXT("type"));
            if (MemberName.IsEmpty() || MemberType.IsEmpty())
            {
                continue;
            }

            const FEdGraphPinType PinType = ParseMemberType(MemberType);

            // Skip malformed members: self-reference (a struct cannot contain
            // itself by value) and unresolved Enum:/Struct: refs.
            UObject* Sub = PinType.PinSubCategoryObject.Get();
            if (Sub == static_cast<UObject*>(S))
            {
                continue;
            }
            if ((MemberType.StartsWith(TEXT("Enum:")) || MemberType.StartsWith(TEXT("Struct:"))) && !Sub)
            {
                continue;
            }

            if (!FStructureEditorUtils::AddVariable(S, PinType))
            {
                continue;
            }
            const FGuid G = FStructureEditorUtils::GetVarDesc(S).Last().VarGuid;
            FStructureEditorUtils::RenameVariable(S, G, MemberName);

            FStructVariableDescription* NewVar = FStructureEditorUtils::GetVarDescByGuid(S, G);
            if (NewVar)
            {
                FString Def;
                if ((*MemberObj)->TryGetStringField(TEXT("default"), Def))
                {
                    FStructureEditorUtils::ChangeVariableDefaultValue(S, G, Def);
                }
                FString Tip;
                if ((*MemberObj)->TryGetStringField(TEXT("tooltip"), Tip))
                {
                    FStructureEditorUtils::ChangeVariableTooltip(S, G, Tip);
                }
                const TSharedPtr<FJsonObject>* Meta = nullptr;
                if ((*MemberObj)->TryGetObjectField(TEXT("metadata"), Meta) && Meta && (*Meta).IsValid())
                {
                    for (const auto& Pair : (*Meta)->Values)
                    {
                        FStructureEditorUtils::SetMetaData(S, G, *Pair.Key, Pair.Value->AsString());
                    }
                }
            }
            ++Imported;
        }

        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(S);
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + FinalName);
        Result->SetStringField(TEXT("structName"), FinalName);
        Result->SetNumberField(TEXT("imported"), Imported);
        Result->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct imported"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
