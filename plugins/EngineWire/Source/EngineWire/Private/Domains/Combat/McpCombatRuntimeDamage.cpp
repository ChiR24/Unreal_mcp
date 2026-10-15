// apply_damage: a hit on an actor in the running game, through UGameplayStatics::ApplyDamage as a projectile or a pain
// volume deals it, so the actor's own damage handling decides what happens (AnyDamage, the health behaviour). Testing
// health, death or a respawn needed a weapon or a pain volume placed in the level first.
#include "Domains/Combat/McpCombatPrivate.h"

#include "Editor.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace McpCombatHandlers
{
namespace
{
// The actor's Health number, as the health behaviour names it; false when it has none.
bool ReadHealth(const AActor* Actor, double& OutHealth)
{
    const FProperty* Property = IsValid(Actor) ? Actor->GetClass()->FindPropertyByName(TEXT("Health")) : nullptr;
    if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
    {
        OutHealth = Float->GetPropertyValue_InContainer(Actor);
        return true;
    }
    if (const FDoubleProperty* Double = CastField<FDoubleProperty>(Property))
    {
        OutHealth = Double->GetPropertyValue_InContainer(Actor);
        return true;
    }
    return false;
}

// An actor of the running game by name; null for one that exists only in the editor level.
AActor* FindRunningActor(const FString& Name)
{
    AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    return Actor && GEditor && Actor->GetWorld() == GEditor->PlayWorld ? Actor : nullptr;
}
}

bool FCombatActionContext::HandleRuntimeDamage() const
{
    if (SubAction != TEXT("apply_damage"))
    {
        return false;
    }
    if (!GEditor || !GEditor->PlayWorld)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("apply_damage hits an actor in the running game: start Play In Editor first (control_editor play)."),
            TEXT("PIE_NOT_RUNNING"));
        return true;
    }
    const FString TargetName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString CauserName = GetJsonStringField(Payload, TEXT("causerName"));
    const FString TypeName = GetJsonStringField(Payload, TEXT("damageType"));
    AActor* Target = FindRunningActor(TargetName);
    AActor* Causer = FindRunningActor(CauserName);
    UClass* DamageType = TypeName.IsEmpty() ? UDamageType::StaticClass() : ResolveClassByName(TypeName);
    const FString Refusal = !Target ? FString::Printf(TEXT("No actor '%s' in the running game."), *TargetName)
                            : !CauserName.IsEmpty() && !Causer
                                ? FString::Printf(TEXT("causerName: no actor '%s' in the running game."), *CauserName)
                            : !DamageType || !DamageType->IsChildOf(UDamageType::StaticClass())
                                ? FString::Printf(TEXT("damageType '%s' is not a DamageType class."), *TypeName)
                                : FString();
    if (!Refusal.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, Refusal, Target ? TEXT("INVALID_ARGUMENT") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    // The causer's controller instigates, as a weapon's owner does; a pawn or a controller names its own.
    AController* Instigator = Cast<AController>(Causer);
    if (!Instigator && Causer)
    {
        const APawn* CauserPawn = Cast<APawn>(Causer);
        Instigator = CauserPawn ? CauserPawn->GetController() : Causer->GetInstigatorController();
    }
    const FString Label = McpActorRef(Target);
    const double Damage = GetJsonNumberField(Payload, TEXT("damage"), 0.0);
    double Before = 0.0;
    const bool bHadHealth = ReadHealth(Target, Before);
    const float Applied = UGameplayStatics::ApplyDamage(Target, static_cast<float>(Damage), Instigator, Causer, DamageType);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), Label);
    Result->SetNumberField(TEXT("damageApplied"), Applied);
    const bool bDestroyed = !IsValid(Target) || Target->IsActorBeingDestroyed();
    Result->SetBoolField(TEXT("destroyed"), bDestroyed);
    double Health = 0.0;
    const bool bHealth = !bDestroyed && ReadHealth(Target, Health);
    if (bHealth)
    {
        Result->SetNumberField(TEXT("health"), Health);
    }
    // ApplyDamage returns the amount even when the actor's own handling ignores it (invulnerable for a moment after a
    // hit, already dead, a heal at full health), so an unchanged Health decides the sentence.
    const bool bIgnored = bHealth && bHadHealth && Health == Before && Damage != 0.0;
    const FString Message = Applied == 0.0f && Damage != 0.0
        ? FString::Printf(TEXT("%s took no damage: it cannot be damaged (Can be Damaged is off)."), *Label)
        : bDestroyed ? FString::Printf(TEXT("%s took %g damage and was destroyed."), *Label, Applied)
        : bIgnored ? FString::Printf(TEXT("%s ignored it: its Health stays %g (invulnerable for a moment after a hit, already dead, or a heal at full health)."), *Label, Health)
        : bHealth ? FString::Printf(TEXT("%s took %g damage; its Health is now %g."), *Label, Applied, Health)
                  : FString::Printf(TEXT("%s took %g damage."), *Label, Applied);
    SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
    return true;
}
}
