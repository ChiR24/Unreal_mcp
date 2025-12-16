#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#endif

/**
 * Process a "manage_blueprint_graph" automation request to inspect or modify a Blueprint graph.
 *
 * The Payload JSON controls the specific operation via the "subAction" field (examples: create_node,
 * connect_pins, get_nodes, break_pin_links, delete_node, create_reroute_node, set_node_property,
 * get_node_details, get_graph_details, get_pin_details). In editor builds this function performs
 * graph/blueprint lookups and edits; in non-editor builds it reports an editor-only error.
 *
 * @param RequestId Unique identifier for the automation request (used in responses).
 * @param Action The requested action name; this handler only processes "manage_blueprint_graph".
 * @param Payload JSON object containing action options such as "assetPath"/"blueprintPath", "graphName",
 *        "subAction" and subaction-specific fields (nodeType, nodeId, pin names, positions, etc.).
 * @param RequestingSocket WebSocket used to send responses and errors back to the requester.
 * @return `true` if the request was handled by this function (Action == "manage_blueprint_graph"), `false` otherwise.
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

  // Node identifier interoperability:
  // - Prefer NodeGuid strings for stable references.
  // - Accept node UObject names (e.g. "K2Node_Event_0") for clients that
  //   mistakenly pass nodeName where nodeId is expected.
  auto FindNodeByIdOrName = [&](const FString &Id) -> UEdGraphNode * {
    if (Id.IsEmpty()) {
      return nullptr;
    }

    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (!Node) {
        continue;
      }

      if (Node->NodeGuid.ToString().Equals(Id, ESearchCase::IgnoreCase) ||
          Node->GetName().Equals(Id, ESearchCase::IgnoreCase)) {
        return Node;
      }
    }

    return nullptr;
  };

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
        // Math Nodes
        {TEXT("Add_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_IntInt"))},
        {TEXT("Subtract_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_IntInt"))},
        {TEXT("Multiply_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_IntInt"))},
        {TEXT("Divide_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_IntInt"))},
        {TEXT("Add_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_DoubleDouble"))},
        {TEXT("Subtract_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_DoubleDouble"))},
        {TEXT("Multiply_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_DoubleDouble"))},
        {TEXT("Divide_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_DoubleDouble"))},
        {TEXT("FTrunc"), MakeTuple(TEXT("UKismetMathLibrary"), TEXT("FTrunc"))},
        // Vector Ops
        {TEXT("MakeVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("MakeVector"))},
        {TEXT("BreakVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("BreakVector"))},
        // Actor/Component Ops
        {TEXT("GetComponentByClass"),
         MakeTuple(TEXT("AActor"), TEXT("GetComponentByClass"))},
        // Timer
        {TEXT("GetWorldTimerManager"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_GetTimerManager"))}};

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
      } else if (ClassName == TEXT("UKismetMathLibrary")) {
        Class = UKismetMathLibrary::StaticClass();
      } else {
        Class = ResolveUClass(ClassName);
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
    } else if (NodeType == TEXT("CallFunction") ||
               NodeType == TEXT("K2Node_CallFunction") ||
               NodeType == TEXT("FunctionCall")) {
      FString MemberName;
      Payload->TryGetStringField(TEXT("memberName"), MemberName);
      FString MemberClass;
      Payload->TryGetStringField(TEXT("memberClass"),
                                 MemberClass); // Optional, for static functions

      UFunction *Func = nullptr;
      if (!MemberClass.IsEmpty()) {
        UClass *Class = ResolveUClass(MemberClass);
        if (Class) {
          Func = Class->FindFunctionByName(*MemberName);
        }
      } else {
        // Try to find in blueprint context
        Func = Blueprint->GeneratedClass->FindFunctionByName(*MemberName);
        if (!Func) {
          // Try global search if simple name, or check common libraries
          Func = FindObject<UFunction>(nullptr, *MemberName);
          if (!Func) {
            // Fallback: Check common libraries
            if (UClass *KSL = UKismetSystemLibrary::StaticClass())
              Func = KSL->FindFunctionByName(*MemberName);
            if (!Func)
              if (UClass *GPS = UGameplayStatics::StaticClass())
                Func = GPS->FindFunctionByName(*MemberName);
            if (!Func)
              if (UClass *KML = UKismetMathLibrary::StaticClass())
                Func = KML->FindFunctionByName(*MemberName);
          }
        }
      }

      if (Func) {
        FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
        UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);
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
      FName VarFName(*VarName);

      // Validation BEFORE creation
      bool bFound = false;
      for (const FBPVariableDescription &VarDesc : Blueprint->NewVariables) {
        if (VarDesc.VarName == VarFName) {
          bFound = true;
          break;
        }
      }
      if (!bFound && Blueprint->GeneratedClass &&
          Blueprint->GeneratedClass->FindPropertyByName(VarFName)) {
        bFound = true;
      }

      if (!bFound) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find variable '%s'"), *VarName),
            TEXT("VARIABLE_NOT_FOUND"));
        return true;
      }

      FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*TargetGraph);
      UK2Node_VariableGet *VarGet = NodeCreator.CreateNode(false);
      VarGet->VariableReference.SetSelfMember(VarFName);
      FinalizeAndReport(NodeCreator, VarGet);
    } else if (NodeType == TEXT("VariableSet")) {
      FString VarName;
      Payload->TryGetStringField(TEXT("variableName"), VarName);
      FName VarFName(*VarName);

      // Validation BEFORE creation
      bool bFound = false;
      for (const FBPVariableDescription &VarDesc : Blueprint->NewVariables) {
        if (VarDesc.VarName == VarFName) {
          bFound = true;
          break;
        }
      }
      if (!bFound && Blueprint->GeneratedClass &&
          Blueprint->GeneratedClass->FindPropertyByName(VarFName)) {
        bFound = true;
      }

      if (!bFound) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find variable '%s'"), *VarName),
            TEXT("VARIABLE_NOT_FOUND"));
        return true;
      }

      FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*TargetGraph);
      UK2Node_VariableSet *VarSet = NodeCreator.CreateNode(false);
      VarSet->VariableReference.SetSelfMember(VarFName);
      FinalizeAndReport(NodeCreator, VarSet);
    } else if (NodeType == TEXT("CustomEvent")) {
      FString EventName;
      Payload->TryGetStringField(TEXT("eventName"), EventName);

      FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*TargetGraph);
      UK2Node_CustomEvent *EventNode = NodeCreator.CreateNode(false);

      EventNode->CustomFunctionName = FName(*EventName);
      FinalizeAndReport(NodeCreator, EventNode);
    } else if (NodeType == TEXT("Event") || NodeType == TEXT("K2Node_Event")) {
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

      // Determine target class: use explicit MemberClass or search hierarchy
      UClass *TargetClass = nullptr;
      UFunction *EventFunc = nullptr;

      if (!MemberClass.IsEmpty()) {
        // Explicit class specified
        TargetClass = ResolveUClass(MemberClass);
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
        FGraphNodeCreator<UK2Node_Event> NodeCreator(*TargetGraph);
        UK2Node_Event *EventNode = NodeCreator.CreateNode(false);
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

    } else if (NodeType == TEXT("Cast") ||
               NodeType.StartsWith(TEXT("CastTo"))) {
      FString TargetClassName;
      Payload->TryGetStringField(TEXT("targetClass"), TargetClassName);

      // If targetClass not specified, try to infer from nodeType
      // "CastTo<ClassName>"
      if (TargetClassName.IsEmpty() && NodeType.StartsWith(TEXT("CastTo"))) {
        TargetClassName = NodeType.Mid(6); // Remove "CastTo" prefix
      }

      UClass *TargetClass = ResolveUClass(TargetClassName);
      if (!TargetClass) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Could not resolve target class '%s' for Cast node"),
                *TargetClassName),
            TEXT("CLASS_NOT_FOUND"));
        return true;
      }

      FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*TargetGraph);
      UK2Node_DynamicCast *CastNode = NodeCreator.CreateNode(false);
      CastNode->TargetType = TargetClass;
      FinalizeAndReport(NodeCreator, CastNode);
    } else if (NodeType == TEXT("Sequence")) {
      FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*TargetGraph);
      UK2Node_ExecutionSequence *NewNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, NewNode);
    } else if (NodeType == TEXT("Branch") || NodeType == TEXT("IfThenElse") ||
               NodeType == TEXT("K2Node_IfThenElse")) {
      FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*TargetGraph);
      UK2Node_IfThenElse *BranchNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, BranchNode);
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
    } else if (NodeType == TEXT("Comment")) {
      FGraphNodeCreator<UEdGraphNode_Comment> NodeCreator(*TargetGraph);
      UEdGraphNode_Comment *CommentNode = NodeCreator.CreateNode(false);

      FString CommentText;
      if (Payload->TryGetStringField(TEXT("comment"), CommentText) &&
          !CommentText.IsEmpty()) {
        CommentNode->NodeComment = CommentText;
      } else {
        CommentNode->NodeComment = TEXT("Comment");
      }

      CommentNode->NodeWidth = 400;
      CommentNode->NodeHeight = 100;

      FinalizeAndReport(NodeCreator, CommentNode);
    } else if (NodeType == TEXT("MakeArray")) {
      FGraphNodeCreator<UK2Node_MakeArray> NodeCreator(*TargetGraph);
      UK2Node_MakeArray *MakeArrayNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, MakeArrayNode);
    } else if (NodeType == TEXT("Return")) {
      FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*TargetGraph);
      UK2Node_FunctionResult *ReturnNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, ReturnNode);
    } else if (NodeType == TEXT("Self")) {
      FGraphNodeCreator<UK2Node_Self> NodeCreator(*TargetGraph);
      UK2Node_Self *SelfNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, SelfNode);
    } else if (NodeType == TEXT("Select")) {
      FGraphNodeCreator<UK2Node_Select> NodeCreator(*TargetGraph);
      UK2Node_Select *SelectNode = NodeCreator.CreateNode(false);
      FinalizeAndReport(NodeCreator, SelectNode);
    } else if (NodeType == TEXT("Timeline")) {
      FGraphNodeCreator<UK2Node_Timeline> NodeCreator(*TargetGraph);
      UK2Node_Timeline *TimelineNode = NodeCreator.CreateNode(false);

      FString TimelineName;
      if (Payload->TryGetStringField(TEXT("timelineName"), TimelineName) &&
          !TimelineName.IsEmpty()) {
        TimelineNode->TimelineName = FName(*TimelineName);
      }

      FinalizeAndReport(NodeCreator, TimelineNode);
    } else if (NodeType == TEXT("MakeStruct")) {
      FString StructName;
      Payload->TryGetStringField(TEXT("structName"), StructName);
      if (StructName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("structName required for MakeStruct"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      UScriptStruct *Struct = FindObject<UScriptStruct>(nullptr, *StructName);
      if (!Struct) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Struct not found"), TEXT("STRUCT_NOT_FOUND"));
        return true;
      }

      FGraphNodeCreator<UK2Node_MakeStruct> NodeCreator(*TargetGraph);
      UK2Node_MakeStruct *MakeStructNode = NodeCreator.CreateNode(false);
      MakeStructNode->StructType = Struct;
      FinalizeAndReport(NodeCreator, MakeStructNode);
    } else if (NodeType == TEXT("BreakStruct")) {
      FString StructName;
      Payload->TryGetStringField(TEXT("structName"), StructName);
      if (StructName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("structName required for BreakStruct"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      UScriptStruct *Struct = FindObject<UScriptStruct>(nullptr, *StructName);
      if (!Struct) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Struct not found"), TEXT("STRUCT_NOT_FOUND"));
        return true;
      }

      FGraphNodeCreator<UK2Node_BreakStruct> NodeCreator(*TargetGraph);
      UK2Node_BreakStruct *BreakStructNode = NodeCreator.CreateNode(false);
      BreakStructNode->StructType = Struct;
      FinalizeAndReport(NodeCreator, BreakStructNode);
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

    UEdGraphNode *FromNode = FindNodeByIdOrName(FromNodeId);
    UEdGraphNode *ToNode = FindNodeByIdOrName(ToNodeId);

    if (!FromNode || !ToNode) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find source or target node."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Handle PinName in format "NodeName.PinName"
    FString FromPinClean = FromPinName;
    if (FromPinName.Contains(TEXT("."))) {
      FromPinName.Split(TEXT("."), nullptr, &FromPinClean);
    }
    FString ToPinClean = ToPinName;
    if (ToPinName.Contains(TEXT("."))) {
      ToPinName.Split(TEXT("."), nullptr, &ToPinClean);
    }

    UEdGraphPin *FromPin = FromNode->FindPin(*FromPinClean);
    UEdGraphPin *ToPin = ToNode->FindPin(*ToPinClean);

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
  } else if (SubAction == TEXT("get_nodes")) {
    TArray<TSharedPtr<FJsonValue>> NodesArray;

    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (!Node)
        continue;

      TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
      NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
      NodeObj->SetStringField(TEXT("nodeName"), Node->GetName());
      NodeObj->SetStringField(TEXT("nodeType"), Node->GetClass()->GetName());
      NodeObj->SetStringField(
          TEXT("nodeTitle"),
          Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
      NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);
      NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
      NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);

      TArray<TSharedPtr<FJsonValue>> PinsArray;
      for (UEdGraphPin *Pin : Node->Pins) {
        if (!Pin)
          continue;

        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
        PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
        PinObj->SetStringField(TEXT("pinType"),
                               Pin->PinType.PinCategory.ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                      ? TEXT("Input")
                                                      : TEXT("Output"));

        // Add pin sub-category object type if applicable
        if (Pin->PinType.PinCategory == TEXT("object") ||
            Pin->PinType.PinCategory == TEXT("class") ||
            Pin->PinType.PinCategory == TEXT("struct")) {
          if (Pin->PinType.PinSubCategoryObject.IsValid()) {
            PinObj->SetStringField(
                TEXT("pinSubType"),
                Pin->PinType.PinSubCategoryObject->GetName());
          }
        }

        TArray<TSharedPtr<FJsonValue>> LinkedToFileArray;
        for (UEdGraphPin *LinkedPin : Pin->LinkedTo) {
          if (LinkedPin && LinkedPin->GetOwningNode()) {
            TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
            LinkObj->SetStringField(
                TEXT("nodeId"),
                LinkedPin->GetOwningNode()->NodeGuid.ToString());
            LinkObj->SetStringField(TEXT("pinName"),
                                    LinkedPin->PinName.ToString());
            LinkedToFileArray.Add(MakeShared<FJsonValueObject>(LinkObj));
          }
        }
        PinObj->SetArrayField(TEXT("linkedTo"), LinkedToFileArray);
        PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
      }
      NodeObj->SetArrayField(TEXT("pins"), PinsArray);

      NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("nodes"), NodesArray);
    Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Nodes retrieved."), Result);
    return true;
  } else if (SubAction == TEXT("break_pin_links")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Break Blueprint Pin Links")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

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

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

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

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (TargetNode) {
      TargetNode->Modify();
      bool bHandled = false;

      if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase) ||
          PropertyName.Equals(TEXT("NodeComment"), ESearchCase::IgnoreCase)) {
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
      } else if (PropertyName.Equals(TEXT("bCommentBubbleVisible"),
                                     ESearchCase::IgnoreCase)) {
        TargetNode->bCommentBubbleVisible = Value.ToBool();
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("bCommentBubblePinned"),
                                     ESearchCase::IgnoreCase)) {
        TargetNode->bCommentBubblePinned = Value.ToBool();
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

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

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

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

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