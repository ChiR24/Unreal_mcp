// Copyright (c) 2024 MCP Automation Bridge Contributors

// get_asset_graph material branch (see Analysis/Shared.h for the contract).

#include "Domains/AssetWorkflow/Analysis/Shared.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Materials/Material.h"
#endif

#if WITH_EDITOR
TSharedPtr<FJsonObject> McpTryBuildMaterialGraphResponse(
    const FString& SafeAssetPath,
    int32 MaxDepth,
    bool& bTruncated)
{
    FAssetData AssetData = UEditorAssetLibrary::FindAssetData(SafeAssetPath);
    if (!AssetData.IsValid()) { return nullptr; }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const bool bIsMaterialClass =
        AssetData.AssetClassPath == UMaterial::StaticClass()->GetClassPathName();
#else
    const bool bIsMaterialClass =
        AssetData.AssetClass == UMaterial::StaticClass()->GetFName();
#endif
    if (!bIsMaterialClass) { return nullptr; }

    // The expression list is editor-only data the registry cannot report, so
    // the object itself is required. Only a real material pays for the load,
    // and a cold one logs it exactly like analyze_graph does.
    UObject* Asset = AssetData.FastGetAsset(/*bLoad=*/false);
    if (!Asset) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
               TEXT("get_asset_graph: '%s' is not resident; loading synchronously"),
               *SafeAssetPath);
        Asset = AssetData.FastGetAsset(/*bLoad=*/true);
    }
    UMaterial* Material = Cast<UMaterial>(Asset);
    if (!Material) { return nullptr; }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    const TArray<TObjectPtr<UMaterialExpression>>* Expressions =
        EditorOnly ? &EditorOnly->ExpressionCollection.Expressions : nullptr;
#else
    // UE 5.0: direct member access; TArray is still TObjectPtr-backed.
    const TArray<TObjectPtr<UMaterialExpression>>* Expressions = &Material->Expressions;
#endif

    // Bounded listing: nodeCount reports what the array actually carries, so a
    // clamp is visible as a truncated flag instead of a silent cut.
    constexpr int32 MaxListedNodes = 100;
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    if (Expressions) {
        for (UMaterialExpression* Expr : *Expressions) {
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
#endif // WITH_EDITOR
