#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpNativeTransport.h"
#include "McpAutomationBridgeSubsystemResponseSanitization.h"
#include "McpBridgeWebSocket.h"
#include "McpConnectionManager.h"

using namespace McpAutomationBridgeSubsystemResponse;

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    const bool bSuccess,
    const FString& Message,
    const TSharedPtr<FJsonObject>& Result,
    const FString& ErrorCode,
    ERequestOrigin Origin)
{
    bool bEffectiveSuccess = bSuccess;
    FString EffectiveMessage = Message;
    FString EffectiveErrorCode = ErrorCode;
    TSharedPtr<FJsonObject> EffectiveResult = Result;

    if (bSuccess && bProcessingAutomationRequest)
    {
        TArray<FString> CapturedErrors;
        int32 TotalCapturedErrorCount = 0;
        bool bCapturedErrorsTruncated = false;
        {
            FScopeLock Lock(&ErrorCaptureMutex);
            if (CurrentErrorCapture.bHasErrors.load())
            {
                CapturedErrors = CurrentErrorCapture.ErrorMessages;
                TotalCapturedErrorCount = CurrentErrorCapture.ErrorCount;
                bCapturedErrorsTruncated = CurrentErrorCapture.bErrorMessagesTruncated;
            }
        }

        if (CapturedErrors.Num() > 0)
        {
            bEffectiveSuccess = false;
            EffectiveErrorCode = TEXT("ENGINE_ERROR");
            const FString FirstError = SanitizeEngineErrorForResponse(CapturedErrors[0]);
            EffectiveMessage = FString::Printf(
                TEXT("Handler reported success but Unreal logged errors: %s"),
                *FirstError.Left(240));

            TSharedPtr<FJsonObject> AugmentedResult = MakeShared<FJsonObject>();
            if (Result.IsValid())
            {
                for (const auto& Pair : Result->Values)
                {
                    AugmentedResult->SetField(Pair.Key, Pair.Value);
                }
            }

            TArray<TSharedPtr<FJsonValue>> ErrorValues;
            const int32 MaxErrorsInResponse = 3;
            const int32 ErrorResponseCount =
                FMath::Min(CapturedErrors.Num(), MaxErrorsInResponse);
            for (int32 ErrorIndex = 0; ErrorIndex < ErrorResponseCount; ++ErrorIndex)
            {
                ErrorValues.Add(MakeShared<FJsonValueString>(
                    SanitizeEngineErrorForResponse(CapturedErrors[ErrorIndex])));
            }
            AugmentedResult->SetBoolField(TEXT("success"), false);
            AugmentedResult->SetNumberField(
                TEXT("engineErrorCount"),
                TotalCapturedErrorCount);
            AugmentedResult->SetArrayField(TEXT("engineErrors"), ErrorValues);
            if (bCapturedErrorsTruncated || CapturedErrors.Num() > MaxErrorsInResponse)
            {
                AugmentedResult->SetBoolField(TEXT("engineErrorsTruncated"), true);
            }

            TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
            ErrorObj->SetStringField(TEXT("code"), EffectiveErrorCode);
            ErrorObj->SetStringField(TEXT("message"), EffectiveMessage);
            AugmentedResult->SetObjectField(TEXT("error"), ErrorObj);
            EffectiveResult = AugmentedResult;
        }
    }

    if (!bEffectiveSuccess)
    {
        EffectiveMessage = SanitizeEngineErrorForResponse(EffectiveMessage);
    }

    ERequestOrigin EffectiveOrigin =
        Origin == ERequestOrigin::WebSocket ? CurrentRequestOrigin : Origin;
    if (Origin == ERequestOrigin::WebSocket && NativeTransport &&
        NativeTransport->HasPendingRequest(RequestId))
    {
        EffectiveOrigin = ERequestOrigin::NativeHTTP;
    }
    if (EffectiveOrigin == ERequestOrigin::NativeHTTP && NativeTransport)
    {
        if (!NativeTransport->CompletePendingRequest(
                RequestId,
                bEffectiveSuccess,
                EffectiveMessage,
                EffectiveResult,
                EffectiveErrorCode))
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Native HTTP response for %s dropped — request already expired or unknown"),
                *RequestId);
        }
        return;
    }
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendAutomationResponse(
            TargetSocket,
            RequestId,
            bEffectiveSuccess,
            EffectiveMessage,
            EffectiveResult,
            EffectiveErrorCode);
    }
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    const FString& Message,
    const FString& ErrorCode)
{
    const FString ResolvedError =
        ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Warning,
        TEXT("Automation request failed (%s): %s"),
        *ResolvedError,
        *SanitizeForLog(Message));
    SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::SendProgressUpdate(
    const FString& RequestId,
    float Percent,
    const FString& Message,
    bool bStillWorking,
    ERequestOrigin Origin)
{
    if (Origin == ERequestOrigin::NativeHTTP && NativeTransport)
    {
        NativeTransport->SendSSEProgressUpdate(RequestId, Percent, Message);
        return;
    }
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendProgressUpdate(RequestId, Percent, Message, bStillWorking);
    }
}

void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(
    const FString& RequestId,
    const bool bSuccess,
    const FString& Message,
    const FString& ErrorCode)
{
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->RecordAutomationTelemetry(
            RequestId,
            bSuccess,
            Message,
            ErrorCode);
    }
}
