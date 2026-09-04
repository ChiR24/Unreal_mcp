// McpAutomationBridge_BlueprintHandlersSnapshotComponents.cpp — SCS component list for the
// blueprint snapshot (manage_blueprint.get_blueprint). Dogfood #22: the snapshot reported only
// scsNodeCount; clients also need the components themselves (name, class, parent).
#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

void McpAppendBlueprintScsComponents(UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Snapshot)
{
  if (!Blueprint || !Snapshot.IsValid())
  {
    return;
  }
  TArray<TSharedPtr<FJsonValue>> Components;
  if (USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript)
  {
    TMap<const USCS_Node *, FString> ParentNames;
    for (USCS_Node *Node : SCS->GetAllNodes())
    {
      if (!Node)
      {
        continue;
      }
      for (USCS_Node *Child : Node->GetChildNodes())
      {
        if (Child)
        {
          ParentNames.Add(Child, Node->GetVariableName().ToString());
        }
      }
    }
    for (USCS_Node *Node : SCS->GetAllNodes())
    {
      if (!Node)
      {
        continue;
      }
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
      Entry->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT(""));
      Entry->SetStringField(TEXT("classPath"), Node->ComponentClass ? Node->ComponentClass->GetPathName() : TEXT(""));
      const FString *ParentName = ParentNames.Find(Node);
      Entry->SetStringField(TEXT("parent"), ParentName ? *ParentName : Node->ParentComponentOrVariableName.ToString());
      Entry->SetBoolField(TEXT("isRoot"), SCS->GetRootNodes().Contains(Node));
      Entry->SetStringField(TEXT("source"), TEXT("SCS"));
      Components.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }
  Snapshot->SetArrayField(TEXT("components"), Components);
}
