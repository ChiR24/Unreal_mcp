// inspect logpoints, the listening half (see McpLogpoints.h): a tracepoint before a marked node opens a run with the
// node's inputs (the compiler puts the code of the pure nodes feeding them before the tracepoint, so they are this run's);
// the wire trace after it adds its outputs. The compiler emits that wire trace as one of the node's own statements
// (GenerateSimpleThenGoto, even when its exec output leads nowhere) and with no exec pin attached, so both traces are
// mapped back by node. A node that has not finished (a latent one still waiting) keeps only its inputs. Play only:
// Simulate (settle_physics runs one) neither records nor empties the timelines.
#include "Domains/Debug/Logpoints/McpLogpoints.h"

#include "Core/Compatibility/McpVersionCompatibility.h" // MCP_PROPERTY_EXPORT_TEXT
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
#include "Blueprint/BlueprintExceptionInfo.h" // in UObject/Script.h before 5.4
#endif

namespace McpLogpoints
{
namespace LogpointTraceParts
{
constexpr int32 MaxValueChars = 200;
FDelegateHandle GExceptionHandle;
FDelegateHandle GBeginPieHandle;

// The output an input takes its value from, past reroute knots (a knot holds no value of its own; the compiler keeps the
// value under the pin the knots lead back to).
const UEdGraphPin* ValueSource(const UEdGraphPin* Pin)
{
    const UEdGraphPin* Source = Pin;
    for (int32 Hops = 0; Hops < 64 && Source->Direction == EGPD_Input && Source->LinkedTo.Num() > 0 && Source->LinkedTo[0]; ++Hops)
    {
        const UEdGraphPin* Out = Source->LinkedTo[0];
        const UEdGraphPin* Through = Out->GetOwningNode() ? Out->GetOwningNode()->GetPassThroughPin(Out) : nullptr;
        if (!Through)
        {
            return Out;
        }
        Source = Through;
    }
    return Source;
}

// A pin's value as this frame holds it: an input reads the output wired into it (the value it takes), an unwired input
// its literal; the value lives in the running function's locals (the event graph's are its persistent frame), else in the
// object itself (a variable, which a Get node reads).
FString PinValue(UBlueprint* Blueprint, const UObject* Object, const FFrame& Frame, const UEdGraphPin* Pin)
{
    const UEdGraphPin* Source = ValueSource(Pin);
    const FProperty* Property = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, Source);
    if (!Property)
    {
        return Source == Pin && Pin->Direction == EGPD_Input ? Pin->GetDefaultAsString() : FString(TEXT("(not kept by the compiler)"));
    }
    const UClass* Owner = Property->GetOwner<UClass>();
    const void* Base = Property->IsIn(Frame.Node) ? static_cast<const void*>(Frame.Locals)
        : (Owner && Object->GetClass()->IsChildOf(Owner)) ? static_cast<const void*>(Object) : nullptr;
    if (!Base)
    {
        return TEXT("(out of scope here)");
    }
    FString Text;
    MCP_PROPERTY_EXPORT_TEXT(Property, Text, Property->ContainerPtrToValuePtr<void>(const_cast<void*>(Base)), nullptr, nullptr, PPF_None);
    return Text.Len() > MaxValueChars ? Text.Left(MaxValueChars) + TEXT("...") : Text;
}

TSharedPtr<FJsonObject> ReadPins(const FPoint& Point, const UObject* Object, const FFrame& Frame, EEdGraphPinDirection Direction)
{
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    const UEdGraphNode* Node = Point.Node.Get();
    UBlueprint* Blueprint = Point.Blueprint.Get();
    // By pin name, as edit_graph and the graph code take pins; an unwired self is the object the run is on.
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        const bool bData = Pin && Pin->Direction == Direction && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !Pin->bHidden;
        if (bData && !(Pin->PinName == UEdGraphSchema_K2::PN_Self && Pin->LinkedTo.Num() == 0))
        {
            Values->SetStringField(Pin->PinName.ToString(), PinValue(Blueprint, Object, Frame, Pin));
        }
    }
    return Values;
}

