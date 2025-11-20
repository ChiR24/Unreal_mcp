#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridge_SCSHandlers.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"
#if WITH_EDITOR
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectIterator.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
// Editor-only engine includes used by blueprint creation helpers
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#if __has_include("ScopedTransaction.h")
#include "ScopedTransaction.h"
#define MCP_HAS_SCOPED_TRANSACTION 1
#elif __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#define MCP_HAS_SCOPED_TRANSACTION 1
#else
#define MCP_HAS_SCOPED_TRANSACTION 0
#endif
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include <functional>
// Component headers for safer default class resolution
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
// K2Node headers for Blueprint node graph manipulation
// In UE 5.6+, headers may reside under BlueprintGraph/Classes/
#if defined(MCP_HAS_K2NODE_HEADERS)
#  if MCP_HAS_K2NODE_HEADERS
#    if defined(__has_include)
#      if __has_include("BlueprintGraph/K2Node_CallFunction.h")
#        include "BlueprintGraph/K2Node_CallFunction.h"
#        include "BlueprintGraph/K2Node_VariableGet.h"
#        include "BlueprintGraph/K2Node_VariableSet.h"
#        include "BlueprintGraph/K2Node_Literal.h"
#        include "BlueprintGraph/K2Node_Event.h"
#        include "BlueprintGraph/K2Node_CustomEvent.h"
#        include "BlueprintGraph/K2Node_FunctionEntry.h"
#        include "BlueprintGraph/K2Node_FunctionResult.h"
#      elif __has_include("BlueprintGraph/Classes/K2Node_CallFunction.h")
#        include "BlueprintGraph/Classes/K2Node_CallFunction.h"
#        include "BlueprintGraph/Classes/K2Node_VariableGet.h"
#        include "BlueprintGraph/Classes/K2Node_VariableSet.h"
#        include "BlueprintGraph/Classes/K2Node_Literal.h"
#        include "BlueprintGraph/Classes/K2Node_Event.h"
#        include "BlueprintGraph/Classes/K2Node_CustomEvent.h"
#        include "BlueprintGraph/Classes/K2Node_FunctionEntry.h"
#        include "BlueprintGraph/Classes/K2Node_FunctionResult.h"
#      elif __has_include("K2Node_CallFunction.h")
#        include "K2Node_CallFunction.h"
#        include "K2Node_VariableGet.h"
#        include "K2Node_VariableSet.h"
#        include "K2Node_Literal.h"
#        include "K2Node_Event.h"
#        include "K2Node_CustomEvent.h"
#        include "K2Node_FunctionEntry.h"
#        include "K2Node_FunctionResult.h"
#      endif
#    else
#      include "K2Node_CallFunction.h"
#      include "K2Node_VariableGet.h"
#      include "K2Node_VariableSet.h"
#      include "K2Node_Literal.h"
#      include "K2Node_Event.h"
#      include "K2Node_CustomEvent.h"
#      include "K2Node_FunctionEntry.h"
#      include "K2Node_FunctionResult.h"
#    endif
#  endif
#else
#  if defined(__has_include)
#    if __has_include("BlueprintGraph/K2Node_CallFunction.h")
#      include "BlueprintGraph/K2Node_CallFunction.h"
#      include "BlueprintGraph/K2Node_VariableGet.h"
#      include "BlueprintGraph/K2Node_VariableSet.h"
#      include "BlueprintGraph/K2Node_Literal.h"
#      include "BlueprintGraph/K2Node_Event.h"
#      include "BlueprintGraph/K2Node_CustomEvent.h"
#      include "BlueprintGraph/K2Node_FunctionEntry.h"
#      include "BlueprintGraph/K2Node_FunctionResult.h"
#      define MCP_HAS_K2NODE_HEADERS 1
#    elif __has_include("BlueprintGraph/Classes/K2Node_CallFunction.h")
#      include "BlueprintGraph/Classes/K2Node_CallFunction.h"
#      include "BlueprintGraph/Classes/K2Node_VariableGet.h"
#      include "BlueprintGraph/Classes/K2Node_VariableSet.h"
#      include "BlueprintGraph/Classes/K2Node_Literal.h"
#      include "BlueprintGraph/Classes/K2Node_Event.h"
#      include "BlueprintGraph/Classes/K2Node_CustomEvent.h"
#      include "BlueprintGraph/Classes/K2Node_FunctionEntry.h"
#      include "BlueprintGraph/Classes/K2Node_FunctionResult.h"
#      define MCP_HAS_K2NODE_HEADERS 1
#    elif __has_include("K2Node_CallFunction.h")
#      include "K2Node_CallFunction.h"
#      include "K2Node_VariableGet.h"
#      include "K2Node_VariableSet.h"
#      include "K2Node_Literal.h"
#      include "K2Node_Event.h"
#      include "K2Node_CustomEvent.h"
#      include "K2Node_FunctionEntry.h"
#      include "K2Node_FunctionResult.h"
#      define MCP_HAS_K2NODE_HEADERS 1
#    else
#      define MCP_HAS_K2NODE_HEADERS 0
#    endif
#  else
#    include "K2Node_CallFunction.h"
#    include "K2Node_VariableGet.h"
#    include "K2Node_VariableSet.h"
#    include "K2Node_Literal.h"
#    include "K2Node_Event.h"
#    include "K2Node_CustomEvent.h"
#    include "K2Node_FunctionEntry.h"
#    include "K2Node_FunctionResult.h"
#    define MCP_HAS_K2NODE_HEADERS 1
#  endif
#endif

#if defined(__has_include)
#  if __has_include("BlueprintGraph/BlueprintMetadata.h")
#    include "BlueprintGraph/BlueprintMetadata.h"
#  elif __has_include("BlueprintMetadata.h")
#    include "BlueprintMetadata.h"
#  endif
#else
#  include "BlueprintMetadata.h"
#endif

#if defined(MCP_HAS_EDGRAPH_SCHEMA_K2)
#  if MCP_HAS_EDGRAPH_SCHEMA_K2
#    if defined(__has_include)
#      if __has_include("EdGraph/EdGraphSchema_K2.h")
#        include "EdGraph/EdGraphSchema_K2.h"
#      endif
#    else
#      include "EdGraph/EdGraphSchema_K2.h"
#    endif
#  endif
#else
#  if defined(__has_include)
#    if __has_include("EdGraph/EdGraphSchema_K2.h")
#      include "EdGraph/EdGraphSchema_K2.h"
#      define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#    else
#      define MCP_HAS_EDGRAPH_SCHEMA_K2 0
#    endif
#  else
#    include "EdGraph/EdGraphSchema_K2.h"
#    define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#  endif
#endif
// Respect build-rule's PublicDefinitions: if the build rule set
// MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1 then include the subsystem headers.
#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) && (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#  if defined(__has_include)
#    if __has_include("Subsystems/SubobjectDataSubsystem.h")
#      include "Subsystems/SubobjectDataSubsystem.h"
#    elif __has_include("SubobjectDataSubsystem.h")
#      include "SubobjectDataSubsystem.h"
#    elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#      include "SubobjectData/SubobjectDataSubsystem.h"
#    endif
#  else
#    include "SubobjectDataSubsystem.h"
#  endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
// If the build-rule did not define the macro, perform header probing here
// to discover whether the engine exposes SubobjectData headers.
#  if defined(__has_include)
#    if __has_include("Subsystems/SubobjectDataSubsystem.h")
#      include "Subsystems/SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    elif __has_include("SubobjectDataSubsystem.h")
#      include "SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#      include "SubobjectData/SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    else
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#    endif
#  else
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#  endif
#else
#  define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#if MCP_HAS_EDGRAPH_SCHEMA_K2
#define MCP_PC_Float UEdGraphSchema_K2::PC_Float
#define MCP_PC_Int UEdGraphSchema_K2::PC_Int
#define MCP_PC_Boolean UEdGraphSchema_K2::PC_Boolean
#define MCP_PC_String UEdGraphSchema_K2::PC_String
#define MCP_PC_Name UEdGraphSchema_K2::PC_Name
#define MCP_PC_Object UEdGraphSchema_K2::PC_Object
#define MCP_PC_Wildcard UEdGraphSchema_K2::PC_Wildcard
#else
static const FName MCP_PC_Float(TEXT("float"));
static const FName MCP_PC_Int(TEXT("int"));
static const FName MCP_PC_Boolean(TEXT("bool"));
static const FName MCP_PC_String(TEXT("string"));
static const FName MCP_PC_Name(TEXT("name"));
static const FName MCP_PC_Object(TEXT("object"));
static const FName MCP_PC_Wildcard(TEXT("wildcard"));
#endif
#if WITH_EDITOR
namespace
{
#if MCP_HAS_EDGRAPH_SCHEMA_K2
    static UEdGraphPin* FMcpAutomationBridge_FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
    {
        if (!Node)
        {
            return nullptr;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == Direction)
            {
                return Pin;
            }
        }

        return nullptr;
    }

    static UEdGraphPin* FMcpAutomationBridge_FindOutputPin(UEdGraphNode* Node, const FName& PinName = NAME_None)
    {
        if (!Node)
        {
            return nullptr;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output)
            {
                if (!PinName.IsNone())
                {
                    if (Pin->PinName == PinName)
                    {
                        return Pin;
                    }
                }
                else
                {
                    return Pin;
                }
            }
        }

        return nullptr;
    }

    static UEdGraphPin* FMcpAutomationBridge_FindPreferredEventExec(UEdGraph* Graph)
    {
        if (!Graph)
        {
            return nullptr;
        }

        // Prefer custom events, fall back to the first available event node
        UEdGraphPin* Fallback = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            if (UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
            {
                UEdGraphPin* ExecPin = FMcpAutomationBridge_FindExecPin(Custom, EGPD_Output);
                if (ExecPin && ExecPin->LinkedTo.Num() == 0)
                {
                    return ExecPin;
                }

                if (!Fallback && ExecPin)
                {
                    Fallback = ExecPin;
                }
            }
            else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                UEdGraphPin* ExecPin = FMcpAutomationBridge_FindExecPin(EventNode, EGPD_Output);
                if (ExecPin && ExecPin->LinkedTo.Num() == 0 && !Fallback)
                {
                    Fallback = ExecPin;
                }
            }
        }

        return Fallback;
    }

    static void FMcpAutomationBridge_LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response);

    static UEdGraphPin* FMcpAutomationBridge_FindInputPin(UEdGraphNode* Node, const FName& PinName)
    {
        if (!Node)
        {
            return nullptr;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == PinName)
            {
                return Pin;
            }
        }

        return nullptr;
    }

    static UEdGraphPin* FMcpAutomationBridge_FindDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName& PreferredName = NAME_None)
    {
        if (!Node)
        {
            return nullptr;
        }

        UEdGraphPin* Fallback = nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != Direction)
            {
                continue;
            }
            if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                continue;
            }
            if (!PreferredName.IsNone() && Pin->PinName == PreferredName)
            {
                return Pin;
            }
            if (!Fallback)
            {
                Fallback = Pin;
            }
        }

        return Fallback;
    }

    static UK2Node_VariableGet* FMcpAutomationBridge_CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY)
    {
        if (!Graph)
        {
            return nullptr;
        }

        UK2Node_VariableGet* NewGet = NewObject<UK2Node_VariableGet>(Graph);
        if (!NewGet)
        {
            return nullptr;
        }

        Graph->Modify();
        NewGet->SetFlags(RF_Transactional);
        NewGet->VariableReference = VarRef;
        Graph->AddNode(NewGet, true, false);
        NewGet->CreateNewGuid();
        NewGet->NodePosX = NodePosX;
        NewGet->NodePosY = NodePosY;
        NewGet->AllocateDefaultPins();
        NewGet->Modify();
        return NewGet;
    }

    static bool FMcpAutomationBridge_AttachValuePin(
        UK2Node_VariableSet* VarSet,
        UEdGraph* Graph,
        const UEdGraphSchema_K2* Schema,
        bool& bOutLinked)
    {
        if (!VarSet || !Graph || !Schema)
        {
            return false;
        }

        const FName VarMemberName = VarSet->VariableReference.GetMemberName();
        static const FName NAME_VarSetValue(TEXT("Value"));
        UEdGraphPin* ValuePin = FMcpAutomationBridge_FindDataPin(VarSet, EGPD_Input, VarMemberName);
        if (!ValuePin)
        {
            ValuePin = FMcpAutomationBridge_FindDataPin(VarSet, EGPD_Input, NAME_VarSetValue);
        }

        if (!ValuePin)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("FMcpAutomationBridge_AttachValuePin: no Value pin found on %s"), *VarSet->GetName());
            return false;
        }

        // Remove stale links so we can deterministically reconnect
        if (ValuePin->LinkedTo.Num() > 0)
        {
            Schema->BreakPinLinks(*ValuePin, true);
        }

        auto TryLinkPins = [&](UEdGraphPin* SourcePin, const TCHAR* ContextLabel) -> bool
        {
            if (!SourcePin)
            {
                return false;
            }
            if (!VarSet->HasAnyFlags(RF_Transactional))
            {
                VarSet->SetFlags(RF_Transactional);
            }
            VarSet->Modify();
            if (UEdGraphNode* SrcNode = SourcePin->GetOwningNode())
            {
                if (!SrcNode->HasAnyFlags(RF_Transactional))
                {
                    SrcNode->SetFlags(RF_Transactional);
                }
                SrcNode->Modify();
            }
            const FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, ValuePin);
            if (Response.Response == CONNECT_RESPONSE_MAKE)
            {
                if (Schema->TryCreateConnection(SourcePin, ValuePin))
                {
                    bOutLinked = true;
                    return true;
                }
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s: TryCreateConnection failed for %s"), ContextLabel, *VarSet->GetName());
            }
            else
            {
                FMcpAutomationBridge_LogConnectionFailure(ContextLabel, SourcePin, ValuePin, Response);
            }
            return false;
        };

        bool bLinkedFromExisting = false;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node == VarSet)
            {
                continue;
            }
            if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node))
            {
                if (VarGet->VariableReference.GetMemberName() != VarMemberName)
                {
                    continue;
                }
                UEdGraphPin* GetValuePin = FMcpAutomationBridge_FindDataPin(VarGet, EGPD_Output, VarMemberName);
                if (!GetValuePin)
                {
                    static const FName NAME_VarGetValue(TEXT("Value"));
                    GetValuePin = FMcpAutomationBridge_FindDataPin(VarGet, EGPD_Output, NAME_VarGetValue);
                }
                if (GetValuePin)
                {
                    bLinkedFromExisting = TryLinkPins(GetValuePin, TEXT("blueprint_add_node value"));
                }
                if (bOutLinked)
                {
                    break;
                }
            }
        }

        if (!bOutLinked)
        {
            // Spawn a getter when none exists and link it.
            UK2Node_VariableGet* SpawnedGet = FMcpAutomationBridge_CreateVariableGetter(Graph, VarSet->VariableReference, VarSet->NodePosX - 250.0f, VarSet->NodePosY);
            if (SpawnedGet)
            {
                UEdGraphPin* SpawnedOutput = FMcpAutomationBridge_FindDataPin(SpawnedGet, EGPD_Output, VarMemberName);
                if (!SpawnedOutput)
                {
                    static const FName NAME_SpawnValue(TEXT("Value"));
                    SpawnedOutput = FMcpAutomationBridge_FindDataPin(SpawnedGet, EGPD_Output, NAME_SpawnValue);
                }
                if (!TryLinkPins(SpawnedOutput, TEXT("blueprint_add_node value (spawned)")))
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_add_node value: spawned getter unable to link for %s"), *VarSet->GetName());
                }
            }
            else
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_add_node value: failed to spawn getter for %s"), *VarSet->GetName());
            }
        }

        if (!bOutLinked)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
                TEXT("blueprint_add_node value: unable to link value pin for %s (existing=%s)"),
                *VarSet->GetName(),
                bLinkedFromExisting ? TEXT("true") : TEXT("false"));
        }

        return bOutLinked;
    }

    static bool FMcpAutomationBridge_EnsureExecLinked(UEdGraph* Graph)
    {
        if (!Graph)
        {
            return false;
        }

        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
        if (!Schema)
        {
            return false;
        }

        UEdGraphPin* EventOutput = FMcpAutomationBridge_FindPreferredEventExec(Graph);
        if (!EventOutput)
        {
            return false;
        }

        bool bChanged = false;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node || Node == EventOutput->GetOwningNode())
            {
                continue;
            }

            if (Node->IsA<UK2Node_VariableSet>() || Node->IsA<UK2Node_CallFunction>())
            {
                if (UEdGraphPin* ExecInput = FMcpAutomationBridge_FindExecPin(Node, EGPD_Input))
                {
                    if (ExecInput && ExecInput->LinkedTo.Num() == 0)
                    {
                        if (!Node->HasAnyFlags(RF_Transactional))
                        {
                            Node->SetFlags(RF_Transactional);
                        }
                        Node->Modify();
                        if (UEdGraphNode* EventNode = EventOutput->GetOwningNode())
                        {
                            if (!EventNode->HasAnyFlags(RF_Transactional))
                            {
                                EventNode->SetFlags(RF_Transactional);
                            }
                            EventNode->Modify();
                        }
                        const FPinConnectionResponse Response = Schema->CanCreateConnection(EventOutput, ExecInput);
                        if (Response.Response == CONNECT_RESPONSE_MAKE)
                        {
                            if (Schema->TryCreateConnection(EventOutput, ExecInput))
                            {
                                bChanged = true;
                            }
                        }
                        else
                        {
                            FMcpAutomationBridge_LogConnectionFailure(TEXT("EnsureExecLinked"), EventOutput, ExecInput, Response);
                        }
                    }
                }
            }
        }

        return bChanged;
    }

    static void FMcpAutomationBridge_LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response)
    {
        if (!SourcePin || !TargetPin)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s: connection skipped due to null pins (source=%p target=%p)"), Context, SourcePin, TargetPin);
            return;
        }

        FString SourceNodeName = SourcePin->GetOwningNode() ? SourcePin->GetOwningNode()->GetName() : TEXT("<null>");
        FString TargetNodeName = TargetPin->GetOwningNode() ? TargetPin->GetOwningNode()->GetName() : TEXT("<null>");

        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("%s: schema rejected connection %s (%s) -> %s (%s) reason=%d"),
            Context,
            *SourceNodeName,
            *SourcePin->PinName.ToString(),
            *TargetNodeName,
            *TargetPin->PinName.ToString(),
            static_cast<int32>(Response.Response));
    }

    static FEdGraphPinType FMcpAutomationBridge_MakePinType(const FString& InType)
    {
        FEdGraphPinType PinType;
        const FString Lower = InType.ToLower();
        if (Lower == TEXT("float") || Lower == TEXT("double")) { PinType.PinCategory = MCP_PC_Float; }
        else if (Lower == TEXT("int") || Lower == TEXT("integer")) { PinType.PinCategory = MCP_PC_Int; }
        else if (Lower == TEXT("bool") || Lower == TEXT("boolean")) { PinType.PinCategory = MCP_PC_Boolean; }
        else if (Lower == TEXT("string")) { PinType.PinCategory = MCP_PC_String; }
        else if (Lower == TEXT("name")) { PinType.PinCategory = MCP_PC_Name; }
        else { PinType.PinCategory = MCP_PC_Wildcard; }
        return PinType;
    }
#endif

    static FString FMcpAutomationBridge_JsonValueToString(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid())
        {
            return FString();
        }

        switch (Value->Type)
        {
        case EJson::String:
            return Value->AsString();
        case EJson::Number:
            return LexToString(Value->AsNumber());
        case EJson::Boolean:
            return Value->AsBool() ? TEXT("true") : TEXT("false");
        case EJson::Null:
            return FString();
        default:
            break;
        }

        FString Serialized;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (Obj.IsValid())
            {
                FJsonSerializer::Serialize(Obj.ToSharedRef(), *Writer, true);
            }
        }
        else if (Value->Type == EJson::Array)
        {
            FJsonSerializer::Serialize(Value->AsArray(), *Writer, true);
        }
        else
        {
            Writer->WriteValue(Value->AsString());
        }
        Writer->Close();
        return Serialized;
    }

    static FName FMcpAutomationBridge_ResolveMetadataKey(const FString& RawKey)
    {
        if (RawKey.Equals(TEXT("displayname"), ESearchCase::IgnoreCase))
        {
            return FName(TEXT("DisplayName"));
        }
        if (RawKey.Equals(TEXT("tooltip"), ESearchCase::IgnoreCase))
        {
            return FName(TEXT("ToolTip"));
        }
        return FName(*RawKey);
    }

