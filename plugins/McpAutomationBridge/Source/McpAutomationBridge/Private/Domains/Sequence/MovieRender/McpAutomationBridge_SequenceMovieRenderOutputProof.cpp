#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"

#include "HAL/FileManager.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Paths.h"
#include "MoviePipelineOutputSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MovieRenderPipelineDataTypes.h"

namespace McpSequenceMovieRender {
namespace {
FString GetOutputDirectory(UMoviePipelineExecutorJob *Job) {
  FString Message, Code;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, Message, Code);
  UMoviePipelineOutputSetting *Output =
      Config ? Cast<UMoviePipelineOutputSetting>(Config->FindSettingByClass(
                   UMoviePipelineOutputSetting::StaticClass(), true))
             : nullptr;
  if (!Output)
    return FString();
  FString ResolvedDirectory, ValidationError;
  return ValidateRenderOutputDirectory(Output->OutputDirectory.Path,
                                       ResolvedDirectory, ValidationError)
             ? ResolvedDirectory
             : FString();
}

bool FindOutputFiles(UMoviePipelineExecutorJob *Job, TArray<FString> &OutFiles,
                     FString &OutError) {
  const FString OutputDirectory = GetOutputDirectory(Job);
  if (OutputDirectory.IsEmpty() ||
      !IFileManager::Get().DirectoryExists(*OutputDirectory))
    return true;
  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  const int32 MaxEntries =
      Settings ? FMath::Max(1, Settings->MaxMovieRenderOutputScanFiles) : 1;
  class FBoundedOutputVisitor : public IPlatformFile::FDirectoryStatVisitor {
   public:
    FBoundedOutputVisitor(TArray<FString> &InFiles, int32 InMaxEntries)
        : Files(InFiles), MaxEntries(InMaxEntries) {}

    bool Visit(const TCHAR *Path, const FFileStatData &Stat) override {
      if (++EntriesVisited > MaxEntries) {
        bLimitExceeded = true;
        return false;
      }
      if (!Stat.bIsDirectory)
        Files.Add(Path);
      return true;
    }

    TArray<FString> &Files;
    const int32 MaxEntries;
    int32 EntriesVisited = 0;
    bool bLimitExceeded = false;
  };
  FBoundedOutputVisitor Visitor(OutFiles, MaxEntries);
  IFileManager::Get().IterateDirectoryStatRecursively(*OutputDirectory,
                                                       Visitor);
  if (Visitor.bLimitExceeded) {
    OutError = FString::Printf(
        TEXT("MRQ output discovery exceeded the configured scan limit of %d entries."),
        MaxEntries);
    return false;
  }
  return true;
}

FRenderFileSnapshot SnapshotFile(const FString &File) {
  const FFileStatData Stat = IFileManager::Get().GetStatData(*File);
  FRenderFileSnapshot Snapshot;
  if (Stat.bIsValid) {
    Snapshot.Size = Stat.FileSize;
    Snapshot.Modified = Stat.ModificationTime;
  }
  return Snapshot;
}

bool MatchesExpectedFormat(const FString &File, const FRenderWaitState &State) {
  FString Format = FPaths::GetCleanFilename(State.ExpectedFileNameFormat);
  Format.ReplaceInline(
      TEXT("{sequence_name}"),
      *FPaths::GetBaseFilename(State.ExpectedSequencePath),
      ESearchCase::IgnoreCase);
  Format.ReplaceInline(TEXT("{job_name}"), *State.ExpectedJobName,
                       ESearchCase::IgnoreCase);
  FString WildcardPattern;
  int32 Cursor = 0;
  bool bHasToken = false;
  while (Cursor < Format.Len()) {
    const int32 Open = Format.Find(TEXT("{"), ESearchCase::CaseSensitive,
                                   ESearchDir::FromStart, Cursor);
    if (Open == INDEX_NONE) {
      WildcardPattern += Format.Mid(Cursor);
      break;
    }
    WildcardPattern += Format.Mid(Cursor, Open - Cursor);
    const int32 Close = Format.Find(TEXT("}"), ESearchCase::CaseSensitive,
                                    ESearchDir::FromStart, Open + 1);
    if (Close == INDEX_NONE)
      return false;
    WildcardPattern += TEXT("*");
    bHasToken = true;
    Cursor = Close + 1;
  }
  const FString BaseName = FPaths::GetBaseFilename(File);
  return !BaseName.IsEmpty() && (bHasToken || !WildcardPattern.IsEmpty()) &&
         BaseName.MatchesWildcard(WildcardPattern, ESearchCase::CaseSensitive);
}

FString MakeOutputFileAlias(const FString &File) {
  FString RelativeFile = FPaths::ConvertRelativePathToFull(File);
  const FString SavedDirectory =
      FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
  if (!FPaths::MakePathRelativeTo(RelativeFile, *SavedDirectory) ||
      RelativeFile.StartsWith(TEXT(".."))) {
    return FString();
  }
  FPaths::NormalizeFilename(RelativeFile);
  return TEXT("/Saved/") + RelativeFile;
}
}

