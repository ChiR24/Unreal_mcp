#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPDiag.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "UObject/Package.h"

// compile with folder: a project audit (after an engine upgrade, or for the graph checks' warnings) took one call per
// Blueprint, 123 for a small game. Every Blueprint under the folder is compiled in one call and the reply lists the
// ones the compiler or the graph checks flag. An audit changes nothing on disk: each package keeps the unsaved flag it
// had, where a single compile marks its package unsaved so the recompile persists.
namespace McpBlueprintHandlers {
void McpCompileBlueprintFolder(const FBlueprintActionContext &Context, const FString &Folder) {
  constexpr int32 MaxBlueprints = 500;
  const FNormalizedAssetPath Root = NormalizeAssetPath(Folder);
  if (!Root.bIsValid) {
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, Root.ErrorMessage,
                                       TEXT("INVALID_ARGUMENT"));
    return;
  }
  FARFilter Filter;
  Filter.PackagePaths.Add(FName(*Root.Path));
  Filter.bRecursivePaths = true;
  Filter.bRecursiveClasses = true;
#if ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
#endif
  TArray<FAssetData> Assets;
  FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, Assets);

  TArray<TSharedPtr<FJsonValue>> Flagged;
  int32 Compiled = 0, Failed = 0;
  for (const FAssetData &Asset : Assets) {
    UBlueprint *Blueprint = Compiled < MaxBlueprints ? Cast<UBlueprint>(Asset.GetAsset()) : nullptr;
    if (!Blueprint) continue;
    UPackage *Package = Blueprint->GetOutermost();
    const bool bWasDirty = Package && Package->IsDirty();
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    FString FirstError;
    const bool bCompiled = McpCompileBlueprintWithDiagnostics(Blueprint, Row, FirstError);
    if (Package) Package->SetDirtyFlag(bWasDirty);
    ++Compiled;
    Failed += bCompiled ? 0 : 1;
    if (bCompiled && !Row->HasField(TEXT("diagnostics"))) continue;
    // The row names the Blueprint once; the check messages are already in its diagnostics.
    Row->RemoveField(TEXT("warnings"));
    Row->RemoveField(TEXT("undoBufferReset"));
    Row->SetStringField(TEXT("blueprintPath"), Asset.PackageName.ToString());
    Flagged.Add(MakeShared<FJsonValueObject>(Row));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("folder"), Root.Path);
  Data->SetNumberField(TEXT("compiledCount"), Compiled);
  Data->SetNumberField(TEXT("failedCount"), Failed);
  Data->SetNumberField(TEXT("flaggedCount"), Flagged.Num());
  Data->SetArrayField(TEXT("blueprints"), Flagged);
  if (Assets.Num() > MaxBlueprints) {
    Data->SetBoolField(TEXT("truncated"), true);
  }
  Context.Bridge.SendAutomationResponse(
      Context.RequestingSocket, Context.RequestId, true,
      FString::Printf(TEXT("Compiled %d Blueprints under %s: %d failed, %d flagged%s"), Compiled, *Root.Path, Failed,
                      Flagged.Num(), Assets.Num() > MaxBlueprints ? TEXT(" (the first 500; name a narrower folder)") : TEXT("")),
      Data);
}
} // namespace McpBlueprintHandlers
