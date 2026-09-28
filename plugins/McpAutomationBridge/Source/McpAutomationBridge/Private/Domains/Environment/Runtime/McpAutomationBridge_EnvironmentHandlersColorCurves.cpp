#include "Domains/Environment/Runtime/McpAutomationBridge_EnvironmentAssetValidation.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Misc/PackageName.h"

namespace McpEnvironmentHandlers {

namespace {

// One key per entry: {time, color {r, g, b, a}}; alpha defaults to 1.
bool McpReadColorCurveKeys(const TArray<TSharedPtr<FJsonValue>> &Values, TArray<TPair<float, FLinearColor>> &OutKeys)
{
    for (const TSharedPtr<FJsonValue> &Value : Values)
    {
        const TSharedPtr<FJsonObject> *KeyObject = nullptr;
        const TSharedPtr<FJsonObject> *ColorObject = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(KeyObject) || !KeyObject || !(*KeyObject)->HasField(TEXT("time")) ||
            !(*KeyObject)->TryGetObjectField(TEXT("color"), ColorObject) || !ColorObject)
        {
            return false;
        }
        const TSharedPtr<FJsonObject> &Color = *ColorObject;
        OutKeys.Emplace(static_cast<float>(GetJsonNumberField(*KeyObject, TEXT("time"))),
                        FLinearColor(static_cast<float>(GetJsonNumberField(Color, TEXT("r"))),
                                     static_cast<float>(GetJsonNumberField(Color, TEXT("g"))),
                                     static_cast<float>(GetJsonNumberField(Color, TEXT("b"))),
                                     static_cast<float>(GetJsonNumberField(Color, TEXT("a"), 1.0))));
    }
    return OutKeys.Num() > 0;
}

}

// curvePath names the curve asset itself: an existing curve is edited, else one is
// created there. It used to be treated as a folder, so every call made a fresh flat
// white <curvePath>/MCP_*ColorCurve and ignored its settings.
bool McpCreateLinearColorCurve(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                               TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    FString CurvePath = GetJsonStringField(Payload, TEXT("curvePath"));
    CurvePath = CurvePath.IsEmpty() ? FString::Printf(TEXT("/Game/Environment/Curves/%s"), *DefaultName)
                                    : FPackageName::ObjectPathToPackageName(CurvePath);
    const FString Name = FPackageName::GetShortName(CurvePath);
    FString PackagePath;
    if (!McpBuildValidatedEnvironmentAssetPath(FPackageName::GetLongPackagePath(CurvePath), Name, TEXT("color curve"),
                                               PackagePath, OutMessage, OutErrorCode))
    {
        return false;
    }

    TArray<TPair<float, FLinearColor>> Keys;
    const TArray<TSharedPtr<FJsonValue>> *KeyValues = nullptr;
    const bool bHasKeys = Payload->TryGetArrayField(TEXT("keys"), KeyValues) && KeyValues;
    if (bHasKeys && !McpReadColorCurveKeys(*KeyValues, Keys))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("keys must be a non-empty array of {time, color {r, g, b, a}}"), TEXT("INVALID_ARGUMENT"));
    }

    UCurveLinearColor *Curve = LoadObject<UCurveLinearColor>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *Name), nullptr, LOAD_NoWarn);
    const bool bCreated = Curve == nullptr;
    if (!bCreated && !bHasKeys)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("%s exists; pass keys to change its colors"), *PackagePath), TEXT("INVALID_ARGUMENT"));
    }
    if (bCreated)
    {
        if (UEditorAssetLibrary::DoesAssetExist(PackagePath))
        {
            return McpFailEnvironmentAction(OutMessage, OutErrorCode,
                FString::Printf(TEXT("%s exists and is not a linear color curve"), *PackagePath), TEXT("INVALID_ARGUMENT"));
        }
        UPackage *Package = CreatePackage(*PackagePath);
        Curve = Package ? NewObject<UCurveLinearColor>(Package, FName(*Name), RF_Public | RF_Standalone) : nullptr;
        if (!Curve)
        {
            return McpFailEnvironmentAction(OutMessage, OutErrorCode,
                TEXT("Failed to create color curve"), TEXT("CREATION_FAILED"));
        }
        FAssetRegistryModule::AssetCreated(Curve);
        if (Keys.Num() == 0)
        {
            Keys.Emplace(0.0f, FLinearColor::White);
        }
    }

    Curve->Modify();
    if (Keys.Num() > 0)
    {
        for (int32 Channel = 0; Channel < 4; ++Channel)
        {
            Curve->FloatCurves[Channel].Reset();
        }
        for (const TPair<float, FLinearColor> &Key : Keys)
        {
            Curve->FloatCurves[0].UpdateOrAddKey(Key.Key, Key.Value.R);
            Curve->FloatCurves[1].UpdateOrAddKey(Key.Key, Key.Value.G);
            Curve->FloatCurves[2].UpdateOrAddKey(Key.Key, Key.Value.B);
            Curve->FloatCurves[3].UpdateOrAddKey(Key.Key, Key.Value.A);
        }
    }
    Curve->MarkPackageDirty();
    if (!McpSafeAssetSave(Curve))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to save color curve"), TEXT("SAVE_FAILED"));
    }

    Resp->SetStringField(TEXT("curvePath"), Curve->GetPathName());
    Resp->SetBoolField(TEXT("created"), bCreated);
    Resp->SetNumberField(TEXT("keyCount"), Curve->FloatCurves[0].GetNumKeys());
    McpHandlerUtils::AddVerification(Resp, Curve);
    OutMessage = bCreated ? TEXT("Color curve created") : TEXT("Color curve updated");
    return true;
}

} // namespace McpEnvironmentHandlers
