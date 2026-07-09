#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR


bool HandleStructAnalysisActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("compare_structs"))
    {
        FString StructPathA = GetPayloadString(Payload, TEXT("structPath"));
        FString StructPathB = GetPayloadString(Payload, TEXT("otherStructPath"));

        if (StructPathA.IsEmpty() || StructPathB.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath or otherStructPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* SA = LoadObject<UUserDefinedStruct>(nullptr, *StructPathA);
        UUserDefinedStruct* SB = LoadObject<UUserDefinedStruct>(nullptr, *StructPathB);
        if (!SA)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPathA), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        if (!SB)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPathB), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        auto BuildMap = [](UUserDefinedStruct* S)
        {
            TMap<FString, const FStructVariableDescription*> Map;
            for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
            {
                Map.Add(Var.FriendlyName, &Var);
            }
            return Map;
        };

        const TMap<FString, const FStructVariableDescription*> MapA = BuildMap(SA);
        const TMap<FString, const FStructVariableDescription*> MapB = BuildMap(SB);

        TArray<TSharedPtr<FJsonValue>> DiffArr;
        TSet<FString> Visited;
        auto AddDiff = [&DiffArr](const FString& Name, const FStructVariableDescription* A, const FStructVariableDescription* B)
        {
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("field"), Name);

            if (A)
            {
                TSharedPtr<FJsonObject> InA = MakeShared<FJsonObject>();
                InA->SetStringField(TEXT("type"), PinTypeToSummary(A->ToPinType()));
                InA->SetStringField(TEXT("default"), A->DefaultValue);
                Diff->SetObjectField(TEXT("inA"), InA);
            }
            if (B)
            {
                TSharedPtr<FJsonObject> InB = MakeShared<FJsonObject>();
                InB->SetStringField(TEXT("type"), PinTypeToSummary(B->ToPinType()));
                InB->SetStringField(TEXT("default"), B->DefaultValue);
                Diff->SetObjectField(TEXT("inB"), InB);
            }

            if (A && !B)
            {
                Diff->SetStringField(TEXT("status"), TEXT("removed"));
            }
            else if (!A && B)
            {
                Diff->SetStringField(TEXT("status"), TEXT("added"));
            }
            else if (A && B)
            {
                const bool bSame = PinTypeToSummary(A->ToPinType()) == PinTypeToSummary(B->ToPinType()) &&
                    A->DefaultValue == B->DefaultValue;
                Diff->SetStringField(TEXT("status"), bSame ? TEXT("same") : TEXT("changed"));
            }

            DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
        };

        for (const auto& Pair : MapA)
        {
            Visited.Add(Pair.Key);
            AddDiff(Pair.Key, Pair.Value, MapB.FindRef(Pair.Key));
        }
        for (const auto& Pair : MapB)
        {
            if (!Visited.Contains(Pair.Key))
            {
                AddDiff(Pair.Key, nullptr, Pair.Value);
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("structPathA"), StructPathA);
        Result->SetStringField(TEXT("structPathB"), StructPathB);
        Result->SetArrayField(TEXT("diff"), DiffArr);
        McpHandlerUtils::AddVerification(Result, SA);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Structs compared"), Result);
        return true;
    }

    if (Lower == TEXT("search_struct_usage"))
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

        TArray<TSharedPtr<FJsonValue>> UsagesArr;
        FString SearchScope = GetPayloadString(Payload, TEXT("searchScope"));
        ForEachReferencingBlueprint(S, [&UsagesArr, &SearchScope](UBlueprint* BP)
        {
            if (!SearchScope.IsEmpty() && !BP->GetPathName().StartsWith(SearchScope))
            {
                return;
            }
            TSharedPtr<FJsonObject> Usage = MakeShared<FJsonObject>();
            Usage->SetStringField(TEXT("assetPath"), BP->GetPathName());
            Usage->SetStringField(TEXT("className"), BP->GetClass()->GetName());
            UsagesArr.Add(MakeShared<FJsonValueObject>(Usage));
        });

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), StructPath);
        Result->SetArrayField(TEXT("usages"), UsagesArr);
        Result->SetNumberField(TEXT("usageCount"), UsagesArr.Num());
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct usages enumerated"), Result);
        return true;
    }

    if (Lower == TEXT("recompile_struct"))
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

        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        TArray<TSharedPtr<FJsonValue>> IssuesArr;
        int32 ErrorCount = 0;

        ForEachReferencingBlueprint(S, [&IssuesArr, &ErrorCount](UBlueprint* BP)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

            FCompilerResultsLog Results;
            FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);

            ErrorCount += Results.NumErrors;
            for (const TSharedRef<FTokenizedMessage>& Msg : Results.Messages)
            {
                const EMessageSeverity::Type Severity = Msg->GetSeverity();
                if (Severity != EMessageSeverity::Error && Severity != EMessageSeverity::Warning)
                {
                    continue;
                }

                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("blueprint"), BP->GetPathName());
                Issue->SetStringField(TEXT("severity"),
                    Severity == EMessageSeverity::Error ? TEXT("Error") : TEXT("Warning"));
                Issue->SetStringField(TEXT("message"), Msg->ToText().ToString());
                IssuesArr.Add(MakeShared<FJsonValueObject>(Issue));
            }
        });

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), StructPath);
        Result->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
        Result->SetNumberField(TEXT("errorCount"), ErrorCount);
        Result->SetArrayField(TEXT("issues"), IssuesArr);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct recompiled"), Result);
        return true;
    }


    return false;
}

#endif // WITH_EDITOR
