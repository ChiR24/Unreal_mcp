#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR


bool HandleStructSerializationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("export_struct"))
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
            TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
            M->SetStringField(TEXT("guid"), Var.VarGuid.ToString());
            M->SetStringField(TEXT("name"), Var.FriendlyName);
            M->SetStringField(TEXT("type"), PinTypeToSummary(Var.ToPinType()));
            M->SetStringField(TEXT("default"), Var.DefaultValue);
            M->SetStringField(TEXT("tooltip"), Var.ToolTip);
            TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
            for (const TPair<FName, FString>& Pair : Var.MetaData)
            {
                Meta->SetStringField(Pair.Key.ToString(), Pair.Value);
            }
            M->SetObjectField(TEXT("metadata"), Meta);
            MembersArr.Add(MakeShared<FJsonValueObject>(M));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), StructPath);
        Result->SetStringField(TEXT("structName"), S->GetName());
        Result->SetArrayField(TEXT("members"), MembersArr);
        Result->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct exported"), Result);
        return true;
    }

    if (Lower == TEXT("list_structs"))
    {
        FString PathFilter = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Structs"));

        // Enumerate ALL UserDefinedStruct assets (including unloaded ones) via
        // the Asset Registry. The synchronous GetAssetsByClass scan is safe on
        // the game thread; only an async LOAD would risk the sync request
        // thread, and we resolve each discovered asset with a synchronous
        // LoadObject below precisely so unloaded structs are discovered.
        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AR = ARM.GetRegistry();

        TArray<FAssetData> StructAssets;
        AR.GetAssetsByClass(UUserDefinedStruct::StaticClass()->GetClassPathName(),
            StructAssets, /*bSearchSubClasses=*/true);

        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const FAssetData& AssetData : StructAssets)
        {
            const FString AssetPath = MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData);
            if (!PathFilter.IsEmpty() && !AssetPath.StartsWith(PathFilter))
            {
                continue;
            }

            // Materialize the struct to report its canonical path/name, so
            // structs not yet loaded into memory are still discovered.
            UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *AssetPath);
            if (!S)
            {
                continue;
            }

            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("assetPath"), S->GetPathName());
            O->SetStringField(TEXT("name"), S->GetName());
            Arr.Add(MakeShared<FJsonValueObject>(O));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("path"), PathFilter);
        Result->SetArrayField(TEXT("structs"), Arr);
        Result->SetNumberField(TEXT("count"), Arr.Num());
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Structs enumerated"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
