#include "Domains/Log/McpScriptErrors.h"

#include "Components/ActorComponent.h"
#include "Domains/Log/McpLogHistory.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
#include "Blueprint/BlueprintExceptionInfo.h"  // in UObject/Script.h before 5.4
#endif

namespace
{
constexpr int32 MaxErrors = 64;
constexpr int32 MaxObjectsNamed = 3;
constexpr int32 MaxValues = 10;

FString ObjectLabel(const UObject* Object)
{
    if (const AActor* Actor = Cast<AActor>(Object))
    {
        return Actor->GetActorNameOrLabel();
    }
    const AActor* Owner = Object ? Object->GetTypedOuter<AActor>() : nullptr;
    return Owner ? Owner->GetActorNameOrLabel() + TEXT(".") + Object->GetName() : GetNameSafe(Object);
}

// The variables the object's Blueprints declare, as they were when the error first fired. Component handles are left
// out: what a node read wrong is in the state, not in the parts.
TSharedPtr<FJsonObject> BlueprintValues(const UObject* Object)
{
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Object->GetClass()); It && Values->Values.Num() < MaxValues; ++It)
    {
        FProperty* Property = *It;
        const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
        const bool bComponent = ObjectProperty && ObjectProperty->PropertyClass &&
            ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass());
        if (bComponent || !Cast<UBlueprintGeneratedClass>(Property->GetOwnerClass()) ||
            !Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
        {
            continue;
        }
        // Kept small: an error holds its snapshot for the session, and a large array or string was exported whole.
        const void* Data = Property->ContainerPtrToValuePtr<void>(Object);
        const FArrayProperty* Array = CastField<FArrayProperty>(Property);
        const FMapProperty* Map = CastField<FMapProperty>(Property);
        const FSetProperty* Set = CastField<FSetProperty>(Property);
        const int32 Elements = Array ? FScriptArrayHelper(Array, Data).Num() : Map ? FScriptMapHelper(Map, Data).Num() : Set ? FScriptSetHelper(Set, Data).Num() : 0;
        if (Elements > 16)
        {
            Values->SetStringField(Property->GetName(), FString::Printf(TEXT("%d elements"), Elements));
            continue;
        }
        if (TSharedPtr<FJsonValue> Value =
                McpPropertyReflection::ExportPropertyToJsonValueOrText(const_cast<UObject*>(Object), Property))
        {
            FString Text;
            if (Value->TryGetString(Text) && Text.Len() > 200) Value = MakeShared<FJsonValueString>(Text.Left(200) + TEXT("..."));
            Values->SetField(Property->GetName(), Value);
        }
    }
    return Values->Values.Num() > 0 ? Values : nullptr;
}
} // namespace

FMcpScriptErrors& FMcpScriptErrors::Get()
{
    static FMcpScriptErrors Instance;
    return Instance;
}

void FMcpScriptErrors::Watch(bool bOn)
{
    if (bOn && !ExceptionHandle.IsValid())
    {
        ExceptionHandle = FBlueprintCoreDelegates::OnScriptException.AddRaw(this, &FMcpScriptErrors::OnScriptException);
        // A new session starts empty; the last one stays readable until then (runtime_report after stop).
        BeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([this](const bool)
        {
            Errors.Reset();
            ByCodeLocation.Reset();
        });
    }
    else if (!bOn && ExceptionHandle.IsValid())
    {
        FBlueprintCoreDelegates::OnScriptException.Remove(ExceptionHandle);
        FEditorDelegates::BeginPIE.Remove(BeginPieHandle);
        ExceptionHandle.Reset();
        BeginPieHandle.Reset();
    }
}

