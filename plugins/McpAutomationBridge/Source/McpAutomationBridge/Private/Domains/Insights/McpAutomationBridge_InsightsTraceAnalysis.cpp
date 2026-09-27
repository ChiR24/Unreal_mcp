#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

namespace McpInsights
{
bool HandleAnalyzeTrace(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Path;
    FString Error;
    FString ErrorCode;
    if (!TryResolveTracePath(Payload, true, false, false, Path, Error, ErrorCode))
    {
        TSharedPtr<FJsonObject> Result =
            CreateInsightsResult(TEXT("analyze_trace"));
        Result->SetBoolField(TEXT("exists"), false);
        Result->SetStringField(TEXT("path"), Path);
        Result->SetStringField(TEXT("analysisScope"),
            TEXT("local_file_metadata"));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, false,
            Error, Result, ErrorCode);
        return true;
    }

    const FFileStatData Stat = IFileManager::Get().GetStatData(*Path);
    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("analyze_trace"));
    Result->SetStringField(TEXT("status"), TEXT("analyzed"));
    Result->SetStringField(TEXT("analysisScope"),
        TEXT("local_file_metadata"));
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("extension"), FPaths::GetExtension(Path));
    Result->SetBoolField(TEXT("exists"), Stat.bIsValid);
    Result->SetBoolField(TEXT("isDirectory"), Stat.bIsDirectory);
    Result->SetBoolField(TEXT("hasUtraceExtension"),
        FPaths::GetExtension(Path).Equals(TEXT("utrace"), ESearchCase::IgnoreCase));
    Result->SetNumberField(TEXT("sizeBytes"), static_cast<double>(Stat.FileSize));
    Result->SetStringField(TEXT("modifiedUtc"),
        Stat.ModificationTime.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));

    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Trace file metadata analyzed."), Result);
    return true;
}
}