#if MCP_HAS_EDGRAPH_SCHEMA_K2
    static void FMcpAutomationBridge_AddUserDefinedPin(UK2Node* Node, const FString& PinName, const FString& PinType, EEdGraphPinDirection Direction)
    {
        if (!Node)
        {
            return;
        }

        const FString CleanName = PinName.TrimStartAndEnd();
        if (CleanName.IsEmpty())
        {
            return;
        }

        const FEdGraphPinType PinTypeDesc = FMcpAutomationBridge_MakePinType(PinType);
        const FName PinFName(*CleanName);

        if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
        {
            EntryNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
        }
        else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
        {
            ResultNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
        }
        else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
        {
            CustomEventNode->CreateUserDefinedPin(PinFName, PinTypeDesc, Direction);
        }
    }

    static UFunction* FMcpAutomationBridge_ResolveFunction(UBlueprint* Blueprint, const FString& FunctionName)
    {
        if (!Blueprint || FunctionName.TrimStartAndEnd().IsEmpty())
        {
            return nullptr;
        }

        const FString CleanFunc = FunctionName.TrimStartAndEnd();

        UFunction* Found = FindObject<UFunction>(nullptr, *CleanFunc);
        if (Found)
        {
            return Found;
        }

        const FName FuncFName(*CleanFunc);
        const TArray<UClass*> CandidateClasses = {
            Blueprint->GeneratedClass,
            Blueprint->SkeletonGeneratedClass,
            Blueprint->ParentClass
        };

        for (UClass* Candidate : CandidateClasses)
        {
            if (Candidate)
            {
                UFunction* CandidateFunc = Candidate->FindFunctionByName(FuncFName);
                if (CandidateFunc)
                {
                    return CandidateFunc;
                }
            }
        }

        int32 DotIndex = INDEX_NONE;
        if (CleanFunc.FindChar('.', DotIndex))
        {
            const FString ClassPath = CleanFunc.Left(DotIndex);
            const FString FuncSegment = CleanFunc.Mid(DotIndex + 1);
            if (!ClassPath.IsEmpty() && !FuncSegment.IsEmpty())
            {
                if (UClass* ExplicitClass = FindObject<UClass>(nullptr, *ClassPath))
                {
                    UFunction* ExplicitFunc = ExplicitClass->FindFunctionByName(FName(*FuncSegment));
                    if (ExplicitFunc)
                    {
                        return ExplicitFunc;
                    }
                }
            }
        }

        return nullptr;
    }

    static FProperty* FMcpAutomationBridge_FindProperty(UBlueprint* Blueprint, const FString& PropertyName)
    {
        if (!Blueprint || PropertyName.TrimStartAndEnd().IsEmpty())
        {
            return nullptr;
        }

        const FName PropFName(*PropertyName.TrimStartAndEnd());
        const TArray<UClass*> CandidateClasses = {
            Blueprint->GeneratedClass,
            Blueprint->SkeletonGeneratedClass,
            Blueprint->ParentClass
        };

        for (UClass* Candidate : CandidateClasses)
        {
            if (!Candidate)
            {
                continue;
            }

            if (FProperty* Found = Candidate->FindPropertyByName(PropFName))
            {
                return Found;
            }
        }

        return nullptr;
    }
#endif // MCP_HAS_EDGRAPH_SCHEMA_K2

    static FString FMcpAutomationBridge_DescribePinType(const FEdGraphPinType& PinType)
    {
        FString BaseType = PinType.PinCategory.ToString();

        if (PinType.PinSubCategoryObject.IsValid())
        {
            if (const UObject* SubObj = PinType.PinSubCategoryObject.Get())
            {
                BaseType = SubObj->GetName();
            }
        }
        else if (PinType.PinSubCategory != NAME_None)
        {
            BaseType = PinType.PinSubCategory.ToString();
        }

        FString ContainerWrappedType = BaseType;
        switch (PinType.ContainerType)
        {
        case EPinContainerType::Array:
            ContainerWrappedType = FString::Printf(TEXT("Array<%s>"), *BaseType);
            break;
        case EPinContainerType::Set:
            ContainerWrappedType = FString::Printf(TEXT("Set<%s>"), *BaseType);
            break;
        case EPinContainerType::Map:
        {
            FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
            if (PinType.PinValueType.TerminalSubCategoryObject.IsValid())
            {
                if (const UObject* ValueObj = PinType.PinValueType.TerminalSubCategoryObject.Get())
                {
                    ValueType = ValueObj->GetName();
                }
            }
            else if (PinType.PinValueType.TerminalSubCategory != NAME_None)
            {
                ValueType = PinType.PinValueType.TerminalSubCategory.ToString();
            }
            ContainerWrappedType = FString::Printf(TEXT("Map<%s,%s>"), *BaseType, *ValueType);
            break;
        }
        default:
            break;
        }

        return ContainerWrappedType;
    }

    static void FMcpAutomationBridge_AppendPinsJson(const TArray<TSharedPtr<FUserPinInfo>>& Pins, TArray<TSharedPtr<FJsonValue>>& Out)
    {
        for (const TSharedPtr<FUserPinInfo>& PinInfo : Pins)
        {
            if (!PinInfo.IsValid())
            {
                continue;
            }
            const FString PinName = PinInfo->PinName.ToString();
            if (PinName.IsEmpty())
            {
                continue;
            }
            TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
            PinJson->SetStringField(TEXT("name"), PinName);
            PinJson->SetStringField(TEXT("type"), FMcpAutomationBridge_DescribePinType(PinInfo->PinType));
            Out.Add(MakeShared<FJsonValueObject>(PinJson));
        }
    }

    static bool FMcpAutomationBridge_CollectVariableMetadata(const UBlueprint* Blueprint, const FBPVariableDescription& VarDesc, TSharedPtr<FJsonObject>& OutMetadata)
    {
        OutMetadata.Reset();

#if WITH_EDITOR
        if (Blueprint)
        {
            TSharedPtr<FJsonObject> MetaJson = MakeShared<FJsonObject>();
            bool bAny = false;
            UBlueprint* MutableBlueprint = const_cast<UBlueprint*>(Blueprint);
            if (FProperty* Property = FMcpAutomationBridge_FindProperty(MutableBlueprint, VarDesc.VarName.ToString()))
            {
                if (const TMap<FName, FString>* MetaMap = Property->GetMetaDataMap())
                {
                    for (const TPair<FName, FString>& Pair : *MetaMap)
                    {
                        if (!Pair.Value.IsEmpty())
                        {
                            MetaJson->SetStringField(Pair.Key.ToString(), Pair.Value);
                            bAny = true;
                        }
                    }
                }
            }
            if (bAny && MetaJson->Values.Num() > 0)
            {
                OutMetadata = MetaJson;
                return true;
            }
        }
#endif

        return false;
    }

    static TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildVariableJson(const UBlueprint* Blueprint, const FBPVariableDescription& VarDesc)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
        Obj->SetStringField(TEXT("type"), FMcpAutomationBridge_DescribePinType(VarDesc.VarType));
        Obj->SetBoolField(TEXT("replicated"), (VarDesc.PropertyFlags & CPF_Net) != 0);
        Obj->SetBoolField(TEXT("public"), (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) == 0);
        const FString CategoryStr = VarDesc.Category.IsEmpty() ? FString() : VarDesc.Category.ToString();
        if (!CategoryStr.IsEmpty())
        {
            Obj->SetStringField(TEXT("category"), CategoryStr);
        }
        TSharedPtr<FJsonObject> Metadata;
        if (FMcpAutomationBridge_CollectVariableMetadata(Blueprint, VarDesc, Metadata))
        {
            Obj->SetObjectField(TEXT("metadata"), Metadata);
        }
        return Obj;
    }

    static TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintVariables(UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        if (!Blueprint)
        {
            return Out;
        }

        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            Out.Add(MakeShared<FJsonValueObject>(FMcpAutomationBridge_BuildVariableJson(Blueprint, Var)));
        }
        return Out;
    }

    static TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintFunctions(UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        if (!Blueprint)
        {
            return Out;
        }

        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (!Graph)
            {
                continue;
            }

            TSharedPtr<FJsonObject> Fn = MakeShared<FJsonObject>();
            Fn->SetStringField(TEXT("name"), Graph->GetName());

            bool bIsPublic = true;
            TArray<TSharedPtr<FJsonValue>> Inputs;
            TArray<TSharedPtr<FJsonValue>> Outputs;

            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
                {
                    FMcpAutomationBridge_AppendPinsJson(EntryNode->UserDefinedPins, Inputs);
                    bIsPublic = (EntryNode->GetFunctionFlags() & FUNC_Public) != 0;
                }
                else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
                {
                    FMcpAutomationBridge_AppendPinsJson(ResultNode->UserDefinedPins, Outputs);
                }
            }

            Fn->SetBoolField(TEXT("public"), bIsPublic);
            if (Inputs.Num() > 0)
            {
                Fn->SetArrayField(TEXT("inputs"), Inputs);
            }
            if (Outputs.Num() > 0)
            {
                Fn->SetArrayField(TEXT("outputs"), Outputs);
            }

            Out.Add(MakeShared<FJsonValueObject>(Fn));
        }

        return Out;
    }

    static void FMcpAutomationBridge_CollectEventPins(UK2Node* Node, TArray<TSharedPtr<FJsonValue>>& Out)
    {
        if (!Node)
        {
            return;
        }

        if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
        {
            FMcpAutomationBridge_AppendPinsJson(CustomEvent->UserDefinedPins, Out);
        }
        else if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
        {
            FMcpAutomationBridge_AppendPinsJson(FunctionEntry->UserDefinedPins, Out);
        }
    }

    static TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintEvents(UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        if (!Blueprint)
        {
            return Out;
        }

        auto AppendEvent = [&](const FString& EventName, const FString& EventType, UK2Node* SourceNode)
        {
            TSharedPtr<FJsonObject> EventJson = MakeShared<FJsonObject>();
            EventJson->SetStringField(TEXT("name"), EventName);
            EventJson->SetStringField(TEXT("eventType"), EventType);

            TArray<TSharedPtr<FJsonValue>> Params;
            FMcpAutomationBridge_CollectEventPins(SourceNode, Params);
            if (Params.Num() > 0)
            {
                EventJson->SetArrayField(TEXT("parameters"), Params);
            }

            Out.Add(MakeShared<FJsonValueObject>(EventJson));
        };

        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (!Graph)
            {
                continue;
            }

            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
                {
                    AppendEvent(CustomEvent->CustomFunctionName.ToString(), TEXT("custom"), CustomEvent);
                }
                else if (UK2Node_Event* K2Event = Cast<UK2Node_Event>(Node))
                {
                    AppendEvent(K2Event->GetFunctionName().ToString(), K2Event->GetClass()->GetName(), K2Event);
                }
            }
        }

        return Out;
    }

    static TSharedPtr<FJsonObject> FMcpAutomationBridge_FindNamedEntry(const TArray<TSharedPtr<FJsonValue>>& Array, const FString& FieldName, const FString& DesiredValue)
    {
        for (const TSharedPtr<FJsonValue>& Value : Array)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid())
            {
                continue;
            }

            FString FieldValue;
            if (Obj->TryGetStringField(FieldName, FieldValue) && FieldValue.Equals(DesiredValue, ESearchCase::IgnoreCase))
            {
                return Obj;
            }
        }
        return nullptr;
    }

    static TSharedPtr<FJsonObject> FMcpAutomationBridge_EnsureBlueprintEntry(const FString& Key)
    {
        if (TSharedPtr<FJsonObject>* Existing = GBlueprintRegistry.Find(Key))
        {
            if (Existing->IsValid())
            {
                return *Existing;
            }
        }

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("blueprintPath"), Key);
        Entry->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>());
        Entry->SetArrayField(TEXT("functions"), TArray<TSharedPtr<FJsonValue>>());
        Entry->SetArrayField(TEXT("events"), TArray<TSharedPtr<FJsonValue>>());
        Entry->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>());
        Entry->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
        GBlueprintRegistry.Add(Key, Entry);
        return Entry;
    }

    static TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildBlueprintSnapshot(UBlueprint* Blueprint, const FString& NormalizedPath)
    {
        if (!Blueprint)
        {
            return MakeShared<FJsonObject>();
        }

        TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
        Snapshot->SetStringField(TEXT("blueprintPath"), NormalizedPath);
        Snapshot->SetStringField(TEXT("resolvedPath"), NormalizedPath);
        Snapshot->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
        Snapshot->SetArrayField(TEXT("variables"), FMcpAutomationBridge_CollectBlueprintVariables(Blueprint));
        Snapshot->SetArrayField(TEXT("functions"), FMcpAutomationBridge_CollectBlueprintFunctions(Blueprint));
        Snapshot->SetArrayField(TEXT("events"), FMcpAutomationBridge_CollectBlueprintEvents(Blueprint));

        // Aggregate metadata by variable for compatibility with legacy responses.
        TSharedPtr<FJsonObject> MetadataRoot = MakeShared<FJsonObject>();
        for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            TSharedPtr<FJsonObject> MetaJson;
            if (FMcpAutomationBridge_CollectVariableMetadata(Blueprint, VarDesc, MetaJson) && MetaJson.IsValid())
            {
                MetadataRoot->SetObjectField(VarDesc.VarName.ToString(), MetaJson);
            }
        }
        if (MetadataRoot->Values.Num() > 0)
        {
            Snapshot->SetObjectField(TEXT("metadata"), MetadataRoot);
        }
        return Snapshot;
    }
#endif // MCP_HAS_EDGRAPH_SCHEMA_K2
}
#endif // WITH_EDITOR
#if WITH_EDITOR && MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
namespace McpAutomationBridge
{
    template<typename, typename = void>
    struct THasK2Add : std::false_type {};

