#pragma once

#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "Engine/DataTable.h"
#include "EdGraphSchema_K2.h"

#if WITH_EDITOR

#ifdef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

// refresh_struct_dependencies
static bool HandleStructAssetAction_Refresh(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("refresh_struct_dependencies"))
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

        // Snapshot the struct package's direct referencers before mutating anything downstream.
        IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
        TArray<FAssetIdentifier> PreReferencers;
        AR.GetReferencers(FAssetIdentifier(S->GetOutermost()->GetFName()), PreReferencers);
        UE_LOG(LogTemp, Verbose, TEXT("McpStructHandlers: refresh_struct_dependencies %s has %d direct referencers"),
            *StructPath, PreReferencers.Num());

        // Recompile the struct, its referencers, and notify matching DataTables via the shared helper.
        TArray<FString> Refreshed;
        TArray<FString> RefreshedDataTables;
        TArray<FString> RefreshedEnums;
        McpRefreshStructDependents(S, &Refreshed, &RefreshedDataTables, &RefreshedEnums);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("structPath"), StructPath);

        TArray<TSharedPtr<FJsonValue>> R;
        for (const FString& P : Refreshed) R.Add(MakeShared<FJsonValueString>(P));
        Result->SetArrayField(TEXT("refreshedBlueprints"), R);
        Result->SetNumberField(TEXT("refreshedCount"), Refreshed.Num());

        TArray<TSharedPtr<FJsonValue>> DT;
        for (const FString& P : RefreshedDataTables) DT.Add(MakeShared<FJsonValueString>(P));
        Result->SetArrayField(TEXT("refreshedDataTables"), DT);
        Result->SetNumberField(TEXT("refreshedDataTableCount"), RefreshedDataTables.Num());

        TArray<TSharedPtr<FJsonValue>> EN;
        for (const FString& P : RefreshedEnums) EN.Add(MakeShared<FJsonValueString>(P));
        Result->SetArrayField(TEXT("refreshedEnums"), EN);

        Result->SetBoolField(TEXT("refreshed"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct dependencies refreshed"), Result);
        return true;
    }

    return false;
}

#endif // MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
#endif // WITH_EDITOR
