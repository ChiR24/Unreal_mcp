// control_editor fuzz_blueprint (see McpFuzz.h): the calls. Each function is called once with every input at its first
// value, then with one input at a time through its other values, so a failing call is already the smallest change from a
// call that works. What a call raised is caught while it runs, on the game thread inside the call: script exceptions (an
// Accessed None, an endless loop) through the engine's exception delegate, and the warnings math and array nodes log (a
// divide by zero, an index past the end) through the log, each read back to the node running at that moment.
#include "Domains/Play/McpFuzz.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
#include "Blueprint/BlueprintExceptionInfo.h" // in UObject/Script.h before 5.4
#endif

namespace McpFuzz
{
namespace FuzzRunParts
{
constexpr double DeadlineSeconds = 20.0; // the reply comes well inside the 30 s a client waits
constexpr int32 MaxFailedShown = 3;      // per function: after its third failing call its other inputs are left untried
constexpr int32 MaxErrorsShown = 3;      // per failing call

struct FFuzzCaught
{
    FString Message;
    FString Node;
    FString NodeId;
    int32 Count = 1;
};

// The innermost Blueprint frame running: a native node (a divide) runs on the frame of the graph that called it.
const FFrame* TopFrame()
{
#if DO_BLUEPRINT_GUARD
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1
    const TArray<const FFrame*>& Stack = FBlueprintContextTracker::Get().GetScriptStack();
#else
    const TArrayView<const FFrame* const> Stack = FBlueprintContextTracker::Get().GetCurrentScriptStack();
#endif
    return Stack.Num() > 0 ? Stack.Last() : nullptr;
#else
    return nullptr;
#endif
}

class FFuzzCapture final : public FOutputDevice
{
public:
    TArray<FFuzzCaught> Caught;

    FFuzzCapture()
    {
        GLog->AddOutputDevice(this);
        Exceptions = FBlueprintCoreDelegates::OnScriptException.AddRaw(this, &FFuzzCapture::OnException);
    }
    virtual ~FFuzzCapture() override
    {
        FBlueprintCoreDelegates::OnScriptException.Remove(Exceptions);
        GLog->RemoveOutputDevice(this);
    }
    // Both, or the log hands the lines over later from its own thread (a buffered device), after the call they belong to;
    // Serialize touches nothing off the game thread.
    virtual bool CanBeUsedOnAnyThread() const override { return true; }
    virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

    // The warnings math and array nodes log ("Script Msg: Divide by zero"): no exception carries them. The engine checks
    // them against LogScriptCore but its script exception handler writes them to LogScript (every 5.x); exceptions log in
    // LogScript too, without the prefix, and are taken from the delegate instead.
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        static const FName Script(TEXT("LogScript"));
        static const FName ScriptCore(TEXT("LogScriptCore"));
        const int32 Level = Verbosity & ELogVerbosity::VerbosityMask;
        if ((Category == Script || Category == ScriptCore) && Level != ELogVerbosity::NoLogging && Level <= ELogVerbosity::Warning &&
            IsInGameThread() && V && FCString::Strncmp(V, TEXT("Script Msg: "), 12) == 0)
        {
            const FFrame* Top = TopFrame();
            Add(V, Top ? Top->Object : nullptr, Top);
        }
    }

private:
    FDelegateHandle Exceptions;

    void OnException(const UObject* Object, const FFrame& Frame, const FBlueprintExceptionInfo& Info)
    {
        const EBlueprintExceptionType::Type Type = Info.GetType();
        if (Type != EBlueprintExceptionType::Breakpoint && Type != EBlueprintExceptionType::Tracepoint &&
            Type != EBlueprintExceptionType::WireTracepoint)
        {
            Add(*Info.GetDescription().ToString(), Object, &Frame);
        }
    }

    void Add(const TCHAR* Text, const UObject* Object, const FFrame* Frame)
    {
        FFuzzCaught Fresh;
        Fresh.Message = Text;
        Fresh.Message.RemoveFromStart(TEXT("Script Msg: "));
        int32 LineEnd = INDEX_NONE;
        if (Fresh.Message.FindChar(TEXT('\n'), LineEnd))
        {
            Fresh.Message.LeftInline(LineEnd);
        }
        Fresh.Message.TrimStartAndEndInline();
        Fresh.Message.LeftInline(300);
        if (Frame && Frame->Node && Frame->Code && Object)
        {
            // The offset the debug data keys on: Code has already stepped past the opcode that is running.
            const int32 Offset = static_cast<int32>(Frame->Code - Frame->Node->Script.GetData()) - 1;
            if (const UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(Object, Frame->Node, Offset, true))
            {
                Fresh.Node = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                Fresh.NodeId = Node->NodeGuid.ToString();
            }
        }
        FFuzzCaught* Same = Caught.FindByPredicate([&Fresh](const FFuzzCaught& Each)
                                                   { return Each.Message == Fresh.Message && Each.NodeId == Fresh.NodeId; });
        if (Same)
        {
            ++Same->Count;
            return;
        }
        Caught.Add(MoveTemp(Fresh));
    }
};

struct FFuzzRun
{
    UWorld* World = nullptr;
    TWeakObjectPtr<AActor> Target;
    int32 MaxCalls = 200;
    double Deadline = 0.0;
    int32 Calls = 0;
    int32 Failures = 0;
    FString Stopped;
    TSharedPtr<FJsonObject> StoppedBy;
    FFuzzCapture Capture;
};

// Why the run cannot go on, else empty; what a call did to the game comes first.
FString StopReason(const FFuzzRun& Run)
{
    if (!Run.Target.IsValid() || Run.Target->IsActorBeingDestroyed())
    {
        return TEXT("target_destroyed");
    }
    if (!GEditor || GEditor->PlayWorld.Get() != Run.World || GEditor->ShouldEndPlayMap())
    {
        return TEXT("play_ending");
    }
    if (Run.Calls >= Run.MaxCalls)
    {
        return TEXT("budget");
    }
    return FPlatformTime::Seconds() > Run.Deadline ? TEXT("deadline") : FString();
}

// The input a call changed from its first value; none for the call with every input at its first value.
TSharedPtr<FJsonObject> Changed(const FPlan& Plan, const TPair<int32, int32>& Call)
{
    TSharedPtr<FJsonObject> Inputs = MakeShared<FJsonObject>();
    if (Call.Key != INDEX_NONE)
    {
        Inputs->SetField(Plan.Inputs[Call.Key]->GetName(), Plan.Values[Call.Key][Call.Value].Shown);
    }
    return Inputs;
}

TSharedPtr<FJsonObject> Failure(const FPlan& Plan, const TPair<int32, int32>& Call, const TArray<FFuzzCaught>& Caught)
{
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (int32 Index = 0; Index < Caught.Num() && Index < MaxErrorsShown; ++Index)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetStringField(TEXT("message"), Caught[Index].Message);
        if (!Caught[Index].NodeId.IsEmpty())
        {
            Error->SetStringField(TEXT("node"), Caught[Index].Node);
            Error->SetStringField(TEXT("nodeId"), Caught[Index].NodeId);
        }
        if (Caught[Index].Count > 1)
        {
            Error->SetNumberField(TEXT("count"), Caught[Index].Count);
        }
        Errors.Add(MakeShared<FJsonValueObject>(Error));
    }
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetObjectField(TEXT("inputs"), Changed(Plan, Call));
    Row->SetArrayField(TEXT("errors"), Errors);
    return Row;
}

