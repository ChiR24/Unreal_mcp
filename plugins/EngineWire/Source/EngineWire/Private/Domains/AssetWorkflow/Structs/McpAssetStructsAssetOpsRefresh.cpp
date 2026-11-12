#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"

// refresh_struct_dependencies
bool HandleStructAssetAction_Refresh(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("refresh_struct_dependencies"))
    {
        const FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        UUserDefinedStruct* S = LoadStructOrReply(Bridge, RequestId, RequestingSocket, StructPath);
        if (!S)
        {
            return true;
        }

        // Recompile the struct, its referencers, and notify matching DataTables via the shared helper.
        TArray<FString> Refreshed;
        TArray<FString> RefreshedDataTables;
        TArray<FString> RefreshedEnums;
        McpRefreshStructDependents(S, &Refreshed, &RefreshedDataTables, &RefreshedEnums);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("structPath"), StructPath);

        Result->SetArrayField(TEXT("refreshedBlueprints"), McpHandlerUtils::ToJsonStringArray(Refreshed));
        Result->SetNumberField(TEXT("refreshedCount"), Refreshed.Num());

        Result->SetArrayField(TEXT("refreshedDataTables"), McpHandlerUtils::ToJsonStringArray(RefreshedDataTables));
        Result->SetNumberField(TEXT("refreshedDataTableCount"), RefreshedDataTables.Num());

        Result->SetArrayField(TEXT("refreshedEnums"), McpHandlerUtils::ToJsonStringArray(RefreshedEnums));

        Result->SetBoolField(TEXT("refreshed"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct dependencies refreshed"), Result);
        return true;
    }

    return false;
}
