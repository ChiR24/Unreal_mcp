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

    // Only configure_landscape_lod lands here (configure_landscape_material goes to
    // HandleSetLandscapeMaterial). It used to report success with nothing applied or with
    // configurationErrors listed.
    Landscape->Modify();
    const int32 AppliedCount = McpApplyEnvironmentSettings(Landscape, Context.Payload, Context.Resp);
    const TArray<TSharedPtr<FJsonValue>> *Errors = nullptr;
    const bool bErrors = Context.Resp->TryGetArrayField(TEXT("configurationErrors"), Errors) && Errors && Errors->Num() > 0;
    Context.Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Context.Resp->SetStringField(TEXT("actorPath"), Landscape->GetPathName());
    if (bErrors || AppliedCount == 0)
    {
        Context.bSuccess = false;
        Context.Message = bErrors
            ? FString(TEXT("Some landscape settings could not be applied; see configurationErrors"))
            : FString(TEXT("No landscape setting applied: pass settings keyed by landscape property name, e.g. {\"LODDistributionSetting\": 1.5}"));
        Context.ErrorCode = bErrors ? TEXT("CONFIGURATION_FAILED") : TEXT("NO_SETTING_SUPPLIED");
        return true;
    }
    Landscape->MarkPackageDirty();
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
    if (LowerSub == TEXT("configure_landscape_lod"))
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