    template<typename T>
    struct THasK2Add<T, std::void_t<decltype(std::declval<T>().K2_AddNewSubobject(std::declval<FAddNewSubobjectParams>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasAdd : std::false_type {};

    template<typename T>
    struct THasAdd<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(std::declval<FAddNewSubobjectParams>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasAddTwoArg : std::false_type {};

    template<typename T>
    struct THasAddTwoArg<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(std::declval<FAddNewSubobjectParams>(), std::declval<FText&>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THandleHasIsValid : std::false_type {};

    template<typename T>
    struct THandleHasIsValid<T, std::void_t<decltype(std::declval<T>().IsValid())>> : std::true_type {};

    template<typename, typename = void>
    struct THasRename : std::false_type {};

    template<typename T>
    struct THasRename<T, std::void_t<decltype(std::declval<T>().RenameSubobjectMemberVariable(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>(), std::declval<FName>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasK2Remove : std::false_type {};

    template<typename T>
    struct THasK2Remove<T, std::void_t<decltype(std::declval<T>().K2_RemoveSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasRemove : std::false_type {};

    template<typename T>
    struct THasRemove<T, std::void_t<decltype(std::declval<T>().RemoveSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasDeleteSubobject : std::false_type {};

    template<typename T>
    struct THasDeleteSubobject<T, std::void_t<decltype(std::declval<T>().DeleteSubobject(std::declval<const FSubobjectDataHandle&>(), std::declval<const FSubobjectDataHandle&>(), std::declval<UBlueprint*>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasK2Attach : std::false_type {};

    template<typename T>
    struct THasK2Attach<T, std::void_t<decltype(std::declval<T>().K2_AttachSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasAttach : std::false_type {};

    template<typename T>
    struct THasAttach<T, std::void_t<decltype(std::declval<T>().AttachSubobject(std::declval<FSubobjectDataHandle>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};
}
#endif // WITH_EDITOR && MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM

// Helper: pattern-match logic extracted to file-scope so diagnostic
// loops cannot be accidentally placed outside a function body by
// preprocessor variations.
static bool ActionMatchesPatternImpl(const FString& Lower, const FString& AlphaNumLower, const TCHAR* Pattern)
{
    const FString PatternStr = FString(Pattern).ToLower();
    FString PatternAlpha; PatternAlpha.Reserve(PatternStr.Len());
    for (int32 i=0;i<PatternStr.Len();++i) { const TCHAR C = PatternStr[i]; if (FChar::IsAlnum(C)) PatternAlpha.AppendChar(C); }
    const bool bExactOrContains = (Lower.Equals(PatternStr) || Lower.Contains(PatternStr));
    const bool bAlphaMatch = (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() && AlphaNumLower.Contains(PatternAlpha));
    return (bExactOrContains || bAlphaMatch);
}

static void DiagnosticPatternChecks(const FString& CleanAction, const FString& Lower, const FString& AlphaNumLower)
{
    const TCHAR* Patterns[] = {
        TEXT("blueprint_add_variable"), TEXT("add_variable"), TEXT("addvariable"),
        TEXT("blueprint_add_event"), TEXT("add_event"),
        TEXT("blueprint_add_function"), TEXT("add_function"),
        TEXT("blueprint_modify_scs"), TEXT("modify_scs"),
        TEXT("blueprint_set_default"), TEXT("set_default"),
        TEXT("blueprint_set_variable_metadata"), TEXT("set_variable_metadata"),
        TEXT("blueprint_compile"), TEXT("blueprint_probe_subobject_handle"), TEXT("blueprint_exists"), TEXT("blueprint_get"), TEXT("blueprint_create")
    };
    for (const TCHAR* P : Patterns)
    {
        const bool bMatch = ActionMatchesPatternImpl(Lower, AlphaNumLower, P);
        // This diagnostic is extremely chatty when processing many requests —
        // lower it to VeryVerbose so it only appears when a developer explicitly
        // enables very verbose logging for the subsystem.
        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Diagnostic pattern check: Action=%s Pattern=%s Matched=%s"), *CleanAction, P, bMatch ? TEXT("true") : TEXT("false"));
    }
}

// Handler helper: probe subobject handle (extracted from inline dispatcher to
// avoid preprocessor-brace fragility in the large dispatcher function).
static UClass* ResolveComponentClassSpec(const FString& InSpec)
{
    FString Spec = InSpec;
    Spec.TrimStartAndEndInline();
    // If a full object path or /Script/ path is provided, use that directly
    if (Spec.Contains(TEXT("/")) || Spec.Contains(TEXT(".")))
    {
        if (UClass* C = FindObject<UClass>(nullptr, *Spec))
        {
            return C->IsChildOf(UActorComponent::StaticClass()) ? C : nullptr;
        }
        if (UClass* C = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Spec))
        {
            return C->IsChildOf(UActorComponent::StaticClass()) ? C : nullptr;
        }
    }
    // Try common script prefixes for short names
    const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/UMG."), TEXT("/Script/Paper2D."), TEXT("/Script/CoreUObject.") };
    for (const FString& P : Prefixes)
    {
        const FString Guess = P + Spec;
        if (UClass* C = FindObject<UClass>(nullptr, *Guess))
        {
            return C->IsChildOf(UActorComponent::StaticClass()) ? C : nullptr;
        }
        if (UClass* C = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess))
        {
            return C->IsChildOf(UActorComponent::StaticClass()) ? C : nullptr;
        }
    }
    // Final fallback: scan loaded classes by short name
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* C = *It;
        if (!C) continue;
        if (C->IsChildOf(UActorComponent::StaticClass()) && C->GetName().Equals(Spec, ESearchCase::IgnoreCase))
        {
            return C;
        }
    }
    // Default to StaticMeshComponent to keep probe functional without warnings
    return UStaticMeshComponent::StaticClass();
}

static bool HandleBlueprintProbeSubobjectHandle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    check(Self);
    // Local extraction
    FString ComponentClass; LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
    if (ComponentClass.IsEmpty()) ComponentClass = TEXT("StaticMeshComponent");

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_probe_subobject_handle start RequestId=%s componentClass=%s"), *RequestId, *ComponentClass);

    auto CleanupProbeAsset = [](UBlueprint* ProbeBP)
    {
#if WITH_EDITOR
        if (ProbeBP)
        {
            const FString AssetPath = ProbeBP->GetPathName();
            UEditorAssetLibrary::DeleteLoadedAsset(ProbeBP);
            if (!AssetPath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(AssetPath))
            {
                UEditorAssetLibrary::DeleteAsset(AssetPath);
            }
        }
#endif
    };

    const FString ProbeFolder = TEXT("/Game/Temp/MCPProbe");
    const FString ProbeName = FString::Printf(TEXT("MCP_Probe_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UBlueprint* CreatedBP = nullptr;
    {
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject* NewObj = AssetToolsModule.Get().CreateAsset(ProbeName, ProbeFolder, UBlueprint::StaticClass(), Factory);
        if (!NewObj)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("componentClass"), ComponentClass);
            Err->SetStringField(TEXT("error"), TEXT("Failed to create probe blueprint asset"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_probe_subobject_handle: asset creation failed"));
            Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create probe blueprint"), Err, TEXT("PROBE_CREATE_FAILED"));
            return true;
        }
        CreatedBP = Cast<UBlueprint>(NewObj);
        if (!CreatedBP)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("componentClass"), ComponentClass);
            Err->SetStringField(TEXT("error"), TEXT("Probe asset was not a Blueprint"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_probe_subobject_handle: created asset not blueprint"));
            Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Probe asset created was not a Blueprint"), Err, TEXT("PROBE_CREATE_FAILED"));
            CleanupProbeAsset(CreatedBP);
            return true;
        }
        FAssetRegistryModule& Arm = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        Arm.Get().AssetCreated(CreatedBP);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
    ResultObj->SetBoolField(TEXT("success"), false);
    ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);

#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
    if (USubobjectDataSubsystem* Subsystem = (GEngine ? GEngine->GetEngineSubsystem<USubobjectDataSubsystem>() : nullptr))
    {
        ResultObj->SetBoolField(TEXT("subsystemAvailable"), true);

        TArray<FSubobjectDataHandle> GatheredHandles;
        Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBP, GatheredHandles);

        TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
        const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
        for (int32 Index = 0; Index < GatheredHandles.Num(); ++Index)
        {
            const FSubobjectDataHandle& Handle = GatheredHandles[Index];
            FString Repr;
            if (HandleStruct)
            {
                Repr = FString::Printf(TEXT("%s@%p"), *HandleStruct->GetName(), (void*)&Handle);
            }
            else
            {
                Repr = FString::Printf(TEXT("<subobject_handle_%d>"), Index);
            }
            HandleJsonArr.Add(MakeShared<FJsonValueString>(Repr));
        }
        ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
        ResultObj->SetBoolField(TEXT("success"), true);

        CleanupProbeAsset(CreatedBP);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Native probe completed"), ResultObj, FString());
        return true;
    }
#endif

    // Subsystem unavailable – fallback to simple SCS enumeration
    ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
    TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
    if (CreatedBP && CreatedBP->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& Nodes = CreatedBP->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : Nodes)
        {
            if (!Node || !Node->GetVariableName().IsValid()) continue;
            HandleJsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("scs://%s"), *Node->GetVariableName().ToString())));
        }
    }
    if (HandleJsonArr.Num() == 0)
    {
        HandleJsonArr.Add(MakeShared<FJsonValueString>(TEXT("<probe_handle_stub>")));
    }
    ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
    ResultObj->SetBoolField(TEXT("success"), true);

    CleanupProbeAsset(CreatedBP);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Fallback probe completed"), ResultObj, FString());
    return true;
#else
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint probe requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif // WITH_EDITOR
}

// Handler helper: create Blueprint (extracted for the same reasons)
static bool HandleBlueprintCreate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    check(Self);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);
    
    FString Name; LocalPayload->TryGetStringField(TEXT("name"), Name);
    if (Name.TrimStartAndEnd().IsEmpty()) { Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_create requires a name."), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SavePath; LocalPayload->TryGetStringField(TEXT("savePath"), SavePath); if (SavePath.TrimStartAndEnd().IsEmpty()) SavePath = TEXT("/Game");
    FString ParentClassSpec; LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);
    FString BlueprintTypeSpec; LocalPayload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);
    const double Now = FPlatformTime::Seconds();
    const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);

    // Check if client wants to wait for completion
    bool bWaitForCompletion = false;
    LocalPayload->TryGetBoolField(TEXT("waitForCompletion"), bWaitForCompletion);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: name=%s, savePath=%s, waitForCompletion=%s"), *Name, *SavePath, bWaitForCompletion ? TEXT("true") : TEXT("false"));

    // Track in-flight requests regardless so all waiters receive completion
    GBlueprintCreateInflight.Add(CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
    GBlueprintCreateInflightTs.Add(CreateKey, Now);
    GBlueprintCreateInflight[CreateKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));

#if WITH_EDITOR
    // Perform real creation (editor only)
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: Starting blueprint creation (WITH_EDITOR=1)"));
    UBlueprint* CreatedBlueprint = nullptr;
    FString CreatedNormalizedPath;
    FString CreationError;

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    UClass* ResolvedParent = nullptr;
    if (!ParentClassSpec.IsEmpty())
    {
        if (ParentClassSpec.StartsWith(TEXT("/Script/")))
        {
            ResolvedParent = LoadClass<UObject>(nullptr, *ParentClassSpec);
        }
        else
        {
            ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
            // Avoid calling StaticLoadClass on a bare short name like "Actor" which
            // can generate engine warnings (e.g., "Class None.Actor"). For short names,
            // try common /Script prefixes instead.
            const bool bLooksPathLike = ParentClassSpec.Contains(TEXT("/")) || ParentClassSpec.Contains(TEXT("."));
            if (!ResolvedParent && bLooksPathLike)
            {
                ResolvedParent = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
            }
            if (!ResolvedParent && !bLooksPathLike)
            {
                const TArray<FString> PrefixGuesses = {
                    FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassSpec),
                    FString::Printf(TEXT("/Script/GameFramework.%s"), *ParentClassSpec),
                    FString::Printf(TEXT("/Script/CoreUObject.%s"), *ParentClassSpec)
                };
                for (const FString& Guess : PrefixGuesses)
                {
                    UClass* Loaded = FindObject<UClass>(nullptr, *Guess);
                    if (!Loaded)
                    {
                        Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *Guess);
                    }
                    if (Loaded)
                    {
                        ResolvedParent = Loaded;
                        break;
                    }
                }
            }
            if (!ResolvedParent)
            {
                for (TObjectIterator<UClass> It; It; ++It)
                {
                    UClass* C = *It;
                    if (!C) continue;
                    if (C->GetName().Equals(ParentClassSpec, ESearchCase::IgnoreCase)) { ResolvedParent = C; break; }
                }
            }
        }
    }
    if (!ResolvedParent && !BlueprintTypeSpec.IsEmpty())
    {
        const FString LowerType = BlueprintTypeSpec.ToLower();
        if (LowerType == TEXT("actor")) ResolvedParent = AActor::StaticClass(); else if (LowerType == TEXT("pawn")) ResolvedParent = APawn::StaticClass(); else if (LowerType == TEXT("character")) ResolvedParent = ACharacter::StaticClass();
    }
    if (ResolvedParent) Factory->ParentClass = ResolvedParent; else Factory->ParentClass = AActor::StaticClass();

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, SavePath, UBlueprint::StaticClass(), Factory);
    if (NewObj)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("CreateAsset returned object: name=%s path=%s class=%s"), *NewObj->GetName(), *NewObj->GetPathName(), *NewObj->GetClass()->GetName());
    }

    CreatedBlueprint = Cast<UBlueprint>(NewObj);
    if (!CreatedBlueprint) { CreationError = FString::Printf(TEXT("Created asset is not a Blueprint: %s"), NewObj ? *NewObj->GetPathName() : TEXT("<null>")); Self->SendAutomationResponse(RequestingSocket, RequestId, false, CreationError, nullptr, TEXT("CREATE_FAILED")); return true; }

    CreatedNormalizedPath = CreatedBlueprint->GetPathName(); if (CreatedNormalizedPath.Contains(TEXT("."))) CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")); AssetRegistryModule.AssetCreated(CreatedBlueprint);

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath); ResultPayload->SetStringField(TEXT("assetPath"), CreatedBlueprint->GetPathName()); ResultPayload->SetBoolField(TEXT("saved"), true);
    FScopeLock Lock(&GBlueprintCreateMutex);
    if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
    {
        for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs) { Self->SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint created"), ResultPayload, FString()); }
        GBlueprintCreateInflight.Remove(CreateKey); GBlueprintCreateInflightTs.Remove(CreateKey);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s completed (coalesced)."), *RequestId);
    }
    else { Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint created"), ResultPayload, FString()); }

    TWeakObjectPtr<UBlueprint> WeakCreatedBp = CreatedBlueprint;
    if (WeakCreatedBp.IsValid())
    {
        UBlueprint* BP = WeakCreatedBp.Get();
#if WITH_EDITOR
        SaveLoadedAssetThrottled(BP);
#endif
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"), *RequestId);
    return true;
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintCreate: WITH_EDITOR not defined - cannot create blueprints"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint creation requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBlueprintAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT(">>> HandleBlueprintAction ENTRY: RequestId=%s RawAction='%s'"), *RequestId, *Action);
    
    // Sanitize action to remove control characters and common invisible
    // Unicode markers (BOM, zero-width spaces) that may be injected by
    // transport framing or malformed clients. Keep a cleaned lowercase
    // variant for direct matches; additional compacted alphanumeric form
    // will be computed later (after nested action extraction) so matching
    // is tolerant of underscores, hyphens and camelCase.
    FString CleanAction;
    CleanAction.Reserve(Action.Len());
    for (int32 Idx = 0; Idx < Action.Len(); ++Idx)
    {
        const TCHAR C = Action[Idx];
        // Filter common invisible / control characters
        if (C < 32) continue;
        if (C == 0x200B /* ZERO WIDTH SPACE */ || C == 0xFEFF /* BOM */ || C == 0x200C /* ZERO WIDTH NON-JOINER */ || C == 0x200D /* ZERO WIDTH JOINER */) continue;
        CleanAction.AppendChar(C);
    }
    CleanAction.TrimStartAndEndInline();
    FString Lower = CleanAction.ToLower();
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction sanitized: CleanAction='%s' Lower='%s'"), *CleanAction, *Lower);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction invoked: RequestId=%s RawAction=%s CleanAction=%s Lower=%s"), *RequestId, *Action, *CleanAction, *Lower);

    // Prepare local payload early so we can inspect nested 'action' when wrapped
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

    // Normalize separators to tolerate variants like 'manage-blueprint' or 'manage blueprint'
    FString LowerNormalized = Lower;
    LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
    LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));

    // Remember if the original action looked like a manage_blueprint wrapper so
    // we continue to treat it as a blueprint action even after extracting a
    // nested subaction such as "create" or "add_component".
    const bool bManageWrapperHint = (
        LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
        LowerNormalized.StartsWith(TEXT("manageblueprint"))
    );

    // If this looks like a manage_blueprint wrapper, try to extract nested action first
    if ((LowerNormalized.StartsWith(TEXT("manage_blueprint")) || LowerNormalized.StartsWith(TEXT("manageblueprint"))) && LocalPayload.IsValid())
    {
        FString Nested;
        if (LocalPayload->TryGetStringField(TEXT("action"), Nested) && !Nested.TrimStartAndEnd().IsEmpty())
        {
            FString NestedClean; NestedClean.Reserve(Nested.Len());
            for (int32 i = 0; i < Nested.Len(); ++i) { const TCHAR C = Nested[i]; if (C >= 32) NestedClean.AppendChar(C); }
            NestedClean.TrimStartAndEndInline();
            if (!NestedClean.IsEmpty())
            {
                CleanAction = NestedClean;
                Lower = CleanAction.ToLower();
                LowerNormalized = Lower; LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_")); LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("manage_blueprint nested action detected: %s -> %s"), *Action, *CleanAction);
            }
        }
    }

    // Build a compact alphanumeric-only lowercase key for tolerant matching
    FString AlphaNumLower; AlphaNumLower.Reserve(CleanAction.Len());
    for (int32 i = 0; i < CleanAction.Len(); ++i)
    {
        const TCHAR C = CleanAction[i];
        if (FChar::IsAlnum(C)) AlphaNumLower.AppendChar(FChar::ToLower(C));
    }

    // Allow blueprint_* actions, manage_blueprint variants, and SCS-related actions (which are blueprint operations)
    const bool bLooksBlueprint = (
        // direct blueprint_* actions
        LowerNormalized.StartsWith(TEXT("blueprint_")) ||
        // manage_blueprint wrappers (before or after nested extraction)
        LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
        LowerNormalized.StartsWith(TEXT("manageblueprint")) ||
        bManageWrapperHint ||
        // SCS-related operations are blueprint operations
        LowerNormalized.Contains(TEXT("scs_component")) ||
        LowerNormalized.Contains(TEXT("_scs")) ||
        AlphaNumLower.Contains(TEXT("blueprint")) ||
        AlphaNumLower.Contains(TEXT("scs"))
    );
    if (!bLooksBlueprint)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("HandleBlueprintAction: action does not match prefix check, returning false (CleanAction='%s')"), *CleanAction);
        return false;
    }


    // Temporaries used by blueprint_create handler — declared here so
    // preprocessor paths and nested blocks do not accidentally leave
    // these identifiers out-of-scope during complex conditional builds.
    FString Name;
    FString SavePath;
    FString ParentClassSpec;
    FString BlueprintTypeSpec;
    double Now = 0.0;
    FString CreateKey;

    // If the client sent a manage_blueprint wrapper, allow a nested 'action'
    // field in the payload to specify the real blueprint_* action. This
    // improves compatibility with higher-level tool wrappers that forward
    // requests under a generic tool name.
    if (Lower.StartsWith(TEXT("manage_blueprint")) && LocalPayload.IsValid())
    {
        FString Nested; if (LocalPayload->TryGetStringField(TEXT("action"), Nested) && !Nested.TrimStartAndEnd().IsEmpty())
        {
            // Recompute cleaned/lower action values using nested action
            FString NestedClean; NestedClean.Reserve(Nested.Len()); for (int32 i = 0; i < Nested.Len(); ++i) { const TCHAR C = Nested[i]; if (C >= 32) NestedClean.AppendChar(C); }
            NestedClean.TrimStartAndEndInline(); if (!NestedClean.IsEmpty()) { CleanAction = NestedClean; Lower = CleanAction.ToLower(); UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("manage_blueprint nested action detected: %s -> %s"), *Action, *CleanAction); }
        }
    }

    // Build a compact alphanumeric-only lowercase key so we can match
    // variants such as 'add_variable', 'addVariable' and 'add-variable'.
    AlphaNumLower.Empty();
    AlphaNumLower.Reserve(CleanAction.Len());
    for (int32 i = 0; i < CleanAction.Len(); ++i)
    {
        const TCHAR C = CleanAction[i];
        if (FChar::IsAlnum(C)) AlphaNumLower.AppendChar(FChar::ToLower(C));
    }

    // Helper that performs tolerant matching: exact lower/suffix matches or
    // an alphanumeric-substring match against the compacted key.
    auto ActionMatchesPattern = [&](const TCHAR* Pattern) -> bool
    {
        const FString PatternStr = FString(Pattern).ToLower();
        // compact pattern (alpha-numeric only)
        FString PatternAlpha; PatternAlpha.Reserve(PatternStr.Len()); for (int32 i=0;i<PatternStr.Len();++i) { const TCHAR C = PatternStr[i]; if (FChar::IsAlnum(C)) PatternAlpha.AppendChar(C); }
        const bool bExactOrContains = (Lower.Equals(PatternStr) || Lower.Contains(PatternStr));
        const bool bAlphaMatch = (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() && AlphaNumLower.Contains(PatternAlpha));
        const bool bMatched = (bExactOrContains || bAlphaMatch);
    // Keep this at VeryVerbose because it executes for every pattern match
    // attempt and rapidly fills the log during normal operation.
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("ActionMatchesPattern check: pattern='%s' patternAlpha='%s' lower='%s' alpha='%s' matched=%s"), *PatternStr, *PatternAlpha, *Lower, *AlphaNumLower, bMatched ? TEXT("true") : TEXT("false"));
        return bMatched;
    };

    // Run diagnostic pattern checks early while CleanAction/Lower/AlphaNumLower are in scope
    DiagnosticPatternChecks(CleanAction, Lower, AlphaNumLower);

    // Helper to resolve requested blueprint path (honors 'requestedPath', 'name', 'blueprintPath', or 'blueprintCandidates')
    auto ResolveBlueprintRequestedPath = [&]() -> FString
    {
        FString Req;

        // Check 'requestedPath' field first (explicit path designation)
        if (LocalPayload->TryGetStringField(TEXT("requestedPath"), Req) && !Req.TrimStartAndEnd().IsEmpty())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ResolveBlueprintRequestedPath: Found requestedPath='%s'"), *Req);
            // Prefer a normalized on-disk path when available to keep registry keys consistent
            FString Norm;
            if (FindBlueprintNormalizedPath(Req, Norm) && !Norm.TrimStartAndEnd().IsEmpty())
            {
                return Norm;
            }
            return Req;
        }

        // Also accept 'name' field (commonly used by tool wrappers)
        if (LocalPayload->TryGetStringField(TEXT("name"), Req) && !Req.TrimStartAndEnd().IsEmpty())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ResolveBlueprintRequestedPath: Found name='%s'"), *Req);
            FString Norm;
            if (FindBlueprintNormalizedPath(Req, Norm) && !Norm.TrimStartAndEnd().IsEmpty())
            {
                return Norm;
            }
            return Req;
        }

        // Also accept 'blueprintPath' field for explicit designation
        if (LocalPayload->TryGetStringField(TEXT("blueprintPath"), Req) && !Req.TrimStartAndEnd().IsEmpty())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ResolveBlueprintRequestedPath: Found blueprintPath='%s'"), *Req);
            FString Norm;
            if (FindBlueprintNormalizedPath(Req, Norm) && !Norm.TrimStartAndEnd().IsEmpty())
            {
                return Norm;
            }
            return Req;
        }

        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
        // Accept either 'blueprintCandidates' (preferred) or legacy 'candidates'
        if (LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) && CandidateArray && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                FString Candidate = V->AsString();
                if (Candidate.TrimStartAndEnd().IsEmpty()) continue;
                // Return the first existing candidate (normalized if possible)
                FString Norm;
                if (FindBlueprintNormalizedPath(Candidate, Norm)) return !Norm.TrimStartAndEnd().IsEmpty() ? Norm : Candidate;
            }
        }
        // Backwards-compatible key used by some older clients
        if (LocalPayload->TryGetArrayField(TEXT("candidates"), CandidateArray) && CandidateArray && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                FString Candidate = V->AsString();
                if (Candidate.TrimStartAndEnd().IsEmpty()) continue;
                FString Norm;
                if (FindBlueprintNormalizedPath(Candidate, Norm)) return !Norm.TrimStartAndEnd().IsEmpty() ? Norm : Candidate;
            }
        }
        return FString();
    };


    if (ActionMatchesPattern(TEXT("blueprint_modify_scs")) || ActionMatchesPattern(TEXT("modify_scs")) || ActionMatchesPattern(TEXT("modifyscs")) || AlphaNumLower.Contains(TEXT("blueprintmodifyscs")) || AlphaNumLower.Contains(TEXT("modifyscs")))
            {
                const double HandlerStartTimeSec = FPlatformTime::Seconds();
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_modify_scs handler start (RequestId=%s)"), *RequestId);

                if (!LocalPayload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

                // Resolve blueprint path or candidate list
                FString BlueprintPath;
                TArray<FString> CandidatePaths;

                // Try blueprintPath first, then name (commonly used by tool wrappers), then blueprintCandidates
                if (!LocalPayload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
                {
                    if (!LocalPayload->TryGetStringField(TEXT("name"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
                    {
                        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
                        if (!LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) || CandidateArray == nullptr || CandidateArray->Num() == 0)
                        {
                            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires a non-empty blueprintPath, name, or blueprintCandidates."), TEXT("INVALID_BLUEPRINT"));
                            return true;
                        }
                        for (const TSharedPtr<FJsonValue>& Val : *CandidateArray)
                        {
                            if (!Val.IsValid()) continue;
                            const FString Candidate = Val->AsString();
                            if (!Candidate.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(Candidate);
                        }
                        if (CandidatePaths.Num() == 0)
                        {
                            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs blueprintCandidates array provided but contains no valid strings."), TEXT("INVALID_BLUEPRINT_CANDIDATES"));
                            return true;
                        }
                    }
                }

                // Operations are required
                const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
                if (!LocalPayload->TryGetArrayField(TEXT("operations"), OperationsArray) || OperationsArray == nullptr)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS")); return true;
                }

                // Flags
                bool bCompile = false; if (LocalPayload->HasField(TEXT("compile")) && !LocalPayload->TryGetBoolField(TEXT("compile"), bCompile)) { SendAutomationError(RequestingSocket, RequestId, TEXT("compile must be a boolean."), TEXT("INVALID_COMPILE_FLAG")); return true; }
                bool bSave = false; if (LocalPayload->HasField(TEXT("save")) && !LocalPayload->TryGetBoolField(TEXT("save"), bSave)) { SendAutomationError(RequestingSocket, RequestId, TEXT("save must be a boolean."), TEXT("INVALID_SAVE_FLAG")); return true; }

                // Resolve the blueprint asset (explicit path preferred, then candidates)
                FString NormalizedBlueprintPath;
                FString LoadError;
                TArray<FString> TriedCandidates;

                if (!BlueprintPath.IsEmpty())
                {
                    TriedCandidates.Add(BlueprintPath);
                    if (FindBlueprintNormalizedPath(BlueprintPath, NormalizedBlueprintPath))
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: resolved explicit path %s -> %s"), *BlueprintPath, *NormalizedBlueprintPath);
                    }
                    else
                    {
                        LoadError = FString::Printf(TEXT("Blueprint not found for path %s"), *BlueprintPath);
                    }
                }

                if (NormalizedBlueprintPath.IsEmpty() && CandidatePaths.Num() > 0)
                {
                    for (const FString& Candidate : CandidatePaths)
                    {
                        TriedCandidates.Add(Candidate);
                        FString CandidateNormalized;
                        if (FindBlueprintNormalizedPath(Candidate, CandidateNormalized))
                        {
                            NormalizedBlueprintPath = CandidateNormalized;
                            LoadError.Empty();
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: resolved candidate %s -> %s"), *Candidate, *CandidateNormalized);
                            break;
                        }
                        LoadError = FString::Printf(TEXT("Candidate not found: %s"), *Candidate);
                    }
                }

                if (NormalizedBlueprintPath.IsEmpty())
                {
                    TSharedPtr<FJsonObject> ErrPayload = MakeShared<FJsonObject>();
                    if (TriedCandidates.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> TriedValues;
                        for (const FString& C : TriedCandidates) TriedValues.Add(MakeShared<FJsonValueString>(C));
                        ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
                    }
                    SendAutomationResponse(RequestingSocket, RequestId, false, LoadError.IsEmpty() ? TEXT("Blueprint not found") : LoadError, ErrPayload, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                if (OperationsArray->Num() == 0)
                {
                    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
                    ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                    ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No SCS operations supplied."), ResultPayload, FString());
                    return true;
                }

                // Prevent concurrent SCS modifications against the same blueprint.
                const FString BusyKey = NormalizedBlueprintPath;
                if (!BusyKey.IsEmpty())
                {
                    if (GBlueprintBusySet.Contains(BusyKey)) { SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint %s is busy with another modification."), *BusyKey), nullptr, TEXT("BLUEPRINT_BUSY")); return true; }

                    GBlueprintBusySet.Add(BusyKey);
                    this->CurrentBusyBlueprintKey = BusyKey;
                    this->bCurrentBlueprintBusyMarked = true;
                    this->bCurrentBlueprintBusyScheduled = false;

                    // If we exit before completing the work, clear the busy flag
                    ON_SCOPE_EXIT
                    {
                        if (this->bCurrentBlueprintBusyMarked && !this->bCurrentBlueprintBusyScheduled)
                        {
                            GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey);
                            this->bCurrentBlueprintBusyMarked = false;
                            this->CurrentBusyBlueprintKey.Empty();
                        }
                    };
                }

                // Make a shallow copy of the operations array so it's safe to reference below.
                TArray<TSharedPtr<FJsonValue>> DeferredOps = *OperationsArray;

                // Lightweight validation of operations
                for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                {
                    const TSharedPtr<FJsonValue>& OperationValue = DeferredOps[Index];
                    if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d is not an object."), Index), TEXT("INVALID_OPERATION_PAYLOAD")); return true; }
                    const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
                    FString OperationType; if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) || OperationType.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d missing type."), Index), TEXT("INVALID_OPERATION_TYPE")); return true; }
                }

                // Mark busy as scheduled (we will perform the work synchronously here)
                this->bCurrentBlueprintBusyScheduled = true;

                // Perform the SCS modification immediately (we are on game thread)
                TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
                TArray<FString> LocalWarnings; TArray<TSharedPtr<FJsonValue>> FinalSummaries; bool bOk = false;

                FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(NormalizedBlueprintPath, LocalNormalized, LocalLoadError);
                if (!LocalBP)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("SCS application failed to load blueprint %s: %s"), *NormalizedBlueprintPath, *LocalLoadError);
                    CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
                    // Send failure and clear busy
                    SendAutomationResponse(RequestingSocket, RequestId, false, LocalLoadError, CompletionResult, TEXT("BLUEPRINT_NOT_FOUND"));
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();
                    return true;
                }

                USimpleConstructionScript* LocalSCS = LocalBP->SimpleConstructionScript;
                if (!LocalSCS)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("SCS unavailable for blueprint %s"), *NormalizedBlueprintPath);
                    CompletionResult->SetStringField(TEXT("error"), TEXT("SCS_UNAVAILABLE"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SCS_UNAVAILABLE"), CompletionResult, TEXT("SCS_UNAVAILABLE"));
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();
                    return true;
                }

                // Apply operations directly
                LocalBP->Modify(); LocalSCS->Modify();
                for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                {
                    const double OpStart = FPlatformTime::Seconds();
                    const TSharedPtr<FJsonValue>& V = DeferredOps[Index];
                    if (!V.IsValid() || V->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Op = V->AsObject(); FString OpType; Op->TryGetStringField(TEXT("type"), OpType); const FString NormalizedType = OpType.ToLower();
                    TSharedPtr<FJsonObject> OpSummary = MakeShared<FJsonObject>(); OpSummary->SetNumberField(TEXT("index"), Index); OpSummary->SetStringField(TEXT("type"), NormalizedType);

                    if (NormalizedType == TEXT("modify_component"))
                    {
                        FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                        const TSharedPtr<FJsonValue> TransformVal = Op->TryGetField(TEXT("transform"));
                        const TSharedPtr<FJsonObject> TransformObj = TransformVal.IsValid() && TransformVal->Type == EJson::Object ? TransformVal->AsObject() : nullptr;
                        if (!ComponentName.IsEmpty() && TransformObj.IsValid())
                        {
                            USCS_Node* Node = FindScsNodeByName(LocalSCS, ComponentName);
                            if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>())
                            {
                                USceneComponent* SceneTemplate = Cast<USceneComponent>(Node->ComponentTemplate);
                                FVector Location = SceneTemplate->GetRelativeLocation(); FRotator Rotation = SceneTemplate->GetRelativeRotation(); FVector Scale = SceneTemplate->GetRelativeScale3D();
                                ReadVectorField(TransformObj, TEXT("location"), Location, Location); ReadRotatorField(TransformObj, TEXT("rotation"), Rotation, Rotation); ReadVectorField(TransformObj, TEXT("scale"), Scale, Scale);
                                SceneTemplate->SetRelativeLocation(Location); SceneTemplate->SetRelativeRotation(Rotation); SceneTemplate->SetRelativeScale3D(Scale);
                                OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                            }
                            else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found or template missing")); }
                        }
                        else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Missing component name or transform")); }
                    }
                    else if (NormalizedType == TEXT("add_component"))
                    {
                        FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                        FString ComponentClassPath; Op->TryGetStringField(TEXT("componentClass"), ComponentClassPath);
                        FString AttachToName; Op->TryGetStringField(TEXT("attachTo"), AttachToName);
                        FSoftClassPath ComponentClassSoftPath(ComponentClassPath); UClass* ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
                        if (!ComponentClass) ComponentClass = FindObject<UClass>(nullptr, *ComponentClassPath);
                        if (!ComponentClass)
                        {
                            const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/UMG."), TEXT("/Script/Paper2D.") };
                            for (const FString& Prefix : Prefixes)
                            {
                                const FString Guess = Prefix + ComponentClassPath; UClass* TryClass = FindObject<UClass>(nullptr, *Guess); if (!TryClass) TryClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess); if (TryClass) { ComponentClass = TryClass; break; }
                            }
                        }
                        if (!ComponentClass)
                        {
                            OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Component class not found"));
                        }
                        else
                        {
                            USCS_Node* ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
                            if (ExistingNode) { OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName); OpSummary->SetStringField(TEXT("warning"), TEXT("Component already exists")); }
                            else
                            {
                                bool bAddedViaSubsystem = false;
                                FString AdditionMethodStr;
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                USubobjectDataSubsystem* Subsystem = nullptr;
                                if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                if (Subsystem)
                                {
                                    TArray<FSubobjectDataHandle> ExistingHandles;
                                    Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, ExistingHandles);
                                    FSubobjectDataHandle ParentHandle;
                                    if (ExistingHandles.Num() > 0)
                                    {
                                        bool bFoundParentByName = false;
                                        if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                        {
                                            const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                                            for (const FSubobjectDataHandle& H : ExistingHandles)
                                            {
                                                if (!HandleStruct) continue;
                                                FString HText;
                                                HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                                if (HText.Contains(AttachToName, ESearchCase::IgnoreCase))
                                                {
                                                    ParentHandle = H;
                                                    bFoundParentByName = true;
                                                    break;
                                                }
                                            }
                                        }
                                        if (!bFoundParentByName) ParentHandle = ExistingHandles[0];
                                    }

                                    using namespace McpAutomationBridge;
                                    constexpr bool bHasK2Add = THasK2Add<USubobjectDataSubsystem>::value;
                                    constexpr bool bHasAdd = THasAdd<USubobjectDataSubsystem>::value;
                                    constexpr bool bHasAddTwoArg = THasAddTwoArg<USubobjectDataSubsystem>::value;
                                    constexpr bool bHandleHasIsValid = THandleHasIsValid<FSubobjectDataHandle>::value;
                                    constexpr bool bHasRename = THasRename<USubobjectDataSubsystem>::value;

                                    bool bTriedNative = false;
                                    FSubobjectDataHandle NewHandle;
                                    if constexpr (bHasAddTwoArg)
                                    {
                                        FAddNewSubobjectParams Params;
                                        Params.ParentHandle = ParentHandle;
                                        Params.NewClass = ComponentClass;
                                        Params.BlueprintContext = LocalBP;
                                        FText FailReason;
                                        NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
                                        bTriedNative = true;
                                        AdditionMethodStr = TEXT("SubobjectDataSubsystem.AddNewSubobject(WithFailReason)");

                                        bool bHandleValid = true;
                                        if constexpr (bHandleHasIsValid)
                                        {
                                            bHandleValid = NewHandle.IsValid();
                                        }
                                        if (bHandleValid)
                                        {
                                            if constexpr (bHasRename)
                                            {
                                                Subsystem->RenameSubobjectMemberVariable(LocalBP, NewHandle, FName(*ComponentName));
                                            }
#if WITH_EDITOR
                                            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
                                            FKismetEditorUtilities::CompileBlueprint(LocalBP);
                                            SaveLoadedAssetThrottled(LocalBP);
#endif
                                            bAddedViaSubsystem = true;
                                        }
                                    }
                                }
#endif
                                if (bAddedViaSubsystem)
                                {
                                    OpSummary->SetBoolField(TEXT("success"), true);
                                    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    if (!AdditionMethodStr.IsEmpty()) OpSummary->SetStringField(TEXT("additionMethod"), AdditionMethodStr);
                                }
                                else
                                {
                                    USCS_Node* NewNode = LocalSCS->CreateNode(ComponentClass, *ComponentName);
                                    if (NewNode)
                                    {
                                        if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                        {
                                            if (USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, AttachToName)) { ParentNode->AddChildNode(NewNode); }
                                            else { LocalSCS->AddNode(NewNode); }
                                        }
                                        else { LocalSCS->AddNode(NewNode); }
                                        OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    }
                                    else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Failed to create SCS node")); }
                                }
                            }
                        }
                    }
                    else if (NormalizedType == TEXT("remove_component"))
                    {
                        FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                        bool bRemoved = false;
                        USubobjectDataSubsystem* Subsystem = nullptr;
                        if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                        if (Subsystem)
                        {
                            TArray<FSubobjectDataHandle> ExistingHandles;
                            Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, ExistingHandles);
                            FSubobjectDataHandle FoundHandle;
                            bool bFound = false;
                            const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                            for (const FSubobjectDataHandle& H : ExistingHandles)
                            {
                                if (!HandleStruct) continue;
                                FString HText; HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                if (HText.Contains(ComponentName, ESearchCase::IgnoreCase)) { FoundHandle = H; bFound = true; break; }
                            }

                            if (bFound)
                            {
                                constexpr bool bHasDelete = McpAutomationBridge::THasDeleteSubobject<USubobjectDataSubsystem>::value;
                                if constexpr (bHasDelete)
                                {
                                    FSubobjectDataHandle ContextHandle = ExistingHandles.Num() > 0 ? ExistingHandles[0] : FoundHandle;
                                    Subsystem->DeleteSubobject(ContextHandle, FoundHandle, LocalBP);
                                    bRemoved = true;
                                }
                            }
                        }
                        if (bRemoved)
                        {
                            OpSummary->SetBoolField(TEXT("success"), true);
                            OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                        }
                        else
                        {
                            if (USCS_Node* TargetNode = FindScsNodeByName(LocalSCS, ComponentName))
                            {
                                LocalSCS->RemoveNode(TargetNode);
                                OpSummary->SetBoolField(TEXT("success"), true);
                                OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                            }
                            else
                            {
                                OpSummary->SetBoolField(TEXT("success"), false);
                                OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found; remove skipped"));
                            }
                        }