FPoint* FindPoint(const UBlueprintGeneratedClass* Class, const UEdGraphNode* Node)
{
    return Node ? Points().FindByPredicate([Class, Node](const FPoint& Point) {
        return Point.Node.Get() == Node && Point.Blueprint.IsValid() && Point.Blueprint->GeneratedClass.Get() == Class;
    }) : nullptr;
}

FString ObjectName(const UObject* Object)
{
    const AActor* Actor = Cast<AActor>(Object);
    return Actor ? Actor->GetActorNameOrLabel() : GetNameSafe(Object);
}

void OnScriptException(const UObject* Object, const FFrame& Frame, const FBlueprintExceptionInfo& Info)
{
    // The marks are game-thread state; script run elsewhere (a thread-safe animation function) is not recorded.
    const EBlueprintExceptionType::Type Type = Info.GetType();
    if (!IsInGameThread() || (Type != EBlueprintExceptionType::Tracepoint && Type != EBlueprintExceptionType::WireTracepoint) ||
        !Frame.Node || !Object)
    {
        return;
    }
    // Every node of every Blueprint in Play passes here: what no marked Blueprint owns stops at this cast and compare.
    UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(Frame.Node->GetOuter());
    const auto OwnsIt = [Class](const FPoint& Point) { return Point.Blueprint.IsValid() && Point.Blueprint->GeneratedClass.Get() == Class; };
    if (!Class || !Points().ContainsByPredicate(OwnsIt))
    {
        return;
    }
    const UWorld* World = Object->GetWorld();
    if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game) || (GEditor && GEditor->IsSimulatingInEditor()))
    {
        return;
    }
    // The offset the debug data keys on: Code has already stepped past the trace opcode. Both traces belong to the node.
    const int32 Offset = static_cast<int32>(Frame.Code - Frame.Node->Script.GetData()) - 1;
    FPoint* Point = FindPoint(Class, Class->GetDebugData().FindSourceNodeFromCodeLocation(Frame.Node, Offset, true));
    if (!Point)
    {
        return;
    }
    if (Type == EBlueprintExceptionType::Tracepoint)
    {
        ++Point->Runs;
        if (Point->Hits.Num() >= Point->MaxHits)
        {
            Point->Hits.RemoveAt(0); // ponytail: O(n) shift per run past the cap (500 at most); a ring buffer if that shows
        }
        FHit& Hit = Point->Hits.AddDefaulted_GetRef();
        Hit.GameSeconds = World->GetTimeSeconds();
        Hit.Object = Object;
        Hit.ObjectName = ObjectName(Object);
        Hit.Inputs = ReadPins(*Point, Object, Frame, EGPD_Input);
        return;
    }
    const int32 Last = Point->Hits.FindLastByPredicate([Object](const FHit& Each) { return Each.Object.Get() == Object; });
    if (Last != INDEX_NONE && !Point->Hits[Last].Outputs.IsValid())
    {
        Point->Hits[Last].Outputs = ReadPins(*Point, Object, Frame, EGPD_Output);
    }
}
} // namespace LogpointTraceParts

TArray<FPoint>& Points()
{
    static TArray<FPoint> All;
    return All;
}

void Listen(bool bOn)
{
    using namespace LogpointTraceParts;
    if (bOn && !GExceptionHandle.IsValid())
    {
        GExceptionHandle = FBlueprintCoreDelegates::OnScriptException.AddStatic(&OnScriptException);
        // A play session starts with empty timelines; the last one stays readable until then, as script errors do.
        // Simulate (settle_physics runs one) records nothing, so it leaves them as they are.
        GBeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([](const bool bIsSimulating) {
            if (bIsSimulating)
            {
                return;
            }
            for (FPoint& Point : Points())
            {
                Point.Hits.Reset();
                Point.Runs = 0;
            }
        });
    }
    else if (!bOn && GExceptionHandle.IsValid())
    {
        FBlueprintCoreDelegates::OnScriptException.Remove(GExceptionHandle);
        FEditorDelegates::BeginPIE.Remove(GBeginPieHandle);
        GExceptionHandle.Reset();
        GBeginPieHandle.Reset();
    }
}
} // namespace McpLogpoints
