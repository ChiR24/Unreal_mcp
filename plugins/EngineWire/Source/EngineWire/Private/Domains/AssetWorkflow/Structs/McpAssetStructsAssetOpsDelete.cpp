#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"

// Tracked delete of a UserDefinedStruct (delete_struct), on the game thread like every handler.
#include "Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h"
#include "Safety/McpSafeOperationsAssetDelete.h"

#ifdef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

// delete_struct
static bool HandleStructAssetAction_Delete(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("delete_struct"))
    {
        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            // A destructive call says whether it removed anything: "deleted (or did not exist)" read as done for
            // a mistyped path while the real struct stayed.
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No Blueprint Struct at '%s'; nothing was deleted."), *StructPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Deleting a struct other assets use breaks their variables and pins (issue #510), so it is refused
        // unless force says to break them; the users are named the way every other delete names them.
        const FString Package = S->GetOutermost()->GetName();
        const TArray<FString> Users = McpOutsideReferencers(Package, {Package});
        if (!GetJsonBoolField(Payload, TEXT("force"), false) && Users.Num() > 0)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("structPath"), StructPath);
            Result->SetNumberField(TEXT("referencerCount"), Users.Num());
            Result->SetArrayField(TEXT("referencers"), McpHandlerUtils::ToJsonStringArray(Users));
            Result->SetBoolField(TEXT("deleted"), false);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Struct '%s' is used by %d asset(s) (referencers); pass force:true to delete it anyway, breaking them."),
                    *StructPath, Users.Num()),
                Result, TEXT("STRUCT_IN_USE"));
            return true;
        }

        // The safe force delete: ObjectTools::DeleteObjects without its dialog deletes nothing while anything
        // references the asset, so force:true never forced and a used struct always failed.
        if (!McpSafeOperations::McpDeleteAssetAndFile(StructPath))
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("deletedPath"), StructPath);
            Result->SetBoolField(TEXT("deleted"), false);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Struct delete failed: the asset or its file is still there."), Result, TEXT("DELETE_FAILED"));
            return true;
        }

        // The loaded Blueprints that used it recompile now, so the broken variables and pins show at once.
        for (const FString& User : Users)
        {
            TArray<FAssetData> PackageAssets;
            FAssetRegistryModule::GetRegistry().GetAssetsByPackageName(FName(*User), PackageAssets);
            for (const FAssetData& AssetData : PackageAssets)
            {
                if (UBlueprint* BP = AssetData.IsAssetLoaded() ? Cast<UBlueprint>(AssetData.GetAsset()) : nullptr)
                {
                    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
                }
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("deletedPath"), StructPath);
        Result->SetBoolField(TEXT("deleted"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct deleted"), Result);
        return true;
    }

    return false;
}

#endif // MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
