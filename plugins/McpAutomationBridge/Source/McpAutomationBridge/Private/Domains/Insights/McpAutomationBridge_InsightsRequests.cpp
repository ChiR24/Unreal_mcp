#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersCommandValidation.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

namespace McpInsights
{
namespace
{
bool IsSafeToken(const FString& Token)
{
    if (Token.IsEmpty() || Token.Len() > 64)
    {
        return false;
    }
    for (int32 Index = 0; Index < Token.Len(); ++Index)
    {
        const TCHAR Ch = Token[Index];
        if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-') &&
            Ch != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}

bool ValidateChannels(const FString& RawChannels, FString& OutError)
{
    if (RawChannels.IsEmpty())
    {
        return true;
    }
    if (McpContainsUnsafeCommandSeparator(RawChannels))
    {
        OutError = TEXT("Trace channels contain unsafe separators.");
        return false;
    }
    TArray<FString> Tokens;
    RawChannels.ParseIntoArray(Tokens, TEXT(","), true);
    if (Tokens.IsEmpty())
    {
        OutError = TEXT("Trace channels are empty.");
        return false;
    }
    for (FString& Token : Tokens)
    {
        Token.TrimStartAndEndInline();
        if (!IsSafeToken(Token))
        {
            OutError = TEXT("Trace channels contain unsupported characters.");
            return false;
        }
    }
    return true;
}
}

bool TryReadChannels(
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutChannels,
    FString& OutError)
{
    if (Payload->TryGetStringField(TEXT("channels"), OutChannels))
    {
        OutChannels.TrimStartAndEndInline();
        return ValidateChannels(OutChannels, OutError);
    }

    const TArray<TSharedPtr<FJsonValue>>* ChannelArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("channels"), ChannelArray))
    {
        OutChannels.Empty();
        return true;
    }

    TArray<FString> Tokens;
    for (const TSharedPtr<FJsonValue>& Value : *ChannelArray)
    {
        FString Token;
        if (!Value.IsValid() || !Value->TryGetString(Token))
        {
            OutError = TEXT("Trace channel arrays must contain strings only.");
            return false;
        }
        Token.TrimStartAndEndInline();
        Tokens.Add(Token);
    }
    OutChannels = FString::Join(Tokens, TEXT(","));
    return ValidateChannels(OutChannels, OutError);
}

