#pragma once

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AutomatedAssetImportData.h"
#include "CoreMinimal.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

// What an import asked of the FBX importer beyond bringing the file in.
struct FMcpFbxImportOptions {
  bool bImportAnimations = false;
  FString SkeletonPath;
  // With SkeletonPath: the skeletal mesh bound to that skeleton, not the take alone.
  bool bImportMesh = false;
  TOptional<bool> CreatePhysicsAsset;
  FString PhysicsAssetPath;
  TOptional<bool> ImportMorphTargets;
  bool Any() const {
    return bImportAnimations || !SkeletonPath.IsEmpty() || bImportMesh || CreatePhysicsAsset.IsSet() ||
           !PhysicsAssetPath.IsEmpty() || ImportMorphTargets.IsSet();
  }
};

inline FMcpFbxImportOptions McpReadFbxImportOptions(const TSharedPtr<FJsonObject> &Payload) {
  FMcpFbxImportOptions Options;
  Options.bImportAnimations = GetJsonBoolField(Payload, TEXT("importAnimations"), false);
  Options.SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
  Options.bImportMesh = GetJsonBoolField(Payload, TEXT("importMesh"), false);
  Options.PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
  bool bValue = false;
  if (Payload.IsValid() && Payload->TryGetBoolField(TEXT("createPhysicsAsset"), bValue)) {
    Options.CreatePhysicsAsset = bValue;
  }
  if (Payload.IsValid() && Payload->TryGetBoolField(TEXT("importMorphTargets"), bValue)) {
    Options.ImportMorphTargets = bValue;
  }
  return Options;
}

// An FBX carrying a mocap take imports as a bare SkeletalMesh and the animation
// is silently dropped. AssetTools routes an automated import through Interchange
// whenever no factory is named, so naming UFbxFactory is what lets any of these
// options reach the importer at all. Returns nullptr when the caller asked for
// none of this, which leaves the default Interchange path untouched.
inline UFactory *McpMakeFbxFactory(UAutomatedAssetImportData *Owner,
                                   const FMcpFbxImportOptions &Options,
                                   FString &OutError,
                                   FString &OutErrorCode) {
  if (!Options.Any()) {
    return nullptr;
  }
  USkeleton *Skel = Options.SkeletonPath.IsEmpty()
                        ? nullptr
                        : LoadObject<USkeleton>(nullptr, *Options.SkeletonPath);
  // Falling back to a mesh import here would answer a request to retarget a
  // take onto a named rig with an unrelated asset, and call it success.
  if (Skel == nullptr && !Options.SkeletonPath.IsEmpty()) {
    OutError = FString::Printf(TEXT("No Skeleton at %s"), *Options.SkeletonPath);
    OutErrorCode = TEXT("SKELETON_NOT_FOUND");
    return nullptr;
  }
  UPhysicsAsset *Physics = Options.PhysicsAssetPath.IsEmpty()
                               ? nullptr
                               : LoadObject<UPhysicsAsset>(nullptr, *Options.PhysicsAssetPath);
  if (Physics == nullptr && !Options.PhysicsAssetPath.IsEmpty()) {
    OutError = FString::Printf(TEXT("No Physics Asset at %s"), *Options.PhysicsAssetPath);
    OutErrorCode = TEXT("PHYSICS_ASSET_NOT_FOUND");
    return nullptr;
  }
  // A skeleton alone means "import the take against this rig", which is what
  // retargeting a downloaded clip onto an existing character needs. With
  // importMesh the mesh itself is bound to the skeleton instead.
  const bool bTakeAlone = Skel != nullptr && !Options.bImportMesh;
  UFbxFactory *Fbx = NewObject<UFbxFactory>(Owner);
  Fbx->ImportUI->bImportAnimations = Options.bImportAnimations || bTakeAlone;
  Fbx->ImportUI->bAutomatedImportShouldDetectType = false;
  Fbx->ImportUI->bImportMesh = !bTakeAlone;
  Fbx->ImportUI->MeshTypeToImport = bTakeAlone ? FBXIT_Animation : FBXIT_SkeletalMesh;
  Fbx->ImportUI->Skeleton = Skel;
  Fbx->ImportUI->PhysicsAsset = Physics;
  Fbx->ImportUI->bCreatePhysicsAsset = Physics == nullptr && Options.CreatePhysicsAsset.Get(true);
  if (Options.ImportMorphTargets.IsSet()) {
    Fbx->ImportUI->SkeletalMeshImportData->bImportMorphTargets = Options.ImportMorphTargets.GetValue();
  }
  // Mocap rarely lands on a whole frame, and the importer rejects a take that
  // does not, with nobody here to answer the prompt it would otherwise raise.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
  Fbx->ImportUI->AnimSequenceImportData->bSnapToClosestFrameBoundary = true;
#endif
  return Fbx;
}

