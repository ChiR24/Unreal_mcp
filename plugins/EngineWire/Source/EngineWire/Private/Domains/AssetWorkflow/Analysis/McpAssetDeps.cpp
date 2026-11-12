// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/AssetWorkflow/Analysis/McpAssetAnalysisShared.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"

#include "AssetRegistry/AssetRegistryModule.h"

namespace {
// The payload's assetPath, sanitized, for the dependency reads; false after the reply when it is missing
// (INVALID_ARGUMENT), refused (INVALID_PATH) or not in the asset registry (ASSET_NOT_FOUND, naming the path).
bool ReadDependencyAssetOrReply(UEngineWireSubsystem *Subsystem, const TSharedPtr<FJsonObject> &Payload,
                                const FString &RequestId, const TSharedPtr<FMcpBridgeWebSocket> &Socket,
                                FString &OutSafePath) {
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                                      nullptr, TEXT("INVALID_ARGUMENT"));
    return false;
  }
  OutSafePath = SanitizeProjectRelativePath(AssetPath);
  if (OutSafePath.IsEmpty()) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false, McpPathRefusalMessage(TEXT("asset path"), AssetPath),
                                      nullptr, TEXT("INVALID_PATH"));
    return false;
  }
  if (!McpAssetExists(OutSafePath)) {
    Subsystem->SendAutomationError(Socket, RequestId,
                                   FString::Printf(TEXT("Asset not found: %s"), *OutSafePath),
                                   TEXT("ASSET_NOT_FOUND"));
    return false;
  }
  return true;
}
} // namespace

bool UEngineWireSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SafeAssetPath;
  if (!ReadDependencyAssetOrReply(this, Payload, RequestId, Socket, SafeAssetPath))
    return true;

  // referencers: the packages that use this asset (the Blueprints that spawn
  // it, the levels that place it), the question before deleting or replacing
  // it; nothing else answered it. The registry is keyed by package, so an
  // object path (/Game/FX/NS_Puff.NS_Puff) is reduced to one.
  bool bReferencers = false;
  Payload->TryGetBoolField(TEXT("referencers"), bReferencers);
  const FName PackageName(*FPackageName::ObjectPathToPackageName(SafeAssetPath));
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FName> Packages;
  if (bReferencers) {
    AssetRegistryModule.Get().GetReferencers(PackageName, Packages);
  } else {
    AssetRegistryModule.Get().GetDependencies(PackageName, Packages);
  }
  Packages.Sort(FNameLexicalLess());

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Packages) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(bReferencers ? TEXT("referencers") : TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         bReferencers ? TEXT("Referencers retrieved") : TEXT("Dependencies retrieved"),
                         Resp, FString());
  return true;
}

/**
 * Handles requests to traverse and return an asset dependency graph.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'maxDepth'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UEngineWireSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SafeAssetPath;
  if (!ReadDependencyAssetOrReply(this, Payload, RequestId, Socket, SafeAssetPath))
    return true;

  // Bounds. The traversal below is a breadth-first walk of the dependency
  // graph; without a cap it is unbounded work on the game thread. `maxDepth`
  // was caller-controlled with no clamp and there was no limit on nodes
  // visited, so a single hub dependency (a shared material function, a level)
  // fans out to thousands of registry queries and the handler never reaches its
  // SendAutomationResponse — the request produced NO reply at all until the
  // transport gave up after 300 s, with no error to explain it.
  constexpr int32 MaxDepthCeiling = 8;
  constexpr int32 MaxVisitedNodes = 2000;
  int32 MaxDepth = 3;
  Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);
  const int32 RequestedDepth = MaxDepth;
  MaxDepth = FMath::Clamp(MaxDepth, 0, MaxDepthCeiling);
  bool bTruncated = RequestedDepth > MaxDepthCeiling;

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Refuse FAST while the registry is still scanning. This handler runs on the
  // game thread and every GetDependencies() call against an incomplete registry
  // can block on the in-progress scan; combined with the traversal below that
  // produced NO response at all until the transport gave up after 300 seconds,
  // with nothing logged to explain the silence. A retryable error is strictly
  // better than an unbounded stall: the caller learns why and when to retry.
  if (AssetRegistry.IsLoadingAssets()) {
    SendAutomationError(
        Socket, RequestId,
        TEXT("Asset Registry is still scanning; the dependency graph would be "
             "incomplete and the query would block the game thread. Retry once "
             "the editor finishes loading assets."),
        TEXT("ASSET_REGISTRY_SCANNING"));
    return true;
  }

  TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();

  // A material answers with its expression nodes; every other asset keeps the dependency walk.
  TSharedPtr<FJsonObject> MaterialResp =
      McpTryBuildMaterialGraphResponse(SafeAssetPath, MaxDepth, bTruncated);
  if (MaterialResp.IsValid()) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                           MaterialResp, FString());
    return true;
  }

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
          if (Visited.Num() >= MaxVisitedNodes) {
            // Stop expanding rather than run unbounded. The edges already
            // collected are still reported; `truncated` tells the caller the
            // graph is partial instead of silently implying completeness.
            bTruncated = true;
            continue;
          }
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
  Resp->SetNumberField(TEXT("nodeCount"), Visited.Num());
  Resp->SetNumberField(TEXT("maxDepth"), MaxDepth);
  Resp->SetBoolField(TEXT("truncated"), bTruncated);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                         Resp, FString());
  return true;
}

bool UEngineWireSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  const TArray<TSharedPtr<FJsonValue>> *Paths = nullptr;
  if (AssetPath.IsEmpty() && Payload->TryGetArrayField(TEXT("assetPaths"), Paths) && Paths->Num() > 0) {
    TSharedPtr<FJsonObject> Each = MakeShared<FJsonObject>();
    int32 Existing = 0;
    for (const TSharedPtr<FJsonValue> &Value : *Paths) {
      const FString Raw = Value.IsValid() ? Value->AsString() : FString();
      const FString Path = SanitizeProjectRelativePath(Raw);
      const bool bExists = !Path.IsEmpty() && McpAssetExists(Path);
      Each->SetBoolField(Path.IsEmpty() ? Raw : Path, bExists);
      Existing += bExists ? 1 : 0;
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("exists"), Existing == Paths->Num());
    Resp->SetObjectField(TEXT("existsByPath"), Each);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%d of %d assets exist"), Existing, Paths->Num()), Resp, FString());
    return true;
  }
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath or assetPaths required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString AssetPathAsGiven = AssetPath;
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           McpPathRefusalMessage(TEXT("assetPath"), AssetPathAsGiven), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  bool bExists = McpAssetExists(AssetPath);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         bExists ? TEXT("Asset exists")
                                 : TEXT("Asset does not exist"),
                         Resp, FString());
  return true;
}
