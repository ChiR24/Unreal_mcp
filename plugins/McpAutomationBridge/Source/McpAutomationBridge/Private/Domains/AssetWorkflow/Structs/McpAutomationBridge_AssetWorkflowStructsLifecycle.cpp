#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR


bool HandleStructLifecycleActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("create_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString Name = GetPayloadString(Payload, TEXT("name"));
        FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Structs"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        // Accept the documented structPath (used by every other struct action)
        // and derive name + parent path from it when name is not given.
        if (Name.IsEmpty() && !StructPath.IsEmpty())
        {
            if (LoadObject<UUserDefinedStruct>(nullptr, *StructPath))
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Struct already exists: %s"), *StructPath), TEXT("ASSET_ALREADY_EXISTS"));
                return true;
            }
            int32 Slash = INDEX_NONE;
            StructPath.FindLastChar('/', Slash);
            Name = StructPath.Mid(Slash + 1);
            if (Slash != INDEX_NONE) { Path = StructPath.Left(Slash); }
        }

        if (Name.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: name or structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString PathError;
        FString SanitizedName = SanitizeAssetName(Name);
        FString PackageName;
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, PathError, TEXT("PACKAGE_CREATE_FAILED"));
            return true;
        }

        const TArray<TSharedPtr<FJsonValue>>* MembersArr = nullptr;
        const bool bHasMembers = Payload->TryGetArrayField(TEXT("members"), MembersArr) && MembersArr;
        TArray<FParsedMember> ParsedMembers;
        if (bHasMembers)
        {
            const FName SelfPath = FName(*FString::Printf(TEXT("%s.%s"), *PackageName, *SanitizedName));
            TArray<FString> ValidationFailures;
            if (!ValidateStructMembers(*MembersArr, SelfPath, ParsedMembers, ValidationFailures))
            {
                TArray<TSharedPtr<FJsonValue>> FailureArr;
                for (const FString& F : ValidationFailures) FailureArr.Add(MakeShared<FJsonValueString>(F));
                TSharedPtr<FJsonObject> ValidationResult = McpHandlerUtils::CreateResultObject();
                ValidationResult->SetNumberField(TEXT("failed"), ValidationFailures.Num());
                ValidationResult->SetArrayField(TEXT("failures"), FailureArr);
                Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                    FString::Printf(TEXT("Struct creation rejected: %d member(s) failed validation"), ValidationFailures.Num()),
                    ValidationResult);
                return true;
            }
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
            return true;
        }

        UUserDefinedStruct* S = FStructureEditorUtils::CreateUserDefinedStruct(
            Package, FName(*SanitizedName), RF_Public | RF_Standalone);

        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create user defined struct"), TEXT("ASSET_CREATE_FAILED"));
            return true;
        }

        // CreateUserDefinedStruct seeds one default bool variable (MemberVar_0).
        // This matches the editor: every UE UserDefinedStruct starts with one
        // variable. We remove it so the struct starts empty per our contract;
        // the engine re-seeds MemberVar_0 on the empty-state compile/save (a UE
        // invariant, identical to the editor). That is expected, not a defect.
        TArray<FGuid> SeededGuids;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            SeededGuids.Add(Var.VarGuid);
        }
        for (const FGuid& G : SeededGuids)
        {
            FStructureEditorUtils::RemoveVariable(S, G);
        }

        if (bHasMembers)
        {
            TArray<FString> ApplyFailures;
            int32 Applied = ApplyParsedStructMembers(S, ParsedMembers, ApplyFailures);
            if (ApplyFailures.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> FailureArr;
                for (const FString& F : ApplyFailures) FailureArr.Add(MakeShared<FJsonValueString>(F));
                TSharedPtr<FJsonObject> ApplyResult = McpHandlerUtils::CreateResultObject();
                ApplyResult->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + SanitizedName);
                ApplyResult->SetNumberField(TEXT("imported"), Applied);
                ApplyResult->SetNumberField(TEXT("failed"), ApplyFailures.Num());
                ApplyResult->SetArrayField(TEXT("failures"), FailureArr);
                Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                    FString::Printf(TEXT("Struct created but %d member(s) failed to apply"), ApplyFailures.Num()),
                    ApplyResult);
            }
        }

        // Compile so the struct is in a valid, consistent (UpToDate) state.
        FStructureEditorUtils::CompileStructure(S);
        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(S);
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + SanitizedName);
        Result->SetStringField(TEXT("structName"), SanitizedName);
        Result->SetStringField(TEXT("status"), TEXT("UpToDate"));
        Result->SetBoolField(TEXT("saved"), bSave);
        if (bHasMembers)
        {
            Result->SetNumberField(TEXT("memberCount"), FStructureEditorUtils::GetVarDesc(S).Num());
        }
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("User defined struct created"), Result);
        return true;
    }

    if (Lower == TEXT("get_struct") || Lower == TEXT("read_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UStruct* Resolved = LoadObject<UStruct>(nullptr, *StructPath);
        if (!Resolved)
        {
            // Native (C++) script structs such as /Script/Engine.Vector are not
            // backed by a package on disk, so LoadObject cannot resolve them.
            // Fall back to an in-memory lookup to handle native USTRUCTs.
            Resolved = FindObject<UStruct>(nullptr, *StructPath);
        }
        if (!Resolved && StructPath.Contains(TEXT("/Script/")))
        {
            // Native (C++) script structs have no on-disk asset; they live in a
            // /Script/<Module> package (e.g. /Script/Engine.Vector). Resolve the
            // module package, then the struct by its trailing name within it.
            const int32 DotIdx = StructPath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            const FString PkgPath = (DotIdx > 0) ? StructPath.Left(DotIdx) : StructPath;
            const FString StructName = (DotIdx > 0) ? StructPath.Mid(DotIdx + 1) : FPaths::GetCleanFilename(StructPath);
            if (UPackage* Pkg = FindPackage(nullptr, *PkgPath))
            {
                Resolved = FindObject<UScriptStruct>(Pkg, *StructName);
            }
            if (!Resolved)
            {
                // Fallback: scan loaded script structs for a name match (covers
                // modules that are loaded but not yet resolvable by package).
                for (TObjectIterator<UScriptStruct> It; It; ++It)
                {
                    if (It->GetName() == StructName)
                    {
                        Resolved = *It;
                        break;
                    }
                }
            }
        }
        if (!Resolved)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        if (UUserDefinedStruct* S = Cast<UUserDefinedStruct>(Resolved))
        {
            TArray<TSharedPtr<FJsonValue>> MembersArr;
            for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
            {
                MembersArr.Add(MakeShared<FJsonValueObject>(VariableDescriptionToJson(Var)));
            }

            FString ValidityMsg;
            const bool bValid = FStructureEditorUtils::IsStructureValid(S, nullptr, &ValidityMsg);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), StructPath);
            Result->SetArrayField(TEXT("members"), MembersArr);
            Result->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
            Result->SetBoolField(TEXT("isValid"), bValid);
            if (!bValid)
            {
                Result->SetStringField(TEXT("validityMessage"), ValidityMsg);
            }
            McpHandlerUtils::AddVerification(Result, S);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("User defined struct read"), Result);
            return true;
        }

        if (UScriptStruct* SS = Cast<UScriptStruct>(Resolved))
        {
            TArray<TSharedPtr<FJsonValue>> MembersArr;
            for (TFieldIterator<FProperty> It(SS); It; ++It)
            {
                MembersArr.Add(MakeShared<FJsonValueObject>(NativePropertyToMemberJson(*It)));
            }

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("assetPath"), StructPath);
            Result->SetArrayField(TEXT("members"), MembersArr);
            Result->SetStringField(TEXT("status"), TEXT("Native"));
            Result->SetBoolField(TEXT("isValid"), true);
            McpHandlerUtils::AddVerification(Result, SS);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Native struct read"), Result);
            return true;
        }

        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Not a struct: %s"), *StructPath), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Lower == TEXT("list_struct_members"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> MembersArr;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            MembersArr.Add(MakeShared<FJsonValueObject>(VariableDescriptionToJson(Var)));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), StructPath);
        Result->SetArrayField(TEXT("members"), MembersArr);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct members listed"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
