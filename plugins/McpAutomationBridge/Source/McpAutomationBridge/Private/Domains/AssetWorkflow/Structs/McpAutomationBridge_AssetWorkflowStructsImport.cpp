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

        const FName SelfPath = StructPath.IsEmpty()
            ? FName(*FString::Printf(TEXT("%s.%s"), *PackageName, *FinalName))
            : FName(*StructPath);
        TArray<FParsedMember> ParsedMembers;
        TArray<FString> Failures;
        if (!ValidateStructMembers(*MembersArr, SelfPath, ParsedMembers, Failures))
        {
            TArray<TSharedPtr<FJsonValue>> FailureArr;
            FailureArr.Reserve(Failures.Num());
            for (const FString& Failure : Failures)
            {
                FailureArr.Add(MakeShared<FJsonValueString>(Failure));
            }
            TSharedPtr<FJsonObject> ValidationResult = McpHandlerUtils::CreateResultObject();
            ValidationResult->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + FinalName);
            ValidationResult->SetStringField(TEXT("structName"), FinalName);
            ValidationResult->SetNumberField(TEXT("imported"), 0);
            ValidationResult->SetNumberField(TEXT("failed"), Failures.Num());
            ValidationResult->SetArrayField(TEXT("failures"), FailureArr);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Struct import validation failed: %d member(s) rejected"), Failures.Num()),
                ValidationResult);
            return true;
        }

        TArray<FGuid> Existing;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Existing.Add(Var.VarGuid);
        }
        for (const FGuid& G : Existing)
        {
            FStructureEditorUtils::RemoveVariable(S, G);
        }

        TArray<FString> ApplyFailures;
        int32 Applied = ApplyParsedStructMembers(S, ParsedMembers, ApplyFailures);

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
        Result->SetNumberField(TEXT("imported"), Applied);
        Result->SetNumberField(TEXT("failed"), ApplyFailures.Num());
        Result->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
        McpHandlerUtils::AddVerification(Result, S);
        if (ApplyFailures.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> FailureArr;
            FailureArr.Reserve(ApplyFailures.Num());
            for (const FString& Failure : ApplyFailures)
            {
                FailureArr.Add(MakeShared<FJsonValueString>(Failure));
            }
            Result->SetArrayField(TEXT("failures"), FailureArr);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Struct imported %d member(s) but %d failed"), Applied, ApplyFailures.Num()), Result);
        }
        else
        {
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Struct imported"), Result);
        }
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
