#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersClassResolution.h"
#include "McpAutomationBridgeHelpersJsonFields.h"
#include "McpAutomationBridgeHelpersScsLookup.h"
#include "McpAutomationBridge_BlueprintHandlersSubobjectTraits.h"

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
void ApplyModifyScsModifyComponent(USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
const TSharedPtr<FJsonValue> TransformVal =
    Op->TryGetField(TEXT("transform"));
const TSharedPtr<FJsonObject> TransformObj =
    TransformVal.IsValid() && TransformVal->Type == EJson::Object
        ? TransformVal->AsObject()
        : nullptr;
if (!ComponentName.IsEmpty() && TransformObj.IsValid()) {
  USCS_Node *Node = FindScsNodeByName(LocalSCS, ComponentName);
  if (Node && Node->ComponentTemplate &&
      Node->ComponentTemplate->IsA<USceneComponent>()) {
    USceneComponent *SceneTemplate =
        Cast<USceneComponent>(Node->ComponentTemplate);
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
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(
        TEXT("warning"),
        TEXT("Component not found or template missing"));
  }
} else {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"), TEXT("Missing component name or transform"));
}
}

void ApplyModifyScsAddComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
FString ComponentClassPath;
Op->TryGetStringField(TEXT("componentClass"), ComponentClassPath);
FString AttachToName;
Op->TryGetStringField(TEXT("attachTo"), AttachToName);

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
  OpSummary->SetStringField(TEXT("warning"),
                            TEXT("Component class not found"));
} else {
  USCS_Node *ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
  if (ExistingNode) {
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
    OpSummary->SetStringField(TEXT("warning"),
                              TEXT("Component already exists"));
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
      if (!AdditionMethodStr.IsEmpty())
        OpSummary->SetStringField(TEXT("additionMethod"),
                                  AdditionMethodStr);
    } else {
      USCS_Node *NewNode =
          LocalSCS->CreateNode(ComponentClass, *ComponentName);
      if (NewNode) {
        if (!AttachToName.TrimStartAndEnd().IsEmpty()) {
          if (USCS_Node *ParentNode =
                  FindScsNodeByName(LocalSCS, AttachToName)) {
            ParentNode->AddChildNode(NewNode);
          } else {
            LocalSCS->AddNode(NewNode);
          }
        } else {
          LocalSCS->AddNode(NewNode);
        }
        OpSummary->SetBoolField(TEXT("success"), true);
        OpSummary->SetStringField(TEXT("componentName"), ComponentName);
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

static void ApplyModifyScsComponentOperation(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const FString &NormalizedType, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
  if (NormalizedType == TEXT("modify_component")) {
    ApplyModifyScsModifyComponent(LocalSCS, Op, OpSummary);
  } else if (NormalizedType == TEXT("add_component")) {
    ApplyModifyScsAddComponent(LocalBP, LocalSCS, Op, OpSummary);
  }
}
#endif
} // namespace McpBlueprintHandlers
