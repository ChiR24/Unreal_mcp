#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/Performance/McpPerfPrivate.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "ScopedTransaction.h"
#include "WorldPartition/WorldPartition.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Scalability.h"

namespace McpPerformanceHandlers
{
namespace
{
// World Settings > World Partition > Enable Streaming, read through its property, which every 5.x version has.
bool IsWorldPartitionStreaming(UWorldPartition* WorldPartition)
{
    const FBoolProperty* Prop = FindFProperty<FBoolProperty>(UWorldPartition::StaticClass(), TEXT("bEnableStreaming"));
    return Prop && Prop->GetPropertyValue_InContainer(WorldPartition);
}

void SetWorldPartitionStreaming(UWorldPartition* WorldPartition, bool bEnabled)
{
    const FScopedTransaction Transaction(NSLOCTEXT("EngineWire", "SetWorldPartitionStreaming", "Set World Partition Enable Streaming"));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    WorldPartition->SetEnableStreaming(bEnabled);
#else
    // 5.0 applies the change in PostEditChangeProperty, behind a confirmation an automated call answers No.
    if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(UWorldPartition::StaticClass(), TEXT("bEnableStreaming")))
    {
        WorldPartition->Modify();
        Prop->SetPropertyValue_InContainer(WorldPartition, bEnabled);
        FPropertyChangedEvent Event(Prop);
        static_cast<UObject*>(WorldPartition)->PostEditChangeProperty(Event);
    }
#endif
}
}