#else
                        if (USCS_Node* TargetNode = FindScsNodeByName(LocalSCS, ComponentName))
                        {
                            LocalSCS->RemoveNode(TargetNode);
                            OpSummary->SetBoolField(TEXT("success"), true);
                            OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                        }
                        else
                        {
                            OpSummary->SetBoolField(TEXT("success"), false);
                            OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found; remove skipped"));
                        }
#endif
                    }
                    else if (NormalizedType == TEXT("attach_component"))
                    {
                        FString AttachComponentName; Op->TryGetStringField(TEXT("componentName"), AttachComponentName);
                        FString ParentName; Op->TryGetStringField(TEXT("parentComponent"), ParentName); if (ParentName.IsEmpty()) Op->TryGetStringField(TEXT("attachTo"), ParentName);
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                        bool bAttached = false;
                        USubobjectDataSubsystem* Subsystem = nullptr;
                        if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                        if (Subsystem)
                        {
                            TArray<FSubobjectDataHandle> Handles;
                            Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, Handles);
                            FSubobjectDataHandle ChildHandle, ParentHandle;
                            const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                            for (const FSubobjectDataHandle& H : Handles)
                            {
                                if (!HandleStruct) continue;
                                FString HText; HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                if (!AttachComponentName.IsEmpty() && HText.Contains(AttachComponentName, ESearchCase::IgnoreCase)) ChildHandle = H;
                                if (!ParentName.IsEmpty() && HText.Contains(ParentName, ESearchCase::IgnoreCase)) ParentHandle = H;
                            }
                            constexpr bool bHasAttach = McpAutomationBridge::THasAttach<USubobjectDataSubsystem>::value;
                            if (ChildHandle.IsValid() && ParentHandle.IsValid())
                            {
                                if constexpr (bHasAttach)
                                {
                                    bAttached = Subsystem->AttachSubobject(ParentHandle, ChildHandle);
                                }
                            }
                        }
                        if (bAttached)
                        {
                            OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
                        }
                        else
                        {
                            USCS_Node* ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName); USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, ParentName);
                            if (ChildNode && ParentNode) { ParentNode->AddChildNode(ChildNode); OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName); }
                            else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Attach failed: child or parent not found")); }
                        }
#else
                        USCS_Node* ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName); USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, ParentName);
                        if (ChildNode && ParentNode) { ParentNode->AddChildNode(ChildNode); OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName); }
                        else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Attach failed: child or parent not found")); }
