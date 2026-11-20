// McpPerfCsvRead.cpp — system_control read_profile: the summary of a CSV profiler capture (start_profiling format csv).
#include "Domains/Performance/McpPerfPrivate.h"
#include "Domains/Performance/Csv/McpCsvProfile.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace McpPerformanceHandlers
{
namespace
{
// ponytail: read whole on the game thread; a capture over this is refused, a worker-thread stream if one ever matters.
constexpr int64 McpMaxCsvProfileBytes = 32 * 1024 * 1024;

FString CsvFolder() { return FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir() / TEXT("CSV")); }

// file names one capture in the folder, by name only; empty picks the newest.
FString PickCapture(const FString& Name, FString& OutError)
{
    if (!Name.IsEmpty())
    {
        if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")) || !Name.EndsWith(TEXT(".csv")))
        {
            OutError = TEXT("file is the name of a .csv capture in Saved/Profiling/CSV, without a folder.");
            return FString();
        }
        const FString Path = CsvFolder() / Name;
        if (!IFileManager::Get().FileExists(*Path)) OutError = FString::Printf(TEXT("No capture %s in Saved/Profiling/CSV."), *Name);
        return OutError.IsEmpty() ? Path : FString();
    }
    TArray<FString> Names;
    IFileManager::Get().FindFiles(Names, *(CsvFolder() / TEXT("*.csv")), true, false);
    FString Newest;
    FDateTime NewestTime = FDateTime::MinValue();
    for (const FString& Each : Names)
    {
        const FDateTime When = IFileManager::Get().GetTimeStamp(*(CsvFolder() / Each));
        if (When > NewestTime) { NewestTime = When; Newest = CsvFolder() / Each; }
    }
    if (Newest.IsEmpty()) OutError = TEXT("No CSV capture yet: start one with start_profiling format csv, play, then stop_profiling format csv.");
    return Newest;
}
} // namespace

bool HandleReadCsvProfile(const FPerformanceActionContext& Context)
{
    FString Name;
    double Top = 5.0;
    double HitchMs = 33.3;
    Context.Payload->TryGetStringField(TEXT("file"), Name);
    Context.Payload->TryGetNumberField(TEXT("top"), Top);
    Context.Payload->TryGetNumberField(TEXT("hitchMs"), HitchMs);
    FString Error;
    const FString Path = PickCapture(Name, Error);
    if (Path.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, Error,
            Error.StartsWith(TEXT("file is")) ? TEXT("INVALID_ARGUMENT") : TEXT("PROFILE_NOT_FOUND"));
        return true;
    }
    const int64 Size = IFileManager::Get().FileSize(*Path);
    FString Text;
    if (Size > McpMaxCsvProfileBytes || !FFileHelper::LoadFileToString(Text, *Path))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, Size > McpMaxCsvProfileBytes
            ? FString::Printf(TEXT("%s is %lld MB, over the %lld MB a read takes: capture a shorter run."), *FPaths::GetCleanFilename(Path), Size >> 20, McpMaxCsvProfileBytes >> 20)
            : FString::Printf(TEXT("%s could not be read."), *FPaths::GetCleanFilename(Path)), TEXT("PROFILE_UNREADABLE"));
        return true;
    }
    const TSharedPtr<FJsonObject> Summary = McpCsvProfile::Summarize(Text, FMath::RoundToInt(Top), FMath::Max(HitchMs, 1.0), Error);
    if (!Summary.IsValid())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, Error, TEXT("PROFILE_UNREADABLE"));
        return true;
    }
    Summary->SetStringField(TEXT("file"), FPaths::GetCleanFilename(Path));
    TArray<FString> Warnings;
    // The profiler writes its closing rows when the capture stops; a file without them is still being written.
    if (!Text.Contains(TEXT("[HasHeaderRowAtEnd]")))
    {
        Warnings.Add(TEXT("This capture has no closing rows yet: it may still be running or being written. Stop it (stop_profiling format csv) and read again."));
    }
    SendStandardSuccessResponse(&Context.Bridge, Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s: %d frames"), *FPaths::GetCleanFilename(Path), static_cast<int32>(Summary->GetNumberField(TEXT("frames")))),
        Summary, Warnings);
    return true;
}
} // namespace McpPerformanceHandlers
