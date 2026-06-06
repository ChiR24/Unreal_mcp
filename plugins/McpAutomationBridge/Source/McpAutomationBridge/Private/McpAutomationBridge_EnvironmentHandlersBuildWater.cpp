#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleBuildWaterAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    const TSharedPtr<FJsonObject> &Payload = Context.Payload;
    TSharedPtr<FJsonObject> &Resp = Context.Resp;
    bool &bSuccess = Context.bSuccess;
    FString &Message = Context.Message;
    FString &ErrorCode = Context.ErrorCode;

    if (LowerSub == TEXT("create_water_body_ocean") || LowerSub == TEXT("create_water_body_lake") ||
             LowerSub == TEXT("create_water_body_river") || LowerSub == TEXT("create_water_body_custom"))
    {
        const FString WaterClassPath = LowerSub == TEXT("create_water_body_ocean") ? TEXT("/Script/Water.WaterBodyOcean") :
            LowerSub == TEXT("create_water_body_lake") ? TEXT("/Script/Water.WaterBodyLake") :
            LowerSub == TEXT("create_water_body_river") ? TEXT("/Script/Water.WaterBodyRiver") :
            TEXT("/Script/Water.WaterBodyCustom");
        UClass *WaterClass = LoadClass<AActor>(nullptr, *WaterClassPath);
        AActor *WaterActor = McpFindOrSpawnEnvironmentActor(Payload, WaterClass, LowerSub);
        if (!WaterActor)
        {
            bSuccess = false;
            Message = FString::Printf(TEXT("Water body class unavailable or spawn failed: %s"), *WaterClassPath);
            ErrorCode = WaterClass ? TEXT("SPAWN_FAILED") : TEXT("CLASS_NOT_FOUND");
            Resp->SetStringField(TEXT("classPath"), WaterClassPath);
        }
        else
        {
            McpApplyEnvironmentSettings(WaterActor, Payload, Resp);
            McpSetMaterialOnActor(WaterActor, Payload, Resp);
            McpSetCollisionOnActor(WaterActor, Payload, Resp);
            bSuccess = true;
            Message = TEXT("Water body created");
            Resp->SetStringField(TEXT("waterBodyName"), WaterActor->GetActorLabel());
            Resp->SetStringField(TEXT("actorPath"), WaterActor->GetPathName());
            Resp->SetStringField(TEXT("classPath"), WaterClassPath);
            McpHandlerUtils::AddVerification(Resp, WaterActor);
        }
    }
    else if (LowerSub == TEXT("configure_water_waves"))
    {
        AActor *WaterActor = McpFindWaterBodyActor(Payload);
        bSuccess = McpConfigureWaterBodyActor(Payload, Resp, Message, ErrorCode);
        if (bSuccess && McpPayloadHasWaterWaveSettings(Payload))
        {
            bSuccess = McpConfigureWaterWavesOnActor(WaterActor, Payload, Resp, Message, ErrorCode);
        }
    }
    else if (LowerSub == TEXT("configure_water_material") || LowerSub == TEXT("configure_water_collision"))
    {
        bSuccess = McpConfigureWaterBodyActor(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_buoyancy_component"))
    {
        bSuccess = McpCreateBuoyancyComponent(Payload, Resp, Message, ErrorCode);
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
