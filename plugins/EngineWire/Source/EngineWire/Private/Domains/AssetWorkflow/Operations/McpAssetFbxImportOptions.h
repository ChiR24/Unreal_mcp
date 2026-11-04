#pragma once

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AutomatedAssetImportData.h"
#include "CoreMinimal.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "Safety/McpSafeOperationsAssetSave.h"
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
  // Interchange, which takes every import without these options, honours the units the file was written in; this
  // factory leaves them unconverted by default, so a character exported in metres (Blender) came in at a hundredth
  // of the size its static meshes did. A file already in centimetres is untouched either way.
  Fbx->ImportUI->SkeletalMeshImportData->bConvertSceneUnit = true;
  Fbx->ImportUI->AnimSequenceImportData->bConvertSceneUnit = true;
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
// is in the way and the caller did not ask to overwrite it. Every import asks:
// a PNG or WAV used to be reimported over whatever sat there, whatever
// overwrite said. bDeleteExisting (an FBX with options) deletes the old asset
// first; any other file replaces it in place, keeping what points at it.
inline bool McpClearImportTarget(const FString &DestPath,
                                 const FString &DestName,
                                 const FString &SourceFile, bool bOverwrite,
                                 FString &OutError,
                                 FString &OutErrorCode, bool bDeleteExisting) {
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
          TEXT("%s/%s already exists. Pass overwrite:true to replace it, or ")
          TEXT("choose an empty destination."),
          *DestPath, *Name);
      OutErrorCode = TEXT("DESTINATION_OCCUPIED");
      return false;
    }
    if (!bDeleteExisting) {
      continue;
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

// Object paths of the assets directly in a folder.
inline TSet<FString> McpAssetPathsIn(const FString &Folder) {
  TArray<FAssetData> Found;
  FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(FName(*Folder), Found,
                                                                                                  false);
  TSet<FString> Paths;
  for (const FAssetData &Data : Found) {
    Paths.Add(MCP_ASSET_DATA_GET_SOFT_PATH(Data));
  }
  return Paths;
}

// The FBX factory hands back the mesh alone: the takes it made beside it were neither listed nor saved, so they were
// gone after a restart. What appeared in the folder during the import joins the list.
inline void McpAddFolderNewcomers(const FString &Folder, const TSet<FString> &Before, TArray<UObject *> &Imported) {
  for (const FString &Path : McpAssetPathsIn(Folder)) {
    UObject *Object = Before.Contains(Path) ? nullptr : FSoftObjectPath(Path).ResolveObject();
    if (Object && !Imported.Contains(Object)) {
      Imported.Add(Object);
    }
  }
}

// Imports the file; with the FBX factory, what appeared in the folder joins what the factory handed back.
inline TArray<UObject *> McpImportAndCollect(IAssetTools &AssetTools, UAutomatedAssetImportData *ImportData,
                                             const FString &DestPath) {
  const TSet<FString> Before = McpAssetPathsIn(DestPath);
  TArray<UObject *> Imported = AssetTools.ImportAssetsAutomated(ImportData);
  if (ImportData->Factory != nullptr) {
    McpAddFolderNewcomers(DestPath, Before, Imported);
  }
  return Imported;
}

// Saves the primary asset (reloaded under its final name) and everything imported beside it: true when all were
// written.
inline bool McpSaveImports(UObject *Reloaded, const UObject *Primary, const TArray<UObject *> &Imported) {
  bool bSaved = Reloaded && McpSafeAssetSave(Reloaded);
  for (UObject *Other : Imported) {
    if (Other && Other != Primary && !McpSafeAssetSave(Other)) {
      bSaved = false;
    }
  }
  return bSaved;
}

// A take whose root bone plays at another scale than the mesh's reference pose draws the character that much bigger or
// smaller. Blender's FBX "Armature" node does it: the importer leaves it out of the bones, so a metre file's unit scale
// on it reaches the mesh's root but not the takes, which are read relative to it (a 93 cm character played as a blob).
inline TArray<TSharedPtr<FJsonValue>> McpTakeScaleWarnings(const USkeletalMesh &Mesh, const TArray<UObject *> &Imported) {
  TArray<FString> Takes;
  double Ratio = 1.0;
  const FReferenceSkeleton &Ref = Mesh.GetRefSkeleton();
  const double RefScale = Ref.GetNum() > 0 ? Ref.GetRefBonePose()[0].GetScale3D().GetAbsMax() : 0.0;
  for (UObject *Object : Imported) {
    UAnimSequence *Take = Cast<UAnimSequence>(Object);
    if (!Take || RefScale <= 0.0 || Take->GetSkeleton() != Mesh.GetSkeleton()) {
      continue;
    }
    FAnimPose Pose;
    UAnimPoseExtensions::GetAnimPoseAtFrame(Take, 0, FAnimPoseEvaluationOptions(), Pose);
    const double TakeRatio =
        UAnimPoseExtensions::GetBonePose(Pose, Ref.GetBoneName(0), EAnimPoseSpaces::Local).GetScale3D().GetAbsMax() / RefScale;
    if (TakeRatio < 0.5 || TakeRatio > 2.0) {
      Ratio = Takes.Num() == 0 ? TakeRatio : Ratio;
      Takes.Add(Take->GetName());
    }
  }
  TArray<TSharedPtr<FJsonValue>> Warnings;
  if (Takes.Num() > 0) {
    Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("%d take(s) play the character at %g times its size (%s): their root bone is read relative to a node the "
             "mesh's reference pose includes (Blender's Armature node carrying the file's unit scale). Export the "
             "character in centimetres with that node at scale 1 (Blender: unit scale 0.01, the armature's scale "
             "applied), then import it again."),
        Takes.Num(), Ratio, *FString::Join(Takes, TEXT(", ")))));
  }
  return Warnings;
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
    const TArray<TSharedPtr<FJsonValue>> Warnings = McpTakeScaleWarnings(*Mesh, Imported);
    if (Warnings.Num() > 0) {
      Resp->SetArrayField(TEXT("warnings"), Warnings);
    }
  }
}
