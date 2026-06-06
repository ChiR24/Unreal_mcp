#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Lightmass/LightmassImportanceVolume.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

bool HandleBuildLighting(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (AWorldSettings* WS = World->GetWorldSettings())
    {
        if (WS->bForceNoPrecomputedLighting)
        {
            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetBoolField(TEXT("skipped"), true);
            Resp->SetStringField(TEXT("reason"), TEXT("bForceNoPrecomputedLighting is true"));
            Resp->SetStringField(
                TEXT("suggestion"),
                TEXT("Set WorldSettings.bForceNoPrecomputedLighting to false to enable lighting builds"));
            Subsystem.SendAutomationResponse(
                RequestingSocket,
                RequestId,
                true,
                TEXT("Lighting build skipped - precomputed lighting disabled in WorldSettings"),
                Resp);
            return true;
        }
    }

    FString Quality;
    Payload->TryGetStringField(TEXT("quality"), Quality);

    FString QualityCmd = TEXT("Production");
    if (!Quality.IsEmpty())
    {
        const FString LowerQuality = Quality.ToLower();
        if (LowerQuality == TEXT("preview") || LowerQuality == TEXT("0"))
        {
            QualityCmd = TEXT("Preview");
        }
        else if (LowerQuality == TEXT("medium") || LowerQuality == TEXT("1"))
        {
            QualityCmd = TEXT("Medium");
        }
        else if (LowerQuality == TEXT("high") || LowerQuality == TEXT("2"))
        {
            QualityCmd = TEXT("High");
        }
        else if (LowerQuality == TEXT("production") || LowerQuality == TEXT("3"))
        {
            QualityCmd = TEXT("Production");
        }
        else
        {
            TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
            Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
            Err->SetStringField(TEXT("quality"), Quality);
            Err->SetStringField(TEXT("validValues"), TEXT("preview/0, medium/1, high/2, production/3"));
            Subsystem.SendAutomationResponse(
                RequestingSocket, RequestId, false, TEXT("Unknown lighting quality"), Err, TEXT("UNKNOWN_QUALITY"));
            return true;
        }
    }

    const FString Command = FString::Printf(TEXT("BuildLighting %s"), *QualityCmd);
    GEditor->Exec(World, *Command);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("quality"), QualityCmd);
    Resp->SetBoolField(TEXT("started"), true);
    Subsystem.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        FString::Printf(TEXT("Lighting build started with quality: %s"), *QualityCmd),
        Resp);
    return true;
}

bool HandleCreateLightmassVolume(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj))
    {
        Location.X = GetJsonNumberField((*LocObj), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocObj), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocObj), TEXT("z"));
    }

    FVector Size(1000, 1000, 1000);
    const TSharedPtr<FJsonObject>* SizeObj;
    if (Payload->TryGetObjectField(TEXT("size"), SizeObj))
    {
        Size.X = GetJsonNumberField((*SizeObj), TEXT("x"));
        Size.Y = GetJsonNumberField((*SizeObj), TEXT("y"));
        Size.Z = GetJsonNumberField((*SizeObj), TEXT("z"));
    }

    AActor* Volume = SpawnActorInActiveWorld<AActor>(
        ALightmassImportanceVolume::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Volume)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to spawn LightmassImportanceVolume"), TEXT("SPAWN_FAILED"));
        return true;
    }

    Volume->SetActorScale3D(Size / 200.0f);
    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        Volume->SetActorLabel(Name);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Volume->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, Volume);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("LightmassImportanceVolume created"), Resp);
    return true;
}

}
#endif
