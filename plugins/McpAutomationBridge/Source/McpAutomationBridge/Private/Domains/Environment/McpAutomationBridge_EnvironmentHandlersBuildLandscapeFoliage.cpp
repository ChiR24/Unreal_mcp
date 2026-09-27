#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {
namespace {

bool ConfigureLandscapeActor(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Context.Payload);
    if (!Landscape)
    {
        Context.bSuccess = false;
        Context.Message = FString::Printf(TEXT("Landscape not found for %s"), *LowerSub);
        Context.ErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return true;
    }

    Landscape->Modify();
    int32 AppliedMaterialSlots = 0;
    if (LowerSub == TEXT("configure_landscape_material"))
    {
        AppliedMaterialSlots = McpSetMaterialOnActor(Landscape, Context.Payload, Context.Resp);
    }
    McpApplyEnvironmentSettings(Landscape, Context.Payload, Context.Resp);
    Landscape->MarkPackageDirty();

    Context.Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Context.Resp->SetStringField(TEXT("actorPath"), Landscape->GetPathName());
    Context.Resp->SetNumberField(TEXT("materialSlotsUpdated"), AppliedMaterialSlots);
    McpHandlerUtils::AddVerification(Context.Resp, Landscape);
    Context.bSuccess = true;
    Context.Message = FString::Printf(TEXT("Landscape action completed: %s"), *LowerSub);
    Context.ErrorCode.Empty();
    return true;
}

}

bool HandleBuildLandscapeAndFoliageAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    FString Message;
    FString ErrorCode;

    if (LowerSub == TEXT("import_heightmap"))
    {
        const bool bResult = McpImportLandscapeHeightmap(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("export_heightmap"))
    {
        const bool bResult = McpExportLandscapeHeightmap(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_landscape_layer_info"))
    {
        const bool bResult = McpCreateLandscapeLayerInfo(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_landscape_splines"))
    {
        const bool bResult = McpConfigureLandscapeSplines(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("create_landscape_streaming_proxy"))
    {
        const bool bResult = McpCreateLandscapeStreamingProxy(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_landscape_material") || LowerSub == TEXT("configure_landscape_lod"))
    {
        return ConfigureLandscapeActor(LowerSub, Context);
    }
    if (LowerSub == TEXT("configure_foliage_mesh") ||
        LowerSub == TEXT("configure_foliage_placement") ||
        LowerSub == TEXT("configure_foliage_lod") ||
        LowerSub == TEXT("configure_foliage_collision") ||
        LowerSub == TEXT("configure_foliage_culling"))
    {
        const bool bResult = McpConfigureFoliageType(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    return false;
}

}
