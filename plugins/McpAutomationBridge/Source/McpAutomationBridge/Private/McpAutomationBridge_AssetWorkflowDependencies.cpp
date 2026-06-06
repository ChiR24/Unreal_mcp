// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bRecursive = false;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FName> Dependencies;
  UE::AssetRegistry::EDependencyCategory Category =
      UE::AssetRegistry::EDependencyCategory::Package;
  AssetRegistryModule.Get().GetDependencies(FName(*SafeAssetPath), Dependencies);

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Dependencies) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dependencies retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to traverse and return an asset dependency graph.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'maxDepth'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 MaxDepth = 3;
  Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();

  TArray<FString> Queue;
  Queue.Add(SafeAssetPath);

  TSet<FString> Visited;
  Visited.Add(SafeAssetPath);

  TMap<FString, int32> Depths;
  Depths.Add(SafeAssetPath, 0);

  int32 Head = 0;
  while (Head < Queue.Num()) {
    FString Current = Queue[Head++];
    int32 CurrentDepth = Depths[Current];

    TArray<FName> Dependencies;
    AssetRegistry.GetDependencies(FName(*Current), Dependencies);

    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName &Dep : Dependencies) {
      FString DepStr = Dep.ToString();
      if (!DepStr.StartsWith(TEXT("/Game")))
        continue; // Only graph Game assets for now

      DepArray.Add(MakeShared<FJsonValueString>(DepStr));

      if (CurrentDepth < MaxDepth) {
        if (!Visited.Contains(DepStr)) {
          Visited.Add(DepStr);
          Depths.Add(DepStr, CurrentDepth + 1);
          Queue.Add(DepStr);
        }
      }
    }
    GraphObj->SetArrayField(Current, DepArray);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("graph"), GraphObj);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to set asset tags. NOTE: Asset Registry tags are distinct
 * from Actor tags. This function currently returns NOT_IMPLEMENTED as generic
 * asset tagging is ambiguous (metadata vs registry tags).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

bool UMcpAutomationBridgeSubsystem::HandleGetAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_asset payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FAssetData AssetData = UEditorAssetLibrary::FindAssetData(SafeAssetPath);
  if (!AssetData.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to find asset data"), nullptr,
                           TEXT("ASSET_DATA_INVALID"));
    return true;
  }

  TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
  AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
#else
  AssetObj->SetStringField(TEXT("path"), AssetData.ToSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
  AssetObj->SetStringField(TEXT("packagePath"),
                           AssetData.PackagePath.ToString());

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("result"), AssetObj);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to generate an asset report (CSV/JSON).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'directory' and 'reportType'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

bool UMcpAutomationBridgeSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         bExists ? TEXT("Asset exists")
                                 : TEXT("Asset does not exist"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
