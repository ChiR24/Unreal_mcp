#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersScsLookup.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersSubobjectTraits.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsTemplateAssets.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsParentResolve.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropagate.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropertyBag.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
namespace {
void ApplyModifyScsModifyComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
const TSharedPtr<FJsonValue> TransformVal = Op->TryGetField(TEXT("transform"));
const TSharedPtr<FJsonObject> TransformObj = TransformVal.IsValid() && TransformVal->Type == EJson::Object ? TransformVal->AsObject() : nullptr;
const TSharedPtr<FJsonValue> PropertiesVal = Op->TryGetField(TEXT("properties"));
const TSharedPtr<FJsonObject> PropertiesObj = PropertiesVal.IsValid() && PropertiesVal->Type == EJson::Object ? PropertiesVal->AsObject() : nullptr;
if (ComponentName.IsEmpty()) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"), TEXT("Missing component name"));
  return;
}
USCS_Node *Node = FindScsNodeByName(LocalSCS, ComponentName);
// An inherited native component (ACharacter's Mesh, CapsuleComponent, ...) has
// no USCS_Node, so the SCS-only lookup reported "Component not found" for a
// component the Blueprint plainly has. set_scs_property has resolved those
// through the CDO for a while; the batch path had not.
UActorComponent *Template = Node ? Node->ComponentTemplate : nullptr;
if (!Template) { Template = McpScsParent::FindInheritedComponent(LocalBP, ComponentName); }
if (!Template) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), FString::Printf(
      TEXT("'%s' is neither a component of this Blueprint nor one it inherits; call get_scs to list what it has."), *ComponentName));
  return;
}
bool bAnySuccess = false;
McpScsPropagate::FDefaults Defaults{Template, LocalBP, FName(*ComponentName)};
if (TransformObj.IsValid() &&
    Template->IsA<USceneComponent>()) {
  for (const TCHAR *Path : {TEXT("RelativeLocation"), TEXT("RelativeRotation"), TEXT("RelativeScale3D")}) Defaults.Capture(Path);
  USceneComponent *SceneTemplate =
      Cast<USceneComponent>(Template);
  FVector Location = SceneTemplate->GetRelativeLocation();
  FRotator Rotation = SceneTemplate->GetRelativeRotation();
  FVector Scale = SceneTemplate->GetRelativeScale3D();
  ReadVectorField(TransformObj, TEXT("location"), Location, Location);
  ReadRotatorField(TransformObj, TEXT("rotation"), Rotation,
                   Rotation);
  ReadVectorField(TransformObj, TEXT("scale"), Scale, Scale);
  SceneTemplate->SetRelativeLocation(Location);
  SceneTemplate->SetRelativeRotation(Rotation);
  SceneTemplate->SetRelativeScale3D(Scale);
  bAnySuccess = true;
}
if (PropertiesObj.IsValid()) {
  const TArray<FString> Rejected = McpScsPropertyBag::Apply(Template, PropertiesObj, Defaults, bAnySuccess);
  if (Rejected.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FString &Entry : Rejected) Values.Add(MakeShared<FJsonValueString>(Entry));
    OpSummary->SetArrayField(TEXT("rejectedProperties"), Values);
  }
}
for (const TCHAR *Path : {TEXT("StaticMesh"), TEXT("OverrideMaterials")}) Defaults.Capture(Path);
bAnySuccess = ApplyScsTemplateAssets(Template, Op) || bAnySuccess;
McpScsPropagate::PropagateAndReport(Defaults, OpSummary);
McpScsPropagate::Pending().Emplace(Template, Defaults);
OpSummary->SetBoolField(TEXT("success"), bAnySuccess);
OpSummary->SetStringField(TEXT("componentName"), ComponentName);
if (!bAnySuccess) {
  OpSummary->SetStringField(TEXT("warning"), TEXT("No transform or properties applied"));
}
}

void ApplyModifyScsAddComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
FString ComponentClassPath;
Op->TryGetStringField(TEXT("componentClass"), ComponentClassPath);
FString AttachToName;
Op->TryGetStringField(TEXT("attachTo"), AttachToName);
// Each add compiles the Blueprint, which replaces its SCS - so by op 2 the
// pointer handed in at the start of the batch is stale and every lookup misses.
if (LocalBP && LocalBP->SimpleConstructionScript) {
  LocalSCS = LocalBP->SimpleConstructionScript;
}

