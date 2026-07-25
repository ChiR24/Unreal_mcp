#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace McpSequenceMedia {
namespace {

FString NormalizeLoadPath(const FString &Input) {
  FString Path = Input.TrimStartAndEnd();
  Path.ReplaceInline(TEXT("\\"), TEXT("/"));
  McpAssetPathCanonical::MapContentRootInline(Path);
  if (Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT("."))) {
    Path += TEXT(".") + FPaths::GetBaseFilename(Path);
  }
  return Path;
}

}

FString CanonicalMediaAction(const FString &Action) {
  FString Result = Action.TrimStartAndEnd().ToLower();
  Result.ReplaceInline(TEXT("-"), TEXT("_"));
  Result.ReplaceInline(TEXT(" "), TEXT("_"));
  if (Result.StartsWith(TEXT("sequence_"))) {
    Result.RightChopInline(9);
  }
  return Result;
}

FString GetStringAny(const TSharedPtr<FJsonObject> &Payload,
                     std::initializer_list<const TCHAR *> Fields,
                     const FString &DefaultValue) {
  if (!Payload.IsValid()) {
    return DefaultValue;
  }
  for (const TCHAR *Field : Fields) {
    FString Value;
    if (Payload->TryGetStringField(Field, Value) &&
        !Value.TrimStartAndEnd().IsEmpty()) {
      return Value.TrimStartAndEnd();
    }
  }
  return DefaultValue;
}

bool GetBoolAny(const TSharedPtr<FJsonObject> &Payload,
                std::initializer_list<const TCHAR *> Fields,
                bool DefaultValue) {
  if (!Payload.IsValid()) {
    return DefaultValue;
  }
  for (const TCHAR *Field : Fields) {
    bool Value = false;
    if (Payload->TryGetBoolField(Field, Value)) {
      return Value;
    }
  }
  return DefaultValue;
}

double GetNumberAny(const TSharedPtr<FJsonObject> &Payload,
                    std::initializer_list<const TCHAR *> Fields,
                    double DefaultValue) {
  if (!Payload.IsValid()) {
    return DefaultValue;
  }
  for (const TCHAR *Field : Fields) {
    double Value = 0.0;
    if (Payload->TryGetNumberField(Field, Value)) {
      return Value;
    }
  }
  return DefaultValue;
}

void SendMediaError(UMcpAutomationBridgeSubsystem *Subsystem,
                    TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const FString &RequestId,
                    const FString &ErrorCode,
                    const FString &Message,
                    const TSharedPtr<FJsonObject> &Details) {
  TSharedPtr<FJsonObject> Result =
      Details.IsValid() ? Details : McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("code"), ErrorCode);
  Result->SetStringField(TEXT("message"), Message);
  Subsystem->SendAutomationResponse(Socket, RequestId, false, Message, Result,
                                    ErrorCode);
}

bool ResolveMediaAssetIdentity(const TSharedPtr<FJsonObject> &Payload,
                               const FString &DefaultFolder,
                               const FString &DefaultName,
                               FString &OutPackageName,
                               FString &OutAssetName,
                               FString &OutObjectPath,
                               FString &OutError) {
  FString AssetPath =
      GetStringAny(Payload, {TEXT("assetPath"), TEXT("objectPath")});
  FString Folder =
      GetStringAny(Payload, {TEXT("packagePath"), TEXT("folder"), TEXT("path")},
                   DefaultFolder);
  FString Name =
      GetStringAny(Payload, {TEXT("name"), TEXT("assetName")}, DefaultName);
  if (!AssetPath.IsEmpty()) {
    AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (AssetPath.Contains(TEXT("."))) {
      AssetPath = AssetPath.Left(AssetPath.Find(TEXT(".")));
    }
    Name = FPaths::GetBaseFilename(AssetPath);
    Folder = FPaths::GetPath(AssetPath);
  }
  Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
  McpAssetPathCanonical::MapContentRootInline(Folder);
  if (Folder.Equals(TEXT("Game")) ||
      Folder.StartsWith(TEXT("Game/"))) {
    Folder = TEXT("/") + Folder;
  } else if (!Folder.StartsWith(TEXT("/"))) {
    Folder = TEXT("/Game/") + Folder.TrimChar(TEXT('/'));
  }
  Name = Name.TrimStartAndEnd();
  if (Name.IsEmpty() || Name.Contains(TEXT("/")) || Name.Contains(TEXT("."))) {
    OutError = TEXT("A valid media asset name is required");
    return false;
  }
  OutPackageName = Folder.TrimChar(TEXT('/'));
  OutPackageName = TEXT("/") + OutPackageName / Name;
  FString ValidatedPackageName;
  if (!McpSequencePathSecurity::ValidateWritableAssetPath(
          OutPackageName, ValidatedPackageName, OutError)) {
    return false;
  }
  OutPackageName = ValidatedPackageName;
  if (!FPackageName::IsValidLongPackageName(OutPackageName)) {
    OutError = FString::Printf(TEXT("Invalid media asset path: %s"),
                               *OutPackageName);
    return false;
  }
  OutAssetName = Name;
  OutObjectPath = OutPackageName + TEXT(".") + Name;
  return true;
}

