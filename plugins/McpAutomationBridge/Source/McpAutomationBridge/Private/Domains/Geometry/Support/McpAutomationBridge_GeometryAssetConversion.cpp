#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Domains/ControlActor/Placement/McpAutomationBridge_PartPlacement.h"
#include "Materials/MaterialInterface.h"

namespace McpGeometryHandlers
{
// outputPath names the converted asset (dogfood #135). A folder
// (trailing '/' or an existing content folder) gets DefaultName appended; anything else
// is the full asset path. Everything passes the project path sanitizer first.
bool ResolveConversionAssetPath(const TSharedPtr<FJsonObject>& Payload, const FString& DefaultName,
                                FString& OutAssetPath, FString& OutError)
{
    const FString OutRequested = GetJsonStringField(Payload, TEXT("outputPath"));
    if (OutRequested.IsEmpty())
    {
        OutAssetPath = TEXT("/Game/GeneratedMeshes/") + DefaultName;
        return true;
    }
    const bool bExplicitFolder = OutRequested.EndsWith(TEXT("/"));
    FString Sanitized = SanitizeProjectRelativePath(OutRequested);
    if (Sanitized.IsEmpty())
    {
        OutError = McpPathRefusalMessage(TEXT("outputPath"), OutRequested);
        return false;
    }
    Sanitized = FPackageName::ObjectPathToPackageName(Sanitized);
    Sanitized.RemoveFromEnd(TEXT("/"));
    if (bExplicitFolder || UEditorAssetLibrary::DoesDirectoryExist(Sanitized))
    {
        Sanitized += TEXT("/") + DefaultName;
    }
    OutAssetPath = Sanitized;
    return true;
}

bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bNanite)
{
    // convert_to_nanite is the same bake with Nanite enabled.
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    FString AssetPath;
    FString PathError;
    if (!ResolveConversionAssetPath(Payload, bNanite ? ActorName + TEXT("_Nanite") : ActorName, AssetPath, PathError))
    {
        Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    const FString CollisionMode = GetJsonStringField(Payload, TEXT("collision"), TEXT("box")).ToLower();
    if (CollisionMode != TEXT("box") && CollisionMode != TEXT("complex") && CollisionMode != TEXT("none"))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown collision '%s'; use box, complex or none."), *CollisionMode), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // The materials are checked before anything is created, so a bad path leaves no half-made asset behind.
    const int32 SlotCount = ConversionSlotCount(Target->Mesh);
    TArray<UMaterialInterface*> Materials;
    FString MaterialError;
    if (!LoadConversionMaterials(Payload, SlotCount, Materials, MaterialError))
    {
        Self->SendAutomationError(Socket, RequestId, MaterialError, TEXT("INVALID_MATERIALS"));
        return true;
    }

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    CreateOptions.bEnableNanite = bNanite;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    // From 5.1 the new mesh takes these settings whole when Nanite is asked for, and their
    // bEnabled defaults to false, so bEnableNanite alone baked every "Nanite" mesh without it.
    CreateOptions.NaniteSettings.bEnabled = bNanite;
#endif

    EGeometryScriptOutcomePins Outcome;
    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Target->Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    // The asset comes out with one material slot more than the mesh's highest material id (all empty but the first)
    // and no collision body; fill the slots and give it a body, then save again, because the first write predates both
    // and without a second save the materials and the collision were gone on the next editor load.
    TArray<TSharedPtr<FJsonValue>> SlotsJson;
    UStaticMesh* CreatedMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *AssetPath));
    if (!CreatedMesh)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("%s was created but could not be loaded back, so its materials and collision body were not applied."), *AssetPath),
            TEXT("ASSET_CREATION_FAILED"));
        return true;
    }
    {
        ApplyConversionMaterials(CreatedMesh, Materials, SlotCount);
        ApplyConversionCollision(CreatedMesh, CollisionMode);
        if (!McpSafeAssetSave(CreatedMesh))
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(
                TEXT("%s was created, but its materials and collision body could not be saved to disk."), *AssetPath), TEXT("SAVE_FAILED"));
            return true;
        }
        const TArray<FStaticMaterial>& Slots = CreatedMesh->GetStaticMaterials();
        for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
        {
            TSharedPtr<FJsonObject> SlotJson = MakeShared<FJsonObject>();
            SlotJson->SetNumberField(TEXT("slot"), Slot);
            SlotJson->SetStringField(TEXT("name"), Slots[Slot].MaterialSlotName.ToString());
            // An empty material is the engine's default material.
            SlotJson->SetStringField(TEXT("material"), Slots[Slot].MaterialInterface ? Slots[Slot].MaterialInterface->GetPathName() : FString());
            SlotsJson.Add(MakeShared<FJsonValueObject>(SlotJson));
        }
    }

    // Read back from the mesh: the reply used to echo the request.
    bool bNaniteOn = false;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
    bNaniteOn = CreatedMesh && CreatedMesh->IsNaniteEnabled();
#else
    bNaniteOn = CreatedMesh && CreatedMesh->NaniteSettings.bEnabled;
#endif
    if (bNanite && !bNaniteOn)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("%s was created, but Nanite is not enabled on it."), *AssetPath), TEXT("NANITE_NOT_ENABLED"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), bNaniteOn);
    Result->SetStringField(TEXT("collision"), CollisionMode);
    Result->SetArrayField(TEXT("slots"), SlotsJson);
    // A mesh replaced in place reshapes every Blueprint part that draws it: say which parts it left sunk.
    McpPartPlacement::AppendMeshUserWarnings(CreatedMesh, Result);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
