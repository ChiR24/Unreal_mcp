#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
FString
FMcpAutomationBridge_JsonValueToString(const TSharedPtr<FJsonValue> &Value) {
  // Delegate to centralized McpHandlerUtils
  return McpHandlerUtils::JsonValueToString(Value);
}

FName FMcpAutomationBridge_ResolveMetadataKey(const FString &RawKey) {
  if (RawKey.Equals(TEXT("displayname"), ESearchCase::IgnoreCase)) {
    return FName(TEXT("DisplayName"));
  }
  if (RawKey.Equals(TEXT("tooltip"), ESearchCase::IgnoreCase)) {
    return FName(TEXT("ToolTip"));
  }
  return FName(*RawKey);
}

#if MCP_HAS_EDGRAPH_SCHEMA_K2
void
FMcpAutomationBridge_AddUserDefinedPin(UK2Node *Node, const FString &PinName,
                                       const FString &PinType,
                                       EEdGraphPinDirection Direction) {
  if (!Node) {
    return;
  }

  const FString CleanName = PinName.TrimStartAndEnd();
  if (CleanName.IsEmpty()) {
    return;
  }

  const FEdGraphPinType PinTypeDesc = FMcpAutomationBridge_MakePinType(PinType);
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

UFunction *
FMcpAutomationBridge_ResolveFunction(UBlueprint *Blueprint,
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

  return nullptr;
}

FProperty *
FMcpAutomationBridge_FindProperty(UBlueprint *Blueprint,
                                  const FString &PropertyName) {
  if (!Blueprint || PropertyName.TrimStartAndEnd().IsEmpty()) {
    return nullptr;
  }

  const FName PropFName(*PropertyName.TrimStartAndEnd());
  const TArray<UClass *> CandidateClasses = {Blueprint->GeneratedClass,
                                             Blueprint->SkeletonGeneratedClass,
                                             Blueprint->ParentClass};

  for (UClass *Candidate : CandidateClasses) {
    if (!Candidate) {
      continue;
    }

    if (FProperty *Found = Candidate->FindPropertyByName(PropFName)) {
      return Found;
    }
  }

  // SCS component properties are stored with "_GEN_VARIABLE" suffix
  const FString GenVariableName = PropertyName + TEXT("_GEN_VARIABLE");
  const FName GenVarFName(*GenVariableName);
  for (UClass *Candidate : CandidateClasses) {
    if (!Candidate) {
      continue;
    }
    if (FProperty *Found = Candidate->FindPropertyByName(GenVarFName)) {
      return Found;
    }
  }

  return nullptr;
}
#endif // MCP_HAS_EDGRAPH_SCHEMA_K2
#endif
} // namespace McpBlueprintHandlers