bool CreateMediaAssetFromPayload(const TSharedPtr<FJsonObject> &Payload,
                                 const FString &DefaultFolder,
                                 const FString &ClassName,
                                 FMediaAssetCreateResult &OutCreated,
                                 FString &OutError) {
  FString PackageName;
  FString AssetName;
  FString ObjectPath;
  if (!ResolveMediaAssetIdentity(Payload, DefaultFolder, FString(),
                                 PackageName, AssetName, ObjectPath, OutError)) {
    return false;
  }
  UClass *AssetClass = ResolveMediaClass(ClassName, OutError);
  return AssetClass &&
         CreateMediaAsset(AssetClass, PackageName, AssetName, OutCreated,
                          OutError);
}

UClass *ResolveMediaClass(const FString &ClassName, FString &OutError) {
  FModuleManager &Modules = FModuleManager::Get();
  if (!Modules.IsModuleLoaded(TEXT("MediaAssets")) &&
      (!Modules.ModuleExists(TEXT("MediaAssets")) ||
       !Modules.LoadModule(TEXT("MediaAssets")))) {
    OutError = TEXT("MediaAssets module is unavailable");
    return nullptr;
  }
  const FString ClassPath =
      FString::Printf(TEXT("/Script/MediaAssets.%s"), *ClassName);
  UClass *Class = LoadObject<UClass>(nullptr, *ClassPath);
  if (!Class) {
    OutError = FString::Printf(TEXT("Media class is unavailable: %s"),
                               *ClassPath);
  }
  return Class;
}

UObject *LoadMediaObject(const FString &ObjectPath,
                         UClass *ExpectedClass,
                         FString &OutResolvedPath,
                         FString &OutError) {
  OutResolvedPath = NormalizeLoadPath(ObjectPath);
  UObject *Object = LoadObject<UObject>(nullptr, *OutResolvedPath);
  if (!Object) {
    OutError =
        FString::Printf(TEXT("Media object not found: %s"), *ObjectPath);
    return nullptr;
  }
  if (ExpectedClass && !Object->IsA(ExpectedClass)) {
    OutError = FString::Printf(TEXT("Object is not a %s: %s"),
                               *ExpectedClass->GetName(), *ObjectPath);
    return nullptr;
  }
  return Object;
}

bool CreateMediaAsset(UClass *AssetClass,
                      const FString &PackageName,
                      const FString &AssetName,
                      FMediaAssetCreateResult &OutCreated,
                      FString &OutError) {
  const FString ObjectPath = PackageName + TEXT(".") + AssetName;
  if (UObject *Existing = LoadObject<UObject>(nullptr, *ObjectPath)) {
    OutError = Existing->IsA(AssetClass)
                   ? TEXT("[MEDIA_ASSET_ALREADY_EXISTS] A media asset already exists at this path")
                   : TEXT("An asset of another class already uses this path");
    return false;
  }
  if (FPackageName::DoesPackageExist(PackageName)) {
    OutError =
        FString::Printf(TEXT("Package already exists: %s"), *PackageName);
    return false;
  }
  UPackage *Package = CreatePackage(*PackageName);
  UObject *Object =
      Package ? NewObject<UObject>(Package, AssetClass, FName(*AssetName),
                                   RF_Public | RF_Standalone | RF_Transactional)
              : nullptr;
  if (!Object) {
    OutError = TEXT("Failed to create media asset");
    return false;
  }
  FAssetRegistryModule::AssetCreated(Object);
  OutCreated.Object = Object;
  OutCreated.PackageName = PackageName;
  OutCreated.ObjectPath = Object->GetPathName();
  OutCreated.bCreated = true;
  return true;
}

TSharedPtr<FJsonObject> BuildAssetResponse(
    const FMediaAssetCreateResult &Created,
    const FString &AssetType) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetType"), AssetType);
  Result->SetStringField(TEXT("assetPath"), Created.ObjectPath);
  Result->SetStringField(TEXT("packageName"), Created.PackageName);
  Result->SetBoolField(TEXT("created"), Created.bCreated);
  Result->SetBoolField(TEXT("saved"), Created.bSaved);
  if (Created.Object) {
    Result->SetStringField(TEXT("classPath"),
                           Created.Object->GetClass()->GetPathName());
    McpHandlerUtils::AddVerification(Result, Created.Object);
  }
  return Result;
}

bool SaveMediaAsset(UObject *Object) {
#if WITH_EDITOR
  return Object && McpSafeOperations::McpSafeAssetSave(Object);
#else
  return false;
#endif
}

}