bool CaptureRenderOutputSnapshot(UMoviePipelineExecutorJob *Job,
                                 TSharedRef<FRenderWaitState> State,
                                 FString &OutMessage, FString &OutCode) {
  if (Job) {
    State->ExpectedJobName = Job->JobName;
    State->ExpectedSequencePath = Job->Sequence.ToString();
    FString Message, Code;
    if (MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config =
            ResolveConfig(Job, Message, Code)) {
      if (UMoviePipelineOutputSetting *Output =
              Cast<UMoviePipelineOutputSetting>(Config->FindSettingByClass(
                  UMoviePipelineOutputSetting::StaticClass(), true)))
        State->ExpectedFileNameFormat = Output->FileNameFormat;
        State->ExpectedOutputDirectory = GetOutputDirectory(Job);
    }
  }
  TArray<FString> Files;
  if (!FindOutputFiles(Job, Files, OutMessage)) {
    OutCode = TEXT("MRQ_OUTPUT_SCAN_LIMIT_EXCEEDED");
    return false;
  }
  for (const FString &File : Files)
    State->OutputFilesBeforeStart.Add(File, SnapshotFile(File));
  return true;
}

void CaptureRenderOutputData(const FMoviePipelineOutputData &OutputData,
                             TSharedRef<FRenderWaitState> State) {
  if (!OutputData.Job ||
      OutputData.Job->JobName != State->ExpectedJobName ||
      OutputData.Job->Sequence.ToString() != State->ExpectedSequencePath)
    return;
  for (const FMoviePipelineShotOutputData &Shot : OutputData.ShotData) {
    for (const TPair<FMoviePipelinePassIdentifier,
                     FMoviePipelineRenderPassOutputData> &Pass :
         Shot.RenderPassData) {
      State->ReportedRenderPasses.Add(Pass.Key.Name);
      for (const FString &File : Pass.Value.FilePaths)
        State->ReportedOutputFiles.Add(File);
    }
  }
}

int32 AppendRenderOutputProof(UMoviePipelineExecutorJob *Job,
                              const FRenderWaitState &State,
                              TSharedPtr<FJsonObject> Result) {
  TArray<FString> Files;
  if (State.ReportedOutputFiles.Num() > 0) {
    for (const FString &File : State.ReportedOutputFiles)
      if (IFileManager::Get().FileExists(*File))
        Files.Add(File);
    Result->SetStringField(TEXT("outputProof"), TEXT("executor_output_data"));
  } else {
    FString ScanError;
    if (!FindOutputFiles(Job, Files, ScanError)) {
      Result->SetStringField(TEXT("outputProof"),
                             TEXT("filesystem_scan_limit_exceeded"));
      Result->SetStringField(TEXT("outputProofErrorCode"),
                             TEXT("MRQ_OUTPUT_SCAN_LIMIT_EXCEEDED"));
      Result->SetStringField(
          TEXT("outputProofError"),
          TEXT("MRQ output discovery exceeded its configured scan limit."));
      Result->SetNumberField(TEXT("outputFileCount"), 0);
      Result->SetArrayField(TEXT("outputFiles"),
                            TArray<TSharedPtr<FJsonValue>>());
      return 0;
    }
    Files.RemoveAll([&State](const FString &File) {
      if (!MatchesExpectedFormat(File, State))
        return true;
      const FRenderFileSnapshot *Before =
          State.OutputFilesBeforeStart.Find(File);
      if (!Before)
        return false;
      const FRenderFileSnapshot After = SnapshotFile(File);
      return Before->Size == After.Size && Before->Modified == After.Modified;
    });
    Result->SetStringField(TEXT("outputProof"), TEXT("filesystem_metadata_delta"));
  }
  Files.Sort();
  Result->SetNumberField(TEXT("outputFileCount"), Files.Num());
  TArray<TSharedPtr<FJsonValue>> FileValues;
  const int32 MaxFiles = FMath::Min(Files.Num(), 20);
  for (int32 Index = 0; Index < MaxFiles; ++Index) {
    const FString Alias = MakeOutputFileAlias(Files[Index]);
    if (!Alias.IsEmpty())
      FileValues.Add(MakeShared<FJsonValueString>(Alias));
  }
  Result->SetArrayField(TEXT("outputFiles"), FileValues);
  TArray<FString> SortedPasses = State.ReportedRenderPasses.Array();
  SortedPasses.Sort();
  TArray<TSharedPtr<FJsonValue>> PassValues;
  for (const FString &PassName : SortedPasses)
    PassValues.Add(MakeShared<FJsonValueString>(PassName));
  Result->SetArrayField(TEXT("renderPasses"), PassValues);
  if (Files.Num() > MaxFiles)
    Result->SetBoolField(TEXT("outputFilesTruncated"), true);
  return Files.Num();
}
}

#endif