// The engine broadcasts script exceptions on the game thread only, where every reader below also runs.
void FMcpScriptErrors::OnScriptException(
    const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
{
    const EBlueprintExceptionType::Type Type = Info.GetType();
    if (Type == EBlueprintExceptionType::Breakpoint || Type == EBlueprintExceptionType::Tracepoint ||
        Type == EBlueprintExceptionType::WireTracepoint || !StackFrame.Node)
    {
        return;
    }
    const UWorld* World = ActiveObject ? ActiveObject->GetWorld() : nullptr;
    if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
    {
        return;
    }
    // The offset FKismetDebugUtilities reads the node from: Code has already stepped past the failing opcode.
    const int32 Offset = static_cast<int32>(StackFrame.Code - StackFrame.Node->Script.GetData()) - 1;
    const TPair<FObjectKey, int32> Key(FObjectKey(StackFrame.Node), Offset);
    if (const int32* Index = ByCodeLocation.Find(Key))
    {
        Record(Errors[*Index], ActiveObject);
        return;
    }
    if (Errors.Num() >= MaxErrors)
    {
        return; // ponytail: a session keeps its first 64 distinct errors; raise MaxErrors if a game ever needs more
    }
    ByCodeLocation.Add(Key, Errors.Num());
    FError& Error = Errors.AddDefaulted_GetRef();
    Error.Message = Info.GetDescription().ToString();
    int32 LineEnd = INDEX_NONE;
    if (Error.Message.FindChar(TEXT('\n'), LineEnd))
    {
        Error.Message.LeftInline(LineEnd);
    }
    Error.Message.TrimStartAndEndInline();
    Error.Function = StackFrame.Node->GetName();
    Error.BlueprintPath = StackFrame.Node->GetOutermost()->GetName();
    if (const UEdGraphNode* Node =
            FKismetDebugUtilities::FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, Offset, true))
    {
        Error.Node = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
        Error.NodeId = Node->NodeGuid.ToString();
        Error.Graph = GetNameSafe(Node->GetGraph());
        if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node))
        {
            Error.BlueprintPath = Blueprint->GetOutermost()->GetName();
        }
    }
    Error.GameSeconds = World->GetTimeSeconds();
    Error.Values = BlueprintValues(ActiveObject);
    Record(Error, ActiveObject);
}

void FMcpScriptErrors::Record(FError& Error, const UObject* ActiveObject)
{
    ++Error.Count;
    if (Error.Times.Num() >= 1024)
    {
        // ponytail: a window counts from the newest 512-1023 hits; keep every time if exact long-run counts matter
        Error.Times.RemoveAt(0, 512);
    }
    Error.Times.Add(FMcpLogHistory::Now());
    if (Error.Objects.Num() < MaxObjectsNamed)
    {
        Error.Objects.AddUnique(ObjectLabel(ActiveObject));
    }
}

TArray<FString> FMcpScriptErrors::LinesSince(double SinceSeconds, TArray<FString>& OutMessages) const
{
    TArray<FString> Lines;
    for (const FError& Error : Errors)
    {
        int32 Hits = 0;
        for (int32 Index = Error.Times.Num() - 1; Index >= 0 && Error.Times[Index] >= SinceSeconds; --Index)
        {
            ++Hits;
        }
        if (Hits == 0)
        {
            continue;
        }
        OutMessages.Add(Error.Message);
        const FString Where = Error.NodeId.IsEmpty()
            ? FString::Printf(TEXT("in function %s of %s"), *Error.Function, *Error.BlueprintPath)
            : FString::Printf(TEXT("at node '%s' (nodeId %s) in %s of %s"), *Error.Node, *Error.NodeId, *Error.Graph,
                  *Error.BlueprintPath);
        const FString Times = Hits > 1 ? FString::Printf(TEXT(" (%dx)"), Hits) : FString();
        Lines.Add(FString::Printf(TEXT("Blueprint runtime error%s: %s, %s, on %s"), *Times, *Error.Message, *Where,
            *FString::Join(Error.Objects, TEXT(", "))));
    }
    return Lines;
}

TArray<TSharedPtr<FJsonValue>> FMcpScriptErrors::ToJson() const
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FError& Error : Errors)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("message"), Error.Message);
        Entry->SetStringField(TEXT("blueprintPath"), Error.BlueprintPath);
        if (Error.NodeId.IsEmpty())
        {
            Entry->SetStringField(TEXT("function"), Error.Function);
        }
        else
        {
            Entry->SetStringField(TEXT("graph"), Error.Graph);
            Entry->SetStringField(TEXT("node"), Error.Node);
            Entry->SetStringField(TEXT("nodeId"), Error.NodeId);
        }
        Entry->SetArrayField(TEXT("objects"), McpHandlerUtils::ToJsonStringArray(Error.Objects));
        Entry->SetNumberField(TEXT("count"), Error.Count);
        Entry->SetNumberField(TEXT("firstGameSeconds"), Error.GameSeconds);
        if (Error.Values.IsValid())
        {
            Entry->SetObjectField(TEXT("values"), Error.Values);
        }
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return Out;
}
