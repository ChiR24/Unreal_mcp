#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"

inline void McpPrepareAutomationEventEnvelope(
    const TSharedPtr<FJsonObject>& Event,
    std::atomic<uint64>& Sequence,
    const FString& RequestId,
    const FString& TraceId)
{
    Event->SetStringField(TEXT("type"), TEXT("automation_event"));
    Event->SetNumberField(TEXT("sequence"), static_cast<double>(++Sequence));
    const FString Timestamp = FDateTime::UtcNow().ToIso8601();
    Event->SetStringField(TEXT("timestamp"), Timestamp);
    TSharedPtr<FJsonObject> Context = MakeShared<FJsonObject>();
    if (!RequestId.IsEmpty())
    {
        Context->SetStringField(TEXT("requestId"), RequestId);
        Event->SetStringField(TEXT("requestId"), RequestId);
    }
    Context->SetStringField(TEXT("traceId"), TraceId.IsEmpty()
        ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower) : TraceId);
    Context->SetNumberField(TEXT("targetPid"), static_cast<double>(FPlatformProcess::GetCurrentProcessId()));
    Context->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Context->SetNumberField(TEXT("thread"), static_cast<double>(FPlatformTLS::GetCurrentThreadId()));
    Context->SetStringField(TEXT("timestamp"), Timestamp);
    Context->SetNumberField(TEXT("eventCursor"), static_cast<double>(Sequence.load()));
    Event->SetObjectField(TEXT("context"), Context);
}
