#include "McpAutomationBridgeSubsystem.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"

namespace
{
FString ExceptionTypeName(EBlueprintExceptionType::Type Type)
{
    switch (Type)
    {
    case EBlueprintExceptionType::Breakpoint: return TEXT("breakpoint");
    case EBlueprintExceptionType::Tracepoint: return TEXT("tracepoint");
    case EBlueprintExceptionType::WireTracepoint: return TEXT("wire_tracepoint");
    case EBlueprintExceptionType::AccessViolation: return TEXT("access_violation");
    case EBlueprintExceptionType::InfiniteLoop: return TEXT("infinite_loop");
    case EBlueprintExceptionType::NonFatalError: return TEXT("non_fatal_error");
    case EBlueprintExceptionType::FatalError: return TEXT("fatal_error");
    case EBlueprintExceptionType::AbortExecution: return TEXT("abort_execution");
    case EBlueprintExceptionType::UserRaisedError: return TEXT("user_raised_error");
    default: return TEXT("unknown");
    }
}

TArray<TSharedPtr<FJsonValue>> ExecutionEntries(
    const TArray<FAutomationExecutionEntry>& Entries)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(Entries.Num());
    for (const FAutomationExecutionEntry& Entry : Entries)
    {
        TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
        Value->SetStringField(TEXT("message"), Entry.Event.Message);
        Value->SetStringField(TEXT("context"), Entry.Event.Context);
        Value->SetStringField(TEXT("file"), Entry.Filename);
        Value->SetNumberField(TEXT("line"), Entry.LineNumber);
        Value->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToIso8601());
        Value->SetStringField(
            TEXT("severity"),
            Entry.Event.Type == EAutomationEventType::Error
                ? TEXT("error")
                : Entry.Event.Type == EAutomationEventType::Warning
                    ? TEXT("warning")
                    : TEXT("info"));
        Values.Add(MakeShared<FJsonValueObject>(Value));
    }
    return Values;
}
}

void UMcpAutomationBridgeSubsystem::InitializeDebugObservability()
{
    BlueprintExceptionHandle = FBlueprintCoreDelegates::OnScriptException.AddUObject(
        this,
        &UMcpAutomationBridgeSubsystem::HandleBlueprintScriptException);
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    AutomationTestStartHandle = Framework.OnTestStartEvent.AddUObject(
        this,
        &UMcpAutomationBridgeSubsystem::HandleAutomationTestStart);
    AutomationTestEndHandle = Framework.OnTestEndEvent.AddUObject(
        this,
        &UMcpAutomationBridgeSubsystem::HandleAutomationTestEnd);
    AutomationTestingCompleteHandle = Framework.PostTestingEvent.AddUObject(
        this,
        &UMcpAutomationBridgeSubsystem::HandleAutomationTestingComplete);
}

void UMcpAutomationBridgeSubsystem::ShutdownDebugObservability()
{
    FBlueprintCoreDelegates::OnScriptException.Remove(BlueprintExceptionHandle);
    FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
    Framework.OnTestStartEvent.Remove(AutomationTestStartHandle);
    Framework.OnTestEndEvent.Remove(AutomationTestEndHandle);
    Framework.PostTestingEvent.Remove(AutomationTestingCompleteHandle);
}

void UMcpAutomationBridgeSubsystem::BeginAutomationTestJob(const FString& RequestId)
{
    ActiveAutomationTestRequestId = RequestId;
    AutomationTestsPassed = 0;
    AutomationTestsFailed = 0;
    AutomationTestWarnings = 0;
    AutomationTestErrors = 0;
}