bool IsInsightsAction(const FString& Action)
{
    return Action.Equals(TEXT("manage_insights"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("start_session"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("start_unreal_insights"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("capture_insights_trace"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("get_trace_status"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("pause_session"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("resume_session"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("stop_session"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("write_snapshot"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("send_snapshot"), ESearchCase::IgnoreCase) ||
        Action.Equals(TEXT("analyze_trace"), ESearchCase::IgnoreCase);
}

FString NormalizeSubAction(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload)
{
    const FString LowerAction = Action.TrimStartAndEnd().ToLower();
    if (LowerAction == TEXT("capture_insights_trace") ||
        LowerAction == TEXT("start_unreal_insights"))
    {
        return TEXT("start_session");
    }
    if (LowerAction != TEXT("manage_insights"))
    {
        return LowerAction;
    }
    FString SubAction;
    if (Payload.IsValid() &&
        (Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
         Payload->TryGetStringField(TEXT("action"), SubAction)))
    {
        SubAction = SubAction.TrimStartAndEnd().ToLower();
        if (SubAction == TEXT("capture_insights_trace") ||
            SubAction == TEXT("start_unreal_insights"))
        {
            return TEXT("start_session");
        }
        return SubAction;
    }
    return FString();
}

TSharedPtr<FJsonObject> CreateInsightsResult(
    const FString& SubAction,
    const FString& TraceAction)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
    Result->SetStringField(TEXT("subAction"), SubAction);
    Result->SetStringField(TEXT("traceAction"), TraceAction);
    return Result;
}

FString ConnectionTypeToString(FTraceAuxiliary::EConnectionType Type)
{
    switch (Type)
    {
    case FTraceAuxiliary::EConnectionType::Network:
        return TEXT("network");
    case FTraceAuxiliary::EConnectionType::File:
        return TEXT("file");
    case FTraceAuxiliary::EConnectionType::None:
        return TEXT("none");
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    case FTraceAuxiliary::EConnectionType::Relay:
        return TEXT("relay");
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    case FTraceAuxiliary::EConnectionType::SecureNetwork:
        return TEXT("secure_network");
#endif
    default:
        return TEXT("unknown");
    }
}

void AddTraceStatus(TSharedPtr<FJsonObject>& Result)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    FGuid SessionGuid;
    FGuid TraceGuid;
    const bool bConnected = FTraceAuxiliary::IsConnected(SessionGuid, TraceGuid);
    Result->SetBoolField(TEXT("connected"), bConnected);
    Result->SetStringField(TEXT("destination"),
        FTraceAuxiliary::GetTraceDestinationString());
    if (bConnected)
    {
        Result->SetStringField(TEXT("sessionGuid"), SessionGuid.ToString());
        Result->SetStringField(TEXT("traceGuid"), TraceGuid.ToString());
    }
    TStringBuilder<512> Channels;
    FTraceAuxiliary::GetActiveChannelsString(Channels);
    Result->SetStringField(TEXT("activeChannels"), Channels.ToString());
    Result->SetStringField(TEXT("connectionType"),
        ConnectionTypeToString(FTraceAuxiliary::GetConnectionType()));
    Result->SetBoolField(TEXT("statusQuerySupported"), true);
#else
    Result->SetBoolField(TEXT("connectedKnown"), false);
    Result->SetBoolField(TEXT("statusQuerySupported"), false);
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    switch (FTraceAuxiliary::GetTraceSystemStatus())
    {
    case FTraceAuxiliary::ETraceSystemStatus::NotAvailable:
        Result->SetStringField(TEXT("traceSystemStatus"), TEXT("not_available"));
        break;
    case FTraceAuxiliary::ETraceSystemStatus::Available:
        Result->SetStringField(TEXT("traceSystemStatus"), TEXT("available"));
        break;
    case FTraceAuxiliary::ETraceSystemStatus::TracingToServer:
        Result->SetStringField(TEXT("traceSystemStatus"), TEXT("tracing_to_server"));
        break;
    case FTraceAuxiliary::ETraceSystemStatus::TracingToFile:
        Result->SetStringField(TEXT("traceSystemStatus"), TEXT("tracing_to_file"));
        break;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    case FTraceAuxiliary::ETraceSystemStatus::TracingToCustomRelay:
        Result->SetStringField(TEXT("traceSystemStatus"),
            TEXT("tracing_to_custom_relay"));
        break;
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    case FTraceAuxiliary::ETraceSystemStatus::TracingToSecureNetwork:
        Result->SetStringField(TEXT("traceSystemStatus"),
            TEXT("tracing_to_secure_network"));
        break;
#endif
    default:
        Result->SetStringField(TEXT("traceSystemStatus"), TEXT("unknown"));
        break;
    }
    Result->SetBoolField(TEXT("paused"), FTraceAuxiliary::IsPaused());
#else
    Result->SetStringField(TEXT("traceSystemStatus"), TEXT("unknown"));
#endif
}

FString StartModeToString(ETraceStartMode Mode)
{
    if (Mode == ETraceStartMode::Network)
    {
        return TEXT("network");
    }
    return TEXT("file");
}

uint32 ReadMaxTailSize(const TSharedPtr<FJsonObject>& Payload)
{
    int32 Value = 0;
    Payload->TryGetNumberField(TEXT("maxTailSize"), Value);
    return Value > 0 ? static_cast<uint32>(Value) : 0;
}

bool ReadOverwrite(const TSharedPtr<FJsonObject>& Payload)
{
    bool bOverwrite = false;
    Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    return bOverwrite;
}
}
