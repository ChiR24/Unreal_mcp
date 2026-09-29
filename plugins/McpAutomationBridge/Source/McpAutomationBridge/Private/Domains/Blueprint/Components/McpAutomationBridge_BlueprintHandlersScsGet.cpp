#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetResolution.h"

namespace McpBlueprintHandlers {
namespace {
constexpr int32 McpScsScanMaxBlueprints = 300;

// componentClass names the class or a parent of it, by name part or /Script path: "TextRender"
// matches TextRenderComponent, "PrimitiveComponent" every primitive. Empty matches all.
bool ComponentClassMatches(const UClass *ComponentClass, const FString &Filter) {
  for (const UClass *Class = ComponentClass; Class && !Filter.IsEmpty(); Class = Class->GetSuperClass()) {
    if (Class->GetName().Contains(Filter) || Class->GetPathName().Equals(Filter, ESearchCase::IgnoreCase)) {
      return true;
    }
  }
  return Filter.IsEmpty();
}

TSharedPtr<FJsonValue> InheritedRow(const FString &Name, const UClass *Class, const FString &OwnerClass) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetStringField(TEXT("componentName"), Name);
  Obj->SetStringField(TEXT("componentType"), Class->GetName());
  Obj->SetBoolField(TEXT("inherited"), true);
  Obj->SetBoolField(TEXT("isSceneComponent"), Class->IsChildOf(USceneComponent::StaticClass()));
  Obj->SetStringField(TEXT("ownerClass"), OwnerClass);
  return MakeShared<FJsonValueObject>(Obj);
}

// What a Blueprint has without owning it: each parent Blueprint's SCS nodes, then the native
// components on its parent class's CDO (a Character's Mesh). Neither is on its own SCS.
void CollectInheritedComponents(UBlueprint *Blueprint, const FString &Filter, TArray<TSharedPtr<FJsonValue>> &Out) {
  McpPropertyCdoComponents::ForEachScsNode(Blueprint, [&](USCS_Node *Node, bool bInherited) {
    if (bInherited && Node->ComponentClass && ComponentClassMatches(Node->ComponentClass, Filter)) {
      const UBlueprint *Owner = Node->GetSCS() ? Node->GetSCS()->GetBlueprint() : nullptr;
      Out.Add(InheritedRow(Node->GetVariableName().ToString(), Node->ComponentClass,
                           Owner && Owner->GeneratedClass ? Owner->GeneratedClass->GetName() : FString()));
    }
    return true;
  });
  UClass *ParentClass = Blueprint->ParentClass;
  if (const AActor *ParentCDO = ParentClass ? Cast<AActor>(ParentClass->GetDefaultObject()) : nullptr) {
    for (const UActorComponent *Comp : ParentCDO->GetComponents()) {
      if (Comp && ComponentClassMatches(Comp->GetClass(), Filter)) {
        Out.Add(InheritedRow(Comp->GetName(), Comp->GetClass(), ParentClass->GetName()));
      }
    }
  }
}

// get_scs with a folder (path) instead of blueprintPath: every Blueprint under it with a
// component matching componentClass, its own or inherited, so "which Blueprints show a
// TextRender" is one call.
bool SendScsFolderScan(UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
                       TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Folder, const FString &Filter) {
  const FNormalizedAssetPath Norm = NormalizeAssetPath(Folder);
  if (!Norm.bIsValid) {
    Bridge.SendAutomationError(Socket, RequestId, Norm.ErrorMessage, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TArray<FAssetData> Assets;
  FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(*Norm.Path), Assets, true);
  Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.PackageName.ToString() < B.PackageName.ToString(); });
  TArray<TSharedPtr<FJsonValue>> Matches;
  int32 Scanned = 0;
  bool bTruncated = false;
  for (const FAssetData &Data : Assets) {
    const FString ClassPath = MCP_ASSET_DATA_GET_CLASS_PATH(Data);
    if (ClassPath != TEXT("/Script/Engine.Blueprint") && ClassPath != TEXT("Blueprint")) {
      continue;
    }
    bTruncated = Scanned >= McpScsScanMaxBlueprints;
    if (bTruncated) {
      break;
    }
    ++Scanned;
    UBlueprint *Blueprint = Cast<UBlueprint>(Data.GetAsset());
    const USimpleConstructionScript *SCS = Blueprint ? Blueprint->SimpleConstructionScript : nullptr;
    TArray<TSharedPtr<FJsonValue>> Components;
    for (const USCS_Node *Node : SCS ? SCS->GetAllNodes() : TArray<USCS_Node *>()) {
      if (Node && Node->ComponentClass && ComponentClassMatches(Node->ComponentClass, Filter)) {
        TSharedPtr<FJsonObject> Comp = McpHandlerUtils::CreateResultObject();
        Comp->SetStringField(TEXT("componentName"), Node->GetVariableName().ToString());
        Comp->SetStringField(TEXT("componentType"), Node->ComponentClass->GetName());
        Components.Add(MakeShared<FJsonValueObject>(Comp));
      }
    }
    if (Blueprint) {
      CollectInheritedComponents(Blueprint, Filter, Components);
    }
    if (Components.Num() > 0) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("blueprintPath"), Data.PackageName.ToString());
      Entry->SetArrayField(TEXT("components"), Components);
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("blueprints"), Matches);
  Result->SetNumberField(TEXT("blueprintCount"), Matches.Num());
  Result->SetNumberField(TEXT("scanned"), Scanned);
  Result->SetBoolField(TEXT("truncated"), bTruncated);
  Bridge.SendAutomationResponse(Socket, RequestId, true,
      FString::Printf(TEXT("%d of %d Blueprint(s) under %s have a matching component"), Matches.Num(), Scanned, *Norm.Path),
      Result, FString());
  return true;
}
} // namespace

