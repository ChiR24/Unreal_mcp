#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"

class AActor;
class UEditorActorSubsystem;
class UNiagaraSystem;
class UWorld;

namespace McpEffectHandlers
{
struct FEffectActionContext
{
    UEngineWireSubsystem& Bridge;
    const FString& RequestId;
    // The normalized sub-action, the lifecycle aliases mapped onto activate_niagara and deactivate_niagara.
    const FString& SubAction;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> Socket;

    void SendError(const FString& Message, const FString& ErrorCode) const
    {
        Bridge.SendAutomationError(Socket, RequestId, Message, ErrorCode);
    }
};

bool IsNiagaraAuthoringSubAction(const FString& SubAction);
bool IsNiagaraGraphSubAction(const FString& SubAction);

FColor ReadColorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FColor& DefaultValue = FColor::White);
FVector ReadScaleField(const TSharedPtr<FJsonObject>& Payload);

using McpHandlerUtils::GetEditorWorld;
UEditorActorSubsystem* GetEditorActorSubsystem();
// Both keep working while PIE runs; the actor lookup searches the play world then.
UObject* LoadEffectAsset(const FString& AssetPath);
AActor* FindActorByLabel(const FString& ActorName);

bool HandleEffectDiscoveryAction(const FEffectActionContext& Context);
bool HandleCreateEffectSubAction(const FEffectActionContext& Context);
bool HandleDrawDebugShape(const FEffectActionContext& Context);
bool HandleParticleEffect(const FEffectActionContext& Context);
FString ReadNiagaraSystemPathField(const TSharedPtr<FJsonObject>& Payload);
bool AuthorProceduralNiagaraSystem(
    const FEffectActionContext& Context,
    const FString& EffectName,
    FString& OutSystemPath,
    TSharedPtr<FJsonObject>& OutDetails,
    FString& OutError,
    FString& OutErrorCode);
bool HandleSetNiagaraParameter(const FEffectActionContext& Context);
bool HandleNiagaraLifecycleAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction);
bool HandleSpawnNiagara(const FEffectActionContext& Context);
bool HandleCreateDynamicLight(const FEffectActionContext& Context);
bool HandleCleanup(const FEffectActionContext& Context);
bool HandleProceduralEffectAction(const FEffectActionContext& Context);
bool CreateNiagaraEffectFromPayload(
    const FEffectActionContext& Context,
    const FString& EffectName,
    const FString& DefaultSystemPath,
    const TSharedPtr<FJsonObject>& ExtraFields = nullptr,
    const FRotator& Rotation = FRotator::ZeroRotator);
}
