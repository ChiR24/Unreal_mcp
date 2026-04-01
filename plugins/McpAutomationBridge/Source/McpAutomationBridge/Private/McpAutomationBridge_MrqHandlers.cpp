// =============================================================================
// McpAutomationBridge_MrqHandlers.cpp
// =============================================================================
// Movie Render Queue (MRQ) Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Queue Management
//   - get_queue                    : List all jobs in the MRQ queue
//   - create_job                   : Create a new render job
//   - delete_job                   : Remove a job from the queue
//
// Section 2: Job Configuration
//   - get_job_config               : Read all settings for a job
//   - get_cvars                    : Read CVar overrides
//   - set_cvars                    : Add/modify/remove CVar overrides
//   - get_output_settings          : Read output settings
//   - set_output_settings          : Modify output settings
//
// Section 3: Preset Management
//   - load_preset                  : Load a preset config asset
//   - save_preset                  : Save config as preset asset
//
// Section 4: Render Execution
//   - render                       : Execute the queue
//   - get_render_status            : Check render progress
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.1: UMoviePipelineMasterConfig
// UE 5.2+: UMoviePipelinePrimaryConfig
// Both are handled via __has_include checks.
//
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// MRQ headers - guarded for optional availability
// UE 5.2+ renamed MasterConfig → PrimaryConfig; handle both.
#if __has_include("MoviePipelineQueueSubsystem.h")
#define MCP_HAS_MRQ 1
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelineExecutor.h"
#if __has_include("MoviePipelinePrimaryConfig.h")
#include "MoviePipelinePrimaryConfig.h"
#elif __has_include("MoviePipelineMasterConfig.h")
#include "MoviePipelineMasterConfig.h"
using UMoviePipelinePrimaryConfig = UMoviePipelineMasterConfig;
#endif
#include "MoviePipelineSetting.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelinePIEExecutor.h"
#include "MovieRenderPipelineDataTypes.h"
#else
#define MCP_HAS_MRQ 0
#endif

// ─── Helper: Get Queue Subsystem ──────────────────────────────────────────────
#if MCP_HAS_MRQ
static UMoviePipelineQueueSubsystem* GetMRQSubsystem()
{
#if WITH_EDITOR
    if (GEditor)
    {
        return GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
    }
#endif
    return nullptr;
}

static UMoviePipelineExecutorJob* GetJobByIndex(UMoviePipelineQueue* Queue, int32 JobIndex)
{
    if (!Queue) return nullptr;
    const TArray<UMoviePipelineExecutorJob*>& Jobs = Queue->GetJobs();
    if (!Jobs.IsValidIndex(JobIndex)) return nullptr;
    return Jobs[JobIndex];
}
#endif

// ─── MRQ Action Dispatcher ───────────────────────────────────────────────────

