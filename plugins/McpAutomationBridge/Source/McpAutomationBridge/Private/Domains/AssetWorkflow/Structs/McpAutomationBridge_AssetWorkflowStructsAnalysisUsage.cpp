#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsAnalysis.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR

bool HandleStructAnalysisSearchUsage(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
    ForEachReferencingAsset(S, [&](UObject* Asset)
    {
        if (!Asset) return;
        if (!SearchScope.IsEmpty() && !Asset->GetPathName().StartsWith(SearchScope))
        {
            return;
        }
        TSharedPtr<FJsonObject> Usage = MakeShared<FJsonObject>();
        Usage->SetStringField(TEXT("assetPath"), Asset->GetPathName());
        Usage->SetStringField(TEXT("className"), Asset->GetClass()->GetName());
        if (Cast<UBlueprint>(Asset))
        {
            Usage->SetStringField(TEXT("referencerType"), TEXT("Blueprint"));
        }
        else if (Cast<UUserDefinedStruct>(Asset))
        {
            Usage->SetStringField(TEXT("referencerType"), TEXT("Struct"));
        }
        else if (Cast<UDataAsset>(Asset))
        {
            Usage->SetStringField(TEXT("referencerType"), TEXT("DataAsset"));
        }
        else
        {
            Usage->SetStringField(TEXT("referencerType"), TEXT("Other"));
        }
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

bool HandleStructAnalysisRecompile(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    TArray<TSharedPtr<FJsonValue>> IssuesArr;
    int32 ErrorCount = 0;

    // Recompile nested struct referencers first so Blueprints rebuild against updated layouts.
    ForEachReferencingAsset(S, [&](UObject* Asset)
    {
        if (UUserDefinedStruct* Nested = Cast<UUserDefinedStruct>(Asset))
        {
            FStructureEditorUtils::CompileStructure(Nested);
            Nested->GetOutermost()->MarkPackageDirty();
        }
    });

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

    if (bSave)
    {
        // Persist the main struct and every dependent package modified by the
        // recompilation loops so their dirty changes are not lost on editor exit.
        McpSafeAssetSave(S);
        ForEachReferencingAsset(S, [](UObject* Asset)
        {
            if (UUserDefinedStruct* Nested = Cast<UUserDefinedStruct>(Asset))
            {
                McpSafeAssetSave(Nested);
            }
        });
        ForEachReferencingBlueprint(S, [](UBlueprint* BP)
        {
            McpSafeAssetSave(BP);
        });
    }

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

#endif // WITH_EDITOR
