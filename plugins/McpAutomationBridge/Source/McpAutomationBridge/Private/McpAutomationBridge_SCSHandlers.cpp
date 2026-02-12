#include "McpAutomationBridge_SCSHandlers.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

#endif


#if WITH_EDITOR
void FSCSHandlers::FinalizeBlueprintSCSChange(UBlueprint *Blueprint,
                                              bool &bOutCompiled,
                                              bool &bOutSaved) {
  bOutCompiled = false;
  bOutSaved = false;

  if (!Blueprint) {
    return;
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  FKismetEditorUtilities::CompileBlueprint(Blueprint);
  bOutCompiled = true;
  
  // UE 5.7+ Fix: Use McpSafeAssetSave instead of SaveLoadedAssetThrottled.
  // SaveLoadedAssetThrottled triggers UEditorAssetLibrary::SaveLoadedAsset() which
  // causes thumbnail generation and recursive FlushRenderingCommands calls (11+ times).
  // This corrupts render thread state and causes access violations in RenderCore.dll.
  // McpSafeAssetSave marks package dirty without triggering disk save operations.
  bOutSaved = McpSafeAssetSave(Blueprint);
  if (!bOutSaved) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("McpSafeAssetSave reported failure for '%s' after SCS "
                "change"),
           *Blueprint->GetPathName());
  }
}

// Check if Play In Editor (PIE) is currently active
static bool IsPlayInEditorActive() {
  if (GEditor && GEditor->IsPlaySessionInProgress()) {
    return true;
  }
  // Additional check for standalone game instances
  if (GEditor) {
    for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
      if (Context.WorldType == EWorldType::PIE ||
          Context.WorldType == EWorldType::Game) {
        return true;
      }
    }
  }
  return false;
}

// Return PIE error result for SCS operations
static TSharedPtr<FJsonObject> PIEActiveError() {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(
      TEXT("error"),
      TEXT("SCS operations cannot modify Blueprints during Play In Editor "
           "(PIE). Please stop the play session first."));
  Result->SetStringField(TEXT("errorCode"), TEXT("PIE_ACTIVE"));
  return Result;
}
#endif

#if !WITH_EDITOR
// Forward declaration for non-editor builds - must be before first call site
static TSharedPtr<FJsonObject> UnsupportedSCSAction() {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(TEXT("error"),
                         TEXT("SCS operations require editor build"));
  return Result;
}
#endif

