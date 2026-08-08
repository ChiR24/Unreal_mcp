#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

inline void McpConfigureBridgeProtocolAck(
    const TSharedPtr<FJsonObject>& Hello,
    const TSharedPtr<FJsonObject>& Ack)
{
    int32 Selected = 1;
    const TArray<TSharedPtr<FJsonValue>>* Requested = nullptr;
    if (Hello->TryGetArrayField(TEXT("supportedProtocolVersions"), Requested))
    {
        for (const TSharedPtr<FJsonValue>& Version : *Requested)
        {
            if (Version.IsValid() && Version->Type == EJson::Number &&
                FMath::RoundToInt(Version->AsNumber()) == 2)
            {
                Selected = 2;
                break;
            }
        }
    }
    Ack->SetNumberField(TEXT("protocolVersion"), Selected);
    Ack->SetNumberField(TEXT("selectedProtocolVersion"), Selected);
    Ack->SetArrayField(TEXT("supportedProtocolVersions"), {
        MakeShared<FJsonValueNumber>(2), MakeShared<FJsonValueNumber>(1)});
    Ack->SetArrayField(TEXT("capabilities"), {
        MakeShared<FJsonValueString>(TEXT("console_commands")),
        MakeShared<FJsonValueString>(TEXT("native_plugin")),
        MakeShared<FJsonValueString>(TEXT("structured_diagnostics")),
        MakeShared<FJsonValueString>(TEXT("correlated_events")),
        MakeShared<FJsonValueString>(TEXT("async_jobs")),
        MakeShared<FJsonValueString>(TEXT("blueprint_diagnostics")),
        MakeShared<FJsonValueString>(TEXT("runtime_probes"))});
}
