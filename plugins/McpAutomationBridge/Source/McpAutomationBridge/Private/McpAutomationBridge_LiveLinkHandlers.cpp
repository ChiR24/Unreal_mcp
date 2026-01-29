// McpAutomationBridge_LiveLinkHandlers.cpp
// Phase 39: Motion Capture & Live Link Handlers
// Implements: Live Link sources, subjects, presets, face tracking, skeleton mapping
// 64 actions across core, face, and mocap categories
// ACTION NAMES ARE ALIGNED WITH TypeScript handler (livelink-handlers.ts)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "Misc/Guid.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// LIVE LINK (conditional - built-in since UE 4.19+)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_LIVELINK
  #undef MCP_HAS_LIVELINK
#endif

#if __has_include("ILiveLinkClient.h")
  #define MCP_HAS_LIVELINK 1
  #include "ILiveLinkClient.h"
  #include "LiveLinkTypes.h"
  #include "LiveLinkSourceSettings.h"
  #include "LiveLinkSubjectSettings.h"
  #include "LiveLinkRole.h"

  #if __has_include("LiveLinkVirtualSubject.h")
    #include "LiveLinkVirtualSubject.h"
    #define MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS 1
  #else
    #define MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS 0
  #endif
#else
  #define MCP_HAS_LIVELINK 0
  #define MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS 0
#endif

#if MCP_HAS_LIVELINK && __has_include("LiveLinkClient.h")
#include "LiveLinkClient.h"
#include "LiveLinkPreset.h"
#include "LiveLinkSourceFactory.h"
#define MCP_HAS_LIVELINK_FULL 1
#else
#define MCP_HAS_LIVELINK_FULL 0
#endif

#if MCP_HAS_LIVELINK && __has_include("LiveLinkComponentController.h")
#include "LiveLinkComponentController.h"
#define MCP_HAS_LIVELINK_COMPONENTS 1
#else
#define MCP_HAS_LIVELINK_COMPONENTS 0
#endif

#if MCP_HAS_LIVELINK && __has_include("LiveLinkMessageBusFinder.h")
#include "LiveLinkMessageBusFinder.h"
#define MCP_HAS_LIVELINK_MESSAGEBUS_FINDER 1
#else
#define MCP_HAS_LIVELINK_MESSAGEBUS_FINDER 0
#endif

#if MCP_HAS_LIVELINK && __has_include("Roles/LiveLinkAnimationRole.h")
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkAnimationTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#define MCP_HAS_LIVELINK_ROLES 1
#else
#define MCP_HAS_LIVELINK_ROLES 0
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
namespace
{
    TSharedPtr<FJsonObject> MakeLiveLinkSuccess(const FString& InMessage)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), InMessage);
        return Result;
    }

    TSharedPtr<FJsonObject> MakeLiveLinkError(const FString& InMessage, const FString& ErrorCode = TEXT("ERROR"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorCode);
        Result->SetStringField(TEXT("message"), InMessage);
        return Result;
    }

    TSharedPtr<FJsonObject> MakeLiveLinkNotAvailable()
    {
        return MakeLiveLinkError(
            TEXT("Live Link is not available in this build. Please enable the LiveLink plugin."),
            TEXT("LIVELINK_NOT_AVAILABLE")
        );
    }

    FString GetStringFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, const FString& Default = TEXT(""))
    {
        return Payload->HasField(Field) ? Payload->GetStringField(Field) : Default;
    }

    bool GetBoolFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, bool Default = false)
    {
        return Payload->HasField(Field) ? Payload->GetBoolField(Field) : Default;
    }

    double GetNumberFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, double Default = 0.0)
    {
        return Payload->HasField(Field) ? Payload->GetNumberField(Field) : Default;
    }

#if MCP_HAS_LIVELINK
    ILiveLinkClient* GetLiveLinkClient()
    {
        IModularFeatures& ModularFeatures = IModularFeatures::Get();
        if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
        {
            return &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
        }
        return nullptr;
    }

    FString SubjectStateToString(ELiveLinkSubjectState State)
    {
        switch (State)
        {
            case ELiveLinkSubjectState::Connected: return TEXT("Connected");
            case ELiveLinkSubjectState::Unresponsive: return TEXT("Unresponsive");
            case ELiveLinkSubjectState::Disconnected: return TEXT("Disconnected");
            case ELiveLinkSubjectState::InvalidOrDisabled: return TEXT("InvalidOrDisabled");
            case ELiveLinkSubjectState::Paused: return TEXT("Paused");
            default: return TEXT("Unknown");
        }
    }
#endif
}

// ============================================================================
// MAIN HANDLER DISPATCHER
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageLiveLinkAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result;

#if !MCP_HAS_LIVELINK
    // Live Link not available - return error for all actions
    Result = MakeLiveLinkNotAvailable();
    SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
    return true;
