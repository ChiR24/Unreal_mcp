#pragma once

#include "Domains/Render/McpAutomationBridge_RenderSupportEnums.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"

#include "Editor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

namespace McpRenderHandlers
{
inline UWorld* GetRenderWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->PlayWorld
        ? GEditor->PlayWorld.Get()
        : GEditor->GetEditorWorldContext().World();
}

inline AActor* FindRenderActor(const FString& Reference)
{
    if (Reference.IsEmpty())
    {
        return nullptr;
    }
    if (AActor* ByPath = FindObject<AActor>(nullptr, *Reference))
    {
        return ByPath;
    }
    UWorld* World = GetRenderWorld();
    return FindActorByNameInWorldForMcp(World, Reference, true);
}

// Writes FPostProcessSettings fields of the volume from JSON (reflection; override flags set).
inline bool ApplyPostProcessSettings(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& Settings,
    TArray<FString>& Applied, TArray<FString>& Unsupported, FString& Error)
{
    return ApplyJsonSettings(&Volume->Settings, FPostProcessSettings::StaticStruct(), Settings, true, Applied, Unsupported, Error);
}

inline bool ApplyPostProcessField(APostProcessVolume* Volume, const FString& Field, const TSharedPtr<FJsonValue>& Value,
    TArray<FString>& Applied, TArray<FString>& Unsupported, FString& Error)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetField(Field, Value);
    return ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error);
}

inline TSharedPtr<FJsonObject> MakeRenderResult(const FString& SubAction)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), TEXT("manage_render"));
    Result->SetStringField(TEXT("subAction"), SubAction);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("applied"), true);
    return Result;
}

// infiniteUnbound / blendWeight are declared on every configure_post_process
// variant, but only configure_pp_blend read them: "make this volume global"
// was silently dropped by exposure, bloom, vignette and the rest. Apply them
// wherever a volume is resolved for one of those calls.
inline void ApplyVolumeBlendFields(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& Payload,
                                   TArray<FString>& Applied)
{
    if (!Volume || !Payload.IsValid())
    {
        return;
    }
    if (Payload->HasTypedField<EJson::Boolean>(TEXT("infiniteUnbound")))
    {
        Volume->bUnbound = Payload->GetBoolField(TEXT("infiniteUnbound"));
        Applied.Add(TEXT("bUnbound"));
    }
    if (Payload->HasTypedField<EJson::Number>(TEXT("blendWeight")))
    {
        Volume->BlendWeight = Payload->GetNumberField(TEXT("blendWeight"));
        Applied.Add(TEXT("BlendWeight"));
    }
}

// configure_exposure declares method/minBrightness/maxBrightness/
// compensationValue but only ever applied the free-form `settings` bag, so a
// contract-following call answered "applied" with appliedSettings: [].
inline bool ApplyDeclaredExposureFields(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& Payload,
                                        TArray<FString>& Applied, TArray<FString>& Unsupported, FString& Error)
{
    TSharedPtr<FJsonObject> Fields = MakeShared<FJsonObject>();
    FString Method;
    if (Payload->TryGetStringField(TEXT("method"), Method))
    {
        FString Value;
        if (!ResolveEnumAlias(AutoExposureMethodMap(), Method, Value, Error))
        {
            return false;
        }
        Fields->SetStringField(TEXT("AutoExposureMethod"), Value);
    }
    static const TPair<const TCHAR*, const TCHAR*> Numbers[] = {
        {TEXT("minBrightness"), TEXT("AutoExposureMinBrightness")},
        {TEXT("maxBrightness"), TEXT("AutoExposureMaxBrightness")},
        {TEXT("compensationValue"), TEXT("AutoExposureBias")}};
    for (const TPair<const TCHAR*, const TCHAR*>& Field : Numbers)
    {
        if (Payload->HasTypedField<EJson::Number>(Field.Key))
        {
            Fields->SetNumberField(Field.Value, Payload->GetNumberField(Field.Key));
        }
    }
    return Fields->Values.Num() == 0 ||
        ApplyPostProcessSettings(Volume, Fields, Applied, Unsupported, Error);
}

inline APostProcessVolume* RequirePostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Reference;
    ReadActorReference(Payload, Reference);
    APostProcessVolume* Volume = Cast<APostProcessVolume>(FindRenderActor(Reference));
    if (!Volume && Reference.IsEmpty())
    {
        // No explicit reference: use the same deterministic resolver as the
        // lens/exposure handlers (single unbound volume, persistent-level
        // preference, spawn one when the level has none). Failing on an empty
        // name split the post-process family into two behaviours for the same
        // level: half AMBIGUOUS, half "PostProcessVolume not found".
        FString ResolveError;
        FString ResolveErrorCode;
        Volume = McpResolvePostProcessVolume(GetRenderWorld(), Payload, true, ResolveError, ResolveErrorCode);
        if (!Volume)
        {
            Subsystem->SendAutomationError(
                Socket, RequestId,
                ResolveError.IsEmpty() ? FString(TEXT("PostProcessVolume not found.")) : ResolveError,
                ResolveErrorCode.IsEmpty() ? FString(TEXT("ACTOR_NOT_FOUND")) : ResolveErrorCode);
        }
        TArray<FString> BlendApplied;
        ApplyVolumeBlendFields(Volume, Payload, BlendApplied);
        return Volume;
    }
    if (!Volume)
    {
        Subsystem->SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("PostProcessVolume not found: %s"), *Reference),
            TEXT("ACTOR_NOT_FOUND"));
    }
    TArray<FString> BlendApplied;
    ApplyVolumeBlendFields(Volume, Payload, BlendApplied);
    return Volume;
}

// Scene-capture actions declare an optional actorName; when it is omitted and
// the level holds exactly one scene capture actor, that actor is the target.
// More than one is reported as AMBIGUOUS with the candidate labels so the
// caller can pick; none is reported as ACTOR_NOT_FOUND.
inline AActor* FindSoleSceneCaptureActor(FString& OutError, FString& OutErrorCode)
{
    OutError.Reset();
    OutErrorCode.Reset();
    UWorld* World = GetRenderWorld();
    if (!World)
    {
        OutError = TEXT("No valid world available.");
        OutErrorCode = TEXT("NO_WORLD");
        return nullptr;
    }
    TArray<AActor*> Captures;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && (Actor->IsA<ASceneCapture2D>() || Actor->IsA<ASceneCaptureCube>()))
        {
            Captures.Add(Actor);
        }
    }
    if (Captures.Num() == 1)
    {
        return Captures[0];
    }
    if (Captures.Num() > 1)
    {
        TArray<FString> Labels;
        for (AActor* Actor : Captures)
        {
            Labels.Add(FString::Printf(TEXT("%s (%s)"), *Actor->GetActorLabel(), *Actor->GetName()));
        }
        OutError = FString::Printf(
            TEXT("Multiple scene capture actors found (%d); pass actorName to pick one. Candidates: %s"),
            Captures.Num(), *FString::Join(Labels, TEXT(", ")));
        OutErrorCode = TEXT("AMBIGUOUS");
        return nullptr;
    }
    OutError = TEXT("Scene capture actor not found: no SceneCapture2D/SceneCaptureCube in the level (create one with create_scene_capture_2d or pass actorName).");
    OutErrorCode = TEXT("ACTOR_NOT_FOUND");
    return nullptr;
}
}
