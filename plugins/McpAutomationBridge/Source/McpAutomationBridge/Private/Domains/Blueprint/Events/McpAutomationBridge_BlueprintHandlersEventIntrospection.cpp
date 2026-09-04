#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"

void McpAppendBlueprintScsComponents(class UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Snapshot); // Events/McpAutomationBridge_BlueprintHandlersSnapshotComponents.cpp
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
TArray<TSharedPtr<FJsonValue>>
FMcpAutomationBridge_CollectBlueprintFunctions(UBlueprint *Blueprint) {
  // Delegate to centralized McpBlueprintUtils
  return McpBlueprintUtils::CollectBlueprintFunctions(Blueprint);
}

void
FMcpAutomationBridge_CollectEventPins(UK2Node *Node,
                                      TArray<TSharedPtr<FJsonValue>> &Out) {
  if (!Node) {
    return;
  }

  if (UK2Node_CustomEvent *CustomEvent = Cast<UK2Node_CustomEvent>(Node)) {
    FMcpAutomationBridge_AppendPinsJson(CustomEvent->UserDefinedPins, Out);
  } else if (UK2Node_FunctionEntry *FunctionEntry =
                 Cast<UK2Node_FunctionEntry>(Node)) {
    FMcpAutomationBridge_AppendPinsJson(FunctionEntry->UserDefinedPins, Out);
  }
}

TArray<TSharedPtr<FJsonValue>>
FMcpAutomationBridge_CollectBlueprintEvents(UBlueprint *Blueprint) {
  TArray<TSharedPtr<FJsonValue>> Out;
  if (!Blueprint) {
    return Out;
  }

  auto AppendEvent = [&](const FString &EventName, const FString &EventType,
                         UK2Node *SourceNode) {
    TSharedPtr<FJsonObject> EventJson = McpHandlerUtils::CreateResultObject();
    EventJson->SetStringField(TEXT("name"), EventName);
    EventJson->SetStringField(TEXT("eventType"), EventType);

    TArray<TSharedPtr<FJsonValue>> Params;
    FMcpAutomationBridge_CollectEventPins(SourceNode, Params);
    if (Params.Num() > 0) {
      EventJson->SetArrayField(TEXT("parameters"), Params);
    }

    Out.Add(MakeShared<FJsonValueObject>(EventJson));
  };

  for (UEdGraph *Graph : Blueprint->UbergraphPages) {
    if (!Graph) {
      continue;
    }

    for (UEdGraphNode *Node : Graph->Nodes) {
      if (UK2Node_CustomEvent *CustomEvent = Cast<UK2Node_CustomEvent>(Node)) {
        AppendEvent(CustomEvent->CustomFunctionName.ToString(), TEXT("custom"),
                    CustomEvent);
      } else if (UK2Node_Event *K2Event = Cast<UK2Node_Event>(Node)) {
        AppendEvent(K2Event->GetFunctionName().ToString(),
                    K2Event->GetClass()->GetName(), K2Event);
      }
    }
  }

  return Out;
}

TSharedPtr<FJsonObject>
FMcpAutomationBridge_FindNamedEntry(const TArray<TSharedPtr<FJsonValue>> &Array,
                                    const FString &FieldName,
                                    const FString &DesiredValue) {
  for (const TSharedPtr<FJsonValue> &Value : Array) {
    if (!Value.IsValid() || Value->Type != EJson::Object) {
      continue;
    }

    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
    if (!Obj.IsValid()) {
      continue;
    }

    FString FieldValue;
    if (Obj->TryGetStringField(FieldName, FieldValue) &&
        FieldValue.Equals(DesiredValue, ESearchCase::IgnoreCase)) {
      return Obj;
    }
  }
  return nullptr;
}

namespace {
// Blueprint kind as a stable lowercase string: the declared type first, then a
// Widget Blueprint detected through its generated class. Every accessor is
// null-checked so a half-constructed asset answers "unknown" instead of crashing.
FString FMcpAutomationBridge_DescribeBlueprintType(UBlueprint *Blueprint) {
  if (!Blueprint) {
    return TEXT("unknown");
  }
  switch (Blueprint->BlueprintType) {
    case BPTYPE_FunctionLibrary:
      return TEXT("functionLibrary");
    case BPTYPE_Interface:
      return TEXT("interface");
    case BPTYPE_MacroLibrary:
      return TEXT("macroLibrary");
    case BPTYPE_LevelScript:
      return TEXT("levelScript");
    case BPTYPE_Const:
      return TEXT("const");
    default:
      break;
  }
  if (Blueprint->GeneratedClass &&
      Blueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass())) {
    return TEXT("widget");
  }
  return TEXT("normal");
}
} // namespace

TSharedPtr<FJsonObject>
FMcpAutomationBridge_EnsureBlueprintEntry(const FString &Key) {
  if (TSharedPtr<FJsonObject> *Existing = GBlueprintRegistry.Find(Key)) {
    if (Existing->IsValid()) {
      return *Existing;
    }
  }

  TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
  Entry->SetStringField(TEXT("blueprintPath"), Key);
  Entry->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>());
  Entry->SetArrayField(TEXT("functions"), TArray<TSharedPtr<FJsonValue>>());
  Entry->SetArrayField(TEXT("events"), TArray<TSharedPtr<FJsonValue>>());
  Entry->SetObjectField(TEXT("defaults"), McpHandlerUtils::CreateResultObject());
  Entry->SetObjectField(TEXT("metadata"), McpHandlerUtils::CreateResultObject());
  GBlueprintRegistry.Add(Key, Entry);
  return Entry;
}