// Get Blueprint SCS structure
TSharedPtr<FJsonObject>
FSCSHandlers::GetBlueprintSCS(const FString &BlueprintPath) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  // Load blueprint
  // Load blueprint
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(
                  TEXT(
                      "Blueprint not found or not a valid Blueprint asset: %s"),
                  *BlueprintPath)
            : ErrorMsg);
    return Result;
  }

  // Get SCS
  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
  if (!SCS) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Blueprint has no SimpleConstructionScript"));
    return Result;
  }

  // Build component tree
  TArray<TSharedPtr<FJsonValue>> Components;
  const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();

  for (USCS_Node *Node : AllNodes) {
    if (Node && Node->GetVariableName().IsValid()) {
      TSharedPtr<FJsonObject> ComponentObj = MakeShareable(new FJsonObject);
      ComponentObj->SetStringField(TEXT("name"),
                                   Node->GetVariableName().ToString());
      ComponentObj->SetStringField(
          TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName()
                                              : TEXT("Unknown"));
      if (!Node->ParentComponentOrVariableName.IsNone()) {
        ComponentObj->SetStringField(
            TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
      }

      // Add transform if component template exists (cast to SceneComponent for
      // UE 5.6)
      if (Node->ComponentTemplate) {
        FTransform Transform = FTransform::Identity;
        if (USceneComponent *SceneComp =
                Cast<USceneComponent>(Node->ComponentTemplate)) {
          Transform = SceneComp->GetRelativeTransform();
        }
        TSharedPtr<FJsonObject> TransformObj = MakeShareable(new FJsonObject);

        FVector Loc = Transform.GetLocation();
        FRotator Rot = Transform.GetRotation().Rotator();
        FVector Scale = Transform.GetScale3D();

        TransformObj->SetStringField(
            TEXT("location"),
            FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"), Loc.X, Loc.Y, Loc.Z));
        TransformObj->SetStringField(
            TEXT("rotation"), FString::Printf(TEXT("P=%.2f Y=%.2f R=%.2f"),
                                              Rot.Pitch, Rot.Yaw, Rot.Roll));
        TransformObj->SetStringField(
            TEXT("scale"), FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"),
                                           Scale.X, Scale.Y, Scale.Z));

        ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
      }

      // Add child count
      ComponentObj->SetNumberField(TEXT("child_count"),
                                   Node->GetChildNodes().Num());

      Components.Add(MakeShareable(new FJsonValueObject(ComponentObj)));
    }
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetArrayField(TEXT("components"), Components);
  Result->SetNumberField(TEXT("count"), Components.Num());
  Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
#else
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(TEXT("error"),
                         TEXT("SCS operations require editor build"));
  return Result;
#endif

  return Result;
}

// Add component to SCS
TSharedPtr<FJsonObject> FSCSHandlers::AddSCSComponent(
    const FString &BlueprintPath, const FString &ComponentClass,
    const FString &ComponentName, const FString &ParentComponentName,
    const FString &MeshPath, const FString &MaterialPath) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  // Check for PIE - cannot modify Blueprints during play
  if (IsPlayInEditorActive()) {
    return PIEActiveError();
  }

  // Load blueprint
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    return Result;
  }

  // Get or create SCS
  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
  if (!SCS) {
    SCS = NewObject<USimpleConstructionScript>(Blueprint);
    Blueprint->SimpleConstructionScript = SCS;
  }

  // Find component class (UE 5.6: ANY_PACKAGE is deprecated, use nullptr)
  // Use the robust utility to resolve class (handles NiagaraComponent, Assets,
  // Native classes)
  UClass *CompClass = ResolveClassByName(ComponentClass);

  if (!CompClass) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"), FString::Printf(TEXT("Component class not found: %s"),
                                       *ComponentClass));
    return Result;
  }

  // Verify it's a component class
  if (!CompClass->IsChildOf(UActorComponent::StaticClass())) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Class is not a component: %s"), *ComponentClass));
    return Result;
  }

  // Find parent node if specified
  USCS_Node *ParentNode = nullptr;
  if (!ParentComponentName.IsEmpty()) {
    for (USCS_Node *Node : SCS->GetAllNodes()) {
      if (Node && Node->GetVariableName().IsValid() &&
          Node->GetVariableName().ToString().Equals(ParentComponentName,
                                                    ESearchCase::IgnoreCase)) {
        ParentNode = Node;
        break;
      }
    }

    if (!ParentNode) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"), FString::Printf(TEXT("Parent component not found: %s"),
                                         *ParentComponentName));
      return Result;
    }
  }

  // Check for duplicate name
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("Component with name '%s' already exists"),
                          *ComponentName));
      return Result;
    }
  }

  // Create new node
  USCS_Node *NewNode = SCS->CreateNode(CompClass, FName(*ComponentName));
  if (!NewNode) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Failed to create SCS node"));
    return Result;
  }

  // Explicitly set the variable name to ensure it's properly registered
  NewNode->SetVariableName(FName(*ComponentName));

  // Set parent or add as root
  if (ParentNode) {
    ParentNode->AddChildNode(NewNode);
  } else {
    SCS->AddNode(NewNode);
  }

  // Apply mesh if specified (Feature #1: Mesh Assignment)
  // Uses: SetStaticMesh - Engine/StaticMeshComponent.cpp:2265
  //       SetSkeletalMesh - Engine/SkeletalMeshComponent.cpp:3322
  bool bMeshApplied = false;
  if (!MeshPath.IsEmpty() && NewNode->ComponentTemplate) {
    if (UStaticMeshComponent *SMC =
            Cast<UStaticMeshComponent>(NewNode->ComponentTemplate)) {
      UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
      if (Mesh) {
        SMC->SetStaticMesh(Mesh);
        bMeshApplied = true;
      }
    } else if (USkeletalMeshComponent *SkMC =
                   Cast<USkeletalMeshComponent>(NewNode->ComponentTemplate)) {
      USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
      if (Mesh) {
        SkMC->SetSkeletalMesh(Mesh, true);
        bMeshApplied = true;
      }
    }
  }

  // Apply material if specified (Feature #2: Material Assignment)
  // Uses: SetMaterial - Engine/PrimitiveComponent.cpp:2477
  bool bMaterialApplied = false;
  if (!MaterialPath.IsEmpty() && NewNode->ComponentTemplate) {
    if (UPrimitiveComponent *PC =
            Cast<UPrimitiveComponent>(NewNode->ComponentTemplate)) {
      UMaterialInterface *Mat =
          LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
      if (Mat) {
        PC->SetMaterial(0, Mat);
        bMaterialApplied = true;
      }
    }
  }

  // Finalize blueprint change (compile/save)
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  // Real test: Verify component exists in SCS
  bool bVerified = false;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      bVerified = true;
      break;
    }
  }

  if (!bVerified) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"), FString::Printf(TEXT("Verification failed: Component "
                                            "'%s' not found in SCS after add"),
                                       *ComponentName));
    return Result;
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' added to SCS"), *ComponentName));
  Result->SetStringField(TEXT("component_name"), ComponentName);
  Result->SetStringField(TEXT("component_class"), CompClass->GetName());
  Result->SetStringField(TEXT("parent"), ParentComponentName.IsEmpty()
                                             ? TEXT("(root)")
                                             : ParentComponentName);
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  // Feature #1, #2: Report mesh/material assignment status
  Result->SetBoolField(TEXT("mesh_applied"), bMeshApplied);
  Result->SetBoolField(TEXT("material_applied"), bMaterialApplied);
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}