void UMcpAutomationBridgeSubsystem::HandleBlueprintScriptException(
    const UObject* ActiveObject,
    const FFrame& StackFrame,
    const FBlueprintExceptionInfo& ExceptionInfo)
{
    if (ExceptionInfo.GetType() == EBlueprintExceptionType::Breakpoint ||
        ExceptionInfo.GetType() == EBlueprintExceptionType::Tracepoint ||
        ExceptionInfo.GetType() == EBlueprintExceptionType::WireTracepoint)
    {
        return;
    }
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(
        TEXT("object"), ActiveObject ? ActiveObject->GetPathName() : FString());
    Payload->SetStringField(
        TEXT("blueprint"),
        ActiveObject && ActiveObject->GetClass()->ClassGeneratedBy
            ? ActiveObject->GetClass()->ClassGeneratedBy->GetPathName()
            : FString());
    Payload->SetStringField(
        TEXT("exceptionType"), ExceptionTypeName(ExceptionInfo.GetType()));
    Payload->SetStringField(TEXT("message"), ExceptionInfo.GetDescription().ToString());
    Payload->SetStringField(TEXT("scriptStack"), FFrame::GetScriptCallstack(true, false));

    if (StackFrame.Node && StackFrame.Code)
    {
        const int32 Offset = static_cast<int32>(
            StackFrame.Code - StackFrame.Node->Script.GetData());
        if (const UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
                ActiveObject,
                StackFrame.Node,
                Offset,
                true))
        {
            Payload->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
            if (Node->GetGraph())
            {
                Payload->SetStringField(TEXT("graph"), Node->GetGraph()->GetName());
            }
        }
    }
    TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
    Event->SetStringField(TEXT("event"), TEXT("blueprint_exception"));
    Event->SetObjectField(TEXT("payload"), Payload);
    BroadcastAutomationEvent(Event);
}

void UMcpAutomationBridgeSubsystem::HandleAutomationTestStart(FAutomationTestBase* Test)
{
    if (ActiveAutomationTestRequestId.IsEmpty() || !Test)
    {
        return;
    }
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("test"), Test->GetTestFullName());
    TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
    Event->SetStringField(TEXT("event"), TEXT("automation_test_started"));
    Event->SetStringField(TEXT("requestId"), ActiveAutomationTestRequestId);
    Event->SetObjectField(TEXT("payload"), Payload);
    BroadcastAutomationEvent(Event);
}

void UMcpAutomationBridgeSubsystem::HandleAutomationTestEnd(FAutomationTestBase* Test)
{
    if (ActiveAutomationTestRequestId.IsEmpty() || !Test)
    {
        return;
    }
    FAutomationTestExecutionInfo Info;
    Test->GetExecutionInfo(Info);
    const bool bPassed = Info.bSuccessful && Info.GetErrorTotal() == 0;
    bPassed ? ++AutomationTestsPassed : ++AutomationTestsFailed;
    AutomationTestWarnings += Info.GetWarningTotal();
    AutomationTestErrors += Info.GetErrorTotal();
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bPassed);
    Result->SetStringField(TEXT("test"), Test->GetTestFullName());
    Result->SetNumberField(TEXT("duration"), Info.Duration);
    Result->SetNumberField(TEXT("warnings"), Info.GetWarningTotal());
    Result->SetNumberField(TEXT("errors"), Info.GetErrorTotal());
    Result->SetArrayField(TEXT("entries"), ExecutionEntries(Info.GetEntries()));
    TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
    Event->SetStringField(TEXT("event"), TEXT("automation_test_completed"));
    Event->SetStringField(TEXT("requestId"), ActiveAutomationTestRequestId);
    Event->SetObjectField(TEXT("result"), Result);
    BroadcastAutomationEvent(Event);
}

void UMcpAutomationBridgeSubsystem::HandleAutomationTestingComplete()
{
    if (ActiveAutomationTestRequestId.IsEmpty())
    {
        return;
    }
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), AutomationTestsFailed == 0);
    Result->SetNumberField(TEXT("passed"), AutomationTestsPassed);
    Result->SetNumberField(TEXT("failed"), AutomationTestsFailed);
    Result->SetNumberField(TEXT("warnings"), AutomationTestWarnings);
    Result->SetNumberField(TEXT("errors"), AutomationTestErrors);
    TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
    Event->SetStringField(TEXT("event"), TEXT("automation_tests_completed"));
    Event->SetStringField(TEXT("requestId"), ActiveAutomationTestRequestId);
    Event->SetObjectField(TEXT("result"), Result);
    BroadcastAutomationEvent(Event);
    ActiveAutomationTestRequestId.Empty();
}
