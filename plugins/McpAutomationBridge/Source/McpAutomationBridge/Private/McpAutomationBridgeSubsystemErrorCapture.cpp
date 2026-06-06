#include "McpAutomationBridgeSubsystem.h"

#include "HAL/PlatformTLS.h"
#include "McpAutomationBridgeSubsystemResponseSanitization.h"
#include "McpRequestErrorDevice.h"

using namespace McpAutomationBridgeSubsystemResponse;

static constexpr int32 MaxCapturedRequestMessages = 32;
static constexpr int32 MaxCapturedRequestMessageChars = 1024;

static bool IsKnownBenignMcpCompilerWarning(const FString& Message)
{
    return Message.Contains(TEXT("CooldownGameplayEffectClass"), ESearchCase::IgnoreCase) &&
           (Message.Contains(TEXT("no tags"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("grant any tags"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("grants no tag"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("granting no tag"), ESearchCase::IgnoreCase));
}

FMcpRequestErrorDevice::FMcpRequestErrorDevice(
    UMcpAutomationBridgeSubsystem* InSubsystem)
    : Subsystem(InSubsystem)
{
}

void FMcpRequestErrorDevice::Serialize(
    const TCHAR* V,
    ELogVerbosity::Type Verbosity,
    const FName& Category)
{
    if (Verbosity != ELogVerbosity::Error && Verbosity != ELogVerbosity::Warning)
    {
        return;
    }
    if (!Subsystem)
    {
        return;
    }

    FScopeLock Lock(&Subsystem->ErrorCaptureMutex);
    auto& Capture = Subsystem->CurrentErrorCapture;
    if (!Capture.bActive ||
        Capture.CapturingThreadId != FPlatformTLS::GetCurrentThreadId())
    {
        return;
    }

    FString Message = FString::Printf(TEXT("[%s] %s"), *Category.ToString(), V);
    if (Message.Len() > MaxCapturedRequestMessageChars)
    {
        Message = Message.Left(MaxCapturedRequestMessageChars) + TEXT("[TRUNCATED]");
    }

    const bool bTreatAsWarning =
        Verbosity == ELogVerbosity::Warning ||
        IsKnownBenignMcpCompilerWarning(Message);
    if (!bTreatAsWarning)
    {
        ++Capture.ErrorCount;
        if (Capture.ErrorMessages.Num() < MaxCapturedRequestMessages)
        {
            Capture.ErrorMessages.Add(Message);
        }
        else
        {
            Capture.bErrorMessagesTruncated = true;
        }
        Capture.bHasErrors = true;
        return;
    }

    ++Capture.WarningCount;
    if (Capture.WarningMessages.Num() < MaxCapturedRequestMessages)
    {
        Capture.WarningMessages.Add(Message);
    }
    else
    {
        Capture.bWarningMessagesTruncated = true;
    }
    Capture.bHasWarnings = true;
}

bool FMcpRequestErrorDevice::CanBeUsedOnAnyThread() const
{
    return true;
}

bool FMcpRequestErrorDevice::CanBeUsedOnMultipleThreads() const
{
    return true;
}

UMcpAutomationBridgeSubsystem::FRequestErrorCapture&
UMcpAutomationBridgeSubsystem::GetCurrentErrorCapture()
{
    return CurrentErrorCapture;
}

void UMcpAutomationBridgeSubsystem::BeginErrorCapture()
{
    if (!RequestErrorDevice.IsValid())
    {
        RequestErrorDevice = MakeShared<FMcpRequestErrorDevice>(this);
    }

    {
        FScopeLock Lock(&ErrorCaptureMutex);
        CurrentErrorCapture.Reset();
        CurrentErrorCapture.CapturingThreadId = FPlatformTLS::GetCurrentThreadId();
        CurrentErrorCapture.bActive = true;
    }

    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->AddOutputDevice(RequestErrorDevice.Get());
    }
}

TArray<FString> UMcpAutomationBridgeSubsystem::EndErrorCapture()
{
    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->RemoveOutputDevice(RequestErrorDevice.Get());
    }

    FScopeLock Lock(&ErrorCaptureMutex);
    TArray<FString> AllMessages;
    AllMessages.Reserve(
        CurrentErrorCapture.ErrorMessages.Num() +
        CurrentErrorCapture.WarningMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    for (const FString& WarningMessage : CurrentErrorCapture.WarningMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(WarningMessage));
    }
    CurrentErrorCapture.bActive = false;
    CurrentErrorCapture.CapturingThreadId = 0;
    return AllMessages;
}

bool UMcpAutomationBridgeSubsystem::HasCapturedErrors() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    return CurrentErrorCapture.bHasErrors.load();
}

TArray<FString> UMcpAutomationBridgeSubsystem::GetCapturedErrorMessages() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    TArray<FString> SanitizedMessages;
    SanitizedMessages.Reserve(CurrentErrorCapture.ErrorMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        SanitizedMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    return SanitizedMessages;
}