// Remove component from SCS
TSharedPtr<FJsonObject>
FSCSHandlers::RemoveSCSComponent(const FString &BlueprintPath,
                                 const FString &ComponentName) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    Result->SetStringField(TEXT("errorCode"), TEXT("ASSET_NOT_FOUND"));
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_NOT_FOUND"));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  // Find node to remove
  USCS_Node *NodeToRemove = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      NodeToRemove = Node;
      break;
    }
  }

  if (!NodeToRemove) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_COMPONENT_NOT_FOUND"));
    return Result;
  }

  SCS->RemoveNode(NodeToRemove);

  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' removed from SCS"), *ComponentName));
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}

// Reparent component within SCS
TSharedPtr<FJsonObject>
FSCSHandlers::ReparentSCSComponent(const FString &BlueprintPath,
                                   const FString &ComponentName,
                                   const FString &NewParentName) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  // Find component to reparent
  USCS_Node *ComponentNode = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    return Result;
  }

  // Find new parent (empty string means root)
  USCS_Node *NewParentNode = nullptr;
  if (!NewParentName.IsEmpty()) {
    // Accept common root synonyms
    const bool bRootSynonym =
        NewParentName.Equals(TEXT("RootComponent"), ESearchCase::IgnoreCase) ||
        NewParentName.Equals(TEXT("DefaultSceneRoot"),
                             ESearchCase::IgnoreCase) ||
        NewParentName.Equals(TEXT("Root"), ESearchCase::IgnoreCase);
    if (bRootSynonym) {
      const TArray<USCS_Node *> &Roots = SCS->GetRootNodes();
      // Prefer an explicit DefaultSceneRoot if present
      for (USCS_Node *R : Roots) {
        if (R && R->GetVariableName().IsValid() &&
            R->GetVariableName().ToString().Equals(TEXT("DefaultSceneRoot"),
                                                   ESearchCase::IgnoreCase)) {
          NewParentNode = R;
          break;
        }
      }
      // Fallback: first root that is not the component itself
      if (!NewParentNode) {
        for (USCS_Node *R : Roots) {
          if (R && R != ComponentNode) {
            NewParentNode = R;
            break;
          }
        }
      }
    }

    if (!NewParentNode) {
      for (USCS_Node *Node : SCS->GetAllNodes()) {
        if (Node && Node->GetVariableName().IsValid() &&
            Node->GetVariableName().ToString().Equals(
                NewParentName, ESearchCase::IgnoreCase)) {
          NewParentNode = Node;
          break;
        }
      }
    }

    if (!NewParentNode) {
      // If caller asked for RootComponent and we can't resolve it, treat as a
      // benign no-op
      if (bRootSynonym) {
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(
            TEXT("message"),
            TEXT("Requested RootComponent not found; component remains at "
                 "current hierarchy (treated as success)."));
        return Result;
      }
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("New parent not found: %s"), *NewParentName));
      return Result;
    }
  }

  // Helper: check if B is a descendant of A (prevent cycles)
  auto IsDescendantOf = [](USCS_Node *A, USCS_Node *B) -> bool {
    if (!A || !B)
      return false;
    TArray<USCS_Node *> Stack;
    Stack.Add(A);
    while (Stack.Num() > 0) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      // UE 5.4+: Uses EAllowShrinking enum
      USCS_Node *Cur = Stack.Pop(EAllowShrinking::No);
#else
      // UE 5.0-5.3: Uses bool bAllowShrinking (or no parameter in 5.0)
      USCS_Node *Cur = Stack.Pop(false);
#endif
      if (!Cur)
        continue;
      const TArray<USCS_Node *> &Kids = Cur->GetChildNodes();
      for (USCS_Node *K : Kids) {
        if (!K)
          continue;
        if (K == B)
          return true;
        Stack.Add(K);
      }
    }
    return false;
  };

  // Remove from current parent (UE 5.6: find parent manually)
  USCS_Node *OldParent = nullptr;
  for (USCS_Node *Candidate : SCS->GetAllNodes()) {
    if (Candidate && Candidate->GetChildNodes().Contains(ComponentNode)) {
      OldParent = Candidate;
      break;
    }
  }

  // No-op checks (already under desired parent)
  if ((OldParent == nullptr && NewParentNode && SCS->GetRootNodes().Num() > 0 &&
       NewParentNode == SCS->GetRootNodes()[0]) ||
      (OldParent != nullptr && NewParentNode == OldParent)) {
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(
        TEXT("message"),
        TEXT("Component already under requested parent; no changes made"));
    return Result;
  }

  // Prevent cycles: new parent cannot be a descendant of the component
  if (NewParentNode && IsDescendantOf(ComponentNode, NewParentNode)) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        TEXT("Cannot create circular parent-child relationship"));
    return Result;
  }

  // Detach from old parent
  if (OldParent) {
    OldParent->RemoveChildNode(ComponentNode);
  } else {
    // Was a root node; remove from root listing when reparenting to non-root
    const bool bReparentingToRoot = (NewParentNode == nullptr);
    if (!bReparentingToRoot) {
      SCS->RemoveNode(ComponentNode);
    }
    // else already at root and staying root would have been returned above
  }

  // Attach to new parent or root
  if (NewParentNode) {
    NewParentNode->AddChildNode(ComponentNode);
  } else {
    SCS->AddNode(ComponentNode);
  }

  // Mark blueprint as modified and finalize change
  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' reparented to '%s'"), *ComponentName,
                      NewParentName.IsEmpty() ? TEXT("(root)")
                                              : *NewParentName));
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}

