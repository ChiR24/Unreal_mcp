#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

class AActor;
class UEditorActorSubsystem;
class UNiagaraSystem;
class UWorld;

namespace McpEffectHandlers
{
struct FEffectActionContext
{
    UMcpAutomationBridgeSubsystem& Bridge;
    const FString& RequestId;
    const FString& Action;
    const FString& Lower;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
};

FString NormalizeNativeSubAction(
    const FString& Lower,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload);
bool IsNiagaraAuthoringSubAction(const FString& SubAction);
bool IsNiagaraGraphSubAction(const FString& SubAction);
FString ResolveCreateEffectSubAction(
    const FString& Action,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload);

FVector ReadVectorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FVector& DefaultValue = FVector::ZeroVector);
FRotator ReadRotatorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FRotator& DefaultValue = FRotator::ZeroRotator);
FColor ReadColorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FColor& DefaultValue = FColor::White);
FVector ReadScaleField(const TSharedPtr<FJsonObject>& Payload);

#if WITH_EDITOR
UWorld* GetEditorWorld();
UEditorActorSubsystem* GetEditorActorSubsystem();
AActor* FindActorByLabel(UEditorActorSubsystem& ActorSubsystem, const FString& ActorName);
#endif

bool HandleEffectDiscoveryAction(const FEffectActionContext& Context);
bool HandleCreateEffectSubAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction);
bool HandleDrawDebugShape(const FEffectActionContext& Context);
bool HandleParticleEffect(const FEffectActionContext& Context);
FString ReadNiagaraSystemPathField(const TSharedPtr<FJsonObject>& Payload);
#if WITH_EDITOR
bool AuthorProceduralNiagaraSystem(
    const FEffectActionContext& Context,
    const FString& EffectName,
    FString& OutSystemPath,
    TSharedPtr<FJsonObject>& OutDetails,
    FString& OutError,
    FString& OutErrorCode);
#endif
bool HandleSetNiagaraParameter(const FEffectActionContext& Context);
bool HandleNiagaraLifecycleAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction);
bool HandleSpawnNiagara(const FEffectActionContext& Context, bool bIsCreateEffect);
bool HandleCreateDynamicLight(const FEffectActionContext& Context);
bool HandleCleanup(const FEffectActionContext& Context, bool bIsCreateEffect);
bool HandleProceduralEffectAction(const FEffectActionContext& Context, bool bIsCreateEffect);
bool CreateNiagaraEffectFromPayload(
    const FEffectActionContext& Context,
    const FString& EffectName,
    const FString& DefaultSystemPath,
    const TSharedPtr<FJsonObject>& ExtraFields = nullptr);
}
