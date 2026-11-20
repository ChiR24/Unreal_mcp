// manage_gas get_live_gas_state: an actor's ability system in Play, in one read: its tags, every attribute of every
// attribute set it holds, and its active effects (class, stacks, time left). saveAs keeps the state under a name for
// this editor session; compareWith lists what changed since one: tags gained and lost, attributes before and after.

#include "Domains/GAS/McpGASActionRouting.h"
#include "Domains/GAS/McpGASRequestContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AttributeSet.h"
#include "Editor.h"
#include "GameplayEffect.h"

namespace McpGASLiveDetail
{
// ponytail: states kept by name for the editor session, never evicted; a caller keeps a few.
TMap<FString, TSharedPtr<FJsonObject>> GSaved;

TSharedPtr<FJsonObject> Attributes(const UAbilitySystemComponent& Asc)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    for (const UAttributeSet* Set : Asc.GetSpawnedAttributes())
    {
        if (!Set)
        {
            continue;
        }
        for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
        {
            const FStructProperty* Struct = CastField<FStructProperty>(*It);
            if (CastField<FFloatProperty>(*It) || (Struct && Struct->Struct == FGameplayAttributeData::StaticStruct()))
            {
                Out->SetNumberField(Set->GetClass()->GetName() + TEXT(".") + It->GetName(), Asc.GetNumericAttribute(FGameplayAttribute(*It)));
            }
        }
    }
    return Out;
}

TArray<TSharedPtr<FJsonValue>> Effects(const UAbilitySystemComponent& Asc, float WorldTime)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FActiveGameplayEffectHandle& Handle : Asc.GetActiveEffects(FGameplayEffectQuery()))
    {
        const FActiveGameplayEffect* Active = Asc.GetActiveGameplayEffect(Handle);
        if (!Active || !Active->Spec.Def)
        {
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("effect"), Active->Spec.Def->GetClass()->GetName());
        Row->SetNumberField(TEXT("stacks"), Asc.GetCurrentStackCount(Handle));
        // GetTimeRemaining compares with the engine's INFINITE_DURATION, a data symbol a delay-loaded module cannot
        // export, so the link failed: its value (-1) is compared here instead.
        const float Duration = Active->Spec.GetDuration();
        Row->SetNumberField(TEXT("secondsLeft"), Duration < 0.f ? -1.0 : Duration - (WorldTime - Active->StartWorldTime));
        Out.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Out;
}

// What changed from Before to Now: tags gained and lost, attributes whose value moved.
TSharedPtr<FJsonObject> Diff(const TSharedPtr<FJsonObject>& Before, const TSharedPtr<FJsonObject>& Now)
{
    TSet<FString> Was, Is;
    for (const TSharedPtr<FJsonValue>& Tag : Before->GetArrayField(TEXT("tags"))) Was.Add(Tag->AsString());
    for (const TSharedPtr<FJsonValue>& Tag : Now->GetArrayField(TEXT("tags"))) Is.Add(Tag->AsString());
    TArray<TSharedPtr<FJsonValue>> Gained, Lost, Moved;
    for (const FString& Tag : Is.Difference(Was)) Gained.Add(MakeShared<FJsonValueString>(Tag));
    for (const FString& Tag : Was.Difference(Is)) Lost.Add(MakeShared<FJsonValueString>(Tag));
    const TSharedPtr<FJsonObject> Then = Before->GetObjectField(TEXT("attributes"));
    for (const auto& Field : Now->GetObjectField(TEXT("attributes"))->Values)
    {
        double Old = 0.0;
        if (Then->TryGetNumberField(McpJsonKey(Field.Key), Old) && !FMath::IsNearlyEqual(Old, Field.Value->AsNumber()))
        {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("attribute"), McpJsonKey(Field.Key));
            Row->SetNumberField(TEXT("before"), Old);
            Row->SetNumberField(TEXT("after"), Field.Value->AsNumber());
            Moved.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("tagsGained"), Gained);
    Out->SetArrayField(TEXT("tagsLost"), Lost);
    Out->SetArrayField(TEXT("attributesChanged"), Moved);
    return Out;
}
} // namespace McpGASLiveDetail

namespace McpGASHandlers
{
bool HandleGASLiveState(const FGASRequestContext& Context, const FString& SubAction)
{
    using namespace McpGASLiveDetail;
    if (SubAction != TEXT("get_live_gas_state"))
    {
        return false;
    }
    UEngineWireSubsystem* Bridge = Context.Subsystem;
    if (!GEditor || !GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("No Play session is running: an ability system's live state exists only in Play (control_editor play)."), TEXT("PIE_NOT_RUNNING"));
        return true;
    }
    const FString Name = GetJsonStringField(Context.Payload, TEXT("actorName"));
    const AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    const UAbilitySystemComponent* Asc = Actor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
    if (!Asc)
    {
        Bridge->SendAutomationError(Context.RequestingSocket, Context.RequestId, Actor
            ? FString::Printf(TEXT("%s has no ability system component (nor does its owner interface name one)."), *Actor->GetActorLabel())
            : FString::Printf(TEXT("No actor named '%s' in the Play world (control_actor list names them)."), *Name),
            Actor ? TEXT("NO_ABILITY_SYSTEM") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    FGameplayTagContainer Tags;
    Asc->GetOwnedGameplayTags(Tags);
    TArray<TSharedPtr<FJsonValue>> TagRows;
    for (const FGameplayTag& Tag : Tags)
    {
        TagRows.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actor"), Actor->GetActorLabel());
    Result->SetArrayField(TEXT("tags"), TagRows);
    Result->SetObjectField(TEXT("attributes"), Attributes(*Asc));
    Result->SetArrayField(TEXT("activeEffects"), Effects(*Asc, Actor->GetWorld() ? Actor->GetWorld()->GetTimeSeconds() : 0.f));
    const FString Compare = GetJsonStringField(Context.Payload, TEXT("compareWith"));
    if (!Compare.IsEmpty())
    {
        const TSharedPtr<FJsonObject>* Before = GSaved.Find(Compare);
        if (!Before)
        {
            Bridge->SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("No state was saved as '%s' this editor session: read once with saveAs first."), *Compare), TEXT("STATE_NOT_FOUND"));
            return true;
        }
        Result->SetObjectField(TEXT("changes"), Diff(*Before, Result));
    }
    const FString Save = GetJsonStringField(Context.Payload, TEXT("saveAs"));
    if (!Save.IsEmpty())
    {
        TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
        Kept->SetArrayField(TEXT("tags"), TagRows);
        Kept->SetObjectField(TEXT("attributes"), Result->GetObjectField(TEXT("attributes")));
        GSaved.Add(Save, Kept);
    }
    Bridge->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, FString::Printf(TEXT("%s: %d tag(s), %d attribute(s), %d active effect(s)"),
        *Actor->GetActorLabel(), TagRows.Num(), Result->GetObjectField(TEXT("attributes"))->Values.Num(), Result->GetArrayField(TEXT("activeEffects")).Num()), Result);
    return true;
}
} // namespace McpGASHandlers
