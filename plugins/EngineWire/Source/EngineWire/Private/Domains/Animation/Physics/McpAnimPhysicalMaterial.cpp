// create_physical_material: the surface a body's contacts use. Friction, bounce (restitution) and density, and how two
// touching surfaces combine them, could only be read off the engine's defaults: nothing made a ball bouncy or a floor
// slippery. A material already at the path is tuned in place, so one call adjusts it.
#include "Domains/Animation/McpAnimActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"

namespace McpAnimationHandlers
{
namespace
{
const TCHAR *const CombineModeNames[] = {TEXT("Average"), TEXT("Min"), TEXT("Multiply"), TEXT("Max")};

const TCHAR *CombineModeName(EFrictionCombineMode::Type Mode)
{
    return CombineModeNames[FMath::Clamp<int32>(static_cast<int32>(Mode), 0, UE_ARRAY_COUNT(CombineModeNames) - 1)];
}

// Average, Min, Multiply or Max, as the details panel names them; a mode given also turns its override on.
bool ReadCombineMode(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Field,
                     TEnumAsByte<EFrictionCombineMode::Type> &Mode, bool &bOverride, FString &OutError)
{
    FString Text;
    if (!Payload->TryGetStringField(Field, Text))
    {
        return true;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CombineModeNames); ++Index)
    {
        if (Text.Equals(CombineModeNames[Index], ESearchCase::IgnoreCase))
        {
            Mode = static_cast<EFrictionCombineMode::Type>(Index);
            bOverride = true;
            return true;
        }
    }
    OutError = FString::Printf(TEXT("%s '%s' is not a combine mode; use Average, Min, Multiply or Max"), Field, *Text);
    return false;
}

// A number between Min and Max; an absent field leaves Value as it was.
bool ReadNumber(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Field, float &Value, double Min, double Max,
                FString &OutError)
{
    double Number = 0.0;
    if (!Payload->TryGetNumberField(Field, Number))
    {
        return true;
    }
    if (!FMath::IsFinite(Number) || Number < Min || Number > Max)
    {
        OutError = FString::Printf(TEXT("%s must be between %g and %g"), Field, Min, Max);
        return false;
    }
    Value = static_cast<float>(Number);
    return true;
}
} // namespace

bool HandleAnimationCreatePhysicalMaterialAction(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload)
{
    FString Name, Folder, ObjectPath, Error;
    Payload->TryGetStringField(TEXT("name"), Name);
    Payload->TryGetStringField(TEXT("path"), Folder);
    if (Folder.TrimStartAndEnd().IsEmpty())
    {
        Folder = TEXT("/Game/Physics");
    }
    if (Name.TrimStartAndEnd().IsEmpty())
    {
        Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("name is required"));
        return false;
    }
    if (!ValidateAssetCreationPath(Folder, Name, ObjectPath, Error))
    {
        Context.Fail(TEXT("INVALID_PATH"), Error);
        return false;
    }
    UPhysicalMaterial *Material = LoadObject<UPhysicalMaterial>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Material && McpAssetExists(ObjectPath))
    {
        Context.Fail(TEXT("ASSET_EXISTS"), FString::Printf(TEXT("%s is an asset of another class"), *ObjectPath));
        return false;
    }

    // Every value is read before the asset is made or changed, so a bad one leaves it as it was.
    const UPhysicalMaterial *Source = Material ? Material : GetDefault<UPhysicalMaterial>();
    float Friction = Source->Friction;
    float StaticFriction = Source->StaticFriction;
    float Restitution = Source->Restitution;
    float Density = Source->Density;
    TEnumAsByte<EFrictionCombineMode::Type> FrictionMode = Source->FrictionCombineMode;
    TEnumAsByte<EFrictionCombineMode::Type> BounceMode = Source->RestitutionCombineMode;
    bool bFrictionOverride = Source->bOverrideFrictionCombineMode;
    bool bBounceOverride = Source->bOverrideRestitutionCombineMode;
    if (!ReadNumber(Payload, TEXT("friction"), Friction, 0.0, 100.0, Error) ||
        !ReadNumber(Payload, TEXT("staticFriction"), StaticFriction, 0.0, 100.0, Error) ||
        !ReadNumber(Payload, TEXT("restitution"), Restitution, 0.0, 1.0, Error) ||
        !ReadNumber(Payload, TEXT("density"), Density, 0.0, 100000.0, Error) ||
        !ReadCombineMode(Payload, TEXT("frictionCombineMode"), FrictionMode, bFrictionOverride, Error) ||
        !ReadCombineMode(Payload, TEXT("restitutionCombineMode"), BounceMode, bBounceOverride, Error))
    {
        Context.Fail(TEXT("INVALID_ARGUMENT"), Error + TEXT(". Nothing was changed."));
        return false;
    }

    const bool bCreated = Material == nullptr;
    if (bCreated)
    {
        UPackage *Package = CreatePackage(*ObjectPath);
        Material = Package ? NewObject<UPhysicalMaterial>(Package, FName(*FPackageName::GetShortName(ObjectPath)),
                                                          RF_Public | RF_Standalone | RF_Transactional)
                           : nullptr;
        if (!Material)
        {
            Context.Fail(TEXT("ASSET_CREATION_FAILED"), FString::Printf(TEXT("Could not create %s"), *ObjectPath));
            return false;
        }
        FAssetRegistryModule::AssetCreated(Material);
    }
    Material->Modify();
    Material->Friction = Friction;
    Material->StaticFriction = StaticFriction;
    Material->Restitution = Restitution;
    Material->Density = Density;
    Material->FrictionCombineMode = FrictionMode;
    Material->bOverrideFrictionCombineMode = bFrictionOverride;
    Material->RestitutionCombineMode = BounceMode;
    Material->bOverrideRestitutionCombineMode = bBounceOverride;
    // Bodies already using it take the new values from the physics scene's copy, which this refreshes.
    Material->PostEditChange();
    Material->MarkPackageDirty();
    const bool bSaved = McpSafeOperations::McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> &Resp = Context.Resp;
    Resp->SetStringField(TEXT("physicalMaterialPath"), Material->GetPathName());
    Resp->SetBoolField(TEXT("created"), bCreated);
    Resp->SetNumberField(TEXT("friction"), Material->Friction);
    Resp->SetNumberField(TEXT("staticFriction"), Material->StaticFriction);
    Resp->SetNumberField(TEXT("restitution"), Material->Restitution);
    Resp->SetNumberField(TEXT("density"), Material->Density);
    if (Material->bOverrideFrictionCombineMode)
    {
        Resp->SetStringField(TEXT("frictionCombineMode"), CombineModeName(Material->FrictionCombineMode));
    }
    if (Material->bOverrideRestitutionCombineMode)
    {
        Resp->SetStringField(TEXT("restitutionCombineMode"), CombineModeName(Material->RestitutionCombineMode));
    }
    if (!bSaved)
    {
        Resp->SetStringField(TEXT("warning"), TEXT("The physical material is set but could not be saved."));
    }
    McpHandlerUtils::AddVerification(Resp, Material);
    Context.bSuccess = true;
    Context.Message = bCreated ? TEXT("Physical material created") : TEXT("Physical material updated");
    return false;
}
} // namespace McpAnimationHandlers
