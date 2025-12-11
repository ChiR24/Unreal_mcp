#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#endif

/**
 * Handles automation requests for Blueprint graph editing (node creation,
 * connection, etc.).
 *
 * @param RequestId Unique request identifier.
 * @param Action Action name (must be 'manage_blueprint_graph').
 * @param Payload JSON payload containing 'blueprintPath', 'graphName',
 * 'subAction', etc.
 * @param RequestingSocket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (Action != TEXT("manage_blueprint_graph")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing payload for blueprint graph action."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    // Allow callers to use "blueprintPath" (as exposed by the consolidated
    // tool schema) as an alias for assetPath so tests and tools do not need
    // to duplicate the same value under two keys.
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (!BlueprintPath.IsEmpty()) {
      AssetPath = BlueprintPath;
    }
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Missing 'assetPath' or 'blueprintPath' in payload."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UBlueprint *Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
  if (!Blueprint) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not load blueprint at path: %s"),
                        *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString GraphName;
  Payload->TryGetStringField(TEXT("graphName"), GraphName);
  UEdGraph *TargetGraph = nullptr;

  // Find the target graph
  if (GraphName.IsEmpty() ||
      GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase)) {
    // Default to the main ubergraph/event graph
    if (Blueprint->UbergraphPages.Num() > 0) {
      TargetGraph = Blueprint->UbergraphPages[0];
    }
  } else {
    // Search in FunctionGraphs and UbergraphPages
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
    if (!TargetGraph) {
      for (UEdGraph *Graph : Blueprint->UbergraphPages) {
        if (Graph->GetName() == GraphName) {
          TargetGraph = Graph;
          break;
        }
      }
    }
  }

  if (!TargetGraph) {
    // Fallback: try finding by name in all graphs
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph *Graph : AllGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
  }

  if (!TargetGraph) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not find graph '%s' in blueprint."),
                        *GraphName),
        TEXT("GRAPH_NOT_FOUND"));
    return true;
  }

  const FString SubAction = Payload->GetStringField(TEXT("subAction"));

  if (SubAction == TEXT("create_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // Helper to finalize and report
    auto FinalizeAndReport = [&](auto &NodeCreator, UEdGraphNode *NewNode) {
      if (NewNode) {
        // Set position BEFORE finalization per FGraphNodeCreator pattern
        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;

        // Finalize() calls CreateNewGuid(), PostPlacedNewNode(), and
        // AllocateDefaultPins() if pins are empty. Do NOT call
        // AllocateDefaultPins() again after this!
        NodeCreator.Finalize();

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node created."), Result);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Failed to create node (unsupported type or internal error)."),
            TEXT("CREATE_FAILED"));
      }
    };

    // Map common Blueprint node names to their CallFunction equivalents
    // This allows users to use nodeType="PrintString" instead of CallFunction
    static TMap<FString, TTuple<FString, FString>> CommonFunctionNodes = {
        {TEXT("PrintString"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("Print"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("PrintText"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintText"))},
        {TEXT("SetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorLocation"))},
        {TEXT("GetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorLocation"))},
        {TEXT("SetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorRotation"))},
        {TEXT("GetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorRotation"))},
        {TEXT("SetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorTransform"))},
        {TEXT("GetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorTransform"))},
        {TEXT("AddActorLocalOffset"),
         MakeTuple(TEXT("AActor"), TEXT("K2_AddActorLocalOffset"))},
        {TEXT("Delay"), MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("Delay"))},
        {TEXT("DestroyActor"),
         MakeTuple(TEXT("AActor"), TEXT("K2_DestroyActor"))},
        {TEXT("SpawnActor"),
         MakeTuple(TEXT("UGameplayStatics"),
                   TEXT("BeginDeferredActorSpawnFromClass"))},
        {TEXT("GetPlayerPawn"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerPawn"))},
        {TEXT("GetPlayerController"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerController"))},
        {TEXT("PlaySound"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySound2D"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySoundAtLocation"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySoundAtLocation"))},
        {TEXT("GetWorldDeltaSeconds"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetWorldDeltaSeconds"))},
        {TEXT("SetTimerByFunctionName"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_SetTimer"))},
        {TEXT("ClearTimer"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_ClearTimer"))},
        {TEXT("IsValid"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValid"))},
        {TEXT("IsValidClass"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValidClass"))},
    };

    // Check if this is a common function node shortcut
    if (const auto *FuncInfo = CommonFunctionNodes.Find(NodeType)) {
      FString ClassName = FuncInfo->Get<0>();
      FString FuncName = FuncInfo->Get<1>();

      // Find the class and function BEFORE creating NodeCreator
      // (FGraphNodeCreator asserts in destructor if not finalized)
      UClass *Class = nullptr;
      if (ClassName == TEXT("UKismetSystemLibrary")) {
        Class = UKismetSystemLibrary::StaticClass();
      } else if (ClassName == TEXT("UGameplayStatics")) {
        Class = UGameplayStatics::StaticClass();
      } else if (ClassName == TEXT("AActor")) {
        Class = AActor::StaticClass();
      } else {
        // Try to find by full path
        Class = FindObject<UClass>(
            nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
      }

      UFunction *Func = nullptr;
      if (Class) {
        Func = Class->FindFunctionByName(*FuncName);
      }

      // Return early with error if function not found (before NodeCreator)
      if (!Func) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Could not find function '%s::%s' for node type '%s'"),
                *ClassName, *FuncName, *NodeType),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
      }

      // Now safe to create NodeCreator since we know we'll finalize it
      FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
      UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);
      CallFuncNode->SetFromFunction(Func);
      FinalizeAndReport(NodeCreator, CallFuncNode);
      return true;
    }

    // Basic node creation logic - this can be expanded significantly
    if (NodeType == TEXT("InputAxisEvent")) {
      FString InputAxisName;
      Payload->TryGetStringField(TEXT("inputAxisName"), InputAxisName);

      if (InputAxisName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inputAxisName required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      FGraphNodeCreator<UK2Node_InputAxisEvent> NodeCreator(*TargetGraph);
      UK2Node_InputAxisEvent *InputNode = NodeCreator.CreateNode(false);
      InputNode->InputAxisName = FName(*InputAxisName);

      FinalizeAndReport(NodeCreator, InputNode);
    } else if (NodeType == TEXT("CallFunction")) {
      FString MemberName;
      Payload->TryGetStringField(TEXT("memberName"), MemberName);
      FString MemberClass;
      Payload->TryGetStringField(TEXT("memberClass"),
                                 MemberClass); // Optional, for static functions

      FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
      UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);

      UFunction *Func = nullptr;
      if (!MemberClass.IsEmpty()) {
        UClass *Class = FindObject<UClass>(nullptr, *MemberClass);
        if (Class) {
          Func = Class->FindFunctionByName(*MemberName);
        }
      } else {
        // Try to find in blueprint context
        Func = Blueprint->GeneratedClass->FindFunctionByName(*MemberName);
        if (!Func) {
          // Try global search if simple name
          Func = FindObject<UFunction>(nullptr, *MemberName);
        }
      }

      if (Func) {
        CallFuncNode->SetFromFunction(Func);
        FinalizeAndReport(NodeCreator, CallFuncNode);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find function '%s'"), *MemberName),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
      }
    } else if (NodeType == TEXT("VariableGet")) {
      FString VarName;
      Payload->TryGetStringField(TEXT("variableName"), VarName);

      FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*TargetGraph);
      UK2Node_VariableGet *VarGet = NodeCreator.CreateNode(false);

      FName VarFName(*VarName);
      // Basic check if variable exists
      bool bFound = false;
      for (const FBPVariableDescription &VarDesc : Blueprint->NewVariables) {
        if (VarDesc.VarName == VarFName) {
          bFound = true;
          break;
        }
      }
      if (bFound) {
        VarGet->VariableReference.SetSelfMember(VarFName);
        FinalizeAndReport(NodeCreator, VarGet);
      } else {
        // Use blueprint utils to resolve inherited variables correctly
        if (Blueprint->GeneratedClass &&
            Blueprint->GeneratedClass->FindPropertyByName(VarFName) !=
                nullptr) {
          VarGet->VariableReference.SetSelfMember(VarFName);
          FinalizeAndReport(NodeCreator, VarGet);
        } else {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Could not find variable '%s'"), *VarName),
              TEXT("VARIABLE_NOT_FOUND"));
          return true;
        }
      }
    } else if (NodeType == TEXT("VariableSet")) {
      FString VarName;
      Payload->TryGetStringField(TEXT("variableName"), VarName);

      FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*TargetGraph);
      UK2Node_VariableSet *VarSet = NodeCreator.CreateNode(false);

      FName VarFName(*VarName);
      bool bFound = false;
      for (const FBPVariableDescription &VarDesc : Blueprint->NewVariables) {
        if (VarDesc.VarName == VarFName) {
          bFound = true;
          break;
        }
      }
      if (bFound || (Blueprint->GeneratedClass &&
                     Blueprint->GeneratedClass->FindPropertyByName(VarFName) !=
                         nullptr)) {
        VarSet->VariableReference.SetSelfMember(VarFName);
        FinalizeAndReport(NodeCreator, VarSet);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find variable '%s'"), *VarName),
            TEXT("VARIABLE_NOT_FOUND"));
        return true;
      }
    } else if (NodeType == TEXT("CustomEvent")) {
      FString EventName;
      Payload->TryGetStringField(TEXT("eventName"), EventName);

      FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*TargetGraph);
      UK2Node_CustomEvent *EventNode = NodeCreator.CreateNode(false);

      EventNode->CustomFunctionName = FName(*EventName);
      FinalizeAndReport(NodeCreator, EventNode);
    } else if (NodeType == TEXT("Event")) {
      FString EventName;
      Payload->TryGetStringField(
          TEXT("eventName"),
          EventName); // e.g., "ReceiveBeginPlay", "ReceiveTick"
      FString MemberClass;
      Payload->TryGetStringField(TEXT("memberClass"),
                                 MemberClass); // Optional class override

      if (EventName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("eventName required for Event node"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      // Map common event name aliases to their actual function names
      static TMap<FString, FString> EventNameAliases = {
          {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
          {TEXT("Tick"), TEXT("ReceiveTick")},
          {TEXT("EndPlay"), TEXT("ReceiveEndPlay")},
          {TEXT("ActorBeginOverlap"), TEXT("ReceiveActorBeginOverlap")},
          {TEXT("ActorEndOverlap"), TEXT("ReceiveActorEndOverlap")},
          {TEXT("Hit"), TEXT("ReceiveHit")},
          {TEXT("BeginCursorOver"), TEXT("ReceiveBeginCursorOver")},
          {TEXT("EndCursorOver"), TEXT("ReceiveEndCursorOver")},
          {TEXT("Clicked"), TEXT("ReceiveClicked")},
          {TEXT("Released"), TEXT("ReceiveReleased")},
          {TEXT("Destroyed"), TEXT("ReceiveDestroyed")},
      };

      if (const FString *Alias = EventNameAliases.Find(EventName)) {
        EventName = *Alias;
      }

      FGraphNodeCreator<UK2Node_Event> NodeCreator(*TargetGraph);
      UK2Node_Event *EventNode = NodeCreator.CreateNode(false);

      // Determine target class: use explicit MemberClass or search hierarchy
      UClass *TargetClass = nullptr;
      UFunction *EventFunc = nullptr;

      if (!MemberClass.IsEmpty()) {
        // Explicit class specified
        TargetClass = FindObject<UClass>(nullptr, *MemberClass);
        if (TargetClass) {
          EventFunc = TargetClass->FindFunctionByName(*EventName);
        }
      } else {
        // Search up the class hierarchy starting from the Blueprint's parent
        // class. Events like ReceiveBeginPlay are defined in AActor, not in
        // the generated Blueprint class.
        UClass *SearchClass = Blueprint->ParentClass;
        while (SearchClass && !EventFunc) {
          EventFunc = SearchClass->FindFunctionByName(
              *EventName, EIncludeSuperFlag::ExcludeSuper);
          if (EventFunc) {
            TargetClass = SearchClass;
            break;
          }
          SearchClass = SearchClass->GetSuperClass();
        }

        // If not found in hierarchy, try the generated class
        if (!EventFunc && Blueprint->GeneratedClass) {
          EventFunc = Blueprint->GeneratedClass->FindFunctionByName(*EventName);
          if (EventFunc) {
            TargetClass = Blueprint->GeneratedClass;
          }
        }
      }

      if (EventFunc && TargetClass) {
        EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
        EventNode->bOverrideFunction = true;
        FinalizeAndReport(NodeCreator, EventNode);
      } else {
        // Provide helpful error message
        FString SearchedClasses;
        UClass *C = Blueprint->ParentClass;
        int ClassCount = 0;
        while (C && ClassCount < 5) {
          if (!SearchedClasses.IsEmpty())
            SearchedClasses += TEXT(", ");
          SearchedClasses += C->GetName();
          C = C->GetSuperClass();
          ClassCount++;
        }
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find event '%s'. Searched classes: "
                                 "%s. Try using the full name like "
                                 "'ReceiveBeginPlay' instead of 'BeginPlay'."),
                            *EventName, *SearchedClasses),
            TEXT("EVENT_NOT_FOUND"));
      }
    } else if (NodeType == TEXT("Branch")) {
      FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*TargetGraph);
      UK2Node_IfThenElse *NewNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, NewNode);
    } else if (NodeType == TEXT("Sequence")) {
      FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*TargetGraph);
      UK2Node_ExecutionSequence *NewNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, NewNode);
    } else if (NodeType == TEXT("Literal")) {
      // Create a literal node that can hold an object reference. This is a
      // fully functional K2 literal node that returns the referenced asset
      // or object when executed in the graph.
      FString LiteralType;
      Payload->TryGetStringField(TEXT("literalType"), LiteralType);
      LiteralType.TrimStartAndEndInline();
      const FString LiteralTypeLower =
          LiteralType.IsEmpty() ? TEXT("object") : LiteralType.ToLower();

      if (LiteralTypeLower == TEXT("object") ||
          LiteralTypeLower == TEXT("asset")) {
        FString ObjectPath;
        Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
        if (ObjectPath.IsEmpty()) {
          // As a convenience, allow callers to use assetPath as the
          // literal source when objectPath is omitted.
          Payload->TryGetStringField(TEXT("assetPath"), ObjectPath);
        }

        if (ObjectPath.IsEmpty()) {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Literal object creation requires "
                                   "'objectPath' or 'assetPath'."),
                              TEXT("INVALID_LITERAL"));
          return true;
        }

        UObject *LoadedObject = LoadObject<UObject>(nullptr, *ObjectPath);
        if (!LoadedObject) {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Literal object not found at path '%s'"),
                              *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
        }

        // Create the node only after successful validation
        FGraphNodeCreator<UK2Node_Literal> NodeCreator(*TargetGraph);
        UK2Node_Literal *LiteralNode = NodeCreator.CreateNode(false);
        if (!LiteralNode) {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Failed to allocate Literal node."),
                              TEXT("CREATE_FAILED"));
          return true;
        }

        // UK2Node_Literal stores the referenced UObject in a private
        // member; use its public setter rather than touching the
        // field directly so we respect engine encapsulation.
        LiteralNode->SetObjectRef(LoadedObject);
        FinalizeAndReport(NodeCreator, LiteralNode);
      } else {
        // Primitive literal support (float/int/bool/strings) can be
        // added later by wiring value pins. For now, fail fast rather
        // than pretending success.
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Unsupported literalType '%s' (only "
                                 "'object'/'asset' supported)."),
                            *LiteralType),
            TEXT("UNSUPPORTED_LITERAL_TYPE"));
        return true;
      }
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Failed to create node (unsupported type or internal error)."),
          TEXT("CREATE_FAILED"));
    }
    return true;
  } else if (SubAction == TEXT("connect_pins")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Connect Blueprint Pins")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString FromNodeId, FromPinName, ToNodeId, ToPinName;
    Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId);
    Payload->TryGetStringField(TEXT("fromPinName"), FromPinName);
    Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId);
    Payload->TryGetStringField(TEXT("toPinName"), ToPinName);

    UEdGraphNode *FromNode = nullptr;
    UEdGraphNode *ToNode = nullptr;

    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == FromNodeId)
        FromNode = Node;
      if (Node->NodeGuid.ToString() == ToNodeId)
        ToNode = Node;
    }

    if (!FromNode || !ToNode) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find source or target node."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin *FromPin = FromNode->FindPin(*FromPinName);
    UEdGraphPin *ToPin = ToNode->FindPin(*ToPinName);

    if (!FromPin || !ToPin) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find source or target pin."),
                          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    FromNode->Modify();
    ToNode->Modify();

    if (TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin)) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Pins connected."));
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to connect pins (schema rejection)."),
                          TEXT("CONNECTION_FAILED"));
    }
    return true;
  } else if (SubAction == TEXT("break_pin_links")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Break Blueprint Pin Links")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    UEdGraphNode *TargetNode = nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == NodeId) {
        TargetNode = Node;
        break;
      }
    }

    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin *Pin = TargetNode->FindPin(*PinName);
    if (!Pin) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."),
                          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    TargetNode->Modify();
    TargetGraph->GetSchema()->BreakPinLinks(*Pin, true);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin links broken."));
    return true;
  }

  else if (SubAction == TEXT("delete_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Delete Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == NodeId) {
        TargetNode = Node;
        break;
      }
    }

    if (TargetNode) {
      FBlueprintEditorUtils::RemoveNode(Blueprint, TargetNode, true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node deleted."));
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("create_reroute_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Reroute Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FGraphNodeCreator<UK2Node_Knot> NodeCreator(*TargetGraph);
    UK2Node_Knot *RerouteNode = NodeCreator.CreateNode(false);

    RerouteNode->NodePosX = X;
    RerouteNode->NodePosY = Y;

    NodeCreator.Finalize();

    RerouteNode->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), RerouteNode->NodeGuid.ToString());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Reroute node created."), Result);
    return true;
  } else if (SubAction == TEXT("set_node_property")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Blueprint Node Property")));
    Blueprint->Modify();
    TargetGraph->Modify();

    // Generic property setter for common node properties used by tools.
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    FString Value;
    Payload->TryGetStringField(TEXT("value"), Value);

    UEdGraphNode *TargetNode = nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == NodeId) {
        TargetNode = Node;
        break;
      }
    }

    if (TargetNode) {
      TargetNode->Modify();
      bool bHandled = false;

      if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase)) {
        TargetNode->NodeComment = Value;
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("X"), ESearchCase::IgnoreCase) ||
                 PropertyName.Equals(TEXT("NodePosX"),
                                     ESearchCase::IgnoreCase)) {
        double NumValue = 0.0;
        if (!Payload->TryGetNumberField(TEXT("value"), NumValue)) {
          NumValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosX = static_cast<float>(NumValue);
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("Y"), ESearchCase::IgnoreCase) ||
                 PropertyName.Equals(TEXT("NodePosY"),
                                     ESearchCase::IgnoreCase)) {
        double NumValue = 0.0;
        if (!Payload->TryGetNumberField(TEXT("value"), NumValue)) {
          NumValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosY = static_cast<float>(NumValue);
        bHandled = true;
      }

      if (bHandled) {
        TargetGraph->NotifyGraphChanged();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node property updated."));
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Unsupported node property '%s'"),
                            *PropertyName),
            TEXT("PROPERTY_NOT_SUPPORTED"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("get_node_details")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == NodeId) {
        TargetNode = Node;
        break;
      }
    }

    if (TargetNode) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
      Result->SetStringField(
          TEXT("nodeTitle"),
          TargetNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
      Result->SetStringField(TEXT("nodeComment"), TargetNode->NodeComment);
      Result->SetNumberField(TEXT("x"), TargetNode->NodePosX);
      Result->SetNumberField(TEXT("y"), TargetNode->NodePosY);

      TArray<TSharedPtr<FJsonValue>> Pins;
      for (UEdGraphPin *Pin : TargetNode->Pins) {
        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
        PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                      ? TEXT("Input")
                                                      : TEXT("Output"));
        PinObj->SetStringField(TEXT("pinType"),
                               Pin->PinType.PinCategory.ToString());
        Pins.Add(MakeShared<FJsonValueObject>(PinObj));
      }
      Result->SetArrayField(TEXT("pins"), Pins);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node details retrieved."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("get_graph_details")) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
    Result->SetNumberField(TEXT("nodeCount"), TargetGraph->Nodes.Num());

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
      NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
      NodeObj->SetStringField(TEXT("nodeName"), Node->GetName());
      NodeObj->SetStringField(
          TEXT("nodeTitle"),
          Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
      Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    Result->SetArrayField(TEXT("nodes"), Nodes);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Graph details retrieved."), Result);
    return true;
  } else if (SubAction == TEXT("get_pin_details")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString PinName;
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    UEdGraphNode *TargetNode = nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (Node->NodeGuid.ToString() == NodeId) {
        TargetNode = Node;
        break;
      }
    }

    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TArray<UEdGraphPin *> PinsToReport;
    if (!PinName.IsEmpty()) {
      UEdGraphPin *Pin = TargetNode->FindPin(*PinName);
      if (!Pin) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."),
                            TEXT("PIN_NOT_FOUND"));
        return true;
      }
      PinsToReport.Add(Pin);
    } else {
      PinsToReport = TargetNode->Pins;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), NodeId);

    TArray<TSharedPtr<FJsonValue>> PinsJson;
    for (UEdGraphPin *Pin : PinsToReport) {
      if (!Pin) {
        continue;
      }

      TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
      PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
      PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                    ? TEXT("Input")
                                                    : TEXT("Output"));
      PinObj->SetStringField(TEXT("pinType"),
                             Pin->PinType.PinCategory.ToString());

      if (Pin->LinkedTo.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> LinkedArray;
        for (UEdGraphPin *LinkedPin : Pin->LinkedTo) {
          if (!LinkedPin) {
            continue;
          }
          FString LinkedNodeId =
              LinkedPin->GetOwningNode()
                  ? LinkedPin->GetOwningNode()->NodeGuid.ToString()
                  : FString();
          const FString LinkedLabel =
              LinkedNodeId.IsEmpty()
                  ? LinkedPin->PinName.ToString()
                  : FString::Printf(TEXT("%s:%s"), *LinkedNodeId,
                                    *LinkedPin->PinName.ToString());
          LinkedArray.Add(MakeShared<FJsonValueString>(LinkedLabel));
        }
        PinObj->SetArrayField(TEXT("linkedTo"), LinkedArray);
      }

      if (!Pin->DefaultValue.IsEmpty()) {
        PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
      } else if (!Pin->DefaultTextValue.IsEmptyOrWhitespace()) {
        PinObj->SetStringField(TEXT("defaultTextValue"),
                               Pin->DefaultTextValue.ToString());
      } else if (Pin->DefaultObject) {
        PinObj->SetStringField(TEXT("defaultObjectPath"),
                               Pin->DefaultObject->GetPathName());
      }

      PinsJson.Add(MakeShared<FJsonValueObject>(PinObj));
    }

    Result->SetArrayField(TEXT("pins"), PinsJson);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin details retrieved."), Result);
    return true;
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Blueprint graph actions are editor-only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