TSharedPtr<FJsonObject> Fuzz(FFuzzRun& Run, const FPlan& Plan)
{
    TArray<TPair<int32, int32>> Calls{{INDEX_NONE, 0}};
    TSharedPtr<FJsonObject> First = MakeShared<FJsonObject>();
    for (int32 Input = 0; Input < Plan.Inputs.Num(); ++Input)
    {
        First->SetField(Plan.Inputs[Input]->GetName(), Plan.Values[Input][0].Shown);
        for (int32 Try = 1; Try < Plan.Values[Input].Num(); ++Try)
        {
            Calls.Add({Input, Try});
        }
    }
    TArray<TSharedPtr<FJsonValue>> Failed;
    int32 Made = 0;
    for (const TPair<int32, int32>& Call : Calls)
    {
        Run.Stopped = StopReason(Run);
        if (!Run.Stopped.IsEmpty() || Failed.Num() >= MaxFailedShown)
        {
            break;
        }
        AActor* Target = Run.Target.Get();
        FMcpScopedParamBlock Block(Plan.Function);
        for (int32 Input = 0; Input < Plan.Inputs.Num(); ++Input)
        {
            const FProperty* Property = Plan.Inputs[Input];
            Write(Property, Property->ContainerPtrToValuePtr<void>(Block.Data()), Plan.Values[Input][Input == Call.Key ? Call.Value : 0],
                  Run.World, Target);
        }
        Run.Capture.Caught.Reset();
        Target->ProcessEvent(Plan.Function, Block.Data());
        ++Made;
        ++Run.Calls;
        if (Run.Capture.Caught.Num() > 0)
        {
            ++Run.Failures;
            Failed.Add(MakeShared<FJsonValueObject>(Failure(Plan, Call, Run.Capture.Caught)));
        }
        const FString After = StopReason(Run);
        if (!Run.StoppedBy.IsValid() && (After == TEXT("target_destroyed") || After == TEXT("play_ending")))
        {
            Run.StoppedBy = MakeShared<FJsonObject>();
            Run.StoppedBy->SetStringField(TEXT("function"), Plan.Function->GetName());
            Run.StoppedBy->SetObjectField(TEXT("inputs"), Changed(Plan, Call));
        }
    }
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("function"), Plan.Function->GetName());
    Row->SetObjectField(TEXT("first"), First);
    Row->SetNumberField(TEXT("planned"), Calls.Num());
    Row->SetNumberField(TEXT("calls"), Made);
    Row->SetArrayField(TEXT("failed"), Failed);
    return Row;
}
} // namespace FuzzRunParts

using namespace FuzzRunParts;

TSharedPtr<FJsonObject> CallAll(const TArray<FPlan>& Plans, UWorld* World, AActor* Target, int32 MaxCalls)
{
    const double Started = FPlatformTime::Seconds();
    FFuzzRun Run;
    Run.World = World;
    Run.Target = Target;
    Run.MaxCalls = MaxCalls;
    Run.Deadline = Started + DeadlineSeconds;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FPlan& Plan : Plans)
    {
        Rows.Add(MakeShared<FJsonValueObject>(Fuzz(Run, Plan)));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("calls"), Run.Calls);
    Result->SetNumberField(TEXT("failures"), Run.Failures);
    Result->SetArrayField(TEXT("functions"), Rows);
    if (!Run.Stopped.IsEmpty())
    {
        Result->SetStringField(TEXT("stopped"), Run.Stopped);
    }
    if (Run.StoppedBy.IsValid())
    {
        Result->SetObjectField(TEXT("stoppedBy"), Run.StoppedBy);
    }
    Result->SetNumberField(TEXT("seconds"), FMath::RoundToDouble((FPlatformTime::Seconds() - Started) * 100.0) / 100.0);
    return Result;
}
} // namespace McpFuzz
