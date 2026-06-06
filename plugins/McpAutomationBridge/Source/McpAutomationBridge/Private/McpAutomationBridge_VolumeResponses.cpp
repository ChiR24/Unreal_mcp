#include "McpAutomationBridge_VolumeResponses.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "McpHandlerUtils.h"

namespace VolumeHelpers
{
#if WITH_EDITOR
TSharedPtr<FJsonObject> CreateVectorObject(const FVector& Value)
{
    TSharedPtr<FJsonObject> Json = McpHandlerUtils::CreateResultObject();
    Json->SetNumberField(TEXT("x"), Value.X);
    Json->SetNumberField(TEXT("y"), Value.Y);
    Json->SetNumberField(TEXT("z"), Value.Z);
    return Json;
}

TSharedPtr<FJsonObject> CreateVolumeResponse(AActor* Volume, const FString& VolumeClass)
{
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), Volume ? Volume->GetActorLabel() : TEXT(""));
    ResponseJson->SetStringField(TEXT("volumeClass"), VolumeClass);
    if (Volume)
    {
        McpHandlerUtils::AddVerification(ResponseJson, Volume);
    }
    return ResponseJson;
}
#endif
}