// UE 5.7 FIX: Use ResolveClassByName to handle short class names like "StaticMeshComponent"
// FSoftClassPath triggers ensure failure when given short package names in UE 5.7+
// ResolveClassByName handles short name resolution via prefix guessing and TObjectIterator
UClass *ComponentClass = ResolveClassByName(ComponentClassPath);

// Fallback: Only use FSoftClassPath if it looks like a full path (contains /)
if (!ComponentClass && ComponentClassPath.Contains(TEXT("/"))) {
  FSoftClassPath ComponentClassSoftPath(ComponentClassPath);
  ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
}
if (!ComponentClass) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), TEXT("Component class not found"));
} else {
  USCS_Node *ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
  if (ExistingNode) {
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
    OpSummary->SetStringField(TEXT("warning"),
                              TEXT("Component already exists"));
    // Re-running a prefab definition is how a caller corrects one, so an
    // existing node takes this call's transform and properties instead of
    // being skipped outright.
    if (Op->HasField(TEXT("transform")) || Op->HasField(TEXT("properties"))) {
      ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Op, OpSummary);
    }
  } else {
    bool bAddedViaSubsystem = false;
    FString AdditionMethodStr;
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
    USubobjectDataSubsystem *Subsystem = nullptr;
    if (GEngine)
      Subsystem =
          GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
    if (Subsystem) {
      TArray<FSubobjectDataHandle> ExistingHandles;
      Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP,
                                                    ExistingHandles);
      FSubobjectDataHandle ParentHandle;
      if (ExistingHandles.Num() > 0) {
        bool bFoundParentByName = false;
        if (!AttachToName.TrimStartAndEnd().IsEmpty()) {
          const UScriptStruct *HandleStruct =
              FSubobjectDataHandle::StaticStruct();
          for (const FSubobjectDataHandle &H : ExistingHandles) {
            if (!HandleStruct)
              continue;
            FString HText;
            HandleStruct->ExportText(HText, &H, nullptr, nullptr,
                                     PPF_None, nullptr);
            if (HText.Contains(AttachToName, ESearchCase::IgnoreCase)) {
              ParentHandle = H;
              bFoundParentByName = true;
              break;
            }
          }
        }
        if (!bFoundParentByName)
          ParentHandle = ExistingHandles[0];
      }

      using namespace McpAutomationBridge;
      constexpr bool bHasK2Add =
          THasK2Add<USubobjectDataSubsystem>::value;
      constexpr bool bHasAdd = THasAdd<USubobjectDataSubsystem>::value;
      constexpr bool bHasAddTwoArg =
          THasAddTwoArg<USubobjectDataSubsystem>::value;
      constexpr bool bHandleHasIsValid =
          THandleHasIsValid<FSubobjectDataHandle>::value;
      constexpr bool bHasRename =
          THasRename<USubobjectDataSubsystem>::value;

      bool bTriedNative = false;
      FSubobjectDataHandle NewHandle;
      if constexpr (bHasAddTwoArg) {
        FAddNewSubobjectParams Params;
        Params.ParentHandle = ParentHandle;
        Params.NewClass = ComponentClass;
        Params.BlueprintContext = LocalBP;
        FText FailReason;
        NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
        bTriedNative = true;
        AdditionMethodStr = TEXT(
            "SubobjectDataSubsystem.AddNewSubobject(WithFailReason)");

        bool bHandleValid = true;
        if constexpr (bHandleHasIsValid) {
          bHandleValid = NewHandle.IsValid();
        }
        if (bHandleValid) {
          if constexpr (bHasRename) {
            // Generate unique name if target already exists
            FString UniqueName = ComponentName;
            FName TargetVarName = FName(*UniqueName);

            // Check if variable already exists in blueprint
            if (LocalBP->GeneratedClass) {
              // Check for existing member variable with same name
              bool bNameExists = false;
              for (TFieldIterator<FProperty> It(LocalBP->GeneratedClass); It; ++It) {
                if (It->GetFName() == TargetVarName) {
                  bNameExists = true;
                  break;
                }
              }

              // Also check the _GEN_VARIABLE suffix naming
              FString GenVarName = UniqueName + TEXT("_GEN_VARIABLE");
              FName GenVarFName = FName(*GenVarName);
              for (TFieldIterator<FProperty> It(LocalBP->GeneratedClass); It; ++It) {
                if (It->GetFName() == GenVarFName) {
                  bNameExists = true;
                  break;
                }
              }

              if (bNameExists) {
                // Generate unique name by appending number
                int32 Suffix = 1;
                while (Suffix < 1000) {
                  UniqueName = FString::Printf(TEXT("%s_%d"), *ComponentName, Suffix);
                  TargetVarName = FName(*UniqueName);

                  bNameExists = false;
                  for (TFieldIterator<FProperty> It(LocalBP->GeneratedClass); It; ++It) {
                    if (It->GetFName() == TargetVarName) {
                      bNameExists = true;
                      break;
                    }
                  }

                  if (!bNameExists) break;
                  Suffix++;
                }

                OpSummary->SetStringField(TEXT("originalName"), ComponentName);
                OpSummary->SetStringField(TEXT("renamedTo"), UniqueName);
              }
            }

            Subsystem->RenameSubobjectMemberVariable(
                LocalBP, NewHandle, TargetVarName);
          }
#if WITH_EDITOR
          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
              LocalBP);
          McpSafeCompileBlueprint(LocalBP);
          SaveLoadedAssetThrottled(LocalBP);
#endif
          bAddedViaSubsystem = true;
        }
      }
    }
