// explain_audible: why an actor's sound is not heard. Its audio components, a sound on them, whether one plays (during
// Play) or starts by itself, its volume, and the listener's distance against the attenuation range: the listener is the
// player's (often the camera, far from the pawn in a side view) during Play, the editor viewport otherwise.
#include "Domains/Debug/Explain/McpExplain.h"

#include "Components/AudioComponent.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace McpExplain
{
namespace
{
bool ListenerAt(UWorld* World, FVector& OutLocation)
{
    if (World && World->IsPlayInEditor())
    {
        if (APlayerController* Player = World->GetFirstPlayerController())
        {
            FVector Front, Right;
            Player->GetAudioListenerPosition(OutLocation, Front, Right);
            return true;
        }
        return false;
    }
    if (FEditorViewportClient* Client = GetActiveEditorViewportClientForMcp())
    {
        OutLocation = Client->GetViewLocation();
        return true;
    }
    return false;
}

void CheckSound(FTree& Tree, const UAudioComponent* Audio, const bool bPlay, const bool bHasListener, const FVector& Listener)
{
    if (!Audio->Sound)
    {
        Tree.Fail(TEXT("sound"), TEXT("NO_SOUND"), FString::Printf(TEXT("%s has no sound assigned."), *Audio->GetName()));
        return;
    }
    Tree.Pass(TEXT("sound"), FString::Printf(TEXT("%s plays '%s'."), *Audio->GetName(), *Audio->Sound->GetName()));
    if (bPlay && !Audio->IsPlaying())
    {
        Tree.Fail(TEXT("playing"), TEXT("NOT_PLAYING"), Audio->bAutoActivate
            ? FString(TEXT("It is not playing now: it started by itself and has finished or been stopped."))
            : FString(TEXT("It is not playing: it does not start by itself (Auto Activate off), so something must call Play.")));
    }
    else if (!bPlay && !Audio->bAutoActivate)
    {
        Tree.Fail(TEXT("playing"), TEXT("NOT_AUTO_ACTIVATED"), TEXT("It does not start by itself (Auto Activate off): only a Play call starts it."));
    }
    if (Audio->VolumeMultiplier <= 0.001f)
    {
        Tree.Fail(TEXT("volume"), TEXT("MUTED"), FString::Printf(TEXT("Its volume multiplier is %.3f."), Audio->VolumeMultiplier));
    }
    const FSoundAttenuationSettings* Attenuation = Audio->GetAttenuationSettingsToApply();
    if (!bHasListener)
    {
        Tree.Unknown(TEXT("range"), TEXT("There is no listener to measure from (no player during Play, no viewport otherwise)."));
    }
    else if (Attenuation && Attenuation->bAttenuate)
    {
        const double Reach = Attenuation->AttenuationShapeExtents.X + Attenuation->FalloffDistance;
        const double Distance = FVector::Dist(Listener, Audio->GetComponentLocation());
        if (Distance > Reach)
        {
            Tree.Fail(TEXT("range"), TEXT("OUT_OF_RANGE"),
                      FString::Printf(TEXT("The listener is %.0f units away, past its attenuation range of about %.0f (inner radius plus falloff)."), Distance, Reach));
        }
        else
        {
            Tree.Pass(TEXT("range"), FString::Printf(TEXT("The listener is %.0f units away, inside its attenuation range of about %.0f."), Distance, Reach));
        }
    }
    else
    {
        Tree.Pass(TEXT("range"), TEXT("It does not attenuate with distance."));
    }
}
} // namespace

bool ExplainAudible(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    if (!Actor)
    {
        Self.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor named '%s' in the level."), *Name), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    TArray<UAudioComponent*> Audio;
    Actor->GetComponents(Audio);
    FTree Tree;
    const bool bPlay = Actor->GetWorld() && Actor->GetWorld()->IsPlayInEditor();
    FVector Listener = FVector::ZeroVector;
    const bool bHasListener = ListenerAt(Actor->GetWorld(), Listener);
    if (Audio.Num() == 0)
    {
        Tree.Fail(TEXT("components"), TEXT("NO_AUDIO_COMPONENT"), FString::Printf(TEXT("'%s' has no audio component, so it plays nothing."), *McpActorRef(Actor)));
    }
    else
    {
        Tree.Pass(TEXT("components"), FString::Printf(TEXT("%d audio component(s), first %s."), Audio.Num(), *Audio[0]->GetName()));
        CheckSound(Tree, Audio[0], bPlay, bHasListener, Listener);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Result->SetStringField(TEXT("listener"), bPlay ? TEXT("player") : TEXT("editor viewport"));
    Self.SendAutomationResponse(Socket, RequestId, true, Tree.Summary(FString::Printf(TEXT("Why '%s' is not heard"), *McpActorRef(Actor))), Result);
    return true;
}
} // namespace McpExplain
