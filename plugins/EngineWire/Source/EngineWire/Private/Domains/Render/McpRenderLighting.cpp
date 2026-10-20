#include "Domains/Render/McpRenderPrivate.h"
#include "Domains/Render/McpRenderLightingChannels.h"
#include "Domains/Render/McpRenderSupport.h"
#include "Domains/Render/McpRenderSupportSettings.h"

#include "Domains/Lighting/McpLightingPrivate.h"
#include "EngineWireSubsystem.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"

namespace McpRenderHandlers
{
namespace
{
bool SendMissingActor(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& Reference,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    Subsystem->SendAutomationError(
        Socket,
        RequestId,
        FString::Printf(TEXT("Actor not found: %s"), *Reference),
        TEXT("ACTOR_NOT_FOUND"));
    return true;
}
}

bool HandleRenderLightingAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("build_lighting_quality"))
    {
        return McpLightingHandlers::HandleBuildLighting(
            *Subsystem, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_lightmass_settings"))
    {
        UWorld* World = GetRenderWorld();
        AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
        if (!WorldSettings)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, TEXT("World settings are not available."), TEXT("WORLD_NOT_AVAILABLE"));
            return true;
        }
        const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
        if (!Settings.IsValid())
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, TEXT("settings is required."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        TArray<FString> Applied;
        TArray<FString> Unsupported;
        FString Error;
        // A Modify() with no transaction open recorded nothing, so undo restored nothing. And without a dirty
        // package the edit was in-memory only while the reply said "configured".
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Lightmass Settings")),
                                                EMcpMutationDurability::EditorStateOnly, {WorldSettings});
        if (!ApplyJsonSettings(
                &WorldSettings->LightmassSettings,
                FLightmassWorldInfoSettings::StaticStruct(),
                Settings,
                false,
                Applied,
                Unsupported,
                Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
        WorldSettings->MarkPackageDirty();
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        AddStringArray(Result, TEXT("appliedSettings"), Applied);
        AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
        Transaction.DescribeInto(Result);
        Subsystem->SendAutomationResponse(
            RequestingSocket, RequestId, true, TEXT("Lightmass settings configured."), Result);
        return true;
    }

    FString Reference;
    ReadActorReference(Payload, Reference);
    AActor* Actor = FindRenderActor(Reference);
    if (!Actor)
    {
        if (SubAction == TEXT("set_light_channel") ||
            SubAction == TEXT("set_actor_light_channel") ||
            SubAction == TEXT("configure_indirect_lighting_cache"))
        {
            return SendMissingActor(Subsystem, RequestId, Reference, RequestingSocket);
        }
        return false;
    }

    if (SubAction == TEXT("set_light_channel"))
    {
        TArray<int32> ChannelList;
        if (!ReadLightChannels(Subsystem, RequestId, Payload, RequestingSocket, ChannelList))
        {
            return true;
        }
        ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>();
        if (!Light)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, TEXT("Actor has no light component."), TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }
        // Set with no Modify, the change was not even unsaved: a save left it out and a close dropped it.
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Light Channel")), EMcpMutationDurability::EditorStateOnly,
                                                {Light});
        FLightingChannels Channels = Light->LightingChannels;
        SetLightChannels(Channels, ChannelList, GetJsonBoolField(Payload, TEXT("enabled"), true));
        Light->SetLightingChannels(Channels.bChannel0, Channels.bChannel1, Channels.bChannel2);
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetNumberField(TEXT("channel"), ChannelList[0]);
        AddNumberArray(Result, TEXT("channels"), ChannelList);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(
            RequestingSocket, RequestId, true, TEXT("Light channel configured."), Result);
        return true;
    }

    TArray<UPrimitiveComponent*> Components;
    Actor->GetComponents<UPrimitiveComponent>(Components);
    if (Components.Num() == 0)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, TEXT("Actor has no primitive components."), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    if (SubAction == TEXT("set_actor_light_channel"))
    {
        TArray<int32> ChannelList;
        if (!ReadLightChannels(Subsystem, RequestId, Payload, RequestingSocket, ChannelList))
        {
            return true;
        }
        TArray<UObject*> Targets;
        for (UPrimitiveComponent* Component : Components)
        {
            Targets.Add(Component);
        }
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Actor Light Channel")),
                                                EMcpMutationDurability::EditorStateOnly, Targets);
        for (UPrimitiveComponent* Component : Components)
        {
            FLightingChannels Channels = Component->LightingChannels;
            SetLightChannels(Channels, ChannelList, GetJsonBoolField(Payload, TEXT("enabled"), true));
            Component->SetLightingChannels(Channels.bChannel0, Channels.bChannel1, Channels.bChannel2);
        }
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetNumberField(TEXT("componentCount"), Components.Num());
        Result->SetNumberField(TEXT("channel"), ChannelList[0]);
        AddNumberArray(Result, TEXT("channels"), ChannelList);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(
            RequestingSocket, RequestId, true, TEXT("Actor light channel configured."), Result);
        return true;
    }

    if (SubAction == TEXT("configure_indirect_lighting_cache"))
    {
        const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
        TArray<FString> Applied;
        TArray<FString> Unsupported;
        FString Error;
        // enabled was declared and never read: it switches each primitive's cache
        // between point sampling and off.
        const bool bHasEnabled = Payload->HasTypedField<EJson::Boolean>(TEXT("enabled"));
        if (!bHasEnabled && !Settings.IsValid())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Pass enabled, or settings naming primitive component properties."), TEXT("NO_SETTING_SUPPLIED"));
            return true;
        }
        // The components were never in a transaction, so undo left the cache as set.
        TArray<UObject*> Targets;
        for (UPrimitiveComponent* Component : Components)
        {
            Targets.Add(Component);
        }
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Indirect Lighting Cache")),
                                                EMcpMutationDurability::EditorStateOnly, Targets);
        for (UPrimitiveComponent* Component : Components)
        {
            if (bHasEnabled)
            {
                Component->IndirectLightingCacheQuality = Payload->GetBoolField(TEXT("enabled")) ? ILCQ_Point : ILCQ_Off;
                Applied.AddUnique(TEXT("IndirectLightingCacheQuality"));
            }
            if (!ApplyJsonSettings(
                    Component,
                    Component->GetClass(),
                    Settings,
                    false,
                    Applied,
                    Unsupported,
                    Error))
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
                return true;
            }
            Component->MarkRenderStateDirty();
        }
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetNumberField(TEXT("componentCount"), Components.Num());
        AddStringArray(Result, TEXT("appliedSettings"), Applied);
        AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
        Transaction.DescribeInto(Result);
        McpHandlerUtils::AddVerification(Result, Actor);
        Subsystem->SendAutomationResponse(
            RequestingSocket, RequestId, true, TEXT("Indirect lighting cache configured."), Result);
        return true;
    }

    return false;
}
}
