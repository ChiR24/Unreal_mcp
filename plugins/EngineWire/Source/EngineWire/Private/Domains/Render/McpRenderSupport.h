#pragma once

#include "Domains/Render/McpRenderSupportEnums.h"
#include "Domains/Render/McpRenderSupportSettings.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"
#include "Foundation/McpScopedEditorTransaction.h"

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
// The render world actor a reference names by label, name or path; a path-restricted caller finds only those in a
// level inside its prefixes.
inline AActor* FindRenderActor(const FString& Reference)
{
    return FindActorByNameInWorldForMcp(McpHandlerUtils::GetPlayOrEditorWorld(), Reference, true);
}

// Writes FPostProcessSettings fields from JSON (reflection; override flags set). Handlers write a copy of the
// volume's settings and put it on the volume once every value is taken (CommitPostProcessVolume), so a refusal
// halfway no longer left the volume half changed.
inline bool ApplyPostProcessSettings(FPostProcessSettings& Target, const TSharedPtr<FJsonObject>& Settings,
    TArray<FString>& Applied, TArray<FString>& Unsupported, FString& Error)
{
    return ApplyJsonSettings(&Target, FPostProcessSettings::StaticStruct(), Settings, true, Applied, Unsupported, Error);
}

inline bool ApplyPostProcessField(FPostProcessSettings& Target, const FString& Field, const TSharedPtr<FJsonValue>& Value,
    TArray<FString>& Applied, TArray<FString>& Unsupported, FString& Error)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetField(Field, Value);
    return ApplyPostProcessSettings(Target, Settings, Applied, Unsupported, Error);
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

// Puts the staged settings on the volume, with the volume's own fields the call gave (blendWeight,
// infiniteUnbound, and enabled when bTakeEnabled), in one undo step once every value has been taken: Modify with no
// transaction open recorded nothing, so undo could not take a post-process change back.
inline void CommitPostProcessVolume(APostProcessVolume* Volume, const FPostProcessSettings& Staged,
                                    const TSharedPtr<FJsonObject>& Payload, bool bTakeEnabled, TArray<FString>& Applied)
{
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Post Process")),
                                                EMcpMutationDurability::EditorStateOnly, {Volume});
        Volume->Settings = Staged;
        if (bTakeEnabled && Payload->HasTypedField<EJson::Boolean>(TEXT("enabled")))
        {
            Volume->bEnabled = Payload->GetBoolField(TEXT("enabled"));
            Applied.Add(TEXT("bEnabled"));
        }
        ApplyVolumeBlendFields(Volume, Payload, Applied);
    }
    Volume->MarkComponentsRenderStateDirty();
}

// configure_exposure declares method/minBrightness/maxBrightness/
// compensationValue but only ever applied the free-form `settings` bag, so a
// contract-following call answered "applied" with appliedSettings: [].
inline bool ApplyDeclaredExposureFields(FPostProcessSettings& Target, const TSharedPtr<FJsonObject>& Payload,
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
        ApplyPostProcessSettings(Target, Fields, Applied, Unsupported, Error);
}

// create_* of a reflection or scene capture actor. The name is actorName, else name (the schema's spelling); an actor
// of another class under it is refused, one of TActor answered as already there, else a new one is spawned at
// location/rotation and returned for the caller to configure and answer. Null after a reply. Noun names the kind in
// those replies ("Reflection actor", "Scene capture actor").
template <typename TActor>
TActor* SpawnNamedRenderActor(UEngineWireSubsystem* Subsystem, const FString& RequestId, const FString& SubAction,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Noun)
{
    const FString Name = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("name")});
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("name (or actorName) is required."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }
    if (AActor* Existing = FindRenderActor(Name))
    {
        // An actor of another class under the name answered "already exists" as if it were the actor asked for.
        if (!Existing->IsA<TActor>())
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("An actor named %s already exists and is a %s, not a %s; pick another name. Nothing was created."),
                    *Name, *Existing->GetClass()->GetName(), *TActor::StaticClass()->GetName()),
                TEXT("NAME_TAKEN"));
            return nullptr;
        }
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        McpHandlerUtils::AddVerification(Result, Existing);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Noun + TEXT(" already exists."), Result);
        return nullptr;
    }
    TActor* Actor = SpawnActorInActiveWorld<TActor>(TActor::StaticClass(),
        ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector),
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator), Name);
    if (!Actor)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to spawn %s."), *Noun.ToLower()), TEXT("SPAWN_FAILED"));
    }
    return Actor;
}

// The volume actorName names, else the level's one unbound volume, else a new unbound one under the asked name (a
// level without one gets one); null after replying with the resolver's refusal, which always names its reason.
inline APostProcessVolume* RequirePostProcessVolume(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ResolveError;
    FString ResolveErrorCode;
    APostProcessVolume* Volume = McpResolvePostProcessVolume(McpHandlerUtils::GetPlayOrEditorWorld(), Payload, true, ResolveError, ResolveErrorCode);
    if (!Volume)
    {
        Subsystem->SendAutomationError(Socket, RequestId, ResolveError, ResolveErrorCode);
    }
    // Callers apply the blend fields through CommitPostProcessVolume, inside its undo step and into their reply.
    return Volume;
}

// For a request that names no actor: the level's one actor IsWanted accepts and the request may touch. Several answer
// AMBIGUOUS listing each as 'Label (Name)', none ACTOR_NOT_FOUND with NotFound; Kind names them ("scene capture").
template <typename TWanted>
AActor* FindSoleRenderActor(TWanted IsWanted, const TCHAR* Kind, const TCHAR* NotFound, FString& OutError, FString& OutErrorCode)
{
    OutError.Reset();
    OutErrorCode.Reset();
    UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld();
    if (!World)
    {
        OutError = TEXT("No valid world available.");
        OutErrorCode = TEXT("NO_WORLD");
        return nullptr;
    }
    TArray<AActor*> Found;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        // The request named no actor: a path-restricted caller chooses among those in a level inside its prefixes.
        if (*It && IsWanted(*It) && McpCapabilityPrincipal::ActiveRequestPermitsObject(*It))
        {
            Found.Add(*It);
        }
    }
    if (Found.Num() == 1)
    {
        return Found[0];
    }
    if (Found.Num() > 1)
    {
        TArray<FString> Labels;
        for (AActor* Actor : Found)
        {
            Labels.Add(FString::Printf(TEXT("%s (%s)"), *Actor->GetActorLabel(), *Actor->GetName()));
        }
        OutError = FString::Printf(TEXT("Multiple %s actors found (%d); pass actorName to pick one. Candidates: %s"),
                                   Kind, Found.Num(), *FString::Join(Labels, TEXT(", ")));
        OutErrorCode = TEXT("AMBIGUOUS");
        return nullptr;
    }
    OutError = NotFound;
    OutErrorCode = TEXT("ACTOR_NOT_FOUND");
    return nullptr;
}
}
