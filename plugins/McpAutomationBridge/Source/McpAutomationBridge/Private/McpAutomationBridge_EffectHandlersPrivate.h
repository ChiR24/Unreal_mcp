#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
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
UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
bool EnsureNiagaraModuleSystem(
    const FEffectActionContext& Context,
    const FString& ModuleName,
    const FString& SystemPath,
    const FString& EmitterName);
#endif

void SendNiagaraModuleResponse(
    const FEffectActionContext& Context,
    bool bSuccess,
    const FString& ModuleName,
    const FString& SystemPath,
    const FString& EmitterName,
    const FString& Message,
    const FString& ErrorCode = FString());

bool HandleEffectDiscoveryAction(const FEffectActionContext& Context);
bool HandleCreateEffectSubAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction);
bool HandleDrawDebugShape(const FEffectActionContext& Context);
bool HandleParticleDebugShape(const FEffectActionContext& Context);
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
    const FString& DefaultSystemPath);
bool HandleNiagaraSpawnModules(const FEffectActionContext& Context);
bool HandleNiagaraBehaviorModules(const FEffectActionContext& Context);
bool HandleNiagaraRenderModules(const FEffectActionContext& Context);
bool HandleNiagaraDataEventModules(const FEffectActionContext& Context);
bool HandleNiagaraParameterModules(const FEffectActionContext& Context);
}
