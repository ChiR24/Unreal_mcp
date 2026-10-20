#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

namespace McpAIHandlers
{
// The component's config for one sense, or a new one when it has none (bOutCreated). Each call used to build a fresh
// config and hand it to ConfigureSense, which swaps out the old one, so every setting a call did not send went back to
// the handler's defaults.
template <typename TConfig>
TConfig* FindOrAddSenseConfig(UAIPerceptionComponent* Perception, bool& bOutCreated)
{
    for (auto It = Perception->GetSensesConfigIterator(); It; ++It)
    {
        if (TConfig* Config = Cast<TConfig>(*It))
        {
            bOutCreated = false;
            Config->Modify();
            return Config;
        }
    }
    bOutCreated = true;
    return NewObject<TConfig>(Perception);
}

// Sight settings from Settings, each written only when sent: the radii, angle, offsets, maxAge and
// detectionByAffiliation {enemies, neutrals, friendlies}. A new config starts at 3000 / 3500 / 90 degrees, detecting
// enemies and neutrals, remembering for 5 s.
inline void ApplySightSettings(UAISenseConfig_Sight* Sight, const TSharedPtr<FJsonObject>& Settings, bool bCreated)
{
    if (bCreated)
    {
        Sight->SightRadius = 3000.0f;
        Sight->PeripheralVisionAngleDegrees = 90.0f;
        Sight->DetectionByAffiliation.bDetectEnemies = true;
        Sight->DetectionByAffiliation.bDetectNeutrals = true;
        Sight->SetMaxAge(5.0f);
    }
    double Value = 0.0;
    const auto Number = [&](const TCHAR* Field, float& Target)
    {
        if (Settings->TryGetNumberField(Field, Value))
        {
            Target = static_cast<float>(Value);
        }
    };
    Number(TEXT("sightRadius"), Sight->SightRadius);
    Sight->LoseSightRadius = bCreated ? Sight->SightRadius + 500.0f : Sight->LoseSightRadius;
    Number(TEXT("loseSightRadius"), Sight->LoseSightRadius);
    Number(TEXT("peripheralVisionAngle"), Sight->PeripheralVisionAngleDegrees);
    Number(TEXT("pointOfViewBackwardOffset"), Sight->PointOfViewBackwardOffset);
    Number(TEXT("nearClippingRadius"), Sight->NearClippingRadius);
    Number(TEXT("autoSuccessRange"), Sight->AutoSuccessRangeFromLastSeenLocation);
    if (Settings->TryGetNumberField(TEXT("maxAge"), Value))
    {
        Sight->SetMaxAge(static_cast<float>(Value));
    }
    const TSharedPtr<FJsonObject>* Affiliation = nullptr;
    bool bFlag = false;
    if (Settings->TryGetObjectField(TEXT("detectionByAffiliation"), Affiliation) && Affiliation && Affiliation->IsValid())
    {
        if ((*Affiliation)->TryGetBoolField(TEXT("enemies"), bFlag)) { Sight->DetectionByAffiliation.bDetectEnemies = bFlag; }
        if ((*Affiliation)->TryGetBoolField(TEXT("neutrals"), bFlag)) { Sight->DetectionByAffiliation.bDetectNeutrals = bFlag; }
        if ((*Affiliation)->TryGetBoolField(TEXT("friendlies"), bFlag)) { Sight->DetectionByAffiliation.bDetectFriendlies = bFlag; }
    }
}

// A sense config's settings under the names its configure action takes, so a reply or a read can be sent back as input:
// maxAge (0 never forgets; GetMaxAge answers the never-happened age for it), the sight radii, angle and
// detectionByAffiliation, the hearing range and detectFriendly.
inline void AddSenseSettings(const UAISenseConfig* Config, const TSharedPtr<FJsonObject>& Out)
{
    const float MaxAge = Config->GetMaxAge();
    Out->SetNumberField(TEXT("maxAge"), MaxAge >= FAIStimulus::NeverHappenedAge ? 0.0f : MaxAge);
    if (const UAISenseConfig_Sight* Sight = Cast<UAISenseConfig_Sight>(Config))
    {
        Out->SetNumberField(TEXT("sightRadius"), Sight->SightRadius);
        Out->SetNumberField(TEXT("loseSightRadius"), Sight->LoseSightRadius);
        Out->SetNumberField(TEXT("peripheralVisionAngle"), Sight->PeripheralVisionAngleDegrees);
        TSharedPtr<FJsonObject> Affiliation = MakeShared<FJsonObject>();
        Affiliation->SetBoolField(TEXT("enemies"), Sight->DetectionByAffiliation.bDetectEnemies != 0);
        Affiliation->SetBoolField(TEXT("neutrals"), Sight->DetectionByAffiliation.bDetectNeutrals != 0);
        Affiliation->SetBoolField(TEXT("friendlies"), Sight->DetectionByAffiliation.bDetectFriendlies != 0);
        Out->SetObjectField(TEXT("detectionByAffiliation"), Affiliation);
    }
    else if (const UAISenseConfig_Hearing* Hearing = Cast<UAISenseConfig_Hearing>(Config))
    {
        Out->SetNumberField(TEXT("hearingRange"), Hearing->HearingRange);
        Out->SetBoolField(TEXT("detectFriendly"), Hearing->DetectionByAffiliation.bDetectFriendlies != 0);
    }
}

// Hearing settings, each written only when sent: hearingRange, detectFriendly and maxAge. A new config hears 3000
// units, enemies and neutrals, remembering for 5 s. No 5.x hearing sense reads LoSHearingRange (5.0 copies it and never
// uses it; 5.2 deprecates it), so it is not offered.
inline void ApplyHearingSettings(UAISenseConfig_Hearing* Hearing, const TSharedPtr<FJsonObject>& Settings, bool bCreated)
{
    if (bCreated)
    {
        Hearing->HearingRange = 3000.0f;
        Hearing->DetectionByAffiliation.bDetectEnemies = true;
        Hearing->DetectionByAffiliation.bDetectNeutrals = true;
        Hearing->SetMaxAge(5.0f);
    }
    double Value = 0.0;
    bool bFlag = false;
    if (Settings->TryGetNumberField(TEXT("hearingRange"), Value)) { Hearing->HearingRange = static_cast<float>(Value); }
    if (Settings->TryGetBoolField(TEXT("detectFriendly"), bFlag)) { Hearing->DetectionByAffiliation.bDetectFriendlies = bFlag; }
    if (Settings->TryGetNumberField(TEXT("maxAge"), Value)) { Hearing->SetMaxAge(static_cast<float>(Value)); }
}

// The named sub-object (sightConfig, hearingConfig, damageConfig) over the top-level fields: the sub-object wins.
inline TSharedPtr<FJsonObject> MergedSenseSettings(const TSharedPtr<FJsonObject>& Payload, const TCHAR* SubObject)
{
    TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>(*Payload);
    const TSharedPtr<FJsonObject>* Nested = nullptr;
    if (Payload->TryGetObjectField(SubObject, Nested) && Nested && Nested->IsValid())
    {
        for (const auto& Pair : (*Nested)->Values)
        {
            // 5.8 keys are not FString: copy the text first.
            Merged->SetField(FString(Pair.Key.Len(), *Pair.Key), Pair.Value);
        }
    }
    return Merged;
}
}