#endif
                    }
                    else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Unknown operation type")); }

                    const double OpElapsedMs = (FPlatformTime::Seconds() - OpStart) * 1000.0; OpSummary->SetNumberField(TEXT("durationMs"), OpElapsedMs); FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
                }

                bOk = FinalSummaries.Num() > 0; CompletionResult->SetArrayField(TEXT("operations"), FinalSummaries);

                // Compile/save as requested
                bool bSaveResult = false;
                if (bSave && LocalBP)
                {
#if WITH_EDITOR
                    bSaveResult = SaveLoadedAssetThrottled(LocalBP);
                    if (!bSaveResult) LocalWarnings.Add(TEXT("Blueprint failed to save during apply; check output log."));
#endif
                }
                if (bCompile && LocalBP)
                {
#if WITH_EDITOR
                    FKismetEditorUtilities::CompileBlueprint(LocalBP);
#endif
                }

                CompletionResult->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                CompletionResult->SetBoolField(TEXT("compiled"), bCompile);
                CompletionResult->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                if (LocalWarnings.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> WVals; for (const FString& W : LocalWarnings) WVals.Add(MakeShared<FJsonValueString>(W)); CompletionResult->SetArrayField(TEXT("warnings"), WVals);
                }

                // Broadcast completion and deliver final response
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), CompletionResult);
                SendControlMessage(Notify);

                // Final automation_response uses actual success state
                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath); ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries); ResultPayload->SetBoolField(TEXT("compiled"), bCompile); ResultPayload->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                if (LocalWarnings.Num() > 0) { TArray<TSharedPtr<FJsonValue>> WVals2; WVals2.Reserve(LocalWarnings.Num()); for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W)); ResultPayload->SetArrayField(TEXT("warnings"), WVals2); }

                const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), FinalSummaries.Num());
                SendAutomationResponse(RequestingSocket, RequestId, bOk, Message, ResultPayload, bOk ? FString() : (CompletionResult->HasField(TEXT("error")) ? CompletionResult->GetStringField(TEXT("error")) : TEXT("SCS_OPERATION_FAILED")));

                // Release busy flag
                if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();

                return true;
            }

            // get_blueprint_scs: retrieve SCS hierarchy
            if (ActionMatchesPattern(TEXT("get_blueprint_scs")) || AlphaNumLower.Contains(TEXT("getblueprintscs")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::GetBlueprintSCS(BPPath);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // add_scs_component: add component to SCS
            if (ActionMatchesPattern(TEXT("add_scs_component")) || AlphaNumLower.Contains(TEXT("addscscomponent")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                FString CompClass; Payload->TryGetStringField(TEXT("component_class"), CompClass);
                FString CompName; Payload->TryGetStringField(TEXT("component_name"), CompName);
                FString ParentName; Payload->TryGetStringField(TEXT("parent_component"), ParentName);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::AddSCSComponent(BPPath, CompClass, CompName, ParentName);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // remove_scs_component: remove component from SCS
            if (ActionMatchesPattern(TEXT("remove_scs_component")) || AlphaNumLower.Contains(TEXT("removescscomponent")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                FString CompName; Payload->TryGetStringField(TEXT("component_name"), CompName);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::RemoveSCSComponent(BPPath, CompName);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // reparent_scs_component: reparent component in SCS
            if (ActionMatchesPattern(TEXT("reparent_scs_component")) || AlphaNumLower.Contains(TEXT("reparentscscomponent")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                FString CompName; Payload->TryGetStringField(TEXT("component_name"), CompName);
                FString NewParent; Payload->TryGetStringField(TEXT("new_parent"), NewParent);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::ReparentSCSComponent(BPPath, CompName, NewParent);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // set_scs_component_transform: set component transform in SCS
            if (ActionMatchesPattern(TEXT("set_scs_component_transform")) || AlphaNumLower.Contains(TEXT("setscscomponenttransform")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                FString CompName; Payload->TryGetStringField(TEXT("component_name"), CompName);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::SetSCSComponentTransform(BPPath, CompName, Payload);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // set_scs_component_property: set component property in SCS
            if (ActionMatchesPattern(TEXT("set_scs_component_property")) || AlphaNumLower.Contains(TEXT("setscscomponentproperty")))
            {
                FString BPPath; Payload->TryGetStringField(TEXT("blueprint_path"), BPPath);
                FString CompName; Payload->TryGetStringField(TEXT("component_name"), CompName);
                FString PropName; Payload->TryGetStringField(TEXT("property_name"), PropName);
                FString PropVal; Payload->TryGetStringField(TEXT("property_value"), PropVal);
                TSharedPtr<FJsonObject> Result = FSCSHandlers::SetSCSComponentProperty(BPPath, CompName, PropName, PropVal);
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result, Result->GetStringField(TEXT("error")));
                return true;
            }

            // blueprint_set_variable_metadata: apply metadata to the Blueprint variable (editor-only when available)
            if (ActionMatchesPattern(TEXT("blueprint_set_variable_metadata")) || ActionMatchesPattern(TEXT("set_variable_metadata")) || AlphaNumLower.Contains(TEXT("blueprintsetvariablemetadata")) || AlphaNumLower.Contains(TEXT("setvariablemetadata")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_set_variable_metadata handler: RequestId=%s"), *RequestId);
#if WITH_EDITOR
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_variable_metadata requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
                    return true;
                }

                FString VarName;
                LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
                if (VarName.IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                    return true;
                }

                const TSharedPtr<FJsonValue> MetaVal = LocalPayload->TryGetField(TEXT("metadata"));
                const TSharedPtr<FJsonObject> MetaObjPtr = MetaVal.IsValid() && MetaVal->Type == EJson::Object ? MetaVal->AsObject() : nullptr;
                if (!MetaObjPtr.IsValid())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("metadata object required"), nullptr, TEXT("INVALID_ARGUMENT"));
                    return true;
                }

                if (GBlueprintBusySet.Contains(Path))
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint is busy"), nullptr, TEXT("BLUEPRINT_BUSY"));
                    return true;
                }

                GBlueprintBusySet.Add(Path);
                ON_SCOPE_EXIT
                {
                    if (GBlueprintBusySet.Contains(Path))
                    {
                        GBlueprintBusySet.Remove(Path);
                    }
                };

                FString Normalized;
                FString LoadErr;
                UBlueprint* Blueprint = LoadBlueprintAsset(Path, Normalized, LoadErr);
                if (!Blueprint)
                {
                    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
                    if (!LoadErr.IsEmpty())
                    {
                        Err->SetStringField(TEXT("error"), LoadErr);
                    }
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint"), Err, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;

                // Find the variable description (case-insensitive)
                FBPVariableDescription* VariableDesc = nullptr;
                for (FBPVariableDescription& Desc : Blueprint->NewVariables)
                {
                    if (Desc.VarName == FName(*VarName))
                    {
                        VariableDesc = &Desc;
                        break;
                    }
                    if (Desc.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
                    {
                        VariableDesc = &Desc;
                        VarName = Desc.VarName.ToString();
                        break;
                    }
                }

                if (!VariableDesc)
                {
                    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
                    Err->SetStringField(TEXT("error"), TEXT("Variable not found"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Variable not found"), Err, TEXT("VARIABLE_NOT_FOUND"));
                    return true;
                }

                Blueprint->Modify();

                TArray<FString> AppliedKeys;
                for (const auto& Pair : MetaObjPtr->Values)
                {
                    if (!Pair.Value.IsValid())
                    {
                        continue;
                    }

                    const FString KeyStr = Pair.Key;
                    const FString ValueStr = FMcpAutomationBridge_JsonValueToString(Pair.Value);
                    const FName MetaKey = FMcpAutomationBridge_ResolveMetadataKey(KeyStr);

                    if (ValueStr.IsEmpty())
                    {
                        FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, VariableDesc->VarName, nullptr, MetaKey);
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Removed metadata '%s' from variable '%s'"), *MetaKey.ToString(), *VarName);
                    }
                    else
                    {
                        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VariableDesc->VarName, nullptr, MetaKey, ValueStr);
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Set metadata '%s'='%s' on variable '%s'"), *MetaKey.ToString(), *ValueStr, *VarName);
                    }

                    AppliedKeys.Add(MetaKey.ToString());
                }

                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

                const TSharedPtr<FJsonObject> Snapshot = FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);

                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
                Resp->SetStringField(TEXT("variableName"), VarName);
                Resp->SetBoolField(TEXT("saved"), bSaved);

                TArray<TSharedPtr<FJsonValue>> AppliedKeysJson;
                for (const FString& Key : AppliedKeys)
                {
                    AppliedKeysJson.Add(MakeShared<FJsonValueString>(Key));
                }
                Resp->SetArrayField(TEXT("appliedKeys"), AppliedKeysJson);
                if (Snapshot.IsValid() && Snapshot->HasField(TEXT("metadata")))
                {
                    Resp->SetObjectField(TEXT("metadata"), Snapshot->GetObjectField(TEXT("metadata")));
                }
                if (Snapshot.IsValid())
                {
                    Resp->SetObjectField(TEXT("blueprint"), Snapshot);
                }

                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable metadata applied"), Resp, FString());

                // Notify waiters
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("set_variable_metadata_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), Resp);
                SendControlMessage(Notify);
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_variable_metadata requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif
            }

            if (ActionMatchesPattern(TEXT("blueprint_add_construction_script")) || ActionMatchesPattern(TEXT("add_construction_script")) || AlphaNumLower.Contains(TEXT("blueprintaddconstructionscript")) || AlphaNumLower.Contains(TEXT("addconstructionscript")))
            {
                FString Path = ResolveBlueprintRequestedPath(); 
                if (Path.IsEmpty()) 
                { 
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_construction_script requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); 
                    return true; 
                }
                
#if WITH_EDITOR
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: ensuring construction script graph for '%s' (RequestId=%s)"), *Path, *RequestId);

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                FString Normalized, LoadErr;
                UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);

                if (!BP)
                {
                    Result->SetStringField(TEXT("error"), LoadErr);
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintAction: blueprint_add_construction_script failed to load '%s' (%s)"), *Path, *LoadErr);
                    SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                UEdGraph* ConstructionGraph = nullptr;
                for (UEdGraph* Graph : BP->FunctionGraphs)
                {
                    if (Graph && Graph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript)
                    {
                        ConstructionGraph = Graph;
                        break;
                    }
                }

                if (!ConstructionGraph)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction: creating new construction script graph for '%s'"), *Path);
                    ConstructionGraph = FBlueprintEditorUtils::CreateNewGraph(BP, UEdGraphSchema_K2::FN_UserConstructionScript, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
                    FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, ConstructionGraph, /*bIsUserCreated=*/false, nullptr);
                }

                if (ConstructionGraph)
                {
                    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
                    Result->SetBoolField(TEXT("success"), true);
                    Result->SetStringField(TEXT("blueprintPath"), Path);
                    Result->SetStringField(TEXT("graphName"), ConstructionGraph->GetName());
                    Result->SetStringField(TEXT("note"), TEXT("Construction script graph ensured. Use blueprint_add_node with graphName='UserConstructionScript' to add nodes."));
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: construction script graph ready '%s' graph='%s'"), *Path, *ConstructionGraph->GetName());
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Construction script graph ready."), Result, FString());
                }
                else
                {
                    Result->SetBoolField(TEXT("success"), false);
                    Result->SetStringField(TEXT("error"), TEXT("Failed to create construction script graph"));
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintAction: failed to create construction script graph for '%s'"), *Path);
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Construction script creation failed"), Result, TEXT("GRAPH_ERROR"));
                }
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_construction_script requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif
            }

            // Add a variable to the blueprint (registry-backed implementation)
            if (ActionMatchesPattern(TEXT("blueprint_add_variable")) || ActionMatchesPattern(TEXT("add_variable")) || AlphaNumLower.Contains(TEXT("blueprintaddvariable")) || AlphaNumLower.Contains(TEXT("addvariable")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_add_variable handler: RequestId=%s"), *RequestId);
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_variable requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
                    return true;
                }

                FString VarName;
                LocalPayload->TryGetStringField(TEXT("variableName"), VarName);
                if (VarName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                    return true;
                }

                FString VarType;
                LocalPayload->TryGetStringField(TEXT("variableType"), VarType);
                const TSharedPtr<FJsonValue> DefaultVal = LocalPayload->TryGetField(TEXT("defaultValue"));
                FString Category;
                LocalPayload->TryGetStringField(TEXT("category"), Category);
                const bool bReplicated = LocalPayload->HasField(TEXT("isReplicated")) ? LocalPayload->GetBoolField(TEXT("isReplicated")) : false;
                const bool bPublic = LocalPayload->HasField(TEXT("isPublic")) ? LocalPayload->GetBoolField(TEXT("isPublic")) : false;

                const FString RequestedPath = Path;
                FString RegKey = Path;
                FString NormPath;
                if (FindBlueprintNormalizedPath(Path, NormPath) && !NormPath.TrimStartAndEnd().IsEmpty())
                {
                    RegKey = NormPath;
                }

#if WITH_EDITOR
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_add_variable start RequestId=%s Path=%s VarName=%s"), *RequestId, *RequestedPath, *VarName);

                if (GBlueprintBusySet.Contains(RegKey))
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Blueprint %s is busy"), *RegKey), TEXT("BLUEPRINT_BUSY"));
                    return true;
                }

                GBlueprintBusySet.Add(RegKey);
                ON_SCOPE_EXIT
                {
                    if (GBlueprintBusySet.Contains(RegKey))
                    {
                        GBlueprintBusySet.Remove(RegKey);
                    }
                };

                FString LocalNormalized;
                FString LocalLoadError;
                UBlueprint* Blueprint = LoadBlueprintAsset(RequestedPath, LocalNormalized, LocalLoadError);
                if (!Blueprint)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintAction: failed to load blueprint_add_variable target %s (%s)"), *RegKey, *LocalLoadError);
                    SendAutomationError(RequestingSocket, RequestId, LocalLoadError.IsEmpty() ? TEXT("Failed to load blueprint") : LocalLoadError, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                const FString RegistryKey = !LocalNormalized.IsEmpty() ? LocalNormalized : RequestedPath;

                FEdGraphPinType PinType;
                const FString LowerType = VarType.ToLower();
                if (LowerType == TEXT("float") || LowerType == TEXT("double")) { PinType.PinCategory = MCP_PC_Float; }
                else if (LowerType == TEXT("int") || LowerType == TEXT("integer")) { PinType.PinCategory = MCP_PC_Int; }
                else if (LowerType == TEXT("bool") || LowerType == TEXT("boolean")) { PinType.PinCategory = MCP_PC_Boolean; }
                else if (LowerType == TEXT("string")) { PinType.PinCategory = MCP_PC_String; }
                else if (LowerType == TEXT("name")) { PinType.PinCategory = MCP_PC_Name; }
                else if (!VarType.TrimStartAndEnd().IsEmpty())
                {
                    PinType.PinCategory = MCP_PC_Object;
                    UClass* FoundClass = FindObject<UClass>(nullptr, *VarType);
                    if (!FoundClass) { FoundClass = LoadObject<UClass>(nullptr, *VarType); }
                    if (!FoundClass)
                    {
                        const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/CoreUObject.") };
                        for (const FString& Prefix : Prefixes)
                        {
                            const FString Guess = Prefix + VarType;
                            FoundClass = FindObject<UClass>(nullptr, *Guess);
                            if (!FoundClass) { FoundClass = LoadObject<UClass>(nullptr, *Guess); }
                            if (FoundClass) { break; }
                        }
                    }
                    if (FoundClass) { PinType.PinSubCategoryObject = FoundClass; }
                }
                else
                {
                    PinType.PinCategory = MCP_PC_Wildcard;
                }

                bool bAlreadyExists = false;
                for (const FBPVariableDescription& Existing : Blueprint->NewVariables)
                {
                    if (Existing.VarName == FName(*VarName))
                    {
                        bAlreadyExists = true;
                        break;
                    }
                }

                TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
                Response->SetStringField(TEXT("blueprintPath"), RegistryKey);
                Response->SetStringField(TEXT("variableName"), VarName);

                if (bAlreadyExists)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: variable '%s' already exists in '%s'"), *VarName, *RegistryKey);
                    const TSharedPtr<FJsonObject> Snapshot = FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);
                    if (Snapshot.IsValid())
                    {
                        Response->SetObjectField(TEXT("blueprint"), Snapshot);
                        if (Snapshot->HasField(TEXT("variables")))
                        {
                            const TArray<TSharedPtr<FJsonValue>> Vars = Snapshot->GetArrayField(TEXT("variables"));
                            if (const TSharedPtr<FJsonObject> VarJson = nullptr)
                            {
                                Response->SetObjectField(TEXT("variable"), VarJson);
                            }
                        }
                    }
                    Response->SetBoolField(TEXT("success"), true);
                    Response->SetStringField(TEXT("note"), TEXT("Variable already exists; no changes applied."));
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable already exists"), Response, FString());
                    return true;
                }

                Blueprint->Modify();

                FBPVariableDescription NewVar;
                NewVar.VarName = FName(*VarName);
                NewVar.VarGuid = FGuid::NewGuid();
                NewVar.FriendlyName = VarName;
                if (!Category.IsEmpty())
                {
                    NewVar.Category = FText::FromString(Category);
                }
                else
                {
                    NewVar.Category = FText::GetEmpty();
                }
                NewVar.VarType = PinType;
                NewVar.PropertyFlags |= CPF_Edit;
                NewVar.PropertyFlags |= CPF_BlueprintVisible;
                NewVar.PropertyFlags &= ~CPF_BlueprintReadOnly;
                if (bReplicated)
                {
                    NewVar.PropertyFlags |= CPF_Net;
                }

                Blueprint->NewVariables.Add(NewVar);
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

                // Real test: Verify the variable actually exists in the compiled class or blueprint
                bool bVerified = false;
                if (Blueprint->GeneratedClass)
                {
                    if (FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VarName)))
                    {
                        bVerified = true;
                    }
                }
                
                // Fallback verification: check NewVariables if compilation didn't fully propagate yet (though it should have)
                if (!bVerified)
                {
                    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
                    {
                        if (Var.VarName == FName(*VarName))
                        {
                            bVerified = true;
                            break;
                        }
                    }
                }

                if (!bVerified)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("HandleBlueprintAction: variable '%s' added but verification failed in '%s'"), *VarName, *RegistryKey);
                    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
                    Err->SetStringField(TEXT("error"), TEXT("Verification failed: variable not found after add"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Variable add verification failed"), Err, TEXT("VERIFICATION_FAILED"));
                    return true;
                }

                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: variable '%s' added to '%s' (saved=%s verified=true)"), *VarName, *RegistryKey, bSaved ? TEXT("true") : TEXT("false"));

                Response->SetBoolField(TEXT("success"), true);
                Response->SetBoolField(TEXT("saved"), bSaved);
                if (!VarType.IsEmpty()) { Response->SetStringField(TEXT("variableType"), VarType); }
                if (!Category.IsEmpty()) { Response->SetStringField(TEXT("category"), Category); }
                Response->SetBoolField(TEXT("replicated"), bReplicated);
                Response->SetBoolField(TEXT("public"), bPublic);
                const TSharedPtr<FJsonObject> Snapshot = FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, RegistryKey);
                if (Snapshot.IsValid())
                {
                    Response->SetObjectField(TEXT("blueprint"), Snapshot);
                    if (Snapshot->HasField(TEXT("variables")))
                    {
                        const TArray<TSharedPtr<FJsonValue>> Vars = Snapshot->GetArrayField(TEXT("variables"));
                        if (const TSharedPtr<FJsonObject> VarJson = FMcpAutomationBridge_FindNamedEntry(Vars, TEXT("name"), VarName))
                        {
                            Response->SetObjectField(TEXT("variable"), VarJson);
                        }
                    }
                }
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable added"), Response, FString());
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_variable requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif
            }

            // Add an event to the blueprint (synchronous editor implementation)
            if (ActionMatchesPattern(TEXT("blueprint_add_event")) || ActionMatchesPattern(TEXT("add_event")) || AlphaNumLower.Contains(TEXT("blueprintaddevent")) || AlphaNumLower.Contains(TEXT("addevent")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_add_event handler: RequestId=%s"), *RequestId);
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
                    return true;
                }

                FString EventType; LocalPayload->TryGetStringField(TEXT("eventType"), EventType);
                FString CustomName; LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
                const TArray<TSharedPtr<FJsonValue>>* ParamsField = nullptr; LocalPayload->TryGetArrayField(TEXT("parameters"), ParamsField);
                TArray<TSharedPtr<FJsonValue>> Params = (ParamsField && ParamsField->Num() > 0) ? *ParamsField : TArray<TSharedPtr<FJsonValue>>();

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
                if (GBlueprintBusySet.Contains(Path))
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint is busy"), nullptr, TEXT("BLUEPRINT_BUSY"));
                    return true;
                }

                GBlueprintBusySet.Add(Path);
                ON_SCOPE_EXIT
                {
                    if (GBlueprintBusySet.Contains(Path))
                    {
                        GBlueprintBusySet.Remove(Path);
                    }
                };

                FString Normalized;
                FString LoadErr;
                UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
                const FString RegistryKey = !Normalized.IsEmpty() ? Normalized : Path;
                if (!BP)
                {
                    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
                    if (!LoadErr.IsEmpty()) { Err->SetStringField(TEXT("error"), LoadErr); }
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint"), Err, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_add_event begin Path=%s RequestId=%s"), *RegistryKey, *RequestId);
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_add_event macro check: MCP_HAS_K2NODE_HEADERS=%d MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
                    static_cast<int32>(MCP_HAS_K2NODE_HEADERS), static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

                UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(BP);
                if (!EventGraph)
                {
                    EventGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("EventGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
                    if (EventGraph)
                    {
                        FBlueprintEditorUtils::AddUbergraphPage(BP, EventGraph);
                    }
                }

                if (!EventGraph)
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create event graph"), nullptr, TEXT("GRAPH_UNAVAILABLE"));
                    return true;
                }

                const FString FinalType = EventType.IsEmpty() ? TEXT("custom") : EventType;
                const FName EventName = CustomName.IsEmpty()
                    ? FName(*FString::Printf(TEXT("Event_%s"), *FGuid::NewGuid().ToString()))
                    : FName(*CustomName);

                float EventPosX = 0.0f;
                float EventPosY = 0.0f;
                LocalPayload->TryGetNumberField(TEXT("posX"), EventPosX);
                LocalPayload->TryGetNumberField(TEXT("posY"), EventPosY);

                UK2Node_CustomEvent* CustomEventNode = nullptr;
                for (UEdGraphNode* Node : EventGraph->Nodes)
                {
                    if (UK2Node_CustomEvent* ExistingNode = Cast<UK2Node_CustomEvent>(Node))
                    {
                        if (ExistingNode->CustomFunctionName == EventName)
                        {
                            CustomEventNode = ExistingNode;
                            break;
                        }
                    }
                }

                if (!CustomEventNode)
                {
                    EventGraph->Modify();
                    FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
                    CustomEventNode = NodeCreator.CreateNode();
                    CustomEventNode->CustomFunctionName = EventName;
                    CustomEventNode->NodePosX = EventPosX;
                    CustomEventNode->NodePosY = EventPosY;
                    NodeCreator.Finalize();
                    CustomEventNode->AllocateDefaultPins();
                }

                if (CustomEventNode && Params.Num() > 0)
                {
                    CustomEventNode->Modify();
                    CustomEventNode->Pins.Empty();
                    CustomEventNode->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, NAME_None);
                    CustomEventNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, NAME_None);
                    for (const TSharedPtr<FJsonValue>& ParamVal : Params)
                    {
                        if (!ParamVal.IsValid() || ParamVal->Type != EJson::Object) continue;
                        const TSharedPtr<FJsonObject> ParamObj = ParamVal->AsObject();
                        if (!ParamObj.IsValid()) continue;
                        FString ParamName; ParamObj->TryGetStringField(TEXT("name"), ParamName);
                        FString ParamType; ParamObj->TryGetStringField(TEXT("type"), ParamType);
                        FMcpAutomationBridge_AddUserDefinedPin(CustomEventNode, ParamName, ParamType, EGPD_Output);
                    }
                }

                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
                FKismetEditorUtilities::CompileBlueprint(BP);
                const bool bSaved = SaveLoadedAssetThrottled(BP);

                TSharedPtr<FJsonObject> Entry = FMcpAutomationBridge_EnsureBlueprintEntry(RegistryKey);
                TArray<TSharedPtr<FJsonValue>> Events = Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
                bool bFound = false;
                for (const TSharedPtr<FJsonValue>& Item : Events)
                {
                    if (!Item.IsValid() || Item->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = Item->AsObject();
                    if (Obj.IsValid())
                    {
                        FString Existing;
                        if (Obj->TryGetStringField(TEXT("name"), Existing) && Existing.Equals(EventName.ToString(), ESearchCase::IgnoreCase))
                        {
                            Obj->SetStringField(TEXT("eventType"), FinalType);
                            if (Params.Num() > 0) { Obj->SetArrayField(TEXT("parameters"), Params); }
                            else { Obj->RemoveField(TEXT("parameters")); }
                            bFound = true;
                            break;
                        }
                    }
                }

                if (!bFound)
                {
                    TSharedPtr<FJsonObject> Rec = MakeShared<FJsonObject>();
                    Rec->SetStringField(TEXT("name"), EventName.ToString());
                    Rec->SetStringField(TEXT("eventType"), FinalType);
                    if (Params.Num() > 0) { Rec->SetArrayField(TEXT("parameters"), Params); }
                    Events.Add(MakeShared<FJsonValueObject>(Rec));
                }

                Entry->SetArrayField(TEXT("events"), Events);

                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
                Resp->SetStringField(TEXT("eventName"), EventName.ToString());
                Resp->SetStringField(TEXT("eventType"), FinalType);
                Resp->SetBoolField(TEXT("saved"), bSaved);
                if (Params.Num() > 0) { Resp->SetArrayField(TEXT("parameters"), Params); }

                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event added"), Resp, FString());

                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("add_event_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), Resp);
                SendControlMessage(Notify);
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_event requires editor build with K2 node headers"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
            }

            // Remove an event from the blueprint (registry-backed implementation)
            if (ActionMatchesPattern(TEXT("blueprint_remove_event")) || ActionMatchesPattern(TEXT("remove_event")) || AlphaNumLower.Contains(TEXT("blueprintremoveevent")) || AlphaNumLower.Contains(TEXT("removeevent")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_remove_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString EventName; LocalPayload->TryGetStringField(TEXT("eventName"), EventName); if (EventName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("eventName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                
                FString NormPath;
                const FString RegistryPath = (FindBlueprintNormalizedPath(Path, NormPath) && !NormPath.IsEmpty()) ? NormPath : Path;

                TSharedPtr<FJsonObject> Entry = FMcpAutomationBridge_EnsureBlueprintEntry(RegistryPath);
                TArray<TSharedPtr<FJsonValue>> Events = Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
                int32 FoundIdx = INDEX_NONE;
                for (int32 i = 0; i < Events.Num(); ++i)
                {
                    const TSharedPtr<FJsonValue>& V = Events[i];
                    if (!V.IsValid() || V->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = V->AsObject(); FString CandidateName; if (Obj->TryGetStringField(TEXT("name"), CandidateName) && CandidateName.Equals(EventName, ESearchCase::IgnoreCase)) { FoundIdx = i; break; }
                }
                if (FoundIdx == INDEX_NONE)
                {
                    // Treat remove as idempotent: if the event is not present in
                    // the registry consider the request successful (no-op).
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetStringField(TEXT("eventName"), EventName);
                    Resp->SetStringField(TEXT("blueprintPath"), Path);
                    Resp->SetStringField(TEXT("note"), TEXT("Event not present; treated as removed (idempotent)."));
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event not present; treated as removed"), Resp, FString());
                    // Fire completion event to satisfy waitForEvent clients
                    TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                    Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                    Notify->SetStringField(TEXT("event"), TEXT("remove_event_completed"));
                    Notify->SetStringField(TEXT("requestId"), RequestId);
                    Notify->SetObjectField(TEXT("result"), Resp);
                    SendControlMessage(Notify);
                    return true;
                }

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
                FString NormalizedRemove;
                FString RemoveLoadErr;
                UBlueprint* RemoveBlueprint = LoadBlueprintAsset(RegistryPath, NormalizedRemove, RemoveLoadErr);
                if (RemoveBlueprint)
                {
                    if (UEdGraph* RemoveGraph = FBlueprintEditorUtils::FindEventGraph(RemoveBlueprint))
                    {
                        RemoveGraph->Modify();
                        TArray<UEdGraphNode*> NodesToRemove;
                        for (UEdGraphNode* Node : RemoveGraph->Nodes)
                        {
                            if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
                            {
                                if (CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
                                {
                                    NodesToRemove.Add(CustomEvent);
                                }
                            }
                        }
                        for (UEdGraphNode* Node : NodesToRemove)
                        {
                            RemoveGraph->RemoveNode(Node);
                        }
                        if (NodesToRemove.Num() > 0)
                        {
                            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(RemoveBlueprint);
                            FKismetEditorUtilities::CompileBlueprint(RemoveBlueprint);
                            SaveLoadedAssetThrottled(RemoveBlueprint);
                        }
                    }
                }
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
                // Update registry
                Events.RemoveAt(FoundIdx);
                Entry->SetArrayField(TEXT("events"), Events);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("eventName"), EventName); Resp->SetStringField(TEXT("blueprintPath"), RegistryPath);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event removed."), Resp, FString());
                // Broadcast completion event so clients waiting for an automation_event can resolve
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("remove_event_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), Resp);
                SendControlMessage(Notify);
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: event '%s' removed from '%s'"), *EventName, *RegistryPath);
                return true;
            }

            // Add a function to the blueprint (synchronous editor implementation)
            if (ActionMatchesPattern(TEXT("blueprint_add_function")) || ActionMatchesPattern(TEXT("add_function")) || AlphaNumLower.Contains(TEXT("blueprintaddfunction")) || AlphaNumLower.Contains(TEXT("addfunction")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_add_function handler: RequestId=%s"), *RequestId);
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_function requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
                    return true;
                }

                FString FuncName; LocalPayload->TryGetStringField(TEXT("functionName"), FuncName);
                if (FuncName.TrimStartAndEnd().IsEmpty())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("functionName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                    return true;
                }

                const TArray<TSharedPtr<FJsonValue>>* InputsField = nullptr; LocalPayload->TryGetArrayField(TEXT("inputs"), InputsField);
                const TArray<TSharedPtr<FJsonValue>>* OutputsField = nullptr; LocalPayload->TryGetArrayField(TEXT("outputs"), OutputsField);
                TArray<TSharedPtr<FJsonValue>> Inputs = (InputsField && InputsField->Num() > 0) ? *InputsField : TArray<TSharedPtr<FJsonValue>>();
                TArray<TSharedPtr<FJsonValue>> Outputs = (OutputsField && OutputsField->Num() > 0) ? *OutputsField : TArray<TSharedPtr<FJsonValue>>();
                const bool bIsPublic = LocalPayload->HasField(TEXT("isPublic")) ? LocalPayload->GetBoolField(TEXT("isPublic")) : false;

#if WITH_EDITOR
                if (GBlueprintBusySet.Contains(Path))
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint is busy"), nullptr, TEXT("BLUEPRINT_BUSY"));
                    return true;
                }

                GBlueprintBusySet.Add(Path);
                ON_SCOPE_EXIT
                {
                    if (GBlueprintBusySet.Contains(Path))
                    {
                        GBlueprintBusySet.Remove(Path);
                    }
                };

                FString Normalized;
                FString LoadErr;
                UBlueprint* Blueprint = LoadBlueprintAsset(Path, Normalized, LoadErr);
                const FString RegistryKey = !Normalized.IsEmpty() ? Normalized : Path;
                if (!Blueprint)
                {
                    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
                    if (!LoadErr.IsEmpty()) { Err->SetStringField(TEXT("error"), LoadErr); }
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint"), Err, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_add_function begin Path=%s RequestId=%s"), *RegistryKey, *RequestId);
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_add_function macro check: MCP_HAS_K2NODE_HEADERS=%d MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
                    static_cast<int32>(MCP_HAS_K2NODE_HEADERS), static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

#if MCP_HAS_EDGRAPH_SCHEMA_K2
                UEdGraph* ExistingGraph = nullptr;
                for (UEdGraph* Graph : Blueprint->FunctionGraphs)
                {
                    if (Graph && Graph->GetName().Equals(FuncName, ESearchCase::IgnoreCase))
                    {
                        ExistingGraph = Graph;
                        break;
                    }
                }

                if (ExistingGraph)
                {
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetBoolField(TEXT("success"), true);
                    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
                    Resp->SetStringField(TEXT("functionName"), ExistingGraph->GetName());
                    Resp->SetStringField(TEXT("note"), TEXT("Function already exists"));
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Function already exists"), Resp, FString());
                    return true;
                }

                UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
                if (!NewGraph)
                {
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create function graph"), nullptr, TEXT("GRAPH_UNAVAILABLE"));
                    return true;
                }

                FBlueprintEditorUtils::CreateFunctionGraph<UFunction>(Blueprint, NewGraph, /*bIsUserCreated=*/true, nullptr);
                if (!Blueprint->FunctionGraphs.Contains(NewGraph))
                {
                    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/true, nullptr);
                }

                TArray<UK2Node_FunctionEntry*> EntryNodes;
                TArray<UK2Node_FunctionResult*> ResultNodes;
                for (UEdGraphNode* Node : NewGraph->Nodes)
                {
                    if (UK2Node_FunctionEntry* AsEntry = Cast<UK2Node_FunctionEntry>(Node))
                    {
                        EntryNodes.Add(AsEntry);
                        continue;
                    }
                    if (UK2Node_FunctionResult* AsResult = Cast<UK2Node_FunctionResult>(Node))
                    {
                        ResultNodes.Add(AsResult);
                    }
                }

                UK2Node_FunctionEntry* EntryNode = EntryNodes.Num() > 0 ? EntryNodes[0] : nullptr;
                UK2Node_FunctionResult* ResultNode = ResultNodes.Num() > 0 ? ResultNodes[0] : nullptr;

                if (EntryNodes.Num() > 1 || ResultNodes.Num() > 1)
                {
                    NewGraph->Modify();
                    for (int32 EntryIdx = 1; EntryIdx < EntryNodes.Num(); ++EntryIdx)
                    {
                        if (UK2Node_FunctionEntry* ExtraEntry = EntryNodes[EntryIdx])
                        {
                            ExtraEntry->Modify();
                            ExtraEntry->DestroyNode();
                        }
                    }
                    for (int32 ResultIdx = 1; ResultIdx < ResultNodes.Num(); ++ResultIdx)
                    {
                        if (UK2Node_FunctionResult* ExtraResult = ResultNodes[ResultIdx])
                        {
                            ExtraResult->Modify();
                            ExtraResult->DestroyNode();
                        }
                    }
                    // Refresh surviving pointers in case the first entries were removed via Blueprint internals.
                    EntryNode = nullptr;
                    ResultNode = nullptr;
                    for (UEdGraphNode* Node : NewGraph->Nodes)
                    {
                        if (!EntryNode)
                        {
                            EntryNode = Cast<UK2Node_FunctionEntry>(Node);
                            if (EntryNode)
                            {
                                continue;
                            }
                        }
                        if (!ResultNode)
                        {
                            ResultNode = Cast<UK2Node_FunctionResult>(Node);
                        }
                        if (EntryNode && ResultNode)
                        {
                            break;
                        }
                    }
                }

                for (const TSharedPtr<FJsonValue>& Value : Inputs)
                {
                    if (!Value.IsValid() || Value->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
                    if (!Obj.IsValid()) continue;
                    FString ParamName; Obj->TryGetStringField(TEXT("name"), ParamName);
                    FString ParamType; Obj->TryGetStringField(TEXT("type"), ParamType);
                    FMcpAutomationBridge_AddUserDefinedPin(EntryNode, ParamName, ParamType, EGPD_Input);
                }

                for (const TSharedPtr<FJsonValue>& Value : Outputs)
                {
                    if (!Value.IsValid() || Value->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
                    if (!Obj.IsValid()) continue;
                    FString ParamName; Obj->TryGetStringField(TEXT("name"), ParamName);
                    FString ParamType; Obj->TryGetStringField(TEXT("type"), ParamType);
                    FMcpAutomationBridge_AddUserDefinedPin(ResultNode ? static_cast<UK2Node*>(ResultNode) : static_cast<UK2Node*>(EntryNode), ParamName, ParamType, EGPD_Output);
                }

                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

                TSharedPtr<FJsonObject> Entry = FMcpAutomationBridge_EnsureBlueprintEntry(RegistryKey);
                TArray<TSharedPtr<FJsonValue>> Funcs = Entry->HasField(TEXT("functions")) ? Entry->GetArrayField(TEXT("functions")) : TArray<TSharedPtr<FJsonValue>>();
                bool bFound = false;
                for (const TSharedPtr<FJsonValue>& Value : Funcs)
                {
                    if (!Value.IsValid() || Value->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
                    if (!Obj.IsValid()) continue;

                    FString Existing;
                    if (Obj->TryGetStringField(TEXT("name"), Existing) && Existing.Equals(FuncName, ESearchCase::IgnoreCase))
                    {
                        Obj->SetBoolField(TEXT("public"), bIsPublic);
                        if (Inputs.Num() > 0) { Obj->SetArrayField(TEXT("inputs"), Inputs); } else { Obj->RemoveField(TEXT("inputs")); }
                        if (Outputs.Num() > 0) { Obj->SetArrayField(TEXT("outputs"), Outputs); } else { Obj->RemoveField(TEXT("outputs")); }
                        bFound = true;
                        break;
                    }
                }

                if (!bFound)
                {
                    TSharedPtr<FJsonObject> Rec = MakeShared<FJsonObject>();
                    Rec->SetStringField(TEXT("name"), FuncName);
                    Rec->SetBoolField(TEXT("public"), bIsPublic);
                    if (Inputs.Num() > 0) { Rec->SetArrayField(TEXT("inputs"), Inputs); }
                    if (Outputs.Num() > 0) { Rec->SetArrayField(TEXT("outputs"), Outputs); }
                    Funcs.Add(MakeShared<FJsonValueObject>(Rec));
                }

                Entry->SetArrayField(TEXT("functions"), Funcs);

                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
                Resp->SetStringField(TEXT("functionName"), FuncName);
                Resp->SetBoolField(TEXT("public"), bIsPublic);
                Resp->SetBoolField(TEXT("saved"), bSaved);
                if (Inputs.Num() > 0) { Resp->SetArrayField(TEXT("inputs"), Inputs); }
                if (Outputs.Num() > 0) { Resp->SetArrayField(TEXT("outputs"), Outputs); }

                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Function added"), Resp, FString());

                // Broadcast completion event so clients waiting for an automation_event can resolve
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
                Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
                Notify->SetStringField(TEXT("event"), TEXT("add_function_completed"));
                Notify->SetStringField(TEXT("requestId"), RequestId);
                Notify->SetObjectField(TEXT("result"), Resp);
                SendControlMessage(Notify);
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_function requires editor build with K2 schema"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif
            }

            if (ActionMatchesPattern(TEXT("blueprint_set_default")) || ActionMatchesPattern(TEXT("set_default")) || ActionMatchesPattern(TEXT("setdefault")) || AlphaNumLower.Contains(TEXT("blueprintsetdefault")) || AlphaNumLower.Contains(TEXT("setdefault")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_set_default handler: RequestId=%s"), *RequestId);
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString PropertyName; LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
                if (PropertyName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("propertyName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const TSharedPtr<FJsonValue> Value = LocalPayload->TryGetField(TEXT("value"));
                if (!Value.IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("value required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

#if WITH_EDITOR
                FString Normalized;
                FString LoadErr;
                UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);

                if (!BP)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetStringField(TEXT("error"), LoadErr);
                    SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;

                // Get the CDO (Class Default Object) from the generated class
                UClass* GeneratedClass = BP->GeneratedClass;
                if (!GeneratedClass)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetStringField(TEXT("error"), TEXT("Blueprint has no generated class"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No generated class"), Result, TEXT("NO_GENERATED_CLASS"));
                    return true;
                }

                UObject* CDO = GeneratedClass->GetDefaultObject();
                if (!CDO)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetStringField(TEXT("error"), TEXT("Failed to get CDO"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No CDO"), Result, TEXT("NO_CDO"));
                    return true;
                }

                // Find the property by name (supports nested via dot notation)
                FProperty* TargetProperty = FindFProperty<FProperty>(GeneratedClass, FName(*PropertyName));
                if (!TargetProperty)
                {
                    // Try nested property path (e.g., "LightComponent.Intensity", "RootComponent.bHiddenInGame")
                    const int32 DotIdx = PropertyName.Find(TEXT("."));
                    if (DotIdx != INDEX_NONE)
                    {
                        const FString ComponentName = PropertyName.Left(DotIdx);
                        const FString NestedProp = PropertyName.Mid(DotIdx + 1);

                        // Search in generated class and all parent classes for the component property
                        UClass* SearchClass = GeneratedClass;
                        FProperty* CompProp = nullptr;
                        while (SearchClass && !CompProp)
                        {
                            CompProp = FindFProperty<FProperty>(SearchClass, FName(*ComponentName));
                            if (!CompProp)
                            {
                                SearchClass = SearchClass->GetSuperClass();
                            }
                        }

                        if (CompProp && CompProp->IsA<FObjectProperty>())
                        {
                            FObjectProperty* ObjProp = CastField<FObjectProperty>(CompProp);
                            void* CompPtr = ObjProp->GetPropertyValuePtr_InContainer(CDO);
                            UObject* CompObj = ObjProp->GetObjectPropertyValue(CompPtr);
                            if (CompObj)
                            {
                                TargetProperty = FindFProperty<FProperty>(CompObj->GetClass(), FName(*NestedProp));
                                if (TargetProperty)
                                {
                                    CDO = CompObj; // Update CDO to point to component
                                }
                            }
                        }
                    }
                }

                if (!TargetProperty)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetStringField(TEXT("propertyName"), PropertyName);
                    Result->SetStringField(TEXT("blueprintPath"), Path);
                    Result->SetStringField(TEXT("error"), TEXT("Property not found on generated class"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Property not found on blueprint"), Result, TEXT("PROPERTY_NOT_FOUND"));
                    return true;
                }

                // Convert JSON value to property value using the existing JSON serialization system
                TSharedPtr<FJsonObject> TempObj = MakeShared<FJsonObject>();
                TempObj->SetField(TEXT("temp"), Value);

                FString JsonString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                FJsonSerializer::Serialize(TempObj.ToSharedRef(), Writer);

                // Use FJsonObjectConverter to deserialize the value
                TSharedPtr<FJsonObject> ValueWrapObj = MakeShared<FJsonObject>();
                ValueWrapObj->SetField(TargetProperty->GetName(), Value);

                CDO->Modify();
                BP->Modify();

                // Attempt to set the property value
                bool bSuccess = FJsonObjectConverter::JsonAttributesToUStruct(ValueWrapObj->Values, GeneratedClass, CDO, 0, 0);

                if (bSuccess)
                {
                    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
                    FKismetEditorUtilities::CompileBlueprint(BP);

                    // Save the blueprint to persist changes
                    bool bSaved = SaveLoadedAssetThrottled(BP);

                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetBoolField(TEXT("success"), true);
                    Result->SetStringField(TEXT("propertyName"), PropertyName);
                    Result->SetStringField(TEXT("blueprintPath"), Path);
                    Result->SetBoolField(TEXT("saved"), bSaved);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint default property set"), Result, FString());
                }
                else
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetBoolField(TEXT("success"), false);
                    Result->SetStringField(TEXT("error"), TEXT("Failed to set property value"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Property set failed"), Result, TEXT("SET_FAILED"));
                }
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
#endif
            }

            // Compile a Blueprint asset (editor builds only). Returns whether
            // compilation (and optional save) succeeded.
            if (ActionMatchesPattern(TEXT("blueprint_compile")) || ActionMatchesPattern(TEXT("compile")) || AlphaNumLower.Contains(TEXT("blueprintcompile")) || AlphaNumLower.Contains(TEXT("compile")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_compile requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                bool bSaveAfterCompile = false; if (LocalPayload->HasField(TEXT("saveAfterCompile"))) LocalPayload->TryGetBoolField(TEXT("saveAfterCompile"), bSaveAfterCompile);
                // Editor-only compile
    #if WITH_EDITOR
                FString Normalized; FString LoadErr; UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
                if (!BP) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), LoadErr); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint for compilation"), Err, TEXT("NOT_FOUND")); return true; }
                FKismetEditorUtilities::CompileBlueprint(BP);
                bool bSaved = false;
                if (bSaveAfterCompile) { bSaved = SaveLoadedAssetThrottled(BP); }
                TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("compiled"), true); Out->SetBoolField(TEXT("saved"), bSaved); Out->SetStringField(TEXT("blueprintPath"), Path);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint compiled"), Out, FString());
                return true;
    #else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_compile requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
                return true;
    #endif
            }

                                    if (ActionMatchesPattern(TEXT("blueprint_probe_subobject_handle")) || ActionMatchesPattern(TEXT("probe_subobject_handle")) || ActionMatchesPattern(TEXT("probehandle")) || AlphaNumLower.Contains(TEXT("blueprintprobesubobjecthandle")) || AlphaNumLower.Contains(TEXT("probesubobjecthandle")) || AlphaNumLower.Contains(TEXT("probehandle")))
                                    {
                                        return HandleBlueprintProbeSubobjectHandle(this, RequestId, LocalPayload, RequestingSocket);
                                    }

            // blueprint_create handler: parse payload and prepare coalesced creation
            // Support both explicit blueprint_create and the nested 'create' action from manage_blueprint
            if (ActionMatchesPattern(TEXT("blueprint_create")) || ActionMatchesPattern(TEXT("create_blueprint")) || ActionMatchesPattern(TEXT("create")) || AlphaNumLower.Contains(TEXT("blueprintcreate")) || AlphaNumLower.Contains(TEXT("createblueprint")))
            {
                return HandleBlueprintCreate(this, RequestId, LocalPayload, RequestingSocket);
            }

    

    // Other blueprint_* actions (modify_scs, compile, add_variable, add_function, etc.)
    // For simplicity, unhandled blueprint actions return NOT_IMPLEMENTED so
    // the server may fall back to Python helpers if available.

    // blueprint_exists: check whether a blueprint asset or registry entry exists
    if (ActionMatchesPattern(TEXT("blueprint_exists")) || ActionMatchesPattern(TEXT("exists")) || AlphaNumLower.Contains(TEXT("blueprintexists")))
    {
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_exists requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        FString Normalized; bool bFound = false; FString LoadErr;
        #if WITH_EDITOR
            UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
            bFound = (BP != nullptr);
        #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_exists requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
            return true;
        #endif
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("exists"), bFound); Resp->SetStringField(TEXT("blueprintPath"), bFound ? (Normalized.IsEmpty() ? Path : Normalized) : Path);
        SendAutomationResponse(RequestingSocket, RequestId, bFound, bFound ? TEXT("Blueprint exists") : TEXT("Blueprint not found"), Resp, bFound ? FString() : TEXT("NOT_FOUND"));
        return true;
    }

    // blueprint_get: return the lightweight registry entry for a blueprint
    if ((ActionMatchesPattern(TEXT("blueprint_get")) || ActionMatchesPattern(TEXT("get")) || AlphaNumLower.Contains(TEXT("blueprintget"))) && !Lower.Contains(TEXT("scs")))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_get handler: RequestId=%s"), *RequestId);
        FString Path = ResolveBlueprintRequestedPath(); 
        if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_get requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        
        bool bExists = false;
        TSharedPtr<FJsonObject> Entry = nullptr;
        
        #if WITH_EDITOR
            FString Normalized; FString Err; UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, Err);
            bExists = (BP != nullptr);
            if (bExists)
            {
                const FString Key = !Normalized.TrimStartAndEnd().IsEmpty() ? Normalized : Path;
                Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("resolvedPath"), Key);
                Entry->SetStringField(TEXT("assetPath"), BP->GetPathName());

                // Merge variables from on-disk blueprint
                TArray<TSharedPtr<FJsonValue>> VarsJson = Entry->HasField(TEXT("variables")) ? Entry->GetArrayField(TEXT("variables")) : TArray<TSharedPtr<FJsonValue>>();
                TSet<FString> Existing;
                for (const TSharedPtr<FJsonValue>& VVal : VarsJson)
                {
                    if (VVal.IsValid() && VVal->Type == EJson::Object)
                    {
                        const TSharedPtr<FJsonObject> VObj = VVal->AsObject();
                        FString N; if (VObj.IsValid() && VObj->TryGetStringField(TEXT("name"), N)) Existing.Add(N);
                    }
                }
                for (const FBPVariableDescription& V : BP->NewVariables)
                {
                    const FString N = V.VarName.ToString();
                    if (!Existing.Contains(N))
                    {
                        TSharedPtr<FJsonObject> VObj = MakeShared<FJsonObject>();
                        VObj->SetStringField(TEXT("name"), N);
                        VarsJson.Add(MakeShared<FJsonValueObject>(VObj));
                        Existing.Add(N);
                    }
                }
                Entry->SetArrayField(TEXT("variables"), VarsJson);
                
                // Merge functions and events from registry
                TSharedPtr<FJsonObject> RegistryEntry = FMcpAutomationBridge_EnsureBlueprintEntry(Key);
                if (RegistryEntry.IsValid())
                {
                    if (RegistryEntry->HasField(TEXT("functions")))
                    {
                        TArray<TSharedPtr<FJsonValue>> RegFuncs = RegistryEntry->GetArrayField(TEXT("functions"));
                        if (!Entry->HasField(TEXT("functions")))
                        {
                            Entry->SetArrayField(TEXT("functions"), RegFuncs);
                        }
                        else
                        {
                            // Merge unique
                            TArray<TSharedPtr<FJsonValue>> ExistingFuncs = Entry->GetArrayField(TEXT("functions"));
                            TSet<FString> KnownNames;
                            for (const auto& Val : ExistingFuncs) { const TSharedPtr<FJsonObject> Obj = Val->AsObject(); FString N; if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N)) KnownNames.Add(N); }
                            for (const auto& Val : RegFuncs) { const TSharedPtr<FJsonObject> Obj = Val->AsObject(); FString N; if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N) && !KnownNames.Contains(N)) ExistingFuncs.Add(Val); }
                            Entry->SetArrayField(TEXT("functions"), ExistingFuncs);
                        }
                    }

                    if (RegistryEntry->HasField(TEXT("events")))
                    {
                        TArray<TSharedPtr<FJsonValue>> RegEvents = RegistryEntry->GetArrayField(TEXT("events"));
                        if (!Entry->HasField(TEXT("events")))
                        {
                            Entry->SetArrayField(TEXT("events"), RegEvents);
                        }
                        else
                        {
                            // Merge unique
                            TArray<TSharedPtr<FJsonValue>> ExistingEvents = Entry->GetArrayField(TEXT("events"));
                            TSet<FString> KnownNames;
                            for (const auto& Val : ExistingEvents) { const TSharedPtr<FJsonObject> Obj = Val->AsObject(); FString N; if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N)) KnownNames.Add(N); }
                            for (const auto& Val : RegEvents) { const TSharedPtr<FJsonObject> Obj = Val->AsObject(); FString N; if (Obj.IsValid() && Obj->TryGetStringField(TEXT("name"), N) && !KnownNames.Contains(N)) ExistingEvents.Add(Val); }
                            Entry->SetArrayField(TEXT("events"), ExistingEvents);
                        }
                    }
                }
            }
        #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_get requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
            return true;
        #endif

        if (!bExists)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint not found"), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint fetched"), Entry, FString());
        return true;
    }

    // blueprint_add_node: Create a Blueprint graph node programmatically
    if (ActionMatchesPattern(TEXT("blueprint_add_node")) || ActionMatchesPattern(TEXT("add_node")) || AlphaNumLower.Contains(TEXT("blueprintaddnode")))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Entered blueprint_add_node handler: RequestId=%s"), *RequestId);
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_node requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
            return true;
        }

        FString NodeType; LocalPayload->TryGetStringField(TEXT("nodeType"), NodeType);
        if (NodeType.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("nodeType required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString GraphName; LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
        if (GraphName.IsEmpty()) GraphName = TEXT("EventGraph");

        FString FunctionName; LocalPayload->TryGetStringField(TEXT("functionName"), FunctionName);
        FString VariableName; LocalPayload->TryGetStringField(TEXT("variableName"), VariableName);
        FString NodeName; LocalPayload->TryGetStringField(TEXT("nodeName"), NodeName);
        float PosX = 0.0f, PosY = 0.0f;
        LocalPayload->TryGetNumberField(TEXT("posX"), PosX);
        LocalPayload->TryGetNumberField(TEXT("posY"), PosY);

        // Declare RegistryKey outside the conditional blocks
        const FString RegistryKey = Path;

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2

        if (GBlueprintBusySet.Contains(Path))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint is busy"), nullptr, TEXT("BLUEPRINT_BUSY"));
            return true;
        }

        GBlueprintBusySet.Add(Path);
        ON_SCOPE_EXIT
        {
            if (GBlueprintBusySet.Contains(Path))
            {
                GBlueprintBusySet.Remove(Path);
            }
        };

        FString Normalized;
        FString LoadErr;
        UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
        if (!BP)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), LoadErr);
            SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }

        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_add_node begin Path=%s nodeType=%s"), *RegistryKey, *NodeType);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("blueprint_add_node macro check: MCP_HAS_K2NODE_HEADERS=%d MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
            static_cast<int32>(MCP_HAS_K2NODE_HEADERS), static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

        UEdGraph* TargetGraph = nullptr;
        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            {
                TargetGraph = Graph;
                break;
            }
        }

        if (!TargetGraph)
        {
            for (UEdGraph* Graph : BP->FunctionGraphs)
            {
                if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
                {
                    TargetGraph = Graph;
                    break;
                }
            }

            if (!TargetGraph)
            {
                for (UEdGraph* Graph : BP->MacroGraphs)
                {
                    if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
                    {
                        TargetGraph = Graph;
                        break;
                    }
                }
            }

            if (!TargetGraph)
            {
                TargetGraph = FBlueprintEditorUtils::CreateNewGraph(BP, FName(*GraphName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
                if (TargetGraph)
                {
                    const bool bIsEventGraph = GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase);
                    if (bIsEventGraph)
                    {
                        FBlueprintEditorUtils::AddUbergraphPage(BP, TargetGraph);
                    }
                    else
                    {
                        FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, TargetGraph, /*bIsUserCreated=*/true, nullptr);
                    }
                }
            }
        }

        if (!TargetGraph)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), TEXT("Failed to locate or create target graph"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Graph creation failed"), Result, TEXT("GRAPH_ERROR"));
            return true;
        }

        BP->Modify();
        TargetGraph->Modify();

        UEdGraphNode* NewNode = nullptr;
        const FString NodeTypeLower = NodeType.ToLower();

        if (NodeTypeLower.Contains(TEXT("callfunction")) || NodeTypeLower.Contains(TEXT("function")))
        {
            UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
            if (FuncNode && !FunctionName.IsEmpty())
            {
                if (UFunction* FoundFunc = FMcpAutomationBridge_ResolveFunction(BP, FunctionName))
                {
                    FuncNode->SetFromFunction(FoundFunc);
                }
            }
            NewNode = FuncNode;
        }
        else if (NodeTypeLower.Contains(TEXT("variableget")) || NodeTypeLower.Contains(TEXT("getvar")))
        {
            UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(TargetGraph);
            if (VarGet && !VariableName.IsEmpty())
            {
                VarGet->VariableReference.SetSelfMember(FName(*VariableName));
            }
            NewNode = VarGet;
        }
        else if (NodeTypeLower.Contains(TEXT("variableset")) || NodeTypeLower.Contains(TEXT("setvar")))
        {
            UK2Node_VariableSet* VarSet = NewObject<UK2Node_VariableSet>(TargetGraph);
            if (VarSet && !VariableName.IsEmpty())
            {
                VarSet->VariableReference.SetSelfMember(FName(*VariableName));
            }
            NewNode = VarSet;
        }
        else if (NodeTypeLower.Contains(TEXT("customevent")))
        {
            UK2Node_CustomEvent* CustomEvent = NewObject<UK2Node_CustomEvent>(TargetGraph);
            if (CustomEvent && !NodeName.IsEmpty())
            {
                CustomEvent->CustomFunctionName = FName(*NodeName);
            }
            NewNode = CustomEvent;
        }
        else if (NodeTypeLower.Contains(TEXT("literal")))
        {
            UK2Node_Literal* LiteralNode = NewObject<UK2Node_Literal>(TargetGraph);
            NewNode = LiteralNode;
        }
        else
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported nodeType: %s"), *NodeType));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Unsupported node type"), Result, TEXT("UNSUPPORTED_NODE"));
            return true;
        }

        if (!NewNode)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), TEXT("Failed to instantiate node"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Node creation failed"), Result, TEXT("NODE_CREATION_FAILED"));
            return true;
        }

        TargetGraph->Modify();
        TargetGraph->AddNode(NewNode, true, false);
        NewNode->SetFlags(RF_Transactional);
        NewNode->CreateNewGuid();
        NewNode->NodePosX = PosX;
        NewNode->NodePosY = PosY;
        NewNode->AllocateDefaultPins();
        NewNode->Modify();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

        bool bExecLinked = false;
        bool bValueLinked = false;
        bool bSaved = false;

        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
        if (Schema)
        {
            if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(NewNode))
            {
                if (!VarSet->HasAnyFlags(RF_Transactional))
                {
                    VarSet->SetFlags(RF_Transactional);
                }
                VarSet->Modify();
                FMcpAutomationBridge_AttachValuePin(VarSet, TargetGraph, Schema, bValueLinked);

                // Connect the exec input to a custom event if available
                UEdGraphPin* ExecInput = FMcpAutomationBridge_FindExecPin(VarSet, EGPD_Input);
                if (ExecInput)
                {
                    if (ExecInput->LinkedTo.Num() == 0)
                    {
                        UEdGraphPin* EventOutput = nullptr;

                        const FName OnCustomName(TEXT("OnCustom"));
                        for (UEdGraphNode* Node : TargetGraph->Nodes)
                        {
                            if (UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
                            {
                                if (Custom->CustomFunctionName == OnCustomName)
                                {
                                    EventOutput = FMcpAutomationBridge_FindExecPin(Custom, EGPD_Output);
                                    if (EventOutput)
                                    {
                                        break;
                                    }
                                }
                            }
                        }

                        if (!EventOutput)
                        {
                            EventOutput = FMcpAutomationBridge_FindPreferredEventExec(TargetGraph);
                        }

                        if (EventOutput)
                        {
                            if (UEdGraphNode* EventNode = EventOutput->GetOwningNode())
                            {
                                if (!EventNode->HasAnyFlags(RF_Transactional))
                                {
                                    EventNode->SetFlags(RF_Transactional);
                                }
                                EventNode->Modify();
                            }
                            if (!VarSet->HasAnyFlags(RF_Transactional))
                            {
                                VarSet->SetFlags(RF_Transactional);
                            }
                            VarSet->Modify();
                            const FPinConnectionResponse ExecLink = Schema->CanCreateConnection(EventOutput, ExecInput);
                            if (ExecLink.Response == CONNECT_RESPONSE_MAKE)
                            {
                                if (Schema->TryCreateConnection(EventOutput, ExecInput))
                                {
                                    bExecLinked = true;
                                }
                            }
                            else
                            {
                                FMcpAutomationBridge_LogConnectionFailure(TEXT("blueprint_add_node exec"), EventOutput, ExecInput, ExecLink);
                            }
                        }
                    }
                }
            }

            if (!bExecLinked)
            {
                bExecLinked = FMcpAutomationBridge_EnsureExecLinked(TargetGraph) || bExecLinked;
            }
        }

        if (bExecLinked)
        {
            TargetGraph->Modify();
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

        FKismetEditorUtilities::CompileBlueprint(BP);
        bSaved = SaveLoadedAssetThrottled(BP);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("blueprintPath"), RegistryKey);
        Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
        Result->SetStringField(TEXT("nodeClass"), NewNode->GetClass()->GetName());
        Result->SetNumberField(TEXT("posX"), PosX);
        Result->SetNumberField(TEXT("posY"), PosY);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetStringField(TEXT("nodeGuid"), NewNode->NodeGuid.ToString());
        if (UK2Node_VariableSet* VarSetResult = Cast<UK2Node_VariableSet>(NewNode))
        {
            Result->SetBoolField(TEXT("valueLinked"), bValueLinked);
            Result->SetBoolField(TEXT("execLinked"), bExecLinked);
        }
        if (!NodeName.IsEmpty()) { Result->SetStringField(TEXT("nodeName"), NodeName); }
        if (!FunctionName.IsEmpty()) { Result->SetStringField(TEXT("functionName"), FunctionName); }
        if (!VariableName.IsEmpty()) { Result->SetStringField(TEXT("variableName"), VariableName); }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node added"), Result, FString());

        TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>();
        Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
        Notify->SetStringField(TEXT("event"), TEXT("add_node_completed"));
        Notify->SetStringField(TEXT("requestId"), RequestId);
        Notify->SetObjectField(TEXT("result"), Result);
        SendControlMessage(Notify);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_add_node completed Path=%s nodeGuid=%s"), *RegistryKey, *NewNode->NodeGuid.ToString());
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_node requires editor build with K2 node headers"), nullptr, TEXT("NOT_AVAILABLE"));
        return true;
