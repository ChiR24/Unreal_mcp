#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace McpEnvironmentHandlers {

namespace {
int32 McpCountLevelActors(const ULevel *Level)
{
    int32 Count = 0;
    if (Level)
    {
        for (const AActor *Actor : Level->Actors)
        {
            if (Actor)
            {
                ++Count;
            }
        }
    }
    return Count;
}
} // namespace

// Level/world summary shared by get_level_details (dedicated inspect action)
// and get_world_settings (the TypeScript alias target of get_level_details),
// so both surfaces expose the same level fields.
void McpAppendLevelDetails(UWorld *World, TSharedPtr<FJsonObject> Resp)
{
    if (!World || !Resp.IsValid())
    {
        return;
    }
    Resp->SetStringField(TEXT("worldName"), World->GetName());
    Resp->SetStringField(TEXT("worldType"), McpGetWorldTypeName(World));
    Resp->SetStringField(TEXT("levelPath"), World->GetPathName());
    Resp->SetStringField(TEXT("packageName"), World->GetOutermost()->GetName());
    Resp->SetBoolField(TEXT("isPlayInEditor"), World->IsPlayInEditor());

    ULevel *Persistent = World->PersistentLevel;
    Resp->SetStringField(TEXT("persistentLevel"), Persistent ? Persistent->GetName() : TEXT(""));
    Resp->SetNumberField(TEXT("persistentLevelActorCount"), McpCountLevelActors(Persistent));
    int32 TotalActors = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        ++TotalActors;
    }
    Resp->SetNumberField(TEXT("actorCount"), TotalActors);

    TArray<TSharedPtr<FJsonValue>> StreamingLevels;
    int32 LoadedCount = 0;
    for (ULevelStreaming *Streaming : World->GetStreamingLevels())
    {
        if (!Streaming)
        {
            continue;
        }
        TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
        Entry->SetStringField(TEXT("name"), Streaming->GetName());
        Entry->SetStringField(TEXT("packageName"), Streaming->GetWorldAssetPackageName());
        Entry->SetBoolField(TEXT("isLoaded"), Streaming->IsLevelLoaded());
        Entry->SetBoolField(TEXT("isVisible"), Streaming->IsLevelVisible());
        Entry->SetBoolField(TEXT("shouldBeLoaded"), Streaming->ShouldBeLoaded());
        Entry->SetBoolField(TEXT("shouldBeVisible"), Streaming->ShouldBeVisible());
        Entry->SetStringField(TEXT("class"), Streaming->GetClass()->GetName());
        if (const ULevel *Loaded = Streaming->GetLoadedLevel())
        {
            ++LoadedCount;
            Entry->SetNumberField(TEXT("actorCount"), McpCountLevelActors(Loaded));
        }
        StreamingLevels.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Resp->SetArrayField(TEXT("streamingLevels"), StreamingLevels);
    Resp->SetNumberField(TEXT("streamingLevelCount"), StreamingLevels.Num());
    Resp->SetNumberField(TEXT("loadedStreamingLevelCount"), LoadedCount);

    const bool bWorldPartition = World->GetWorldPartition() != nullptr;
    TSharedPtr<FJsonObject> Settings = McpHandlerUtils::CreateResultObject();
    Settings->SetBoolField(TEXT("worldPartitionEnabled"), bWorldPartition);
    if (AWorldSettings *WorldSettings = World->GetWorldSettings())
    {
        UClass *GameMode = WorldSettings->DefaultGameMode.Get();
        Settings->SetStringField(TEXT("class"), WorldSettings->GetClass()->GetPathName());
        Settings->SetStringField(TEXT("gameModeOverride"), GameMode ? GameMode->GetPathName() : TEXT(""));
        Settings->SetNumberField(TEXT("killZ"), WorldSettings->KillZ);
        Settings->SetNumberField(TEXT("worldGravityZ"), WorldSettings->GetGravityZ());
        Settings->SetNumberField(TEXT("timeDilation"), WorldSettings->TimeDilation);
        Settings->SetBoolField(TEXT("enableWorldBoundsChecks"), WorldSettings->bEnableWorldBoundsChecks);
        Settings->SetBoolField(TEXT("forceNoPrecomputedLighting"), WorldSettings->bForceNoPrecomputedLighting);
    }
    Resp->SetObjectField(TEXT("worldSettings"), Settings);
    Resp->SetBoolField(TEXT("worldPartitionEnabled"), bWorldPartition);
    // NumLightingUnbuiltObjects is the counter behind the "LIGHTING NEEDS TO BE
    // REBUILT" viewport warning; zero means the static lighting build is current.
    const int32 UnbuiltLighting = static_cast<int32>(World->NumLightingUnbuiltObjects);
    Resp->SetNumberField(TEXT("unbuiltLightingObjects"), UnbuiltLighting);
    Resp->SetBoolField(TEXT("lightingBuilt"), UnbuiltLighting == 0);
}

bool HandleInspectLevelDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UWorld *World = McpGetRuntimeInspectionWorld();
    if (!World)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                                   TEXT("No world available"),
                                   TEXT("WORLD_NOT_FOUND"));
        return true;
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    McpAppendLevelDetails(World, Resp);
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Level details retrieved"), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