// UFbxFactory::FactoryCreateFile hands an already-loaded destination object to
// FReimportManager and returns early, which throws away the options above: the
// same call then yields a different result depending on whether the asset
// happens to be resident in memory, and it is resident exactly when a previous
// import in this session put it there. Clearing the object first makes the
// caller's options decide the import instead of the editor's memory state.
// The factory keys that early return on the name AssetTools derives from the
// SOURCE file, and HandleImportAsset only renames to DestName afterwards, so
// both names have to be clear. Returns false and fills OutError when something
// is in the way and the caller did not ask to overwrite it.
inline bool McpClearFbxImportTarget(const FString &DestPath,
                                    const FString &DestName,
                                    const FString &SourceFile, bool bOverwrite,
                                    FString &OutError,
                                    FString &OutErrorCode) {
  TArray<FString> Names;
  Names.Add(DestName);
  Names.AddUnique(ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(SourceFile)));
  // Ask the registry, not memory: whether the asset happens to be loaded is
  // exactly the accident that made this call non-deterministic to begin with.
  IAssetRegistry &Registry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
          .Get();
  for (const FString &Name : Names) {
    const FString Target = DestPath / Name + TEXT(".") + Name;
    if (!Registry.GetAssetByObjectPath(MCP_ASSET_REGISTRY_OBJECT_PATH(Target), false).IsValid()) {
      continue;
    }
    if (!bOverwrite) {
      OutError = FString::Printf(
          TEXT("%s/%s already exists and would be reimported with its own stored ")
          TEXT("settings, dropping the import options given here. Pass ")
          TEXT("overwrite:true to replace it, or choose an empty destination."),
          *DestPath, *Name);
      OutErrorCode = TEXT("DESTINATION_OCCUPIED");
      return false;
    }
    // The reference check keeps an asset something else still points at from
    // being deleted out from under it. Its refusal is a message dialog, which
    // the engine answers by itself only while unattended -- and this runs on a
    // later tick, after the request's own unattended guard has ended.
    TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);
    UObject *Existing = FSoftObjectPath(Target).TryLoad();
    if (Existing == nullptr ||
        !ObjectTools::DeleteSingleObject(Existing, /*bPerformReferenceCheck=*/true)) {
      OutError = FString::Printf(
          TEXT("Could not replace the existing %s/%s; it may still be referenced by another asset"),
          *DestPath, *Name);
      OutErrorCode = TEXT("REPLACE_FAILED");
      return false;
    }
  }
  return true;
}

// The asset an import is about: a mesh before its animations, before its materials and textures. The first one the
// importer happened to return could be a texture, which then took the destination name.
inline UObject *McpPickPrimaryImport(const TArray<UObject *> &Imported) {
  for (UClass *Kind : {USkeletalMesh::StaticClass(), UStaticMesh::StaticClass(), UAnimSequence::StaticClass(), UObject::StaticClass()}) {
    for (UObject *Object : Imported) {
      if (Object && Object->IsA(Kind)) {
        return Object;
      }
    }
  }
  return nullptr;
}

// Everything the import made besides the primary asset, and for a skeletal mesh the skeleton, physics asset, bone and
// morph target counts it ended up with, so the next call can use them without searching.
inline void McpDescribeImport(const TArray<UObject *> &Imported, const UObject *Primary, UObject *Reloaded,
                              const TSharedPtr<FJsonObject> &Resp) {
  TArray<TSharedPtr<FJsonValue>> Others;
  for (const UObject *Object : Imported) {
    if (Object && Object != Primary && Others.Num() < 50) {
      Others.Add(MakeShared<FJsonValueString>(Object->GetPathName()));
    }
  }
  if (Others.Num() > 0) {
    Resp->SetArrayField(TEXT("alsoImported"), Others);
  }
  if (USkeletalMesh *Mesh = Cast<USkeletalMesh>(Reloaded)) {
    Resp->SetStringField(TEXT("skeleton"), Mesh->GetSkeleton() ? Mesh->GetSkeleton()->GetPathName() : FString());
    Resp->SetStringField(TEXT("physicsAsset"), Mesh->GetPhysicsAsset() ? Mesh->GetPhysicsAsset()->GetPathName() : FString());
    Resp->SetNumberField(TEXT("bones"), Mesh->GetRefSkeleton().GetNum());
    Resp->SetNumberField(TEXT("morphTargets"), Mesh->GetMorphTargets().Num());
  }
}