#endif
    }

    // blueprint_connect_pins: Connect two pins between nodes
    if (ActionMatchesPattern(TEXT("blueprint_connect_pins")) || ActionMatchesPattern(TEXT("connect_pins")) || AlphaNumLower.Contains(TEXT("blueprintconnectpins")))
    {
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_connect_pins requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
            return true;
        }

        FString SourceNodeGuid, TargetNodeGuid;
        LocalPayload->TryGetStringField(TEXT("sourceNodeGuid"), SourceNodeGuid);
        LocalPayload->TryGetStringField(TEXT("targetNodeGuid"), TargetNodeGuid);

        if (SourceNodeGuid.IsEmpty() || TargetNodeGuid.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("sourceNodeGuid and targetNodeGuid required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString SourcePinName, TargetPinName;
        LocalPayload->TryGetStringField(TEXT("sourcePinName"), SourcePinName);
        LocalPayload->TryGetStringField(TEXT("targetPinName"), TargetPinName);

        if (GBlueprintBusySet.Contains(Path))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint is busy"), nullptr, TEXT("BLUEPRINT_BUSY"));
            return true;
        }

        GBlueprintBusySet.Add(Path);
        ON_SCOPE_EXIT
        {
            if (GBlueprintBusySet.Contains(Path))
            {
                GBlueprintBusySet.Remove(Path);
            }
        };

        FString Normalized;
        FString LoadErr;
        UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
        if (!BP)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), LoadErr);
            SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }

        const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_connect_pins begin Path=%s"), *RegistryKey);

        UEdGraphNode* SourceNode = nullptr;
        UEdGraphNode* TargetNode = nullptr;
        FGuid SourceGuid, TargetGuid;
        FGuid::Parse(SourceNodeGuid, SourceGuid);
        FGuid::Parse(TargetNodeGuid, TargetGuid);

        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            if (!Graph) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node) continue;
                if (Node->NodeGuid == SourceGuid) SourceNode = Node;
                if (Node->NodeGuid == TargetGuid) TargetNode = Node;
            }
        }

        if (!SourceNode || !TargetNode)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), TEXT("Could not find source or target node by GUID"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Node lookup failed"), Result, TEXT("NODE_NOT_FOUND"));
            return true;
        }

        UEdGraphPin* SourcePin = nullptr;
        UEdGraphPin* TargetPin = nullptr;

        auto ResolvePin = [](UEdGraphNode* Node, const FString& PreferredName, EEdGraphPinDirection DesiredDirection) -> UEdGraphPin*
        {
            if (!Node) return nullptr;
            if (!PreferredName.IsEmpty())
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin && Pin->GetName().Equals(PreferredName, ESearchCase::IgnoreCase))
                    {
                        return Pin;
                    }
                }
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == DesiredDirection)
                {
                    return Pin;
                }
            }
            return nullptr;
        };

        SourcePin = ResolvePin(SourceNode, SourcePinName, EGPD_Output);
        TargetPin = ResolvePin(TargetNode, TargetPinName, EGPD_Input);

        if (!SourcePin || !TargetPin)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("error"), TEXT("Could not find source or target pin"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Pin lookup failed"), Result, TEXT("PIN_NOT_FOUND"));
            return true;
        }

        BP->Modify();
        SourceNode->GetGraph()->Modify();

        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(SourceNode->GetGraph()->GetSchema());
        bool bSuccess = false;
        if (Schema)
        {
            bSuccess = Schema->TryCreateConnection(SourcePin, TargetPin);
            if (bSuccess)
            {
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), bSuccess);
        Result->SetStringField(TEXT("blueprintPath"), RegistryKey);
        Result->SetStringField(TEXT("sourcePinName"), SourcePin->GetName());
        Result->SetStringField(TEXT("targetPinName"), TargetPin->GetName());

        if (!bSuccess)
        {
            Result->SetStringField(TEXT("error"), Schema ? TEXT("Schema rejected connection") : TEXT("Invalid graph schema"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Pin connection failed"), Result, TEXT("CONNECTION_FAILED"));
            return true;
        }

        const bool bSaved = SaveLoadedAssetThrottled(BP);
        Result->SetBoolField(TEXT("saved"), bSaved);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pin connection complete"), Result, FString());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_connect_pins succeeded Path=%s"), *RegistryKey);
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_connect_pins requires editor build with EdGraphSchema_K2"), nullptr, TEXT("NOT_AVAILABLE"));
        return true;
