#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {

bool HandleBuildWaterAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    FString Message;
    FString ErrorCode;

    if (LowerSub == TEXT("create_water_body_ocean"))
    {
        const bool bResult = McpConfigureWaterBody(Context.Payload, TEXT("/Script/Water.WaterBodyOcean"),
            TEXT("WaterBodyOcean"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_water_body_lake"))
    {
        const bool bResult = McpConfigureWaterBody(Context.Payload, TEXT("/Script/Water.WaterBodyLake"),
            TEXT("WaterBodyLake"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_water_body_river"))
    {
        const bool bResult = McpConfigureWaterBody(Context.Payload, TEXT("/Script/Water.WaterBodyRiver"),
            TEXT("WaterBodyRiver"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_water_body_custom"))
    {
        const bool bResult = McpConfigureWaterBody(Context.Payload, TEXT("/Script/Water.WaterBodyCustom"),
            TEXT("WaterBodyCustom"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_water_waves"))
    {
        AActor *WaterActor = McpFindWaterBodyActor(Context.Payload);
        const bool bResult = McpConfigureWaterWavesOnActor(
            WaterActor, Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_water_material") || LowerSub == TEXT("configure_water_collision"))
    {
        const bool bResult = McpConfigureWaterBodyActor(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_buoyancy_component"))
    {
        const bool bResult = McpCreateBuoyancyComponent(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    return false;
}

}
