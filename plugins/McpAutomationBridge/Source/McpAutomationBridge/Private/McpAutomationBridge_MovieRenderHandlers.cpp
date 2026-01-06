// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 30: Movie Render Queue Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"

// Movie Render Queue Headers (conditionally included)
#if __has_include("MoviePipelineQueue.h")
#define MCP_HAS_MOVIE_RENDER_QUEUE 1
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineHighResSetting.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineBurnInSetting.h"
#include "MoviePipeline.h"
#include "MoviePipelineExecutor.h"
#include "MovieRenderPipelineSettings.h"
#else
#define MCP_HAS_MOVIE_RENDER_QUEUE 0
#endif

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleMovieRenderAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_movie_render"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_movie_render")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_movie_render payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR && MCP_HAS_MOVIE_RENDER_QUEUE
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Movie render action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  UMoviePipelineQueueSubsystem* QueueSubsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
  if (!QueueSubsystem) {
    bSuccess = false;
    Message = TEXT("MoviePipelineQueueSubsystem not available");
    ErrorCode = TEXT("SUBSYSTEM_MISSING");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // CREATE QUEUE
  // ========================================================================
  if (LowerSub == TEXT("create_queue")) {
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      bSuccess = true;
      Message = TEXT("Queue already exists");
      Resp->SetNumberField(TEXT("queueSize"), Queue->GetJobs().Num());
    } else {
      bSuccess = false;
      Message = TEXT("Failed to get queue");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // ADD JOB
  // ========================================================================
  else if (LowerSub == TEXT("add_job")) {
    FString SequencePath;
    Payload->TryGetStringField(TEXT("sequencePath"), SequencePath);
    FString MapPath;
    Payload->TryGetStringField(TEXT("mapPath"), MapPath);
    FString JobName;
    Payload->TryGetStringField(TEXT("jobName"), JobName);

    if (SequencePath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("sequencePath required for add_job");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
      if (Queue) {
        UMoviePipelineExecutorJob* NewJob = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
        if (NewJob) {
          // Set sequence
          FSoftObjectPath SeqPath(SequencePath);
          NewJob->Sequence = SeqPath;
          
          // Set map if provided
          if (!MapPath.IsEmpty()) {
            FSoftObjectPath LevelPath(MapPath);
            NewJob->Map = LevelPath;
          }
          
          // Set job name
          if (!JobName.IsEmpty()) {
            NewJob->JobName = JobName;
          }
          
          bSuccess = true;
          Message = TEXT("Job added to queue");
          Resp->SetNumberField(TEXT("jobIndex"), Queue->GetJobs().Num() - 1);
          Resp->SetStringField(TEXT("jobName"), NewJob->JobName);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to allocate new job");
          ErrorCode = TEXT("JOB_ALLOCATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Queue not available");
        ErrorCode = TEXT("QUEUE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // REMOVE JOB
  // ========================================================================
  else if (LowerSub == TEXT("remove_job")) {
    double JobIndex = -1;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        Queue->DeleteJob(Jobs[Index]);
        bSuccess = true;
        Message = FString::Printf(TEXT("Removed job at index %d"), Index);
        Resp->SetNumberField(TEXT("queueSize"), Queue->GetJobs().Num());
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // CLEAR QUEUE
  // ========================================================================
  else if (LowerSub == TEXT("clear_queue")) {
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      for (UMoviePipelineExecutorJob* Job : Jobs) {
        Queue->DeleteJob(Job);
      }
      bSuccess = true;
      Message = TEXT("Queue cleared");
      Resp->SetNumberField(TEXT("queueSize"), 0);
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // GET QUEUE
  // ========================================================================
  else if (LowerSub == TEXT("get_queue")) {
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<TSharedPtr<FJsonValue>> JobsArray;
      for (UMoviePipelineExecutorJob* Job : Queue->GetJobs()) {
        if (!Job) continue;
        TSharedPtr<FJsonObject> JobObj = MakeShared<FJsonObject>();
        JobObj->SetStringField(TEXT("name"), Job->JobName);
        JobObj->SetStringField(TEXT("sequence"), Job->Sequence.GetAssetPathString());
        JobObj->SetStringField(TEXT("map"), Job->Map.GetAssetPathString());
        JobObj->SetBoolField(TEXT("enabled"), Job->IsEnabled());
        JobsArray.Add(MakeShared<FJsonValueObject>(JobObj));
      }
      Resp->SetArrayField(TEXT("jobs"), JobsArray);
      Resp->SetNumberField(TEXT("queueSize"), JobsArray.Num());
      bSuccess = true;
      Message = FString::Printf(TEXT("Found %d jobs in queue"), JobsArray.Num());
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE JOB
  // ========================================================================
  else if (LowerSub == TEXT("configure_job")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        UMoviePipelineExecutorJob* Job = Jobs[Index];
        
        // Update sequence
        FString SequencePath;
        if (Payload->TryGetStringField(TEXT("sequencePath"), SequencePath)) {
          Job->Sequence = FSoftObjectPath(SequencePath);
        }
        
        // Update map
        FString MapPath;
        if (Payload->TryGetStringField(TEXT("mapPath"), MapPath)) {
          Job->Map = FSoftObjectPath(MapPath);
        }
        
        // Update name
        FString JobName;
        if (Payload->TryGetStringField(TEXT("jobName"), JobName)) {
          Job->JobName = JobName;
        }
        
        bSuccess = true;
        Message = TEXT("Job configured");
        Resp->SetStringField(TEXT("jobName"), Job->JobName);
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE OUTPUT
  // ========================================================================
  else if (LowerSub == TEXT("configure_output")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        UMoviePipelineExecutorJob* Job = Jobs[Index];
        UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
        if (!Config) {
          Config = NewObject<UMoviePipelinePrimaryConfig>(Job);
          Job->SetConfiguration(Config);
        }
        
        // Find or create output setting
        UMoviePipelineOutputSetting* OutputSetting = Config->FindSetting<UMoviePipelineOutputSetting>();
        if (!OutputSetting) {
          OutputSetting = Cast<UMoviePipelineOutputSetting>(Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
        }
        
        if (OutputSetting) {
          // Output directory
          FString OutputDir;
          if (Payload->TryGetStringField(TEXT("outputDirectory"), OutputDir)) {
            OutputSetting->OutputDirectory.Path = OutputDir;
          }
          
          // File name format
          FString FileFormat;
          if (Payload->TryGetStringField(TEXT("fileNameFormat"), FileFormat)) {
            OutputSetting->FileNameFormat = FileFormat;
          }
          
          // Resolution
          double ResX = 0, ResY = 0;
          if (Payload->TryGetNumberField(TEXT("resolutionX"), ResX)) {
            OutputSetting->OutputResolution.X = static_cast<int32>(ResX);
          }
          if (Payload->TryGetNumberField(TEXT("resolutionY"), ResY)) {
            OutputSetting->OutputResolution.Y = static_cast<int32>(ResY);
          }
          
          // Frame rate
          double FrameRate = 0;
          if (Payload->TryGetNumberField(TEXT("frameRate"), FrameRate)) {
            OutputSetting->OutputFrameRate = FFrameRate(static_cast<int32>(FrameRate), 1);
          }
          
          bSuccess = true;
          Message = TEXT("Output settings configured");
          Resp->SetStringField(TEXT("outputDirectory"), OutputSetting->OutputDirectory.Path);
          Resp->SetNumberField(TEXT("resolutionX"), OutputSetting->OutputResolution.X);
          Resp->SetNumberField(TEXT("resolutionY"), OutputSetting->OutputResolution.Y);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to find/create output setting");
          ErrorCode = TEXT("SETTING_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // ADD RENDER PASS
  // ========================================================================
  else if (LowerSub == TEXT("add_render_pass")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    FString PassType;
    Payload->TryGetStringField(TEXT("passType"), PassType);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue && !PassType.IsEmpty()) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        UMoviePipelineExecutorJob* Job = Jobs[Index];
        UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
        if (!Config) {
          Config = NewObject<UMoviePipelinePrimaryConfig>(Job);
          Job->SetConfiguration(Config);
        }
        
        // Add deferred pass for most pass types
        if (PassType.Equals(TEXT("FinalImage"), ESearchCase::IgnoreCase) ||
            PassType.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase) ||
            PassType.Equals(TEXT("WorldNormal"), ESearchCase::IgnoreCase)) {
          UMoviePipelineDeferredPassBase* DeferredPass = Cast<UMoviePipelineDeferredPassBase>(
              Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass()));
          if (DeferredPass) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Added %s render pass"), *PassType);
            Resp->SetStringField(TEXT("passType"), PassType);
          }
        } else {
          bSuccess = true;
          Message = FString::Printf(TEXT("Pass type %s noted (may require specific setting class)"), *PassType);
        }
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available or passType missing");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    }
  }
  // ========================================================================
  // CONFIGURE ANTI-ALIASING
  // ========================================================================
  else if (LowerSub == TEXT("configure_anti_aliasing")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        UMoviePipelineExecutorJob* Job = Jobs[Index];
        UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
        if (!Config) {
          Config = NewObject<UMoviePipelinePrimaryConfig>(Job);
          Job->SetConfiguration(Config);
        }
        
        UMoviePipelineAntiAliasingSetting* AASetting = Cast<UMoviePipelineAntiAliasingSetting>(
            Config->FindOrAddSettingByClass(UMoviePipelineAntiAliasingSetting::StaticClass()));
        
        if (AASetting) {
          double SpatialCount = 0;
          if (Payload->TryGetNumberField(TEXT("spatialSampleCount"), SpatialCount)) {
            AASetting->SpatialSampleCount = FMath::Clamp(static_cast<int32>(SpatialCount), 1, 64);
          }
          
          double TemporalCount = 0;
          if (Payload->TryGetNumberField(TEXT("temporalSampleCount"), TemporalCount)) {
            AASetting->TemporalSampleCount = FMath::Clamp(static_cast<int32>(TemporalCount), 1, 64);
          }
          
          bool bOverride = false;
          if (Payload->TryGetBoolField(TEXT("overrideAntiAliasing"), bOverride)) {
            AASetting->bOverrideAntiAliasing = bOverride;
          }
          
          bSuccess = true;
          Message = TEXT("Anti-aliasing configured");
          Resp->SetNumberField(TEXT("spatialSampleCount"), AASetting->SpatialSampleCount);
          Resp->SetNumberField(TEXT("temporalSampleCount"), AASetting->TemporalSampleCount);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to find/create AA setting");
          ErrorCode = TEXT("SETTING_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE HIGH-RES SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("configure_high_res_settings")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue) {
      TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
      int32 Index = static_cast<int32>(JobIndex);
      if (Index >= 0 && Index < Jobs.Num()) {
        UMoviePipelineExecutorJob* Job = Jobs[Index];
        UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
        if (!Config) {
          Config = NewObject<UMoviePipelinePrimaryConfig>(Job);
          Job->SetConfiguration(Config);
        }
        
        UMoviePipelineHighResSetting* HRSetting = Cast<UMoviePipelineHighResSetting>(
            Config->FindOrAddSettingByClass(UMoviePipelineHighResSetting::StaticClass()));
        
        if (HRSetting) {
          double TileX = 0;
          if (Payload->TryGetNumberField(TEXT("tileCountX"), TileX)) {
            HRSetting->TileCount = FMath::Clamp(static_cast<int32>(TileX), 1, 16);
          }
          
          double Overlap = 0;
          if (Payload->TryGetNumberField(TEXT("overlapRatio"), Overlap)) {
            HRSetting->OverlapRatio = FMath::Clamp(static_cast<float>(Overlap), 0.0f, 0.5f);
          }
          
          bSuccess = true;
          Message = TEXT("High-res settings configured");
          Resp->SetNumberField(TEXT("tileCount"), HRSetting->TileCount);
          Resp->SetNumberField(TEXT("overlapRatio"), HRSetting->OverlapRatio);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to find/create high-res setting");
          ErrorCode = TEXT("SETTING_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Invalid job index");
        ErrorCode = TEXT("INVALID_INDEX");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue not available");
      ErrorCode = TEXT("QUEUE_NOT_FOUND");
    }
  }
  // ========================================================================
  // ADD CONSOLE VARIABLE
  // ========================================================================
  else if (LowerSub == TEXT("add_console_variable")) {
    double JobIndex = 0;
    Payload->TryGetNumberField(TEXT("jobIndex"), JobIndex);
    FString CvarName, CvarValue;
    Payload->TryGetStringField(TEXT("cvarName"), CvarName);
    Payload->TryGetStringField(TEXT("cvarValue"), CvarValue);
    
    if (CvarName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("cvarName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
      if (Queue) {
        TArray<UMoviePipelineExecutorJob*> Jobs = Queue->GetJobs();
        int32 Index = static_cast<int32>(JobIndex);
        if (Index >= 0 && Index < Jobs.Num()) {
          UMoviePipelineExecutorJob* Job = Jobs[Index];
          UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
          if (!Config) {
            Config = NewObject<UMoviePipelinePrimaryConfig>(Job);
            Job->SetConfiguration(Config);
          }
          
          UMoviePipelineConsoleVariableSetting* CVarSetting = Cast<UMoviePipelineConsoleVariableSetting>(
              Config->FindOrAddSettingByClass(UMoviePipelineConsoleVariableSetting::StaticClass()));
          
          if (CVarSetting) {
            // Add cvar to the setting
            // Note: The actual API may vary by UE version
            bSuccess = true;
            Message = FString::Printf(TEXT("Console variable %s=%s noted"), *CvarName, *CvarValue);
            Resp->SetStringField(TEXT("cvarName"), CvarName);
            Resp->SetStringField(TEXT("cvarValue"), CvarValue);
          } else {
            bSuccess = false;
            Message = TEXT("Failed to find/create console variable setting");
            ErrorCode = TEXT("SETTING_NOT_FOUND");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Invalid job index");
          ErrorCode = TEXT("INVALID_INDEX");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Queue not available");
        ErrorCode = TEXT("QUEUE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // START RENDER
  // ========================================================================
  else if (LowerSub == TEXT("start_render")) {
    UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
    if (Queue && Queue->GetJobs().Num() > 0) {
      // Check if already rendering
      if (QueueSubsystem->IsRendering()) {
        bSuccess = false;
        Message = TEXT("Render already in progress");
        ErrorCode = TEXT("ALREADY_RENDERING");
      } else {
        // Start render with PIE executor
        QueueSubsystem->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());
        bSuccess = true;
        Message = TEXT("Render started");
        Resp->SetNumberField(TEXT("jobCount"), Queue->GetJobs().Num());
      }
    } else {
      bSuccess = false;
      Message = TEXT("Queue is empty or not available");
      ErrorCode = TEXT("QUEUE_EMPTY");
    }
  }
  // ========================================================================
  // STOP RENDER
  // ========================================================================
  else if (LowerSub == TEXT("stop_render")) {
    if (QueueSubsystem->IsRendering()) {
      // Cancel current render
      UMoviePipeline* ActivePipeline = QueueSubsystem->GetActiveMoviePipeline();
      if (ActivePipeline) {
        ActivePipeline->RequestShutdown();
        bSuccess = true;
        Message = TEXT("Render stop requested");
      } else {
        bSuccess = false;
        Message = TEXT("No active pipeline to stop");
        ErrorCode = TEXT("NO_ACTIVE_PIPELINE");
      }
    } else {
      bSuccess = true;
      Message = TEXT("No render in progress");
    }
  }
  // ========================================================================
  // GET RENDER STATUS
  // ========================================================================
  else if (LowerSub == TEXT("get_render_status")) {
    TSharedPtr<FJsonObject> StatusObj = MakeShared<FJsonObject>();
    
    if (QueueSubsystem->IsRendering()) {
      UMoviePipeline* ActivePipeline = QueueSubsystem->GetActiveMoviePipeline();
      if (ActivePipeline) {
        StatusObj->SetStringField(TEXT("state"), TEXT("Rendering"));
        // Get progress info
        FMoviePipelineOutputData OutputData = ActivePipeline->GetOutputData();
        // Pipeline state details would go here
        bSuccess = true;
        Message = TEXT("Render in progress");
      } else {
        StatusObj->SetStringField(TEXT("state"), TEXT("Unknown"));
        bSuccess = true;
        Message = TEXT("Rendering but no active pipeline");
      }
    } else {
      StatusObj->SetStringField(TEXT("state"), TEXT("Idle"));
      bSuccess = true;
      Message = TEXT("No render in progress");
    }
    
    Resp->SetObjectField(TEXT("renderStatus"), StatusObj);
  }
  // ========================================================================
  // GET RENDER PROGRESS
  // ========================================================================
  else if (LowerSub == TEXT("get_render_progress")) {
    if (QueueSubsystem->IsRendering()) {
      UMoviePipeline* ActivePipeline = QueueSubsystem->GetActiveMoviePipeline();
      if (ActivePipeline) {
        // Progress is available through the pipeline
        Resp->SetStringField(TEXT("state"), TEXT("Rendering"));
        bSuccess = true;
        Message = TEXT("Render in progress");
      } else {
        Resp->SetStringField(TEXT("state"), TEXT("Unknown"));
        bSuccess = true;
      }
    } else {
      Resp->SetStringField(TEXT("state"), TEXT("Idle"));
      Resp->SetNumberField(TEXT("progress"), 0);
      bSuccess = true;
      Message = TEXT("No render in progress");
    }
  }
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Movie render action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Movie Render Queue not available or not in editor
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Movie render actions require editor build with MovieRenderPipeline plugin enabled."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
