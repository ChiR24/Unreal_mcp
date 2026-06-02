#include "UnrealAgentEvidenceLedger.h"

#include "UnrealAgentStudioKit.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    constexpr const TCHAR* LedgerVersion = TEXT("1");

    FString NormalizeEvidenceProjectDirectory(const FString& ProjectDirectory)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(ProjectDirectory.IsEmpty() ? FPaths::ProjectDir() : ProjectDirectory);
        FPaths::NormalizeDirectoryName(Normalized);
        return Normalized;
    }

    FString GetLedgerDirectory(const FString& ProjectDirectory)
    {
        return FPaths::Combine(NormalizeEvidenceProjectDirectory(ProjectDirectory), TEXT("Saved"), TEXT("UnrealAgent"));
    }

    FString GetEvidenceDirectory(const FString& ProjectDirectory)
    {
        return FPaths::Combine(GetLedgerDirectory(ProjectDirectory), TEXT("evidence"));
    }

    FString GetStatePath(const FString& ProjectDirectory)
    {
        return FPaths::Combine(GetLedgerDirectory(ProjectDirectory), TEXT("state.json"));
    }

    FString GetDecisionsPath(const FString& ProjectDirectory)
    {
        return FPaths::Combine(GetLedgerDirectory(ProjectDirectory), TEXT("decisions.md"));
    }

    FString MakeSafeSlug(FString Value)
    {
        Value = Value.TrimStartAndEnd().ToLower();
        if (Value.IsEmpty())
        {
            Value = TEXT("event");
        }

        for (TCHAR& Character : Value)
        {
            const bool bAllowed = (Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9'))
                || Character == TEXT('-')
                || Character == TEXT('_');
            if (!bAllowed)
            {
                Character = TEXT('-');
            }
        }

        while (Value.Contains(TEXT("--")))
        {
            Value.ReplaceInline(TEXT("--"), TEXT("-"));
        }
        return Value.Left(48).TrimChar(TEXT('-'));
    }

    FString JsonObjectToString(const TSharedRef<FJsonObject>& Object)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        FJsonSerializer::Serialize(Object, Writer);
        return Json;
    }

    bool SaveStateFile(const FString& ProjectDirectory, const FString& Summary)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), LedgerVersion);
        Root->SetStringField(TEXT("updatedAt"), FDateTime::UtcNow().ToIso8601());
        Root->SetStringField(TEXT("latestSummary"), FUnrealAgentStudioKit::RedactSensitiveText(Summary));
        return FFileHelper::SaveStringToFile(JsonObjectToString(Root), *GetStatePath(ProjectDirectory), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FUnrealAgentEvidenceLedger::EnsureLedger(const FString& ProjectDirectory, FUnrealAgentEvidenceSummary* OutSummary)
{
    const FString LedgerDirectory = GetLedgerDirectory(ProjectDirectory);
    const FString EvidenceDirectory = GetEvidenceDirectory(ProjectDirectory);
    if (!IFileManager::Get().MakeDirectory(*EvidenceDirectory, true))
    {
        if (OutSummary != nullptr)
        {
            *OutSummary = LoadSummary(ProjectDirectory);
        }
        return false;
    }

    const FString DecisionsPath = GetDecisionsPath(ProjectDirectory);
    if (!FPaths::FileExists(DecisionsPath))
    {
        const FString InitialDecisions = FString()
            + TEXT("# Unreal Agent Decisions\n\n")
            + FUnrealAgentStudioKit::GetStudioKitVersionMarker()
            + TEXT("\n\n");
        if (!FFileHelper::SaveStringToFile(InitialDecisions, *DecisionsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            if (OutSummary != nullptr)
            {
                *OutSummary = LoadSummary(ProjectDirectory);
            }
            return false;
        }
    }

    const FString StatePath = GetStatePath(ProjectDirectory);
    if (!FPaths::FileExists(StatePath) && !SaveStateFile(ProjectDirectory, TEXT("Ledger initialized.")))
    {
        if (OutSummary != nullptr)
        {
            *OutSummary = LoadSummary(ProjectDirectory);
        }
        return false;
    }

    if (OutSummary != nullptr)
    {
        *OutSummary = LoadSummary(ProjectDirectory);
        OutSummary->bWritable = true;
    }
    return true;
}

bool FUnrealAgentEvidenceLedger::RecordEvent(const FString& ProjectDirectory, const FString& EventType, const FString& Status, const FString& Summary, const FString& Details, FString* OutEvidencePath)
{
    if (OutEvidencePath != nullptr)
    {
        OutEvidencePath->Reset();
    }

    if (!EnsureLedger(ProjectDirectory))
    {
        return false;
    }

    const FString SafeEventType = MakeSafeSlug(EventType);
    const FDateTime CreatedAt = FDateTime::UtcNow();
    const FString Timestamp = CreatedAt.ToString(TEXT("%Y%m%d-%H%M%S"));
    const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString EvidencePath = FPaths::Combine(GetEvidenceDirectory(ProjectDirectory), FString::Printf(TEXT("%s-%s-%s.json"), *Timestamp, *SafeEventType, *UniqueSuffix));

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("version"), LedgerVersion);
    Root->SetStringField(TEXT("type"), EventType);
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("summary"), FUnrealAgentStudioKit::RedactSensitiveText(Summary));
    Root->SetStringField(TEXT("details"), FUnrealAgentStudioKit::RedactSensitiveText(Details));
    Root->SetStringField(TEXT("createdAt"), CreatedAt.ToIso8601());

    if (!FFileHelper::SaveStringToFile(JsonObjectToString(Root), *EvidencePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return false;
    }

    const FString DecisionLine = FString::Printf(
        TEXT("- %s [%s] %s: %s\n"),
        *CreatedAt.ToIso8601(),
        *Status,
        *EventType,
        *FUnrealAgentStudioKit::RedactSensitiveText(Summary));
    if (!FFileHelper::SaveStringToFile(DecisionLine, *GetDecisionsPath(ProjectDirectory), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
    {
        return false;
    }
    if (!SaveStateFile(ProjectDirectory, Summary))
    {
        return false;
    }

    if (OutEvidencePath != nullptr)
    {
        *OutEvidencePath = EvidencePath;
    }
    return true;
}

FUnrealAgentEvidenceSummary FUnrealAgentEvidenceLedger::LoadSummary(const FString& ProjectDirectory)
{
    FUnrealAgentEvidenceSummary Summary;
    Summary.StatePath = GetStatePath(ProjectDirectory);
    Summary.DecisionsPath = GetDecisionsPath(ProjectDirectory);
    Summary.EvidenceDirectory = GetEvidenceDirectory(ProjectDirectory);
    Summary.bWritable = FPaths::DirectoryExists(Summary.EvidenceDirectory);

    TArray<FString> EvidenceFiles;
    IFileManager::Get().FindFiles(EvidenceFiles, *FPaths::Combine(Summary.EvidenceDirectory, TEXT("*.json")), true, false);
    Summary.EvidenceCount = EvidenceFiles.Num();

    FString StateJson;
    if (FFileHelper::LoadFileToString(StateJson, *Summary.StatePath))
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StateJson);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            Root->TryGetStringField(TEXT("latestSummary"), Summary.LastSummary);
        }
    }
    return Summary;
}
