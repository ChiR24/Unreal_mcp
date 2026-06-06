#include "McpAutomationBridge_VolumeCullDistances.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/CullDistanceVolume.h"
#include "McpAutomationBridgeHelpers.h"

namespace VolumeHelpers
{
#if WITH_EDITOR
void SetCullDistancesFromPayload(AActor* VolumeActor, const TSharedPtr<FJsonObject>& Payload)
{
    ACullDistanceVolume* Volume = Cast<ACullDistanceVolume>(VolumeActor);
    if (!Volume || !Payload->HasTypedField<EJson::Array>(TEXT("cullDistances")))
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> CullDistancesJson = Payload->GetArrayField(TEXT("cullDistances"));
    TArray<FCullDistanceSizePair> CullDistances;
    for (const TSharedPtr<FJsonValue>& Entry : CullDistancesJson)
    {
        if (Entry->Type != EJson::Object)
        {
            continue;
        }
        TSharedPtr<FJsonObject> EntryObj = Entry->AsObject();
        FCullDistanceSizePair Pair;
        Pair.Size = GetJsonNumberField(EntryObj, TEXT("size"), 100.0f);
        Pair.CullDistance = GetJsonNumberField(EntryObj, TEXT("cullDistance"), 5000.0f);
        CullDistances.Add(Pair);
    }
    if (CullDistances.Num() > 0)
    {
        Volume->CullDistances = CullDistances;
    }
}
#endif
}