bool UMcpAutomationBridgeSubsystem::HandleMrqAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if !MCP_HAS_MRQ
    TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
    ErrorResult->SetStringField(TEXT("error"), TEXT("MRQ_NOT_AVAILABLE"));
    ErrorResult->SetStringField(TEXT("message"), TEXT("Movie Render Queue plugin is not available in this engine build"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Movie Render Queue plugin is not available"), ErrorResult, TEXT("MRQ_NOT_AVAILABLE"));
    return true;
#else
    FString SubAction;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("action"), SubAction);
    }
    const FString Lower = SubAction.ToLower();

    UMoviePipelineQueueSubsystem* Subsystem = GetMRQSubsystem();
    if (!Subsystem)
    {
        TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
        ErrorResult->SetStringField(TEXT("error"), TEXT("SUBSYSTEM_NOT_FOUND"));
        SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("MRQ subsystem not available (editor only)"), ErrorResult, TEXT("SUBSYSTEM_NOT_FOUND"));
        return true;
    }

    UMoviePipelineQueue* Queue = Subsystem->GetQueue();

    // ─── get_queue ────────────────────────────────────────────────────────────
    if (Lower == TEXT("get_queue"))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> JobsArray;

        if (Queue)
        {
            const TArray<UMoviePipelineExecutorJob*>& Jobs = Queue->GetJobs();
            for (int32 i = 0; i < Jobs.Num(); i++)
            {
                UMoviePipelineExecutorJob* Job = Jobs[i];
                if (!Job) continue;

                TSharedPtr<FJsonObject> JobObj = MakeShared<FJsonObject>();
                JobObj->SetNumberField(TEXT("index"), i);
                JobObj->SetStringField(TEXT("jobName"), Job->JobName);
                JobObj->SetStringField(TEXT("map"), Job->Map.ToString());
                JobObj->SetStringField(TEXT("sequence"), Job->Sequence.ToString());
                JobObj->SetBoolField(TEXT("enabled"), Job->IsEnabled());

                // Config summary
                UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
                if (Config)
                {
                    TArray<UMoviePipelineSetting*> Settings = Config->GetUserSettings();
                    JobObj->SetNumberField(TEXT("settingCount"), Settings.Num());

                    TArray<TSharedPtr<FJsonValue>> SettingNames;
                    for (UMoviePipelineSetting* Setting : Settings)
                    {
                        if (Setting)
                        {
                            SettingNames.Add(MakeShared<FJsonValueString>(Setting->GetClass()->GetName()));
                        }
                    }
                    JobObj->SetArrayField(TEXT("settings"), SettingNames);
                }

                JobsArray.Add(MakeShared<FJsonValueObject>(JobObj));
            }
        }

        Result->SetArrayField(TEXT("jobs"), JobsArray);
        Result->SetNumberField(TEXT("jobCount"), JobsArray.Num());
        Result->SetBoolField(TEXT("isRendering"), Subsystem->IsRendering());
        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Queue has %d jobs"), JobsArray.Num()), Result);
        return true;
    }

    // ─── get_job_config ───────────────────────────────────────────────────────
    if (Lower == TEXT("get_job_config"))
    {
        int32 JobIndex = 0;
        if (Payload.IsValid()) Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("jobName"), Job->JobName);
        Result->SetStringField(TEXT("map"), Job->Map.ToString());
        Result->SetStringField(TEXT("sequence"), Job->Sequence.ToString());

        UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
        if (Config)
        {
            // Output settings
            UMoviePipelineOutputSetting* Output = Config->FindSetting<UMoviePipelineOutputSetting>();
            if (Output)
            {
                TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
                OutputObj->SetStringField(TEXT("outputDirectory"), Output->OutputDirectory.Path);
                OutputObj->SetNumberField(TEXT("resolutionX"), Output->OutputResolution.X);
                OutputObj->SetNumberField(TEXT("resolutionY"), Output->OutputResolution.Y);
                OutputObj->SetStringField(TEXT("fileNameFormat"), Output->FileNameFormat);
                OutputObj->SetBoolField(TEXT("overrideExisting"), Output->bOverrideExistingOutput);
                OutputObj->SetNumberField(TEXT("zeroPadFrameNumbers"), Output->ZeroPadFrameNumbers);
                OutputObj->SetNumberField(TEXT("frameNumberOffset"), Output->FrameNumberOffset);
                OutputObj->SetNumberField(TEXT("handleFrameCount"), Output->HandleFrameCount);
                Result->SetObjectField(TEXT("outputSettings"), OutputObj);
            }

            // CVar overrides
            UMoviePipelineConsoleVariableSetting* CVars = Config->FindSetting<UMoviePipelineConsoleVariableSetting>();
            if (CVars)
            {
                TSharedPtr<FJsonObject> CVarObj = MakeShared<FJsonObject>();
                TArray<FMoviePipelineConsoleVariableEntry> CVarEntries = CVars->GetConsoleVariables();
                for (const FMoviePipelineConsoleVariableEntry& Entry : CVarEntries)
                {
                    CVarObj->SetNumberField(Entry.Name, Entry.Value);
                }
                Result->SetObjectField(TEXT("consoleVariables"), CVarObj);

                TArray<TSharedPtr<FJsonValue>> StartCmds;
                for (const FString& Cmd : CVars->StartConsoleCommands)
                {
                    StartCmds.Add(MakeShared<FJsonValueString>(Cmd));
                }
                Result->SetArrayField(TEXT("startConsoleCommands"), StartCmds);

                TArray<TSharedPtr<FJsonValue>> EndCmds;
                for (const FString& Cmd : CVars->EndConsoleCommands)
                {
                    EndCmds.Add(MakeShared<FJsonValueString>(Cmd));
                }
                Result->SetArrayField(TEXT("endConsoleCommands"), EndCmds);
            }

            // Anti-aliasing
            UMoviePipelineAntiAliasingSetting* AA = Config->FindSetting<UMoviePipelineAntiAliasingSetting>();
            if (AA)
            {
                TSharedPtr<FJsonObject> AAObj = MakeShared<FJsonObject>();
                AAObj->SetNumberField(TEXT("spatialSampleCount"), AA->SpatialSampleCount);
                AAObj->SetNumberField(TEXT("temporalSampleCount"), AA->TemporalSampleCount);
                AAObj->SetBoolField(TEXT("overrideAntiAliasing"), AA->bOverrideAntiAliasing);
                Result->SetObjectField(TEXT("antiAliasing"), AAObj);
            }

            // All settings summary
            TArray<TSharedPtr<FJsonValue>> AllSettings;
            for (UMoviePipelineSetting* Setting : Config->GetUserSettings())
            {
                if (Setting)
                {
                    TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
                    S->SetStringField(TEXT("class"), Setting->GetClass()->GetName());
                    S->SetBoolField(TEXT("enabled"), Setting->IsEnabled());
                    AllSettings.Add(MakeShared<FJsonValueObject>(S));
                }
            }
            Result->SetArrayField(TEXT("allSettings"), AllSettings);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Job config retrieved"), Result);
        return true;
    }

    // ─── get_cvars ────────────────────────────────────────────────────────────
    if (Lower == TEXT("get_cvars"))
    {
        int32 JobIndex = 0;
        if (Payload.IsValid()) Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
        if (Config)
        {
            UMoviePipelineConsoleVariableSetting* CVars = Config->FindSetting<UMoviePipelineConsoleVariableSetting>();
            if (CVars)
            {
                TSharedPtr<FJsonObject> CVarObj = MakeShared<FJsonObject>();
                TArray<FMoviePipelineConsoleVariableEntry> CVarEntries = CVars->GetConsoleVariables();
                for (const FMoviePipelineConsoleVariableEntry& Entry : CVarEntries)
                {
                    CVarObj->SetNumberField(Entry.Name, Entry.Value);
                }
                Result->SetObjectField(TEXT("consoleVariables"), CVarObj);

                TArray<TSharedPtr<FJsonValue>> StartCmds;
                for (const FString& Cmd : CVars->StartConsoleCommands)
                {
                    StartCmds.Add(MakeShared<FJsonValueString>(Cmd));
                }
                Result->SetArrayField(TEXT("startConsoleCommands"), StartCmds);

                TArray<TSharedPtr<FJsonValue>> EndCmds;
                for (const FString& Cmd : CVars->EndConsoleCommands)
                {
                    EndCmds.Add(MakeShared<FJsonValueString>(Cmd));
                }
                Result->SetArrayField(TEXT("endConsoleCommands"), EndCmds);
            }
            else
            {
                Result->SetObjectField(TEXT("consoleVariables"), MakeShared<FJsonObject>());
                Result->SetStringField(TEXT("note"), TEXT("No CVar setting found on this job config"));
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("CVar overrides retrieved"), Result);
        return true;
    }

    // ─── set_cvars ────────────────────────────────────────────────────────────
    if (Lower == TEXT("set_cvars"))
    {
        if (!Payload.IsValid())
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_PAYLOAD"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("set_cvars requires a valid payload"), Err, TEXT("INVALID_PAYLOAD"));
            return true;
        }
        int32 JobIndex = 0;
        Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
        if (!Config)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("NO_CONFIG"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Job has no pipeline config"), Err, TEXT("NO_CONFIG"));
            return true;
        }

        UMoviePipelineConsoleVariableSetting* CVars = Cast<UMoviePipelineConsoleVariableSetting>(Config->FindOrAddSettingByClass(UMoviePipelineConsoleVariableSetting::StaticClass()));
        if (!CVars)
        {
            TSharedPtr<FJsonObject> Err2 = MakeShared<FJsonObject>();
            Err2->SetStringField(TEXT("error"), TEXT("SETTING_NOT_AVAILABLE"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Failed to find or create CVar setting"), Err2, TEXT("SETTING_NOT_AVAILABLE"));
            return true;
        }

        // Set CVars from payload
        const TSharedPtr<FJsonObject>* SetObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("set"), SetObj) && SetObj && (*SetObj).IsValid())
        {
            for (const auto& Pair : (*SetObj)->Values)
            {
                double Value = 0.0;
                if (Pair.Value->TryGetNumber(Value))
                {
                    CVars->AddOrUpdateConsoleVariable(Pair.Key, (float)Value);
                }
            }
        }

        // Remove CVars from payload
        const TArray<TSharedPtr<FJsonValue>>* RemoveArr = nullptr;
        if (Payload->TryGetArrayField(TEXT("remove"), RemoveArr) && RemoveArr)
        {
            for (const auto& Val : *RemoveArr)
            {
                FString Key;
                if (Val->TryGetString(Key))
                {
                    CVars->RemoveConsoleVariable(Key);
                }
            }
        }

        // Start console commands
        const TArray<TSharedPtr<FJsonValue>>* StartArr = nullptr;
        if (Payload->TryGetArrayField(TEXT("startCommands"), StartArr) && StartArr)
        {
            CVars->StartConsoleCommands.Empty();
            for (const auto& Val : *StartArr)
            {
                FString Cmd;
                if (Val->TryGetString(Cmd))
                {
                    CVars->StartConsoleCommands.Add(Cmd);
                }
            }
        }

        // End console commands
        const TArray<TSharedPtr<FJsonValue>>* EndArr = nullptr;
        if (Payload->TryGetArrayField(TEXT("endCommands"), EndArr) && EndArr)
        {
            CVars->EndConsoleCommands.Empty();
            for (const auto& Val : *EndArr)
            {
                FString Cmd;
                if (Val->TryGetString(Cmd))
                {
                    CVars->EndConsoleCommands.Add(Cmd);
                }
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("totalCVars"), CVars->GetConsoleVariables().Num());
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("CVar overrides updated"), Result);
        return true;
    }

    // ─── get_output_settings ──────────────────────────────────────────────────
    if (Lower == TEXT("get_output_settings"))
    {
        int32 JobIndex = 0;
        if (Payload.IsValid()) Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
        if (Config)
        {
            UMoviePipelineOutputSetting* Output = Config->FindSetting<UMoviePipelineOutputSetting>();
            if (Output)
            {
                Result->SetStringField(TEXT("outputDirectory"), Output->OutputDirectory.Path);
                Result->SetNumberField(TEXT("resolutionX"), Output->OutputResolution.X);
                Result->SetNumberField(TEXT("resolutionY"), Output->OutputResolution.Y);
                Result->SetStringField(TEXT("fileNameFormat"), Output->FileNameFormat);
                Result->SetBoolField(TEXT("overrideExisting"), Output->bOverrideExistingOutput);
                Result->SetNumberField(TEXT("zeroPadFrameNumbers"), Output->ZeroPadFrameNumbers);
                Result->SetNumberField(TEXT("frameNumberOffset"), Output->FrameNumberOffset);
                Result->SetNumberField(TEXT("handleFrameCount"), Output->HandleFrameCount);
                Result->SetBoolField(TEXT("useCustomFrameRate"), Output->bUseCustomFrameRate);
                Result->SetBoolField(TEXT("useCustomPlaybackRange"), Output->bUseCustomPlaybackRange);
                Result->SetNumberField(TEXT("customStartFrame"), Output->CustomStartFrame);
                Result->SetNumberField(TEXT("customEndFrame"), Output->CustomEndFrame);
            }
            else
            {
                Result->SetStringField(TEXT("note"), TEXT("No output setting found on this job config"));
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Output settings retrieved"), Result);
        return true;
    }

    // ─── set_output_settings ──────────────────────────────────────────────────
    if (Lower == TEXT("set_output_settings"))
    {
        if (!Payload.IsValid())
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_PAYLOAD"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("set_output_settings requires a valid payload"), Err, TEXT("INVALID_PAYLOAD"));
            return true;
        }
        int32 JobIndex = 0;
        Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        UMoviePipelinePrimaryConfig* Config = Cast<UMoviePipelinePrimaryConfig>(Job->GetConfiguration());
        if (!Config)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("NO_CONFIG"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Job has no pipeline config"), Err, TEXT("NO_CONFIG"));
            return true;
        }

        UMoviePipelineOutputSetting* Output = Cast<UMoviePipelineOutputSetting>(Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
        if (!Output)
        {
            TSharedPtr<FJsonObject> Err2 = MakeShared<FJsonObject>();
            Err2->SetStringField(TEXT("error"), TEXT("SETTING_NOT_AVAILABLE"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Failed to find or create output setting"), Err2, TEXT("SETTING_NOT_AVAILABLE"));
            return true;
        }

        FString StrVal;
        double NumVal;
        bool BoolVal;

        if (Payload->TryGetStringField(TEXT("outputDirectory"), StrVal))
        {
            // Normalize and reject path traversal
            FString CleanDir = StrVal;
            FPaths::NormalizeFilename(CleanDir);
            FPaths::CollapseRelativeDirectories(CleanDir);
            if (CleanDir.Contains(TEXT("..")))
            {
                TSharedPtr<FJsonObject> Err2 = MakeShared<FJsonObject>();
                Err2->SetStringField(TEXT("error"), TEXT("INVALID_PATH"));
                SendAutomationResponse(RequestingSocket, RequestId, false,
                    TEXT("outputDirectory rejected: path traversal (..) not allowed"), Err2, TEXT("INVALID_PATH"));
                return true;
            }
            Output->OutputDirectory.Path = CleanDir;
        }
        if (Payload->TryGetNumberField(TEXT("resolutionX"), NumVal))
            Output->OutputResolution.X = (int32)NumVal;
        if (Payload->TryGetNumberField(TEXT("resolutionY"), NumVal))
            Output->OutputResolution.Y = (int32)NumVal;
        if (Payload->TryGetStringField(TEXT("fileNameFormat"), StrVal))
            Output->FileNameFormat = StrVal;
        if (Payload->TryGetBoolField(TEXT("overrideExisting"), BoolVal))
            Output->bOverrideExistingOutput = BoolVal;
        if (Payload->TryGetNumberField(TEXT("zeroPadFrameNumbers"), NumVal))
            Output->ZeroPadFrameNumbers = (int32)NumVal;
        if (Payload->TryGetNumberField(TEXT("frameNumberOffset"), NumVal))
            Output->FrameNumberOffset = (int32)NumVal;
        if (Payload->TryGetNumberField(TEXT("handleFrameCount"), NumVal))
            Output->HandleFrameCount = (int32)NumVal;
        if (Payload->TryGetBoolField(TEXT("useCustomFrameRate"), BoolVal))
            Output->bUseCustomFrameRate = BoolVal;
        if (Payload->TryGetBoolField(TEXT("useCustomPlaybackRange"), BoolVal))
            Output->bUseCustomPlaybackRange = BoolVal;
        if (Payload->TryGetNumberField(TEXT("customStartFrame"), NumVal))
            Output->CustomStartFrame = (int32)NumVal;
        if (Payload->TryGetNumberField(TEXT("customEndFrame"), NumVal))
            Output->CustomEndFrame = (int32)NumVal;

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("outputDirectory"), Output->OutputDirectory.Path);
        Result->SetNumberField(TEXT("resolutionX"), Output->OutputResolution.X);
        Result->SetNumberField(TEXT("resolutionY"), Output->OutputResolution.Y);
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Output settings updated"), Result);
        return true;
    }

    // ─── create_job ───────────────────────────────────────────────────────────
    if (Lower == TEXT("create_job"))
    {
        if (!Queue)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("NO_QUEUE"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("No MRQ queue available"), Err, TEXT("NO_QUEUE"));
            return true;
        }

        FString MapPath, SequencePath, JobName;
        if (Payload.IsValid())
        {
            Payload->TryGetStringField(TEXT("map"), MapPath);
            Payload->TryGetStringField(TEXT("sequence"), SequencePath);
            Payload->TryGetStringField(TEXT("jobName"), JobName);
        }

        if (MapPath.IsEmpty() || SequencePath.IsEmpty())
        {
            TSharedPtr<FJsonObject> Err2 = MakeShared<FJsonObject>();
            Err2->SetStringField(TEXT("error"), TEXT("MISSING_REQUIRED_FIELDS"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("create_job requires both 'map' and 'sequence'"), Err2, TEXT("MISSING_REQUIRED_FIELDS"));
            return true;
        }

        UMoviePipelineExecutorJob* NewJob = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
        NewJob->Map = FSoftObjectPath(MapPath);
        NewJob->Sequence = FSoftObjectPath(SequencePath);
        if (!JobName.IsEmpty())
            NewJob->JobName = JobName;
        else
            NewJob->JobName = FString::Printf(TEXT("Job_%d"), Queue->GetJobs().Num() - 1);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("jobIndex"), Queue->GetJobs().Num() - 1);
        Result->SetStringField(TEXT("jobName"), NewJob->JobName);
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Job created"), Result);
        return true;
    }

    // ─── delete_job ───────────────────────────────────────────────────────────
    if (Lower == TEXT("delete_job"))
    {
        int32 JobIndex = 0;
        if (!Payload.IsValid() || !Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex))
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("MISSING_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("delete_job requires explicit jobIndex"), Err, TEXT("MISSING_JOB_INDEX"));
            return true;
        }

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        Queue->DeleteJob(Job);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("remainingJobs"), Queue->GetJobs().Num());
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Job deleted"), Result);
        return true;
    }

    // ─── render ───────────────────────────────────────────────────────────────
    if (Lower == TEXT("render"))
    {
        if (Subsystem->IsRendering())
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("ALREADY_RENDERING"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("A render is already in progress"), Err, TEXT("ALREADY_RENDERING"));
            return true;
        }

        if (!Queue || Queue->GetJobs().Num() == 0)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("EMPTY_QUEUE"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Queue is empty — add jobs before rendering"), Err, TEXT("EMPTY_QUEUE"));
            return true;
        }

        Subsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("jobCount"), Queue->GetJobs().Num());
        Result->SetBoolField(TEXT("renderStarted"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Render queue started"), Result);
        return true;
    }

    // ─── get_render_status ────────────────────────────────────────────────────
    if (Lower == TEXT("get_render_status"))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("isRendering"), Subsystem->IsRendering());

        UMoviePipelineExecutorBase* Executor = Subsystem->GetActiveExecutor();
        if (Executor)
        {
            Result->SetStringField(TEXT("executorClass"), Executor->GetClass()->GetName());
            Result->SetStringField(TEXT("statusMessage"), Executor->GetStatusMessage());
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Render status retrieved"), Result);
        return true;
    }

    // ─── load_preset ──────────────────────────────────────────────────────────
    if (Lower == TEXT("load_preset"))
    {
        int32 JobIndex = 0;
        FString PresetPath;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
            Payload->TryGetStringField(TEXT("presetPath"), PresetPath);
        }

        if (PresetPath.IsEmpty())
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("MISSING_PRESET_PATH"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("presetPath is required"), Err, TEXT("MISSING_PRESET_PATH"));
            return true;
        }

        UMoviePipelineExecutorJob* Job = GetJobByIndex(Queue, JobIndex);
        if (!Job)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("INVALID_JOB_INDEX"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("No job at index %d"), JobIndex), Err, TEXT("INVALID_JOB_INDEX"));
            return true;
        }

        UMoviePipelinePrimaryConfig* Preset = LoadObject<UMoviePipelinePrimaryConfig>(
            nullptr, *PresetPath);
        if (!Preset)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("PRESET_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Preset not found: %s"), *PresetPath), Err, TEXT("PRESET_NOT_FOUND"));
            return true;
        }

        Job->SetConfiguration(Preset);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("presetPath"), PresetPath);
        Result->SetNumberField(TEXT("settingCount"), Preset->GetUserSettings().Num());
        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Preset loaded"), Result);
        return true;
    }

    // ─── Unknown action fallback ──────────────────────────────────────────────
    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("error"), TEXT("UNKNOWN_ACTION"));
    Err->SetStringField(TEXT("action"), SubAction);
    SendAutomationResponse(RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Unknown MRQ action: %s"), *SubAction), Err, TEXT("UNKNOWN_ACTION"));
    return true;
#endif // MCP_HAS_MRQ
}
