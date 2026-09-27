#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASCueNotify(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_cue_notify"))
    {
        const FString CueType = GetJsonStringField(Payload, TEXT("cueType"), TEXT("Static"));
        const FString CueTag = GetJsonStringField(Payload, TEXT("cueTag"));
        const bool bActorCue = NormalizeGASToken(CueType) == TEXT("actor");

        bool bReusedExisting = false;
        const TSharedPtr<FJsonObject> Result = CreateGASAsset(Context,
            bActorCue ? AGameplayCueNotify_Actor::StaticClass() : UGameplayCueNotify_Static::StaticClass(),
            bActorCue ? TEXT("GameplayCueNotify_Actor") : TEXT("GameplayCueNotify_Static"), bReusedExisting,
            [&CueTag](UBlueprint* Blueprint) {
                // A new notify gets the cue tag, when one was given.
                UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
                if (CueTag.IsEmpty() || !CDO)
                {
                    return;
                }
                const FGameplayTag Tag = GetOrRequestTag(CueTag);
                if (UGameplayCueNotify_Static* StaticCue = Cast<UGameplayCueNotify_Static>(CDO))
                {
                    StaticCue->GameplayCueTag = Tag;
                }
                else if (AGameplayCueNotify_Actor* ActorCue = Cast<AGameplayCueNotify_Actor>(CDO))
                {
                    ActorCue->GameplayCueTag = Tag;
                }
            });
        if (!Result)
        {
            return true;
        }
        Result->SetStringField(TEXT("cueType"), CueType);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Cue notify already exists") : TEXT("Cue notify created"), Result);
        return true;
    }

    // configure_cue_trigger - REAL IMPLEMENTATION adding trigger configuration
    return false;
}
}
