#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleBuildLandscapeAndFoliageAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    const TSharedPtr<FJsonObject> &Payload = Context.Payload;
    TSharedPtr<FJsonObject> &Resp = Context.Resp;
    bool &bSuccess = Context.bSuccess;
    FString &Message = Context.Message;
    FString &ErrorCode = Context.ErrorCode;

    if (LowerSub == TEXT("import_heightmap"))
    {
        bSuccess = McpImportLandscapeHeightmap(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("export_heightmap"))
    {
        bSuccess = McpExportLandscapeHeightmap(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_landscape_layer_info"))
    {
        bSuccess = McpCreateLandscapeLayerInfo(Payload, Resp, Message, ErrorCode);
    }
    // -------------------------------------------------------------------------
    // configure_landscape_splines / configure_landscape_lod / streaming proxy
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("configure_landscape_splines"))
    {
        bSuccess = McpConfigureLandscapeSplines(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_landscape_lod"))
    {
        bSuccess = false;
        ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
        if (!Landscape)
        {
            Message = TEXT("Landscape not found for LOD configuration");
            ErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        }
        else
        {
            Landscape->Modify();
            McpApplyEnvironmentSettings(Landscape, Payload, Resp);
            Landscape->PostEditChange();
            bSuccess = true;
            Message = TEXT("Landscape LOD configuration updated");
            Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
            McpHandlerUtils::AddVerification(Resp, Landscape);
        }
    }
    else if (LowerSub == TEXT("create_landscape_streaming_proxy"))
    {
        bSuccess = McpCreateLandscapeStreamingProxy(Payload, Resp, Message, ErrorCode);
    }
    // -------------------------------------------------------------------------
    // configure_foliage_*: Update existing foliage type assets
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("configure_foliage_mesh") || LowerSub == TEXT("configure_foliage_placement") ||
             LowerSub == TEXT("configure_foliage_lod") || LowerSub == TEXT("configure_foliage_collision") ||
             LowerSub == TEXT("configure_foliage_culling"))
    {
        bSuccess = false;
        UFoliageType *FoliageType = McpLoadFoliageTypeForEnvironmentAction(Payload);
        if (!FoliageType)
        {
            Message = TEXT("Foliage type not found");
            ErrorCode = TEXT("ASSET_NOT_FOUND");
        }
        else
        {
            FoliageType->Modify();
            if (LowerSub == TEXT("configure_foliage_mesh"))
            {
                FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
                MeshPath = SanitizeProjectRelativePath(MeshPath);
                if (UFoliageType_InstancedStaticMesh *InstancedType = Cast<UFoliageType_InstancedStaticMesh>(FoliageType))
                {
                    if (UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
                    {
                        InstancedType->SetStaticMesh(StaticMesh);
                        Resp->SetStringField(TEXT("meshPath"), MeshPath);
                    }
                }
            }
            double Density = 0.0;
            if (Payload->TryGetNumberField(TEXT("density"), Density))
            {
                FoliageType->Density = static_cast<float>(Density);
            }
            double MinScale = 0.0;
            double MaxScale = 0.0;
            if (Payload->TryGetNumberField(TEXT("minScale"), MinScale) && Payload->TryGetNumberField(TEXT("maxScale"), MaxScale))
            {
                FoliageType->ScaleX = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
                FoliageType->ScaleY = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
                FoliageType->ScaleZ = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
            }
            bool bBoolValue = false;
            if (Payload->TryGetBoolField(TEXT("alignToNormal"), bBoolValue))
            {
                FoliageType->AlignToNormal = bBoolValue;
            }
            if (Payload->TryGetBoolField(TEXT("randomYaw"), bBoolValue))
            {
                FoliageType->RandomYaw = bBoolValue;
            }
            int32 CullDistance = 0;
            if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance) && CullDistance >= 0)
            {
                FoliageType->CullDistance.Min = 0;
                FoliageType->CullDistance.Max = CullDistance;
            }
            McpApplyEnvironmentSettings(FoliageType, Payload, Resp);
            TArray<FString> ConfigurationErrors;
            const TArray<TSharedPtr<FJsonValue>> *ConfigurationErrorValues = nullptr;
            if (Resp->TryGetArrayField(TEXT("configurationErrors"), ConfigurationErrorValues) && ConfigurationErrorValues)
            {
                for (const TSharedPtr<FJsonValue> &Value : *ConfigurationErrorValues)
                {
                    if (!Value.IsValid())
                    {
                        continue;
                    }
                    const FString ErrorText = Value->AsString();
                    if (!ErrorText.StartsWith(TEXT("cullDistance:")))
                    {
                        ConfigurationErrors.Add(ErrorText);
                    }
                }
                McpAddStringArrayField(Resp, TEXT("configurationErrors"), ConfigurationErrors);
            }
            FoliageType->MarkPackageDirty();
            McpSafeAssetSave(FoliageType);
            bSuccess = true;
            Message = TEXT("Foliage type configuration updated");
            Resp->SetStringField(TEXT("foliageTypePath"), FoliageType->GetOutermost()->GetName());
            McpHandlerUtils::AddVerification(Resp, FoliageType);
        }
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