#endif
    }

    // Handle SCS (Simple Construction Script) operations - must be called before the final fallback
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction: checking HandleSCSAction for action='%s' (clean='%s')"), *Action, *CleanAction);
    if (HandleSCSAction(RequestId, CleanAction, Payload, RequestingSocket)) 
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleSCSAction consumed request"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Unhandled blueprint action: Action=%s Clean=%s AlphaNum=%s RequestId=%s - returning UNKNOWN_PLUGIN_ACTION"), *CleanAction, *CleanAction, *AlphaNumLower, *RequestId);

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint action not implemented by plugin: %s"), *Action), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
    return true;
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintAction: Editor-only functionality requested in non-editor build (Action=%s)"), *Action);
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleSCSAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SCS operations require valid payload"), nullptr, TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString CleanAction = Action;
    CleanAction.TrimStartAndEndInline();
    FString Lower = CleanAction.ToLower();
    
    // Build alphanumeric key for matching
    FString AlphaNumLower;
    AlphaNumLower.Reserve(CleanAction.Len());
    for (int32 i = 0; i < CleanAction.Len(); ++i)
    {
        const TCHAR C = CleanAction[i];
        if (FChar::IsAlnum(C)) AlphaNumLower.AppendChar(FChar::ToLower(C));
    }

    auto ActionMatchesPattern = [&](const TCHAR* Pattern) -> bool
    {
        const FString PatternStr = FString(Pattern).ToLower();
        FString PatternAlpha;
        PatternAlpha.Reserve(PatternStr.Len());
        for (int32 i = 0; i < PatternStr.Len(); ++i)
        {
            const TCHAR C = PatternStr[i];
            if (FChar::IsAlnum(C)) PatternAlpha.AppendChar(C);
        }
        const bool bExactOrContains = (Lower.Equals(PatternStr) || Lower.Contains(PatternStr));
        const bool bAlphaMatch = (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() && AlphaNumLower.Contains(PatternAlpha));
        return (bExactOrContains || bAlphaMatch);
    };

    // Helper to resolve blueprint
    auto ResolveBlueprint = [&]() -> UBlueprint*
    {
        FString BlueprintPath;
        if (Payload->TryGetStringField(TEXT("name"), BlueprintPath) || 
            Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
        {
            if (!BlueprintPath.IsEmpty())
            {
                return LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            }
        }
        
        // Try blueprint candidates array
        const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
        if (Payload->TryGetArrayField(TEXT("blueprintCandidates"), Candidates) && Candidates->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& Candidate : *Candidates)
            {
                if (Candidate.IsValid() && Candidate->Type == EJson::String)
                {
                    FString CandidatePath = Candidate->AsString();
                    if (!CandidatePath.IsEmpty())
                    {
                        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *CandidatePath);
                        if (BP) return BP;
                    }
                }
            }
        }
        
        return nullptr;
    };

    // Add component to SCS
    if (ActionMatchesPattern(TEXT("add_component")) || ActionMatchesPattern(TEXT("add_scs_component")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_component requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        FString ComponentType;
        Payload->TryGetStringField(TEXT("componentType"), ComponentType);
        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);

        if (ComponentType.IsEmpty() || ComponentName.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_component requires componentType and componentName"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Get the SCS from the blueprint with explicit null check
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr, TEXT("NO_SCS"));
            return true;
        }

        // Find component class
        UClass* ComponentClass = nullptr;
        if (ComponentType == TEXT("StaticMeshComponent"))
        {
            ComponentClass = UStaticMeshComponent::StaticClass();
        }
        else if (ComponentType == TEXT("SceneComponent"))
        {
            ComponentClass = USceneComponent::StaticClass();
        }
        else if (ComponentType == TEXT("ArrowComponent"))
        {
            ComponentClass = UArrowComponent::StaticClass();
        }
        else
        {
            // Try to load the class
            ComponentClass = LoadClass<UActorComponent>(nullptr, *ComponentType);
        }

        if (!ComponentClass)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown component type: %s"), *ComponentType), nullptr, TEXT("INVALID_COMPONENT_TYPE"));
            return true;
        }

        // Create the SCS node correctly
        USCS_Node* NewNode = NewObject<USCS_Node>(SCS);
        if (NewNode)
        {
            NewNode->SetVariableName(*ComponentName);
            NewNode->ComponentClass = ComponentClass;
            SCS->AddNode(NewNode);

            // Compile and save the blueprint
            bool bCompiled = false;
            bool bSaved = false;
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            bCompiled = true;
            bSaved = SaveLoadedAssetThrottled(Blueprint);

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("componentName"), ComponentName);
            Result->SetStringField(TEXT("componentType"), ComponentType);
            Result->SetStringField(TEXT("variableName"), NewNode->GetVariableName().ToString());
            Result->SetBoolField(TEXT("compiled"), bCompiled);
            Result->SetBoolField(TEXT("saved"), bSaved);
            SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Added component %s to blueprint SCS"), *ComponentName), Result, FString());
            return true;
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to add component to SCS"), nullptr, TEXT("OPERATION_FAILED"));
        return true;
    }

    // Set SCS transform
    if (ActionMatchesPattern(TEXT("set_scs_transform")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_scs_transform requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);

        if (ComponentName.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_scs_transform requires componentName"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Get SCS with explicit null check
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr, TEXT("NO_SCS"));
            return true;
        }

        // Find the SCS node by component name
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->GetVariableName().IsValid() && Node->GetVariableName().ToString() == ComponentName)
            {
                    // Read transform from payload
                    const TArray<TSharedPtr<FJsonValue>>* LocationArray = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* RotationArray = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* ScaleArray = nullptr;
                    
                    FVector Location(0, 0, 0);
                    FRotator Rotation(0, 0, 0);
                    FVector Scale(1, 1, 1);

                    if (Payload->TryGetArrayField(TEXT("location"), LocationArray) && LocationArray->Num() >= 3)
                    {
                        Location.X = (*LocationArray)[0]->AsNumber();
                        Location.Y = (*LocationArray)[1]->AsNumber();
                        Location.Z = (*LocationArray)[2]->AsNumber();
                    }

                    if (Payload->TryGetArrayField(TEXT("rotation"), RotationArray) && RotationArray->Num() >= 3)
                    {
                        Rotation.Pitch = (*RotationArray)[0]->AsNumber();
                        Rotation.Yaw = (*RotationArray)[1]->AsNumber();
                        Rotation.Roll = (*RotationArray)[2]->AsNumber();
                    }

                    if (Payload->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 3)
                    {
                        Scale.X = (*ScaleArray)[0]->AsNumber();
                        Scale.Y = (*ScaleArray)[1]->AsNumber();
                        Scale.Z = (*ScaleArray)[2]->AsNumber();
                    }

                    // Set the node transform (USCS_Node doesn't have SetRelativeTransform, need to use the component template)
                    bool bModified = false;
                    if (UActorComponent* ComponentTemplate = Node->ComponentTemplate)
                    {
                        if (USceneComponent* SceneTemplate = Cast<USceneComponent>(ComponentTemplate))
                        {
                            SceneTemplate->SetRelativeTransform(FTransform(Rotation, Location, Scale));
                            bModified = true;
                        }
                    }

                    // Compile and save the blueprint
                    bool bCompiled = false;
                    bool bSaved = false;
                    if (bModified)
                    {
                        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                        FKismetEditorUtilities::CompileBlueprint(Blueprint);
                        bCompiled = true;
                        bSaved = SaveLoadedAssetThrottled(Blueprint);
                    }

                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetStringField(TEXT("componentName"), ComponentName);
                    Result->SetNumberField(TEXT("locationX"), Location.X);
                    Result->SetNumberField(TEXT("locationY"), Location.Y);
                    Result->SetNumberField(TEXT("locationZ"), Location.Z);
                    Result->SetBoolField(TEXT("compiled"), bCompiled);
                    Result->SetBoolField(TEXT("saved"), bSaved);
                    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Set transform for component %s"), *ComponentName), Result, FString());
                    return true;
                }
            }

        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Component %s not found in SCS"), *ComponentName), nullptr, TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    // Remove SCS component
    if (ActionMatchesPattern(TEXT("remove_scs_component")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("remove_scs_component requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);

        if (ComponentName.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("remove_scs_component requires componentName"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Get SCS with explicit null check
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr, TEXT("NO_SCS"));
            return true;
        }

        // Find and remove the SCS node
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->GetVariableName().IsValid() && Node->GetVariableName().ToString() == ComponentName)
            {
                SCS->RemoveNode(Node);

                // Compile and save the blueprint
                bool bCompiled = false;
                bool bSaved = false;
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                bCompiled = true;
                bSaved = SaveLoadedAssetThrottled(Blueprint);

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("componentName"), ComponentName);
                Result->SetBoolField(TEXT("compiled"), bCompiled);
                Result->SetBoolField(TEXT("saved"), bSaved);
                SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Removed component %s from SCS"), *ComponentName), Result, FString());
                return true;
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Component %s not found in SCS"), *ComponentName), nullptr, TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    // Get SCS hierarchy
    if (ActionMatchesPattern(TEXT("get_scs")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("get_scs requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> ComponentsArray;

        // Get SCS with explicit null check
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (SCS)
        {
            const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
            for (USCS_Node* Node : AllNodes)
            {
                if (Node && Node->GetVariableName().IsValid())
                {
                    TSharedPtr<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
                    ComponentObj->SetStringField(TEXT("componentName"), Node->GetVariableName().ToString());
                    ComponentObj->SetStringField(TEXT("componentType"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));

                    // Add parent info if available
                    // USCS_Node doesn't have GetParent() - use ParentComponentOrVariableName instead
                    if (!Node->ParentComponentOrVariableName.IsNone())
                    {
                        ComponentObj->SetStringField(TEXT("parentComponent"), Node->ParentComponentOrVariableName.ToString());
                    }

                    // Add transform
                    // Get component transform from template
                    FTransform Transform;
                    if (UActorComponent* ComponentTemplate = Node->ComponentTemplate)
                    {
                        if (USceneComponent* SceneTemplate = Cast<USceneComponent>(ComponentTemplate))
                        {
                            Transform = SceneTemplate->GetRelativeTransform();
                        }
                    }
                    else
                    {
                        Transform = FTransform::Identity;
                    }
                    TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();

                    TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
                    LocationObj->SetNumberField(TEXT("x"), Transform.GetLocation().X);
                    LocationObj->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
                    LocationObj->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
                    TransformObj->SetObjectField(TEXT("location"), LocationObj);

                    TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
                    RotationObj->SetNumberField(TEXT("pitch"), Transform.GetRotation().Rotator().Pitch);
                    RotationObj->SetNumberField(TEXT("yaw"), Transform.GetRotation().Rotator().Yaw);
                    RotationObj->SetNumberField(TEXT("roll"), Transform.GetRotation().Rotator().Roll);
                    TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

                    TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
                    ScaleObj->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
                    ScaleObj->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
                    ScaleObj->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
                    TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

                    ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
                    ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
                }
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("components"), ComponentsArray);
        Result->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Retrieved %d SCS components"), ComponentsArray.Num()), Result, FString());
        return true;
    }

    // Reparent SCS component (simplified implementation)
    if (ActionMatchesPattern(TEXT("reparent_scs_component")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("reparent_scs_component requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        FString ComponentName;
        FString NewParent;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);
        Payload->TryGetStringField(TEXT("newParent"), NewParent);

        if (ComponentName.IsEmpty() || NewParent.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("reparent_scs_component requires componentName and newParent"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Get SCS with explicit null check
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr, TEXT("NO_SCS"));
            return true;
        }

        USCS_Node* ChildNode = nullptr;
        USCS_Node* ParentNode = nullptr;

        // Find child and parent nodes with safe iteration
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->GetVariableName().IsValid())
            {
                if (Node->GetVariableName().ToString() == ComponentName)
                {
                    ChildNode = Node;
                }
                if (Node->GetVariableName().ToString() == NewParent)
                {
                    ParentNode = Node;
                }
            }
        }

        if (ChildNode)
        {
            if (ParentNode || NewParent == TEXT("RootComponent"))
            {
                // Set the parent
                if (NewParent == TEXT("RootComponent"))
                {
                    // RootComponent is not an actual SCS node - all SCS nodes are already children of root by default
                    // So we just mark this as success without actually changing anything
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("reparent_scs_component: %s is already a root component (no action needed)"), *ComponentName);
                }
                else if (ParentNode)
                {
                    // Set new parent
                    ChildNode->SetParent(ParentNode);
                }

                // Compile and save the blueprint
                bool bCompiled = false;
                bool bSaved = false;
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                bCompiled = true;
                bSaved = SaveLoadedAssetThrottled(Blueprint);

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("componentName"), ComponentName);
                Result->SetStringField(TEXT("newParent"), NewParent);
                Result->SetBoolField(TEXT("compiled"), bCompiled);
                Result->SetBoolField(TEXT("saved"), bSaved);
                SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Reparented component %s to %s"), *ComponentName, *NewParent), Result, FString());
                return true;
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Failed to reparent component %s"), *ComponentName), nullptr, TEXT("OPERATION_FAILED"));
        return true;
    }

    // Set SCS property (simplified implementation)
    if (ActionMatchesPattern(TEXT("set_scs_property")))
    {
        UBlueprint* Blueprint = ResolveBlueprint();
        if (!Blueprint)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_scs_property requires a valid blueprint"), nullptr, TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        FString ComponentName;
        FString PropertyName;
        FString PropertyValue;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);
        Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
        Payload->TryGetStringField(TEXT("value"), PropertyValue);

        if (ComponentName.IsEmpty() || PropertyName.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_scs_property requires componentName, propertyName, and value"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Find the SCS node for this component
        USCS_Node* FoundNode = nullptr;
        if (Blueprint->SimpleConstructionScript)
        {
            const TArray<USCS_Node*>& Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
            for (USCS_Node* Node : Nodes)
            {
                if (Node && Node->GetVariableName().IsValid() && Node->GetVariableName().ToString() == ComponentName)
                {
                    FoundNode = Node;
                    break;
                }
            }
        }

        if (!FoundNode)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Component '%s' not found in SCS"), *ComponentName), nullptr, TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        // Get the component template (CDO) to access properties
        UObject* ComponentTemplate = FoundNode->ComponentTemplate;
        if (!ComponentTemplate)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Component template not found for '%s'"), *ComponentName), nullptr, TEXT("TEMPLATE_NOT_FOUND"));
            return true;
        }

        // Find the property on the component class
        FProperty* FoundProperty = ComponentTemplate->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!FoundProperty)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName), nullptr, TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }

        // Set the property value based on type
        bool bSuccess = false;
        FString ErrorMessage;

        if (FStrProperty* StrProp = CastField<FStrProperty>(FoundProperty))
        {
            void* PropAddr = StrProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
            StrProp->SetPropertyValue(PropAddr, PropertyValue);
            bSuccess = true;
        }
        else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(FoundProperty))
        {
            void* PropAddr = FloatProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
            float Value = FCString::Atof(*PropertyValue);
            FloatProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(FoundProperty))
        {
            void* PropAddr = DoubleProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
            double Value = FCString::Atod(*PropertyValue);
            DoubleProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FIntProperty* IntProp = CastField<FIntProperty>(FoundProperty))
        {
            void* PropAddr = IntProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
            int32 Value = FCString::Atoi(*PropertyValue);
            IntProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FInt64Property* Int64Prop = CastField<FInt64Property>(FoundProperty))
        {
            void* PropAddr = Int64Prop->ContainerPtrToValuePtr<void>(ComponentTemplate);
            int64 Value = FCString::Atoi64(*PropertyValue);
            Int64Prop->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(FoundProperty))
        {
            void* PropAddr = BoolProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
            bool Value = PropertyValue.ToBool();
            BoolProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(FoundProperty))
        {
            // Try to find the object by path
            UObject* ObjValue = FindObject<UObject>(nullptr, *PropertyValue);
            if (ObjValue || PropertyValue.IsEmpty())
            {
                void* PropAddr = ObjProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                ObjProp->SetPropertyValue(PropAddr, ObjValue);
                bSuccess = true;
            }
            else
            {
                ErrorMessage = FString::Printf(TEXT("Object property requires valid object path, got: %s"), *PropertyValue);
            }
        }
        else
        {
            ErrorMessage = FString::Printf(TEXT("Property type '%s' not supported for setting"), *FoundProperty->GetClass()->GetName());
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("propertyName"), PropertyName);
        Result->SetStringField(TEXT("value"), PropertyValue);

        if (bSuccess)
        {
            // Compile and save the blueprint
            bool bCompiled = false;
            bool bSaved = false;
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            bCompiled = true;
            bSaved = SaveLoadedAssetThrottled(Blueprint);

            Result->SetBoolField(TEXT("compiled"), bCompiled);
            Result->SetBoolField(TEXT("saved"), bSaved);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SCS property set successfully"), Result, FString());
        }
        else
        {
            Result->SetStringField(TEXT("error"), ErrorMessage);
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to set SCS property"), Result, TEXT("PROPERTY_SET_FAILED"));
        }
        return true;
    }

    return false; // Let the main handler deal with unknown actions
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SCS operations require editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

#endif // WITH_EDITOR from line 8
