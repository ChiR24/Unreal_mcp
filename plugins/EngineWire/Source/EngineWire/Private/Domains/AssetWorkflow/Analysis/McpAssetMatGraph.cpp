// Copyright (c) 2025 EngineWire Contributors

// get_asset_graph material branch (see Analysis/McpAssetAnalysisShared.h for the contract).

#include "Domains/AssetWorkflow/Analysis/McpAssetAnalysisShared.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Materials/Material.h"

TSharedPtr<FJsonObject> McpTryBuildMaterialGraphResponse(
    const FString& SafeAssetPath,
    int32 MaxDepth,
    bool& bTruncated)
{
    FAssetData AssetData;
    if (!McpAssetExists(SafeAssetPath, &AssetData)) { return nullptr; }

    if (!AssetData.IsInstanceOf(UMaterial::StaticClass())) { return nullptr; }

    // The expression list is editor-only data the registry cannot report, so
    // the object itself is required. Only a real material pays for the load,
    // and a cold one logs it exactly like analyze_graph does.
    UObject* Asset = AssetData.FastGetAsset(/*bLoad=*/false);
    if (!Asset) {
        UE_LOG(LogEngineWireSubsystem, Display,
               TEXT("get_asset_graph: '%s' is not resident; loading synchronously"),
               *SafeAssetPath);
        Asset = AssetData.FastGetAsset(/*bLoad=*/true);
    }
    UMaterial* Material = Cast<UMaterial>(Asset);
    if (!Material) { return nullptr; }

    // Bounded listing: nodeCount reports what the array actually carries, so a
    // clamp is visible as a truncated flag instead of a silent cut.
    constexpr int32 MaxListedNodes = 100;
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    for (UMaterialExpression* Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
        if (!Expr) { continue; }
        if (NodeArray.Num() >= MaxListedNodes) {
            bTruncated = true;
            break;
        }
        TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("nodeId"), Expr->GetName());
        Node->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
        NodeArray.Add(MakeShared<FJsonValueObject>(Node));
    }

    TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
    GraphObj->SetArrayField(SafeAssetPath, NodeArray);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetObjectField(TEXT("graph"), GraphObj);
    Resp->SetNumberField(TEXT("nodeCount"), NodeArray.Num());
    Resp->SetNumberField(TEXT("maxDepth"), MaxDepth);
    Resp->SetBoolField(TEXT("truncated"), bTruncated);
    return Resp;
}
