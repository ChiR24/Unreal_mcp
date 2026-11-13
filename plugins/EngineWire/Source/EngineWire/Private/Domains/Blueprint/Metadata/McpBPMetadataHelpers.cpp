#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
FName FEngineWire_ResolveMetadataKey(const FString &RawKey) {
  if (RawKey.Equals(TEXT("displayname"), ESearchCase::IgnoreCase)) {
    return FName(TEXT("DisplayName"));
  }
  if (RawKey.Equals(TEXT("tooltip"), ESearchCase::IgnoreCase)) {
    return FName(TEXT("ToolTip"));
  }
  return FName(*RawKey);
}

void
FEngineWire_AddUserDefinedPin(UK2Node *Node, const FString &PinName,
                                       const FString &PinType,
                                       EEdGraphPinDirection Direction) {
  if (!Node) {
    return;
  }

  const FString CleanName = PinName.TrimStartAndEnd();
  if (CleanName.IsEmpty()) {
    return;
  }

  const FEdGraphPinType PinTypeDesc = McpBlueprintUtils::MakePinType(PinType);
  const FName PinFName(*CleanName);

  if (UK2Node_FunctionEntry *EntryNode = Cast<UK2Node_FunctionEntry>(Node)) {
    EntryNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
  } else if (UK2Node_FunctionResult *ResultNode =
                 Cast<UK2Node_FunctionResult>(Node)) {
    ResultNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
  } else if (UK2Node_CustomEvent *CustomEventNode =
                 Cast<UK2Node_CustomEvent>(Node)) {
    CustomEventNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
  }
}

void FEngineWire_AddUserPins(UK2Node_EditablePinBase *Node, const TArray<TSharedPtr<FJsonValue>> &Params,
                             EEdGraphPinDirection Direction, bool bSkipExisting) {
  for (const TSharedPtr<FJsonValue> &Value : Params) {
    if (!Value.IsValid() || Value->Type != EJson::Object)
      continue;
    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
    if (!Obj.IsValid())
      continue;
    FString ParamName;
    Obj->TryGetStringField(TEXT("name"), ParamName);
    FString ParamType;
    Obj->TryGetStringField(TEXT("type"), ParamType);
    const FName PinName(*ParamName.TrimStartAndEnd());
    if (bSkipExisting && Node && Node->UserDefinedPins.ContainsByPredicate(
            [&PinName](const TSharedPtr<FUserPinInfo> &Pin) { return Pin.IsValid() && Pin->PinName == PinName; })) {
      continue;
    }
    FEngineWire_AddUserDefinedPin(Node, ParamName, ParamType, Direction);
  }
}

UFunction *
FEngineWire_ResolveFunction(UBlueprint *Blueprint,
                                     const FString &FunctionName) {
  if (!Blueprint || FunctionName.TrimStartAndEnd().IsEmpty()) {
    return nullptr;
  }

  const FString CleanFunc = FunctionName.TrimStartAndEnd();

  UFunction *Found = FindObject<UFunction>(nullptr, *CleanFunc);
  if (Found) {
    return Found;
  }

  const FName FuncFName(*CleanFunc);
  const TArray<UClass *> CandidateClasses = {Blueprint->GeneratedClass,
                                             Blueprint->SkeletonGeneratedClass,
                                             Blueprint->ParentClass};

  for (UClass *Candidate : CandidateClasses) {
    if (Candidate) {
      UFunction *CandidateFunc = Candidate->FindFunctionByName(FuncFName);
      if (CandidateFunc) {
        return CandidateFunc;
      }
    }
  }

  int32 DotIndex = INDEX_NONE;
  if (CleanFunc.FindChar('.', DotIndex)) {
    const FString ClassPath = CleanFunc.Left(DotIndex);
    const FString FuncSegment = CleanFunc.Mid(DotIndex + 1);
    if (!ClassPath.IsEmpty() && !FuncSegment.IsEmpty()) {
      if (UClass *ExplicitClass = FindObject<UClass>(nullptr, *ClassPath)) {
        UFunction *ExplicitFunc =
            ExplicitClass->FindFunctionByName(FName(*FuncSegment));
        if (ExplicitFunc) {
          return ExplicitFunc;
        }
      }
    }
  }

  // Callers pass bare K2 function names ("PrintString",
  // "GetPlayerPawn", "ApplyDamage", ...). Those live in Blueprint function
  // libraries, not on the Blueprint's own classes, so the checks above miss them
  // and the node was created unbound — compiling as
  // 'Could not find a function named "None"'. Probe the common libraries here so
  // the generic add_node path binds real gameplay functions.
  static const TCHAR *const BlueprintFunctionLibraries[] = {
      TEXT("/Script/Engine.KismetSystemLibrary"),
      TEXT("/Script/Engine.KismetMathLibrary"),
      TEXT("/Script/Engine.KismetStringLibrary"),
      TEXT("/Script/Engine.KismetTextLibrary"),
      TEXT("/Script/Engine.KismetArrayLibrary"),
      TEXT("/Script/Engine.KismetInputLibrary"),
      TEXT("/Script/Engine.KismetMaterialLibrary"),
      TEXT("/Script/Engine.KismetRenderingLibrary"),
      TEXT("/Script/Engine.GameplayStatics"),
      TEXT("/Script/Engine.Actor"),
      TEXT("/Script/Engine.Pawn"),
      TEXT("/Script/Engine.Character"),
      TEXT("/Script/Engine.Controller"),
      TEXT("/Script/Engine.PlayerController"),
      TEXT("/Script/Engine.LevelScriptActor"),
      TEXT("/Script/AIModule.AIBlueprintHelperLibrary"),
      TEXT("/Script/NavigationSystem.NavigationSystemV1"),
      TEXT("/Script/UMG.WidgetBlueprintLibrary"),
      TEXT("/Script/Niagara.NiagaraFunctionLibrary"),
  };
  for (const TCHAR *LibraryPath : BlueprintFunctionLibraries) {
    if (UClass *Library = FindObject<UClass>(nullptr, LibraryPath)) {
      if (UFunction *LibraryFunc = Library->FindFunctionByName(FuncFName)) {
        return LibraryFunc;
      }
    }
  }

  return nullptr;
}

} // namespace McpBlueprintHandlers