// Set component transform in SCS
TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentTransform(
    const FString &BlueprintPath, const FString &ComponentName,
    const TSharedPtr<FJsonObject> &TransformData) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    Result->SetStringField(TEXT("errorCode"), TEXT("ASSET_NOT_FOUND"));
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_NOT_FOUND"));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  // Find component
  USCS_Node *ComponentNode = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode || !ComponentNode->ComponentTemplate) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component or template not found: %s"),
                        *ComponentName));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_COMPONENT_TEMPLATE_NOT_FOUND"));
    return Result;
  }

  // Parse transform from JSON
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  FVector Scale(1, 1, 1);

  // Parse location array [x, y, z]
  const TArray<TSharedPtr<FJsonValue>> *LocArray;
  if (TransformData->TryGetArrayField(TEXT("location"), LocArray) &&
      LocArray->Num() >= 3) {
    Location.X = (*LocArray)[0]->AsNumber();
    Location.Y = (*LocArray)[1]->AsNumber();
    Location.Z = (*LocArray)[2]->AsNumber();
  }

  // Parse rotation array [pitch, yaw, roll]
  const TArray<TSharedPtr<FJsonValue>> *RotArray;
  if (TransformData->TryGetArrayField(TEXT("rotation"), RotArray) &&
      RotArray->Num() >= 3) {
    Rotation.Pitch = (*RotArray)[0]->AsNumber();
    Rotation.Yaw = (*RotArray)[1]->AsNumber();
    Rotation.Roll = (*RotArray)[2]->AsNumber();
  }

  // Parse scale array [x, y, z]
  const TArray<TSharedPtr<FJsonValue>> *ScaleArray;
  if (TransformData->TryGetArrayField(TEXT("scale"), ScaleArray) &&
      ScaleArray->Num() >= 3) {
    Scale.X = (*ScaleArray)[0]->AsNumber();
    Scale.Y = (*ScaleArray)[1]->AsNumber();
    Scale.Z = (*ScaleArray)[2]->AsNumber();
  }

  // Apply transform to component template
  FTransform NewTransform(Rotation, Location, Scale);

  if (USceneComponent *SceneComp =
          Cast<USceneComponent>(ComponentNode->ComponentTemplate)) {
    SceneComp->SetRelativeTransform(NewTransform);

    bool bCompiled = false;
    bool bSaved = false;
    FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(
        TEXT("message"),
        FString::Printf(TEXT("Transform set for component '%s'"),
                        *ComponentName));
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("saved"), bSaved);
  } else {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        TEXT("Component is not a SceneComponent (no transform)"));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_NOT_SCENE_COMPONENT"));
  }
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}