#endif
    if (bAddedViaSubsystem) {
      OpSummary->SetBoolField(TEXT("success"), true);
      OpSummary->SetStringField(TEXT("componentName"), ComponentName);
      // The parent search above matches AttachToName against the EXPORTED TEXT
      // of an FSubobjectDataHandle, which is an opaque id and never contains a
      // component name -- so it always fell through to ExistingHandles[0], the
      // root. Re-attach by name now that the node exists, and report it.
      McpScsParent::AttachAndReport(LocalBP, LocalBP->SimpleConstructionScript,
                                    ComponentName, AttachToName, OpSummary);
      if (!AdditionMethodStr.IsEmpty())
        OpSummary->SetStringField(TEXT("additionMethod"), AdditionMethodStr);
      // The contract says add_component also takes `transform` and a
      // `properties` bag, but this only ever created the node: in a batch every
      // placement and every mesh assignment was accepted, reported success, and
      // silently dropped, so a prefab built this way came out as a pile of empty
      // components at the origin. Reuse the modify path on the node just added.
      if (Op->HasField(TEXT("transform")) || Op->HasField(TEXT("properties"))) {
        TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>(*Op);
        FString RenamedTo;
        if (OpSummary->TryGetStringField(TEXT("renamedTo"), RenamedTo))
          Applied->SetStringField(TEXT("componentName"), RenamedTo);
        ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Applied, OpSummary);
      }
    } else {
      USCS_Node *NewNode =
          LocalSCS->CreateNode(ComponentClass, *ComponentName);
      if (NewNode) {
        LocalSCS->AddNode(NewNode);
        OpSummary->SetBoolField(TEXT("success"), true);
        OpSummary->SetStringField(TEXT("componentName"), ComponentName);
        McpScsParent::AttachAndReport(LocalBP, LocalSCS, ComponentName,
                                      AttachToName, OpSummary);
      } else {
        OpSummary->SetBoolField(TEXT("success"), false);
        OpSummary->SetStringField(TEXT("warning"),
                                  TEXT("Failed to create SCS node"));
      }
    }
  }
}
}
} // namespace

void ApplyModifyScsComponentOperation(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const FString &NormalizedType, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
  if (NormalizedType == TEXT("modify_component")) {
    ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else if (NormalizedType == TEXT("add_component")) {
    ApplyModifyScsAddComponent(LocalBP, LocalSCS, Op, OpSummary);
  }
}
#endif
} // namespace McpBlueprintHandlers