TSharedPtr<FJsonObject>
FMcpAutomationBridge_BuildBlueprintSnapshot(UBlueprint *Blueprint,
                                            const FString &NormalizedPath) {
  if (!Blueprint) {
    return McpHandlerUtils::CreateResultObject();
  }

  TSharedPtr<FJsonObject> Snapshot = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> Variables =
      FMcpAutomationBridge_CollectBlueprintVariables(Blueprint);
  TSharedPtr<FJsonObject> Defaults =
      FMcpAutomationBridge_CollectBlueprintDefaults(Blueprint, Variables);
  Snapshot->SetStringField(TEXT("blueprintPath"), NormalizedPath);
  Snapshot->SetStringField(TEXT("resolvedPath"), NormalizedPath);
  Snapshot->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
  Snapshot->SetStringField(TEXT("parentClass"),
                           Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());

  // Blueprint identity and compile health, all cheap reads. Each field is
  // defensive: a half-created asset answers with what it has instead of
  // dereferencing a null class, script, or graph.
  Snapshot->SetStringField(
      TEXT("generatedClass"),
      Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName()
                                : FString());
  Snapshot->SetStringField(TEXT("blueprintType"),
                           FMcpAutomationBridge_DescribeBlueprintType(Blueprint));
  Snapshot->SetNumberField(
      TEXT("scsNodeCount"),
      Blueprint->SimpleConstructionScript
          ? Blueprint->SimpleConstructionScript->GetAllNodes().Num()
          : 0);
  McpAppendBlueprintScsComponents(Blueprint, Snapshot); // dogfood #22: components beside scsNodeCount
  FString CompileStatus = TEXT("unknown");
  if (const UEnum *StatusEnum = StaticEnum<EBlueprintStatus>()) {
    const FString StatusName =
        StatusEnum->GetNameStringByValue(static_cast<int64>(Blueprint->Status));
    if (!StatusName.IsEmpty()) {
      CompileStatus = StatusName;
    }
  }
  Snapshot->SetStringField(TEXT("compileStatus"), CompileStatus);
  Snapshot->SetBoolField(TEXT("hasCompileErrors"),
                         Blueprint->Status == BS_Error);

  // Graph census: function and ubergraph (event) graph names plus node counts,
  // so a caller can size a Blueprint without walking its graphs itself.
  TArray<TSharedPtr<FJsonValue>> FunctionGraphs;
  for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
    if (!Graph) {
      continue;
    }
    TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();
    GraphObj->SetStringField(TEXT("name"), Graph->GetName());
    GraphObj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
    FunctionGraphs.Add(MakeShared<FJsonValueObject>(GraphObj));
  }
  Snapshot->SetArrayField(TEXT("functionGraphs"), FunctionGraphs);

  TArray<TSharedPtr<FJsonValue>> EventGraphs;
  for (UEdGraph *Graph : Blueprint->UbergraphPages) {
    if (!Graph) {
      continue;
    }
    TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();
    GraphObj->SetStringField(TEXT("name"), Graph->GetName());
    GraphObj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
    EventGraphs.Add(MakeShared<FJsonValueObject>(GraphObj));
  }
  Snapshot->SetArrayField(TEXT("eventGraphs"), EventGraphs);

  Snapshot->SetArrayField(TEXT("variables"), Variables);
  Snapshot->SetArrayField(
      TEXT("functions"),
      FMcpAutomationBridge_CollectBlueprintFunctions(Blueprint));
  Snapshot->SetArrayField(
      TEXT("events"), FMcpAutomationBridge_CollectBlueprintEvents(Blueprint));
  Snapshot->SetObjectField(TEXT("defaults"), Defaults);

  // Aggregate metadata by variable for compatibility with legacy responses.
  TSharedPtr<FJsonObject> MetadataRoot = McpHandlerUtils::CreateResultObject();
  for (const TSharedPtr<FJsonValue> &VariableValue : Variables) {
    if (!VariableValue.IsValid() || VariableValue->Type != EJson::Object) {
      continue;
    }
    const TSharedPtr<FJsonObject> VariableObj = VariableValue->AsObject();
    if (!VariableObj.IsValid() || !VariableObj->HasField(TEXT("metadata"))) {
      continue;
    }

    FString VariableName;
    const TSharedPtr<FJsonObject> MetaJson = VariableObj->GetObjectField(TEXT("metadata"));
    if (VariableObj->TryGetStringField(TEXT("name"), VariableName) &&
        !VariableName.IsEmpty() && MetaJson.IsValid()) {
      MetadataRoot->SetObjectField(VariableName, MetaJson);
    }
  }
  if (MetadataRoot->Values.Num() > 0) {
    Snapshot->SetObjectField(TEXT("metadata"), MetadataRoot);
  }
  return Snapshot;
}
#endif
} // namespace McpBlueprintHandlers
