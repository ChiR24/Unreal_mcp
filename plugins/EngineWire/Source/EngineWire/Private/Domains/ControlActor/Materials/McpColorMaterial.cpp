// Copyright (c) 2024 EngineWire Contributors
#include "Domains/ControlActor/Materials/McpColorMaterial.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/Package.h"

namespace McpColorMaterial
{
namespace
{
const TCHAR* const ColorFolder = TEXT("/Game/Materials/Colors/");
const TCHAR* const ParentPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
const TCHAR* const ColorParameter = TEXT("Color"); // the vector parameter BasicShapeMaterial feeds its base colour

struct FNamedColor
{
    const TCHAR* Name;
    const TCHAR* Hex;
};
// The names a plain request uses, as the sRGB a colour picker shows. White and black stay inside what a painted
// surface reflects: audit_materials flags a base colour brighter or darker than that.
const FNamedColor NamedColors[] = {
    {TEXT("red"), TEXT("E53935")},    {TEXT("orange"), TEXT("FB8C00")}, {TEXT("yellow"), TEXT("FDD835")},
    {TEXT("green"), TEXT("43A047")},  {TEXT("blue"), TEXT("1E88E5")},   {TEXT("cyan"), TEXT("00ACC1")},
    {TEXT("teal"), TEXT("00897B")},   {TEXT("purple"), TEXT("8E24AA")}, {TEXT("pink"), TEXT("EC407A")},
    {TEXT("magenta"), TEXT("D81B60")}, {TEXT("brown"), TEXT("6D4C41")}, {TEXT("gray"), TEXT("808080")},
    {TEXT("grey"), TEXT("808080")},   {TEXT("white"), TEXT("EEEEEE")},  {TEXT("black"), TEXT("212121")},
};

/** "#rrggbb", "#rgb" (with or without the #) or a name; false for anything else. */
bool ReadColor(const FString& Text, FColor& Out)
{
    FString Hex = Text.TrimStartAndEnd();
    for (const FNamedColor& Named : NamedColors)
    {
        if (Hex.Equals(Named.Name, ESearchCase::IgnoreCase))
        {
            Hex = Named.Hex;
        }
    }
    Hex.RemoveFromStart(TEXT("#"));
    if (Hex.Len() != 3 && Hex.Len() != 6)
    {
        return false;
    }
    for (const TCHAR Digit : Hex)
    {
        if (!FChar::IsHexDigit(Digit))
        {
            return false; // FColor::FromHex reads a stray letter as 0
        }
    }
    Out = FColor::FromHex(Hex);
    return true;
}

UMaterialInterface* MakeInstance(const FString& PackagePath, const FString& Name, const FColor& Color, FString& Error)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, ParentPath);
    FLinearColor Default;
    if (!Parent ||
        !Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(FName(ColorParameter))),
                                         Default))
    {
        Error = TEXT("the engine's /Engine/BasicShapes/BasicShapeMaterial with its Color parameter did not load");
        return nullptr;
    }
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;
    UPackage* Package = CreatePackage(*PackagePath);
    UMaterialInstanceConstant* Instance = Package ? Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
        UMaterialInstanceConstant::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn))
                                                  : nullptr;
    if (!Instance)
    {
        Error = FString::Printf(TEXT("could not make the material instance %s"), *PackagePath);
        return nullptr;
    }
    Instance->SetVectorParameterValueEditorOnly(FName(ColorParameter), FLinearColor(Color)); // sRGB to linear
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Instance);
    McpSafeAssetSave(Instance);
    return Instance;
}

/** Whether actorName, or one of actorNames, names an actor. */
bool AnyTarget(const TSharedPtr<FJsonObject>& Payload, TFunctionRef<AActor*(const FString&)> Find)
{
    FString Name;
    if (Payload->TryGetStringField(TEXT("actorName"), Name) && !Name.IsEmpty() && Find(Name))
    {
        return true;
    }
    const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
    if (Payload->TryGetArrayField(TEXT("actorNames"), Names))
    {
        for (const TSharedPtr<FJsonValue>& Named : *Names)
        {
            if (Named.IsValid() && Find(Named->AsString()))
            {
                return true;
            }
        }
    }
    return false;
}
} // namespace

bool Resolve(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutPayload, FString& ErrorCode,
             FString& Error, TFunctionRef<AActor*(const FString&)> Find)
{
    OutPayload = Payload;
    FString Text;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("color"), Text))
    {
        return true;
    }
    ErrorCode = TEXT("INVALID_ARGUMENT");
    if (Payload->HasField(TEXT("materialPath")) || Payload->HasField(TEXT("materials")))
    {
        Error = TEXT("color takes the place of materialPath and materials: send one of them");
        return false;
    }
    FColor Color;
    if (!ReadColor(Text, Color))
    {
        TArray<FString> Names;
        for (const FNamedColor& Named : NamedColors)
        {
            Names.Add(Named.Name);
        }
        Error = FString::Printf(TEXT("color \"%s\" is not #rrggbb, #rgb or one of %s"), *Text,
                                *FString::Join(Names, TEXT(", ")));
        return false;
    }
    if (!AnyTarget(Payload, Find))
    {
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
        Error = TEXT("Actor not found");
        return false;
    }
    const FString Name = FString::Printf(TEXT("MI_Color_%02X%02X%02X"), Color.R, Color.G, Color.B);
    const FString PackagePath = FString(ColorFolder) + Name;
    const FString ObjectPath = PackagePath + TEXT(".") + Name;
    UMaterialInterface* Material = FindObject<UMaterialInterface>(nullptr, *ObjectPath);
    if (!Material && FPackageName::DoesPackageExist(PackagePath))
    {
        Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
    }
    if (!Material)
    {
        Material = MakeInstance(PackagePath, Name, Color, Error);
    }
    if (!Material)
    {
        ErrorCode = TEXT("CREATE_FAILED");
        return false;
    }
    OutPayload = MakeShared<FJsonObject>();
    OutPayload->Values = Payload->Values;
    OutPayload->RemoveField(TEXT("color"));
    OutPayload->SetStringField(TEXT("materialPath"), Material->GetPathName());
    return true;
}
} // namespace McpColorMaterial