// Set component property in SCS
TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentProperty(
    const FString &BlueprintPath, const FString &ComponentName,
    const FString &PropertyName, const TSharedPtr<FJsonValue> &PropertyValue) {
  TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    Result->SetStringField(TEXT("errorCode"), TEXT("ASSET_NOT_FOUND"));
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  // Find component
  USCS_Node *ComponentNode = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode || !ComponentNode->ComponentTemplate) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component or template not found: %s"),
                        *ComponentName));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_COMPONENT_TEMPLATE_NOT_FOUND"));
    return Result;
  }

  if (PropertyValue.IsValid()) {
    // ResolveNestedPropertyPath now returns the container pointer
    void *ContainerPtr = nullptr;
    FString ResolveError;
    FString FailureMessage;
    FString FailureCode;
    bool bAppliedValue = false;
    FProperty *TargetProp =
        ResolveNestedPropertyPath(ComponentNode->ComponentTemplate,
                                  PropertyName, ContainerPtr, ResolveError);

    if (!TargetProp || !ContainerPtr) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          ResolveError.IsEmpty()
              ? FString::Printf(TEXT("Property not found: %s"), *PropertyName)
              : ResolveError);
      Result->SetStringField(TEXT("errorCode"), TEXT("SCS_PROPERTY_NOT_FOUND"));
      return Result;
    }

    if (ApplyJsonValueToProperty(ContainerPtr, TargetProp, PropertyValue,
                                 FailureMessage)) {
      bAppliedValue = true;
    } else {
      FailureCode = TEXT("SCS_PROPERTY_APPLY_FAILED");
    }

    if (!bAppliedValue) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"),
                             FailureMessage.IsEmpty()
                                 ? TEXT("Failed to apply property value")
                                 : FailureMessage);
      if (!FailureCode.IsEmpty()) {
        Result->SetStringField(TEXT("errorCode"), FailureCode);
      }
      return Result;
    }
  } else {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Property value is invalid"));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_PROPERTY_INVALID_VALUE"));
    return Result;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Property '%s' set on component '%s'"),
                      *PropertyName, *ComponentName));
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