bool HandleScsGet(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("get_scs"))) {
    FString Folder;
    FString Filter;
    FString RequestedBlueprint;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("path"), Folder);
      Payload->TryGetStringField(TEXT("componentClass"), Filter);
      Payload->TryGetStringField(TEXT("blueprintPath"), RequestedBlueprint);
    }
    if (!Folder.IsEmpty() && RequestedBlueprint.IsEmpty()) {
      return SendScsFolderScan(Bridge, RequestId, RequestingSocket, Folder, Filter);
    }
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("get_scs requires a valid blueprint"),
                             nullptr, TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;

    // Get SCS with explicit null check
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();
      for (USCS_Node *Node : AllNodes) {
        if (Node && Node->GetVariableName().IsValid() && ComponentClassMatches(Node->ComponentClass, Filter)) {
          TSharedPtr<FJsonObject> ComponentObj = McpHandlerUtils::CreateResultObject();
          ComponentObj->SetStringField(TEXT("componentName"),
                                       Node->GetVariableName().ToString());
          ComponentObj->SetStringField(TEXT("componentType"),
                                       Node->ComponentClass
                                           ? Node->ComponentClass->GetName()
                                           : TEXT("Unknown"));

          // Add parent info if available
          // USCS_Node doesn't have GetParent() - use
          // ParentComponentOrVariableName instead
          // SCS-owned parents are found through the tree; ParentComponentOrVariableName
          // only names inherited native parents (dogfood #193: reparented nodes showed no parent).
          if (USCS_Node *ParentNode = SCS->FindParentNode(Node)) {
            ComponentObj->SetStringField(TEXT("parentComponent"), ParentNode->GetVariableName().ToString());
          } else if (!Node->ParentComponentOrVariableName.IsNone()) {
            ComponentObj->SetStringField(TEXT("parentComponent"), Node->ParentComponentOrVariableName.ToString());
          }
          // A non-scene component (RotatingMovementComponent, a movement or
          // audio component) is a top-level SCS node but is not the Blueprint's
          // root: reporting isRoot for both made a pickup look like it had two
          // roots. Distinguish the two, and name an inherited parent.
          const bool bIsSceneComponent =
              Node->ComponentClass &&
              Node->ComponentClass->IsChildOf(USceneComponent::StaticClass());
          ComponentObj->SetBoolField(TEXT("isSceneComponent"), bIsSceneComponent);
          ComponentObj->SetBoolField(
              TEXT("isRoot"),
              bIsSceneComponent && SCS->GetRootNodes().Contains(Node));
          if (Node->bIsParentComponentNative &&
              !Node->ParentComponentOrVariableName.IsNone()) {
            ComponentObj->SetBoolField(TEXT("parentIsInherited"), true);
          }

          // Add transform
          // Get component transform from template
          FTransform Transform;
          if (UActorComponent *ComponentTemplate = Node->ComponentTemplate) {
            if (USceneComponent *SceneTemplate =
                    Cast<USceneComponent>(ComponentTemplate)) {
              Transform = SceneTemplate->GetRelativeTransform();
            }
          } else {
            Transform = FTransform::Identity;
          }
          TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();

          TransformObj->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Transform.GetLocation()));

          TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
          RotationObj->SetNumberField(TEXT("pitch"),
                                      Transform.GetRotation().Rotator().Pitch);
          RotationObj->SetNumberField(TEXT("yaw"),
                                      Transform.GetRotation().Rotator().Yaw);
          RotationObj->SetNumberField(TEXT("roll"),
                                      Transform.GetRotation().Rotator().Roll);
          TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

          TransformObj->SetObjectField(TEXT("scale"), McpHandlerUtils::VectorToJson(Transform.GetScale3D()));

          ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
          ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
        }
      }
    }

    // A Character Blueprint with no SCS nodes reported "Retrieved 0 SCS
    // components", which reads as "this Blueprint has no components" -- it has
    // several, inherited from its parent class, and they are the ones callers
    // need to name as an attach parent. List them alongside, marked inherited.
    TArray<TSharedPtr<FJsonValue>> InheritedArray;
    CollectInheritedComponents(Blueprint, Filter, InheritedArray);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("components"), ComponentsArray);
    Result->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Result->SetArrayField(TEXT("inheritedComponents"), InheritedArray);
    Result->SetNumberField(TEXT("inheritedComponentCount"), InheritedArray.Num());
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Retrieved %d SCS component(s) and %d inherited component(s)"),
                        ComponentsArray.Num(), InheritedArray.Num()),
        Result, FString());
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