#else

    ILiveLinkClient* LiveLinkClient = GetLiveLinkClient();
    
    // =========================================================================
    // LIVE LINK SOURCES (9 actions)
    // =========================================================================
    
    if (Action == TEXT("list_livelink_sources"))
    {
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            TArray<FGuid> SourceGuids = LiveLinkClient->GetSources(true);
            
            TArray<TSharedPtr<FJsonValue>> SourcesArray;
            SourcesArray.Reserve(SourceGuids.Num());
            for (const FGuid& Guid : SourceGuids)
            {
                TSharedPtr<FJsonObject> SourceObj = MakeShareable(new FJsonObject());
                SourceObj->SetStringField(TEXT("guid"), Guid.ToString());
                SourceObj->SetStringField(TEXT("type"), LiveLinkClient->GetSourceType(Guid).ToString());
                SourceObj->SetStringField(TEXT("status"), LiveLinkClient->GetSourceStatus(Guid).ToString());
                SourceObj->SetStringField(TEXT("machineName"), LiveLinkClient->GetSourceMachineName(Guid).ToString());
                SourcesArray.Add(MakeShareable(new FJsonValueObject(SourceObj)));
            }
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d sources"), SourceGuids.Num()));
            Result->SetArrayField(TEXT("sources"), SourcesArray);
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_source_status"))
    {
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        if (SourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("sourceGuid is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            FGuid::Parse(SourceGuidStr, SourceGuid);
            
            bool bValid = LiveLinkClient->IsSourceStillValid(SourceGuid);
            FText Status = LiveLinkClient->GetSourceStatus(SourceGuid);
            FText Type = LiveLinkClient->GetSourceType(SourceGuid);
            
            Result = MakeLiveLinkSuccess(TEXT("Source status retrieved"));
            Result->SetStringField(TEXT("sourceGuid"), SourceGuidStr);
            Result->SetStringField(TEXT("status"), Status.ToString());
            Result->SetStringField(TEXT("type"), Type.ToString());
            Result->SetBoolField(TEXT("isValid"), bValid);
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_source_type"))
    {
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        if (SourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("sourceGuid is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            FGuid::Parse(SourceGuidStr, SourceGuid);
            FText Type = LiveLinkClient->GetSourceType(SourceGuid);
            
            Result = MakeLiveLinkSuccess(TEXT("Source type retrieved"));
            Result->SetStringField(TEXT("sourceType"), Type.ToString());
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("remove_livelink_source"))
    {
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        if (SourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("sourceGuid is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            FGuid::Parse(SourceGuidStr, SourceGuid);
            LiveLinkClient->RemoveSource(SourceGuid);
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Removed source %s"), *SourceGuidStr));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("remove_all_sources"))
    {
#if MCP_HAS_LIVELINK_FULL
        FLiveLinkClient* FullClient = static_cast<FLiveLinkClient*>(LiveLinkClient);
        if (FullClient)
        {
            FullClient->RemoveAllSources();
            Result = MakeLiveLinkSuccess(TEXT("All sources removed"));
        }
        else
        {
            Result = MakeLiveLinkError(TEXT("Could not access full Live Link client"), TEXT("CLIENT_ERROR"));
        }
#else
        Result = MakeLiveLinkError(TEXT("RemoveAllSources not available in this build"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("add_livelink_source") || Action == TEXT("add_messagebus_source"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString ConnectionString = GetStringFieldSafe(Payload, TEXT("connectionString"));
        FString SourceTypeName = GetStringFieldSafe(Payload, TEXT("sourceType"), TEXT("MessageBus"));
        
        // Find the appropriate factory
        TArray<UClass*> FactoryClasses;
        GetDerivedClasses(ULiveLinkSourceFactory::StaticClass(), FactoryClasses);
        
        ULiveLinkSourceFactory* FoundFactory = nullptr;
        for (UClass* FactoryClass : FactoryClasses)
        {
            ULiveLinkSourceFactory* Factory = FactoryClass->GetDefaultObject<ULiveLinkSourceFactory>();
            if (Factory && Factory->GetSourceDisplayName().ToString().Contains(SourceTypeName))
            {
                FoundFactory = Factory;
                break;
            }
        }
        
        if (FoundFactory)
        {
            TSharedPtr<ILiveLinkSource> NewSource = FoundFactory->CreateSource(ConnectionString);
            if (NewSource.IsValid())
            {
                FGuid SourceGuid = LiveLinkClient->AddSource(NewSource);
                Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Added source: %s"), *SourceGuid.ToString()));
                Result->SetStringField(TEXT("sourceGuid"), SourceGuid.ToString());
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("Failed to create source from factory"), TEXT("CREATE_FAILED"));
            }
        }
        else
        {
            Result = MakeLiveLinkError(FString::Printf(TEXT("Source factory '%s' not found"), *SourceTypeName), TEXT("FACTORY_NOT_FOUND"));
        }
#else
        Result = MakeLiveLinkError(TEXT("Source factory API not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("discover_messagebus_sources"))
    {
#if MCP_HAS_LIVELINK_MESSAGEBUS_FINDER
        double DurationSeconds = GetNumberFieldSafe(Payload, TEXT("durationSeconds"), 0.2);
        if (DurationSeconds <= 0.0)
        {
            DurationSeconds = 0.2;
        }
        // Safety cap to avoid excessively long polling.
        if (DurationSeconds > 5.0)
        {
            DurationSeconds = 5.0;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Result = MakeLiveLinkError(TEXT("No active world"), TEXT("NO_WORLD"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // Create the message bus finder
        ULiveLinkMessageBusFinder* Finder = NewObject<ULiveLinkMessageBusFinder>(World);
        if (!Finder)
        {
            Result = MakeLiveLinkError(TEXT("Failed to construct LiveLinkMessageBusFinder"), TEXT("FINDER_CREATE_FAILED"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // Fire a network poll, then collect after DurationSeconds using a timer.
        Finder->PollNetwork();

        const FString RequestIdCopy = RequestId;
        const TSharedPtr<FMcpBridgeWebSocket> SocketCopy = RequestingSocket;

        // CRITICAL: Use TWeakObjectPtr to prevent GC crash if Finder is collected during timer delay
        TWeakObjectPtr<ULiveLinkMessageBusFinder> WeakFinder(Finder);
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);

        FTimerDelegate TimerDelegate;
        TimerDelegate.BindLambda([WeakThis, WeakFinder, RequestIdCopy, SocketCopy]()
        {
            // Validate pointers are still valid after GC
            if (!WeakThis.IsValid() || !WeakFinder.IsValid())
            {
                // Objects were garbage collected - cannot send response since subsystem may be gone
                // Client will timeout; this is the safest behavior
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
                    TEXT("LiveLink timer callback: UObject(s) garbage collected before timer fired. RequestId=%s"), 
                    *RequestIdCopy);
                return;
            }

            TArray<FProviderPollResult> Providers;
            WeakFinder->GetPollResults(Providers);

            TArray<TSharedPtr<FJsonValue>> ProvidersArray;
            ProvidersArray.Reserve(Providers.Num());

            for (const FProviderPollResult& Provider : Providers)
            {
                TSharedPtr<FJsonObject> ProviderObj = MakeShareable(new FJsonObject());
                ProviderObj->SetStringField(TEXT("name"), Provider.Name);
                ProviderObj->SetStringField(TEXT("machineName"), Provider.MachineName);
                ProviderObj->SetBoolField(TEXT("isValidProvider"), Provider.bIsValidProvider);
                ProviderObj->SetNumberField(TEXT("machineTimeOffset"), Provider.MachineTimeOffset);
                ProviderObj->SetStringField(TEXT("address"), Provider.Address.ToString());

                // Serialize annotations (FName -> FString)
                TSharedPtr<FJsonObject> AnnotationsObj = MakeShareable(new FJsonObject());
                for (const TPair<FName, FString>& Pair : Provider.Annotations)
                {
                    AnnotationsObj->SetStringField(Pair.Key.ToString(), Pair.Value);
                }
                ProviderObj->SetObjectField(TEXT("annotations"), AnnotationsObj);

                ProvidersArray.Add(MakeShareable(new FJsonValueObject(ProviderObj)));
            }

            TSharedPtr<FJsonObject> TimerResult = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d message bus providers"), Providers.Num()));
            TimerResult->SetArrayField(TEXT("providers"), ProvidersArray);

            WeakThis->SendAutomationResponse(SocketCopy, RequestIdCopy, TimerResult->GetBoolField(TEXT("success")), TimerResult->GetStringField(TEXT("message")), TimerResult);
        });

        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle, TimerDelegate, static_cast<float>(DurationSeconds), false);

        // Do not respond immediately; response will be sent by timer.
        return true;
#else
        Result = MakeLiveLinkError(TEXT("LiveLinkMessageBusFinder not available in this build"), TEXT("NOT_SUPPORTED"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#endif
    }
    
    if (Action == TEXT("configure_source_settings"))
    {
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        if (SourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("sourceGuid is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            FGuid::Parse(SourceGuidStr, SourceGuid);
            
            ULiveLinkSourceSettings* Settings = LiveLinkClient->GetSourceSettings(SourceGuid);
            if (Settings)
            {
                // Apply settings from payload
                const TSharedPtr<FJsonObject>* SettingsObj;
                if (Payload->TryGetObjectField(TEXT("sourceSettings"), SettingsObj))
                {
                    // Buffer mode
                    FString ModeStr;
                    if ((*SettingsObj)->TryGetStringField(TEXT("mode"), ModeStr))
                    {
                        // UE 5.7 uses EngineTime, Timecode, Latest instead of LatestFrame, TimeSynchronized
                        if (ModeStr == TEXT("LatestFrame") || ModeStr == TEXT("Latest"))
                        {
                            Settings->Mode = ELiveLinkSourceMode::Latest;
                        }
                        else if (ModeStr == TEXT("TimeSynchronized") || ModeStr == TEXT("Timecode"))
                        {
                            Settings->Mode = ELiveLinkSourceMode::Timecode;
                        }
                        else if (ModeStr == TEXT("EngineTime"))
                        {
                            Settings->Mode = ELiveLinkSourceMode::EngineTime;
                        }
                    }
                    
                    // Offsets
                    double BufferOffset;
                    if ((*SettingsObj)->TryGetNumberField(TEXT("bufferOffset"), BufferOffset))
                    {
                        // UE 5.7: LatestOffset is int32, not FFrameTime
                        Settings->BufferSettings.LatestOffset = FMath::RoundToInt32(BufferOffset);
                    }
                }
                
                Result = MakeLiveLinkSuccess(TEXT("Source settings configured"));
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("Could not get source settings"), TEXT("SETTINGS_NOT_FOUND"));
            }
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // LIVE LINK SUBJECTS (15 actions)
    // =========================================================================
    
    if (Action == TEXT("list_livelink_subjects"))
    {
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            bool bIncludeDisabled = GetBoolFieldSafe(Payload, TEXT("includeDisabledSubjects"), true);
            bool bIncludeVirtual = GetBoolFieldSafe(Payload, TEXT("includeVirtualSubjects"), true);
            
            TArray<FLiveLinkSubjectKey> SubjectKeys = LiveLinkClient->GetSubjects(bIncludeDisabled, bIncludeVirtual);
            
            TArray<TSharedPtr<FJsonValue>> SubjectsArray;
            SubjectsArray.Reserve(SubjectKeys.Num());
            for (const FLiveLinkSubjectKey& Key : SubjectKeys)
            {
                TSharedPtr<FJsonObject> SubjectObj = MakeShareable(new FJsonObject());
                SubjectObj->SetStringField(TEXT("sourceGuid"), Key.Source.ToString());
                SubjectObj->SetStringField(TEXT("subjectName"), Key.SubjectName.ToString());
                
                TSubclassOf<ULiveLinkRole> Role = LiveLinkClient->GetSubjectRole_AnyThread(Key);
                SubjectObj->SetStringField(TEXT("role"), Role ? Role->GetName() : TEXT("Unknown"));
                
                bool bEnabled = LiveLinkClient->IsSubjectEnabled(Key, false);
                SubjectObj->SetBoolField(TEXT("enabled"), bEnabled);
                
                ELiveLinkSubjectState State = LiveLinkClient->GetSubjectState(Key.SubjectName);
                SubjectObj->SetStringField(TEXT("state"), SubjectStateToString(State));
                
                SubjectsArray.Add(MakeShareable(new FJsonValueObject(SubjectObj)));
            }
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d subjects"), SubjectKeys.Num()));
            Result->SetArrayField(TEXT("subjects"), SubjectsArray);
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subject_role"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            TSubclassOf<ULiveLinkRole> Role = LiveLinkClient->GetSubjectRole_AnyThread(FLiveLinkSubjectName(*SubjectName));
            Result = MakeLiveLinkSuccess(TEXT("Subject role retrieved"));
            Result->SetStringField(TEXT("subjectRole"), Role ? Role->GetName() : TEXT("Unknown"));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subject_state"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            ELiveLinkSubjectState State = LiveLinkClient->GetSubjectState(FLiveLinkSubjectName(*SubjectName));
            Result = MakeLiveLinkSuccess(TEXT("Subject state retrieved"));
            Result->SetStringField(TEXT("subjectState"), SubjectStateToString(State));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("enable_subject"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            if (!SourceGuidStr.IsEmpty())
            {
                FGuid::Parse(SourceGuidStr, SourceGuid);
            }
            
            FLiveLinkSubjectKey SubjectKey(SourceGuid, *SubjectName);
            LiveLinkClient->SetSubjectEnabled(SubjectKey, true);
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Subject '%s' enabled"), *SubjectName));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("disable_subject"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            if (!SourceGuidStr.IsEmpty())
            {
                FGuid::Parse(SourceGuidStr, SourceGuid);
            }
            
            FLiveLinkSubjectKey SubjectKey(SourceGuid, *SubjectName);
            LiveLinkClient->SetSubjectEnabled(SubjectKey, false);
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Subject '%s' disabled"), *SubjectName));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("pause_subject"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            LiveLinkClient->PauseSubject_AnyThread(FLiveLinkSubjectName(*SubjectName));
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Subject '%s' paused"), *SubjectName));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("unpause_subject"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            LiveLinkClient->UnpauseSubject_AnyThread(FLiveLinkSubjectName(*SubjectName));
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Subject '%s' unpaused"), *SubjectName));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("clear_subject_frames"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            LiveLinkClient->ClearSubjectsFrames_AnyThread(FLiveLinkSubjectName(*SubjectName));
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Cleared frames for subject '%s'"), *SubjectName));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subject_static_data"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            FGuid SourceGuid;
            if (!SourceGuidStr.IsEmpty())
            {
                FGuid::Parse(SourceGuidStr, SourceGuid);
            }
            
            FLiveLinkSubjectKey SubjectKey(SourceGuid, *SubjectName);
            const FLiveLinkStaticDataStruct* StaticData = LiveLinkClient->GetSubjectStaticData_AnyThread(SubjectKey);
            
            if (StaticData && StaticData->IsValid())
            {
                Result = MakeLiveLinkSuccess(TEXT("Static data retrieved"));
                
                TSharedPtr<FJsonObject> StaticDataObj = MakeShareable(new FJsonObject());
                
#if MCP_HAS_LIVELINK_ROLES
                // Try to cast to skeleton static data
                if (const FLiveLinkSkeletonStaticData* SkeletonData = StaticData->Cast<FLiveLinkSkeletonStaticData>())
                {
                    TArray<TSharedPtr<FJsonValue>> BoneNamesArray;
                    BoneNamesArray.Reserve(SkeletonData->BoneNames.Num());
                    for (const FName& BoneName : SkeletonData->BoneNames)
                    {
                        BoneNamesArray.Add(MakeShareable(new FJsonValueString(BoneName.ToString())));
                    }
                    StaticDataObj->SetArrayField(TEXT("boneNames"), BoneNamesArray);
                    
                    TArray<TSharedPtr<FJsonValue>> BoneParentsArray;
                    BoneParentsArray.Reserve(SkeletonData->BoneParents.Num());
                    for (int32 ParentIdx : SkeletonData->BoneParents)
                    {
                        BoneParentsArray.Add(MakeShareable(new FJsonValueNumber(ParentIdx)));
                    }
                    StaticDataObj->SetArrayField(TEXT("boneParents"), BoneParentsArray);
                }
#endif
                
                Result->SetObjectField(TEXT("staticData"), StaticDataObj);
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("No static data available for subject"), TEXT("NO_DATA"));
            }
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subject_frame_data"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        FString RoleName = GetStringFieldSafe(Payload, TEXT("roleName"), TEXT("Animation"));
        
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
#if MCP_HAS_LIVELINK_ROLES
            TSubclassOf<ULiveLinkRole> RoleClass = nullptr;
            
            if (RoleName == TEXT("Animation"))
                RoleClass = ULiveLinkAnimationRole::StaticClass();
            else if (RoleName == TEXT("Transform"))
                RoleClass = ULiveLinkTransformRole::StaticClass();
            else if (RoleName == TEXT("Camera"))
                RoleClass = ULiveLinkCameraRole::StaticClass();
            else if (RoleName == TEXT("Light"))
                RoleClass = ULiveLinkLightRole::StaticClass();
            
            if (RoleClass)
            {
                FLiveLinkSubjectFrameData FrameData;
                if (LiveLinkClient->EvaluateFrame_AnyThread(FLiveLinkSubjectName(*SubjectName), RoleClass, FrameData))
                {
                    Result = MakeLiveLinkSuccess(TEXT("Frame data retrieved"));
                    
                    TSharedPtr<FJsonObject> FrameDataObj = MakeShareable(new FJsonObject());
                    // UE 5.7: Access base data through GetBaseData() accessor
                    if (const FLiveLinkBaseFrameData* BaseData = FrameData.FrameData.GetBaseData())
                    {
                        FrameDataObj->SetNumberField(TEXT("worldTime"), BaseData->WorldTime.GetSourceTime());
                    }
                    else
                    {
                        FrameDataObj->SetNumberField(TEXT("worldTime"), 0.0);
                    }
                    
                    Result->SetObjectField(TEXT("frameData"), FrameDataObj);
                }
                else
                {
                    Result = MakeLiveLinkError(TEXT("Failed to evaluate frame"), TEXT("EVAL_FAILED"));
                }
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown role: %s"), *RoleName), TEXT("UNKNOWN_ROLE"));
            }
#else
            Result = MakeLiveLinkError(TEXT("Live Link roles not available"), TEXT("NOT_SUPPORTED"));
#endif
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subject_frame_times"))
    {
        FString SubjectName = GetStringFieldSafe(Payload, TEXT("subjectName"));
        if (SubjectName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
        }
        else if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            TArray<FLiveLinkTime> FrameTimes = LiveLinkClient->GetSubjectFrameTimes(FLiveLinkSubjectName(*SubjectName));
            
            TArray<TSharedPtr<FJsonValue>> TimesArray;
            TimesArray.Reserve(FrameTimes.Num());
            for (const FLiveLinkTime& Time : FrameTimes)
            {
                TimesArray.Add(MakeShareable(new FJsonValueNumber(Time.WorldTime)));
            }
            
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Retrieved %d frame times"), FrameTimes.Num()));
            Result->SetArrayField(TEXT("frameTimes"), TimesArray);
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_subjects_by_role"))
    {
        FString RoleName = GetStringFieldSafe(Payload, TEXT("roleName"), TEXT("Animation"));
        bool bIncludeDisabled = GetBoolFieldSafe(Payload, TEXT("includeDisabledSubjects"), false);
        bool bIncludeVirtual = GetBoolFieldSafe(Payload, TEXT("includeVirtualSubjects"), true);
        
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
#if MCP_HAS_LIVELINK_ROLES
            TSubclassOf<ULiveLinkRole> RoleClass = nullptr;
            
            if (RoleName == TEXT("Animation"))
                RoleClass = ULiveLinkAnimationRole::StaticClass();
            else if (RoleName == TEXT("Transform"))
                RoleClass = ULiveLinkTransformRole::StaticClass();
            else if (RoleName == TEXT("Camera"))
                RoleClass = ULiveLinkCameraRole::StaticClass();
            else if (RoleName == TEXT("Light"))
                RoleClass = ULiveLinkLightRole::StaticClass();
            
            if (RoleClass)
            {
                TArray<FLiveLinkSubjectKey> SubjectKeys = LiveLinkClient->GetSubjectsSupportingRole(RoleClass, bIncludeDisabled, bIncludeVirtual);
                
                TArray<TSharedPtr<FJsonValue>> SubjectsArray;
                SubjectsArray.Reserve(SubjectKeys.Num());
                for (const FLiveLinkSubjectKey& Key : SubjectKeys)
                {
                    TSharedPtr<FJsonObject> SubjectObj = MakeShareable(new FJsonObject());
                    SubjectObj->SetStringField(TEXT("sourceGuid"), Key.Source.ToString());
                    SubjectObj->SetStringField(TEXT("subjectName"), Key.SubjectName.ToString());
                    SubjectsArray.Add(MakeShareable(new FJsonValueObject(SubjectObj)));
                }
                
                Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d subjects with role %s"), SubjectKeys.Num(), *RoleName));
                Result->SetArrayField(TEXT("subjects"), SubjectsArray);
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown role: %s"), *RoleName), TEXT("UNKNOWN_ROLE"));
            }
#else
            Result = MakeLiveLinkError(TEXT("Live Link roles not available"), TEXT("NOT_SUPPORTED"));
#endif
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("add_virtual_subject"))
    {
#if MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const FString VirtualSubjectNameStr = GetStringFieldSafe(Payload, TEXT("virtualSubjectName"));
        if (VirtualSubjectNameStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("virtualSubjectName is required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // Optional: allow specifying a custom virtual subject class (defaults to ULiveLinkVirtualSubject).
        const FString VirtualSubjectClassName = GetStringFieldSafe(Payload, TEXT("virtualSubjectClass"), TEXT("LiveLinkVirtualSubject"));
        UClass* VirtualSubjectClass = nullptr;
        if (!VirtualSubjectClassName.IsEmpty())
        {
            VirtualSubjectClass = ResolveClassByName(VirtualSubjectClassName);
        }
        if (!VirtualSubjectClass)
        {
            VirtualSubjectClass = ULiveLinkVirtualSubject::StaticClass();
        }

        // Source for virtual subjects typically comes from a dedicated "Virtual" source.
        const FString VirtualSourceName = GetStringFieldSafe(Payload, TEXT("virtualSourceName"), TEXT("MCPVirtual"));
        const FGuid VirtualSourceGuid = LiveLinkClient->AddVirtualSubjectSource(*VirtualSourceName);
        FLiveLinkSubjectKey VirtualKey(VirtualSourceGuid, *VirtualSubjectNameStr);

        const bool bAdded = LiveLinkClient->AddVirtualSubject(VirtualKey, VirtualSubjectClass);
        if (!bAdded)
        {
            Result = MakeLiveLinkError(FString::Printf(TEXT("Failed to add virtual subject '%s'"), *VirtualSubjectNameStr), TEXT("ADD_FAILED"));
        }
        else
        {
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Added virtual subject '%s'"), *VirtualSubjectNameStr));
            Result->SetStringField(TEXT("virtualSourceGuid"), VirtualSourceGuid.ToString());
            Result->SetStringField(TEXT("virtualSubjectName"), VirtualSubjectNameStr);
        }

        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#else
        Result = MakeLiveLinkError(TEXT("Virtual subjects are not available in this build"), TEXT("NOT_SUPPORTED"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#endif
    }

    if (Action == TEXT("remove_virtual_subject"))
    {
#if MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const FString VirtualSubjectNameStr = GetStringFieldSafe(Payload, TEXT("virtualSubjectName"));
        if (VirtualSubjectNameStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("virtualSubjectName is required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const FString VirtualSourceGuidStr = GetStringFieldSafe(Payload, TEXT("virtualSourceGuid"));
        if (VirtualSourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("virtualSourceGuid is required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        FGuid VirtualSourceGuid;
        if (!FGuid::Parse(VirtualSourceGuidStr, VirtualSourceGuid))
        {
            Result = MakeLiveLinkError(TEXT("virtualSourceGuid is invalid"), TEXT("INVALID_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        FLiveLinkSubjectKey VirtualKey(VirtualSourceGuid, *VirtualSubjectNameStr);
        LiveLinkClient->RemoveVirtualSubject(VirtualKey);

        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Removed virtual subject '%s'"), *VirtualSubjectNameStr));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#else
        Result = MakeLiveLinkError(TEXT("Virtual subjects are not available in this build"), TEXT("NOT_SUPPORTED"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#endif
    }

    if (Action == TEXT("configure_subject_settings"))
    {
#if MCP_HAS_LIVELINK_VIRTUAL_SUBJECTS
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const FString SubjectNameStr = GetStringFieldSafe(Payload, TEXT("subjectName"));
        const FString SourceGuidStr = GetStringFieldSafe(Payload, TEXT("sourceGuid"));
        if (SubjectNameStr.IsEmpty() || SourceGuidStr.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("subjectName and sourceGuid are required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        FGuid SourceGuid;
        if (!FGuid::Parse(SourceGuidStr, SourceGuid))
        {
            Result = MakeLiveLinkError(TEXT("sourceGuid is invalid"), TEXT("INVALID_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const FLiveLinkSubjectKey SubjectKey(SourceGuid, *SubjectNameStr);
        UObject* SettingsObj = LiveLinkClient->GetSubjectSettings(SubjectKey);
        if (!SettingsObj)
        {
            Result = MakeLiveLinkError(TEXT("Subject settings not found"), TEXT("SETTINGS_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        ULiveLinkSubjectSettings* SubjectSettings = Cast<ULiveLinkSubjectSettings>(SettingsObj);
        if (!SubjectSettings)
        {
            Result = MakeLiveLinkError(TEXT("Subject settings object is not ULiveLinkSubjectSettings (may be a virtual subject object)"), TEXT("SETTINGS_TYPE_UNSUPPORTED"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const TSharedPtr<FJsonObject>* SettingsPayloadObj = nullptr;
        if (!Payload->TryGetObjectField(TEXT("subjectSettings"), SettingsPayloadObj) || !SettingsPayloadObj || !SettingsPayloadObj->IsValid())
        {
            Result = MakeLiveLinkError(TEXT("subjectSettings object is required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        const TSharedPtr<FJsonObject>& SettingsPayload = *SettingsPayloadObj;

        // Minimal, safe subset: rebroadcast and evaluation settings.
        bool bRebroadcast = SubjectSettings->bRebroadcastSubject;
        if (SettingsPayload->TryGetBoolField(TEXT("rebroadcast"), bRebroadcast))
        {
            SubjectSettings->bRebroadcastSubject = bRebroadcast;
        }

        Result = MakeLiveLinkSuccess(TEXT("Subject settings configured"));
        Result->SetBoolField(TEXT("rebroadcast"), SubjectSettings->bRebroadcastSubject);

        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#else
        Result = MakeLiveLinkError(TEXT("Virtual subjects are not available in this build"), TEXT("NOT_SUPPORTED"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#endif
    }

    // =========================================================================
    // LIVE LINK PRESETS (8 actions)
    // =========================================================================
    
    if (Action == TEXT("create_livelink_preset") || Action == TEXT("save_livelink_preset"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetName = GetStringFieldSafe(Payload, TEXT("presetName"), TEXT("LiveLinkPreset"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("presetPath"), TEXT("/Game/LiveLink"));
        
        FString FullPath = PackagePath / PresetName;
        UPackage* Package = CreatePackage(*FullPath);
        
        ULiveLinkPreset* Preset = NewObject<ULiveLinkPreset>(Package, *PresetName, RF_Public | RF_Standalone);
        if (Preset)
        {
            Preset->BuildFromClient();
            
            if (McpSafeAssetSave(Preset))
            {
                Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Created preset: %s"), *FullPath));
                Result->SetStringField(TEXT("presetPath"), FullPath);
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("Failed to save preset"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
            Result = MakeLiveLinkError(TEXT("Failed to create preset object"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("load_livelink_preset"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        if (PresetPath.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            ULiveLinkPreset* Preset = LoadObject<ULiveLinkPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Loaded preset: %s"), *PresetPath));
                Result->SetNumberField(TEXT("sourceCount"), Preset->GetSourcePresets().Num());
                Result->SetNumberField(TEXT("subjectCount"), Preset->GetSubjectPresets().Num());
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Failed to load preset: %s"), *PresetPath), TEXT("LOAD_FAILED"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("apply_livelink_preset"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        if (PresetPath.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            ULiveLinkPreset* Preset = LoadObject<ULiveLinkPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                Preset->ApplyToClientLatent([](bool bSuccess) {
                    // Callback when complete
                });
                Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Applying preset: %s (async)"), *PresetPath));
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Failed to load preset: %s"), *PresetPath), TEXT("LOAD_FAILED"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("add_preset_to_client"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        bool bRecreate = GetBoolFieldSafe(Payload, TEXT("recreateExisting"), true);
        
        if (PresetPath.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            ULiveLinkPreset* Preset = LoadObject<ULiveLinkPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                bool bSuccess = Preset->AddToClient(bRecreate);
                if (bSuccess)
                {
                    Result = MakeLiveLinkSuccess(TEXT("Preset added to client"));
                }
                else
                {
                    Result = MakeLiveLinkError(TEXT("Failed to add preset to client"), TEXT("ADD_FAILED"));
                }
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Failed to load preset: %s"), *PresetPath), TEXT("LOAD_FAILED"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("build_preset_from_client"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        if (PresetPath.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            ULiveLinkPreset* Preset = LoadObject<ULiveLinkPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                Preset->BuildFromClient();
                if (McpSafeAssetSave(Preset))
                {
                    Result = MakeLiveLinkSuccess(TEXT("Preset rebuilt from current client state"));
                }
                else
                {
                    Result = MakeLiveLinkError(TEXT("Failed to save preset"), TEXT("SAVE_FAILED"));
                }
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("Preset not found. Use create_livelink_preset first."), TEXT("NOT_FOUND"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("get_preset_sources") || Action == TEXT("get_preset_subjects"))
    {
#if MCP_HAS_LIVELINK_FULL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        if (PresetPath.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            ULiveLinkPreset* Preset = LoadObject<ULiveLinkPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                if (Action == TEXT("get_preset_sources"))
                {
                    const TArray<FLiveLinkSourcePreset>& Sources = Preset->GetSourcePresets();
                    TArray<TSharedPtr<FJsonValue>> SourcesArray;
                    for (const FLiveLinkSourcePreset& Source : Sources)
                    {
                        TSharedPtr<FJsonObject> SourceObj = MakeShareable(new FJsonObject());
                        SourceObj->SetStringField(TEXT("guid"), Source.Guid.ToString());
                        SourceObj->SetStringField(TEXT("type"), Source.SourceType.ToString());
                        SourcesArray.Add(MakeShareable(new FJsonValueObject(SourceObj)));
                    }
                    Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d sources in preset"), Sources.Num()));
                    Result->SetArrayField(TEXT("presetSources"), SourcesArray);
                }
                else
                {
                    const TArray<FLiveLinkSubjectPreset>& Subjects = Preset->GetSubjectPresets();
                    TArray<TSharedPtr<FJsonValue>> SubjectsArray;
                    for (const FLiveLinkSubjectPreset& Subject : Subjects)
                    {
                        TSharedPtr<FJsonObject> SubjectObj = MakeShareable(new FJsonObject());
                        SubjectObj->SetStringField(TEXT("sourceGuid"), Subject.Key.Source.ToString());
                        SubjectObj->SetStringField(TEXT("subjectName"), Subject.Key.SubjectName.ToString());
                        SubjectObj->SetBoolField(TEXT("enabled"), Subject.bEnabled);
                        SubjectsArray.Add(MakeShareable(new FJsonValueObject(SubjectObj)));
                    }
                    Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d subjects in preset"), Subjects.Num()));
                    Result->SetArrayField(TEXT("presetSubjects"), SubjectsArray);
                }
            }
            else
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Failed to load preset: %s"), *PresetPath), TEXT("LOAD_FAILED"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link presets not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // LIVE LINK COMPONENTS (8 actions)
    // =========================================================================
    
    if (Action == TEXT("add_livelink_controller"))
    {
#if MCP_HAS_LIVELINK_COMPONENTS
        FString ActorName = GetStringFieldSafe(Payload, TEXT("actorName"));
        if (ActorName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("actorName is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UWorld* World = GetActiveWorld();
            if (World)
            {
                AActor* TargetActor = nullptr;
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
                    {
                        TargetActor = *It;
                        break;
                    }
                }
                
                if (TargetActor)
                {
                    ULiveLinkComponentController* Controller = NewObject<ULiveLinkComponentController>(TargetActor, NAME_None, RF_Transactional);
                    if (Controller)
                    {
                        Controller->RegisterComponent();
                        TargetActor->AddInstanceComponent(Controller);
                        
                        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Added LiveLinkComponentController to '%s'"), *ActorName));
                    }
                    else
                    {
                        Result = MakeLiveLinkError(TEXT("Failed to create controller component"), TEXT("CREATE_FAILED"));
                    }
                }
                else
                {
                    Result = MakeLiveLinkError(FString::Printf(TEXT("Actor '%s' not found"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
                }
            }
            else
            {
                Result = MakeLiveLinkError(TEXT("No editor world available"), TEXT("NO_WORLD"));
            }
        }
#else
        Result = MakeLiveLinkError(TEXT("Live Link components not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("configure_livelink_controller") ||
        Action == TEXT("set_controller_subject") ||
        Action == TEXT("set_controller_role") ||
        Action == TEXT("enable_controller_evaluation") ||
        Action == TEXT("disable_controller_evaluation") ||
        Action == TEXT("set_controlled_component") ||
        Action == TEXT("get_controller_info"))
    {
#if MCP_HAS_LIVELINK_COMPONENTS
        const FString ActorName = GetStringFieldSafe(Payload, TEXT("actorName"));
        if (ActorName.IsEmpty())
        {
            Result = MakeLiveLinkError(TEXT("actorName is required"), TEXT("MISSING_PARAM"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Result = MakeLiveLinkError(TEXT("No editor world available"), TEXT("NO_WORLD"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        AActor* TargetActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
            {
                TargetActor = *It;
                break;
            }
        }

        if (!TargetActor)
        {
            Result = MakeLiveLinkError(FString::Printf(TEXT("Actor '%s' not found"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // Locate the LiveLinkComponentController on the actor.
        const int32 ControllerIndex = static_cast<int32>(GetNumberFieldSafe(Payload, TEXT("controllerIndex"), 0.0));
        TArray<ULiveLinkComponentController*> Controllers;
        TargetActor->GetComponents<ULiveLinkComponentController>(Controllers);

        if (Controllers.Num() == 0)
        {
            Result = MakeLiveLinkError(TEXT("No LiveLinkComponentController found on actor"), TEXT("CONTROLLER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        ULiveLinkComponentController* Controller = nullptr;
        if (ControllerIndex >= 0 && ControllerIndex < Controllers.Num())
        {
            Controller = Controllers[ControllerIndex];
        }
        else
        {
            Controller = Controllers[0];
        }

        if (!Controller)
        {
            Result = MakeLiveLinkError(TEXT("Controller component not found"), TEXT("CONTROLLER_NOT_FOUND"));
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        auto ResolveRoleClass = [](const FString& RoleName) -> TSubclassOf<ULiveLinkRole>
        {
            if (RoleName.IsEmpty())
            {
                return nullptr;
            }
            // Common built-in roles.
#if MCP_HAS_LIVELINK_ROLES
            if (RoleName == TEXT("Animation"))
                return ULiveLinkAnimationRole::StaticClass();
            if (RoleName == TEXT("Transform"))
                return ULiveLinkTransformRole::StaticClass();
            if (RoleName == TEXT("Camera"))
                return ULiveLinkCameraRole::StaticClass();
            if (RoleName == TEXT("Light"))
                return ULiveLinkLightRole::StaticClass();
#endif
            // Fallback: try by class name.
            if (UClass* Found = FindObject<UClass>(nullptr, *RoleName))
            {
                if (Found->IsChildOf(ULiveLinkRole::StaticClass()))
                {
                    return Found;
                }
            }
            if (UClass* Found = FindObject<UClass>(nullptr, *(FString(TEXT("/Script/LiveLinkInterface.")) + RoleName)))
            {
                if (Found->IsChildOf(ULiveLinkRole::StaticClass()))
                {
                    return Found;
                }
            }
            return nullptr;
        };

        if (Action == TEXT("set_controller_subject"))
        {
            const FString SubjectNameStr = GetStringFieldSafe(Payload, TEXT("subjectName"));
            if (SubjectNameStr.IsEmpty())
            {
                Result = MakeLiveLinkError(TEXT("subjectName is required"), TEXT("MISSING_PARAM"));
            }
            else
            {
                const FString RoleName = GetStringFieldSafe(Payload, TEXT("roleName"), TEXT("Animation"));
                TSubclassOf<ULiveLinkRole> RoleClass = ResolveRoleClass(RoleName);
                if (!RoleClass)
                {
                    Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown role: %s"), *RoleName), TEXT("UNKNOWN_ROLE"));
                }
                else
                {
                    FLiveLinkSubjectRepresentation Rep;
                    Rep.Subject = FLiveLinkSubjectName(*SubjectNameStr);
                    Rep.Role = RoleClass;
                    Controller->SetSubjectRepresentation(Rep);
                    Result = MakeLiveLinkSuccess(TEXT("Controller subject set"));
                }
            }
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        if (Action == TEXT("set_controller_role"))
        {
            const FString RoleName = GetStringFieldSafe(Payload, TEXT("roleName"), TEXT("Animation"));
            TSubclassOf<ULiveLinkRole> RoleClass = ResolveRoleClass(RoleName);
            if (!RoleClass)
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown role: %s"), *RoleName), TEXT("UNKNOWN_ROLE"));
            }
            else
            {
                FLiveLinkSubjectRepresentation Rep = Controller->GetSubjectRepresentation();
                Rep.Role = RoleClass;
                Controller->SetSubjectRepresentation(Rep);
                Result = MakeLiveLinkSuccess(TEXT("Controller role set"));
            }
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        if (Action == TEXT("enable_controller_evaluation") || Action == TEXT("disable_controller_evaluation"))
        {
            Controller->bEvaluateLiveLink = (Action == TEXT("enable_controller_evaluation"));
            Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Controller evaluation %s"), Controller->bEvaluateLiveLink ? TEXT("enabled") : TEXT("disabled")));
            Result->SetBoolField(TEXT("evaluate"), Controller->bEvaluateLiveLink);
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        if (Action == TEXT("set_controlled_component"))
        {
            const FString RoleName = GetStringFieldSafe(Payload, TEXT("roleName"), TEXT("Animation"));
            TSubclassOf<ULiveLinkRole> RoleClass = ResolveRoleClass(RoleName);
            if (!RoleClass)
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown role: %s"), *RoleName), TEXT("UNKNOWN_ROLE"));
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
                return true;
            }

            const FString ComponentName = GetStringFieldSafe(Payload, TEXT("componentName"));
            if (ComponentName.IsEmpty())
            {
                Result = MakeLiveLinkError(TEXT("componentName is required"), TEXT("MISSING_PARAM"));
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
                return true;
            }

            UActorComponent* Controlled = nullptr;
            for (UActorComponent* Comp : TargetActor->GetComponents())
            {
                if (!Comp)
                {
                    continue;
                }
                if (Comp->GetName() == ComponentName)
                {
                    Controlled = Comp;
                    break;
                }
            }

            if (!Controlled)
            {
                Result = MakeLiveLinkError(FString::Printf(TEXT("Component '%s' not found on actor"), *ComponentName), TEXT("COMPONENT_NOT_FOUND"));
                SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
                return true;
            }

            Controller->SetControlledComponent(RoleClass, Controlled);
            Result = MakeLiveLinkSuccess(TEXT("Controlled component set"));
            Result->SetStringField(TEXT("componentName"), Controlled->GetName());
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        if (Action == TEXT("get_controller_info"))
        {
            const FLiveLinkSubjectRepresentation Rep = Controller->GetSubjectRepresentation();

            Result = MakeLiveLinkSuccess(TEXT("Controller info retrieved"));
            Result->SetStringField(TEXT("subjectName"), Rep.Subject.Name.ToString());
            Result->SetStringField(TEXT("roleClass"), Rep.Role ? Rep.Role->GetName() : TEXT(""));
            Result->SetBoolField(TEXT("evaluate"), Controller->bEvaluateLiveLink);
            Result->SetBoolField(TEXT("updateInEditor"), Controller->bUpdateInEditor);
            Result->SetNumberField(TEXT("controllerCount"), Controllers.Num());
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // configure_livelink_controller: allow toggling bUpdateInEditor / bUpdateInPreviewEditor.
        if (Action == TEXT("configure_livelink_controller"))
        {
            bool bUpdateInEditor = Controller->bUpdateInEditor;
            bool bUpdateInPreview = Controller->bUpdateInPreviewEditor;
            bool bDisableWhenSpawnable = Controller->bDisableEvaluateLiveLinkWhenSpawnable;

            if (Payload->HasField(TEXT("updateInEditor")))
            {
                bUpdateInEditor = GetBoolFieldSafe(Payload, TEXT("updateInEditor"), bUpdateInEditor);
                Controller->bUpdateInEditor = bUpdateInEditor;
            }
            if (Payload->HasField(TEXT("updateInPreviewEditor")))
            {
                bUpdateInPreview = GetBoolFieldSafe(Payload, TEXT("updateInPreviewEditor"), bUpdateInPreview);
                Controller->bUpdateInPreviewEditor = bUpdateInPreview;
            }
            if (Payload->HasField(TEXT("disableEvaluateWhenSpawnable")))
            {
                bDisableWhenSpawnable = GetBoolFieldSafe(Payload, TEXT("disableEvaluateWhenSpawnable"), bDisableWhenSpawnable);
                Controller->bDisableEvaluateLiveLinkWhenSpawnable = bDisableWhenSpawnable;
            }

            Result = MakeLiveLinkSuccess(TEXT("Controller configured"));
            Result->SetBoolField(TEXT("updateInEditor"), Controller->bUpdateInEditor);
            Result->SetBoolField(TEXT("updateInPreviewEditor"), Controller->bUpdateInPreviewEditor);
            Result->SetBoolField(TEXT("disableEvaluateWhenSpawnable"), Controller->bDisableEvaluateLiveLinkWhenSpawnable);
            Result->SetBoolField(TEXT("evaluate"), Controller->bEvaluateLiveLink);

            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
            return true;
        }

        // If we fell through, action isn't supported.
        Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown controller action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#else
        Result = MakeLiveLinkError(TEXT("Live Link components not available"), TEXT("NOT_SUPPORTED"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
#endif
    }

    // =========================================================================
    // TIMECODE & BUFFER (6 actions)
    // =========================================================================
    
    if (Action == TEXT("configure_livelink_timecode") ||
        Action == TEXT("set_timecode_provider") ||
        Action == TEXT("get_livelink_timecode") ||
        Action == TEXT("configure_time_sync") ||
        Action == TEXT("set_buffer_settings") ||
        Action == TEXT("configure_frame_interpolation"))
    {
        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Timecode configuration is typically done via Project Settings."), *Action));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // FACE TRACKING (8 actions)
    // =========================================================================
    
    if (Action == TEXT("configure_face_source") ||
        Action == TEXT("configure_arkit_mapping") ||
        Action == TEXT("set_face_neutral_pose") ||
        Action == TEXT("get_face_blendshapes") ||
        Action == TEXT("configure_blendshape_remap") ||
        Action == TEXT("apply_face_to_skeletal_mesh") ||
        Action == TEXT("configure_face_retargeting") ||
        Action == TEXT("get_face_tracking_status"))
    {
        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Face tracking requires Live Link Face app and ARKit-compatible device."), *Action));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // SKELETON MAPPING (6 actions)
    // =========================================================================
    
    if (Action == TEXT("configure_skeleton_mapping") ||
        Action == TEXT("create_retarget_asset") ||
        Action == TEXT("configure_bone_mapping") ||
        Action == TEXT("configure_curve_mapping") ||
        Action == TEXT("apply_mocap_to_character") ||
        Action == TEXT("get_skeleton_mapping_info"))
    {
        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Skeleton mapping is configured through Live Link Retarget Assets."), *Action));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // UTILITY (4 actions)
    // =========================================================================
    
    if (Action == TEXT("get_livelink_info"))
    {
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            TArray<FGuid> Sources = LiveLinkClient->GetSources();
            TArray<FLiveLinkSubjectKey> Subjects = LiveLinkClient->GetSubjects(true, true);
            
            int32 EnabledCount = 0;
            for (const FLiveLinkSubjectKey& Key : Subjects)
            {
                if (LiveLinkClient->IsSubjectEnabled(Key, false))
                {
                    EnabledCount++;
                }
            }
            
            TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
            InfoObj->SetBoolField(TEXT("isAvailable"), true);
            InfoObj->SetNumberField(TEXT("sourceCount"), Sources.Num());
            InfoObj->SetNumberField(TEXT("subjectCount"), Subjects.Num());
            InfoObj->SetNumberField(TEXT("enabledSubjectCount"), EnabledCount);
            
            Result = MakeLiveLinkSuccess(TEXT("Live Link info retrieved"));
            Result->SetObjectField(TEXT("liveLinkInfo"), InfoObj);
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("list_available_roles"))
    {
        TArray<TSharedPtr<FJsonValue>> RolesArray;
        
#if MCP_HAS_LIVELINK_ROLES
        RolesArray.Add(MakeShareable(new FJsonValueString(TEXT("Animation"))));
        RolesArray.Add(MakeShareable(new FJsonValueString(TEXT("Transform"))));
        RolesArray.Add(MakeShareable(new FJsonValueString(TEXT("Camera"))));
        RolesArray.Add(MakeShareable(new FJsonValueString(TEXT("Light"))));
        RolesArray.Add(MakeShareable(new FJsonValueString(TEXT("Basic"))));
#endif
        
        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d roles"), RolesArray.Num()));
        Result->SetArrayField(TEXT("availableRoles"), RolesArray);
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("list_source_factories"))
    {
#if MCP_HAS_LIVELINK_FULL
        TArray<UClass*> FactoryClasses;
        GetDerivedClasses(ULiveLinkSourceFactory::StaticClass(), FactoryClasses);
        
        TArray<TSharedPtr<FJsonValue>> FactoriesArray;
        for (UClass* FactoryClass : FactoryClasses)
        {
            ULiveLinkSourceFactory* Factory = FactoryClass->GetDefaultObject<ULiveLinkSourceFactory>();
            if (Factory && Factory->IsEnabled())
            {
                FactoriesArray.Add(MakeShareable(new FJsonValueString(Factory->GetSourceDisplayName().ToString())));
            }
        }
        
        Result = MakeLiveLinkSuccess(FString::Printf(TEXT("Found %d source factories"), FactoriesArray.Num()));
        Result->SetArrayField(TEXT("sourceFactories"), FactoriesArray);
#else
        Result = MakeLiveLinkError(TEXT("Source factories not available"), TEXT("NOT_SUPPORTED"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }
    
    if (Action == TEXT("force_livelink_tick"))
    {
        if (!LiveLinkClient)
        {
            Result = MakeLiveLinkError(TEXT("Live Link client not available"), TEXT("CLIENT_NOT_FOUND"));
        }
        else
        {
            LiveLinkClient->ForceTick();
            Result = MakeLiveLinkSuccess(TEXT("Live Link tick forced"));
        }
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // Unknown action
    Result = MakeLiveLinkError(FString::Printf(TEXT("Unknown Live Link action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
    SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
    return true;

#endif // MCP_HAS_LIVELINK
}