bool HandleAdvancedOptimizationAction(const FPerformanceActionContext& Context)
{
    TArray<FString> Applied, NotSet;
    if (Context.Lower == TEXT("apply_baseline_settings"))
    {
        FString Profile = TEXT("balanced");
        Context.Payload->TryGetStringField(TEXT("profile"), Profile);

        // The engine's own levels: performance = Low, balanced = High, quality = Epic. Any other name used to
        // apply balanced and report the misspelled profile as applied.
        const int32 QualityLevel = Profile.Equals(TEXT("performance"), ESearchCase::IgnoreCase) ? 0
            : Profile.Equals(TEXT("balanced"), ESearchCase::IgnoreCase) ? 2
            : Profile.Equals(TEXT("quality"), ESearchCase::IgnoreCase) ? 3 : INDEX_NONE;
        if (QualityLevel == INDEX_NONE)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("profile "
                "must be performance, balanced or quality, got '%s'. Nothing was changed."), *Profile), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Scalability::FQualityLevels Levels;
        Levels.SetFromSingleQualityLevel(QualityLevel);
        Scalability::SetQualityLevels(Levels);
        // Saved where the editor loads it at startup, as set_scalability and the editor's own Scalability menu do.
        Scalability::SaveState(GEditorSettingsIni);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("profile"), Profile.ToLower());
        Resp->SetNumberField(TEXT("qualityLevel"), QualityLevel);
        Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, FString::Printf(
            TEXT("Baseline %s applied: every scalability group at level %d"), *Profile.ToLower(), QualityLevel), Resp);
        return true;
    }

    if (Context.Lower == TEXT("optimize_draw_calls"))
    {
        bool bEnabled = true;
        if (!Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled))
        {
            Context.Payload->TryGetBoolField(TEXT("enableBatching"), bEnabled);
        }
        bool bInstancing = true;
        Context.Payload->TryGetBoolField(TEXT("enableInstancing"), bInstancing);
        SetPerfCVar(TEXT("r.MeshDrawCommands.DynamicInstancing"), bInstancing ? TEXT("1") : TEXT("0"), Applied, NotSet);
        SetPerfCVar(TEXT("r.MeshDrawCommands.UseCachedCommands"), bEnabled ? TEXT("1") : TEXT("0"), Applied, NotSet);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("optimized"), bEnabled);
        Resp->SetBoolField(TEXT("instancing"), bInstancing);
        SendPerfCVarResult(Context, TEXT("Draw call settings set"), Applied, NotSet, Resp);
        return true;
    }

    if (Context.Lower == TEXT("configure_occlusion_culling"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        // slop and minScreenRadius wrote r.OcclusionSlop and r.OcclusionCullMinScreenRadius, which no 5.x engine has
        // (the slop is a renderer constant), and answered success.
        SetPerfCVar(TEXT("r.AllowOcclusionQueries"), bEnabled ? TEXT("1") : TEXT("0"), Applied, NotSet);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("enabled"), bEnabled);
        SendPerfCVarResult(Context, bEnabled ? TEXT("Occlusion culling on") : TEXT("Occlusion culling off"), Applied, NotSet, Resp);
        return true;
    }

    if (Context.Lower == TEXT("optimize_shaders"))
    {
        FString Mode = TEXT("changed");
        Context.Payload->TryGetStringField(TEXT("mode"), Mode);
        bool bForceRecompile = false;
        Context.Payload->TryGetBoolField(TEXT("forceRecompile"), bForceRecompile);
        // "recompileshaders material" needs a material name and recompiled nothing without one, and any other mode
        // ran changed under the name it was given.
        if (!bForceRecompile && !Mode.Equals(TEXT("changed"), ESearchCase::IgnoreCase) && !Mode.Equals(TEXT("global"), ESearchCase::IgnoreCase))
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("mode must "
                "be changed or global, got '%s'; forceRecompile true recompiles every shader. One material recompiles "
                "with manage_asset compile_material. Nothing was run."), *Mode), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const FString Command = bForceRecompile ? FString(TEXT("recompileshaders all")) : TEXT("recompileshaders ") + Mode.ToLower();
        if (!GEditor)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
            return true;
        }
        if (!GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Command))
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                FString::Printf(TEXT("The engine did not run '%s'"), *Command), TEXT("EXEC_FAILED"));
            return true;
        }
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("mode"), bForceRecompile ? FString(TEXT("all")) : Mode.ToLower());
        Resp->SetBoolField(TEXT("forceRecompile"), bForceRecompile);
        Resp->SetStringField(TEXT("command"), Command);
        Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Ran %s"), *Command), Resp);
        return true;
    }

    if (Context.Lower != TEXT("configure_world_partition"))
    {
        return false;
    }
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UWorldPartition* WorldPartition = EditorWorld ? EditorWorld->GetWorldPartition() : nullptr;
    if (!WorldPartition)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("The loaded level has no World Partition; enable it first (manage_level_structure enable_world_partition) before configuring streaming"),
            TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }
    bool bEnabled = true;
    double CellSize = 0.0;
    double LoadingRange = 0.0;
    const bool bHasEnabled = Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    const bool bHasCellSize = Context.Payload->TryGetNumberField(TEXT("cellSize"), CellSize);
    const bool bHasRange = Context.Payload->TryGetNumberField(TEXT("streamingDistance"), LoadingRange);
    if (!bHasEnabled && !bHasCellSize && !bHasRange)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("Pass enabled, cellSize and/or streamingDistance. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // These wrote wp.Runtime.EnableStreaming, RuntimeCellSize and RuntimeStreamingRange, none of which any 5.x engine
    // has, and answered success. Streaming is the World Partition's Enable Streaming; cell size and range live on
    // its runtime grid.
    if (bHasEnabled && IsWorldPartitionStreaming(WorldPartition) != bEnabled)
    {
        SetWorldPartitionStreaming(WorldPartition, bEnabled);
        if (IsWorldPartitionStreaming(WorldPartition) != bEnabled)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("World "
                "Partition streaming stays %s: the engine kept it (UE 5.0 confirms this change in a dialog an automated call "
                "answers No); switch Enable Streaming in World Settings by hand. Nothing was changed."), bEnabled ? TEXT("off") : TEXT("on")),
                TEXT("STREAMING_NOT_CHANGED"));
            return true;
        }
    }
    if (bHasCellSize || bHasRange)
    {
        TSharedPtr<FJsonObject> GridPayload = MakeShared<FJsonObject>();
        if (bHasCellSize)
        {
            GridPayload->SetNumberField(TEXT("gridCellSize"), CellSize);
        }
        if (bHasRange)
        {
            GridPayload->SetNumberField(TEXT("loadingRange"), LoadingRange);
        }
        GridPayload->SetBoolField(TEXT("createIfMissing"), false);
        return McpLevelStructure::HandleConfigureGridSize(&Context.Bridge, Context.RequestId, GridPayload, Context.RequestingSocket);
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("streamingEnabled"), bEnabled);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, FString::Printf(TEXT("World "
        "Partition streaming %s; the level is unsaved"), bEnabled ? TEXT("on") : TEXT("off")), Resp);
    return true;
}
}
