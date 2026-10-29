// Copyright (c) 2024 EngineWire Contributors
#include "Domains/ControlActor/Materials/McpColorMaterial.h"

#include "Domains/ControlActor/Materials/McpMaterialBuild.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"

namespace McpColorMaterial
{
namespace
{
const TCHAR* const ColorFolder = TEXT("/Game/Materials/Colors/");
const TCHAR* const ParentPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
const TCHAR* const ColorParameter = TEXT("Color"); // the vector parameter BasicShapeMaterial feeds its base colour
const TCHAR* const GlowBaseName = TEXT("M_ColorGlow"); // base colour Color, emissive Color x Glow
const TCHAR* const GlowParameter = TEXT("Glow");

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

FHashedMaterialParameterInfo NamedParam(const TCHAR* Name)
{
    return FHashedMaterialParameterInfo(FMaterialParameterInfo(FName(Name)));
}

// The glowing colours' parent, made once: base colour Color, emissive Color x Glow (white and 1 unless set).
UMaterialInterface* MakeGlowBase()
{
    UMaterial* Material = McpNewMaterial(ColorFolder, GlowBaseName);
    if (!Material)
    {
        return nullptr;
    }
    UMaterialExpressionVectorParameter* Color =
        McpAddMaterialExpression<UMaterialExpressionVectorParameter>(Material, -500, 0);
    Color->ParameterName = ColorParameter;
    Color->DefaultValue = FLinearColor::White;
    UMaterialExpressionScalarParameter* Glow =
        McpAddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -500, 250);
    Glow->ParameterName = GlowParameter;
    Glow->DefaultValue = 1.0f;
    UMaterialExpressionMultiply* Emissive = McpAddMaterialExpression<UMaterialExpressionMultiply>(Material, -250, 150);
    Color->ConnectExpression(&Emissive->A, 0); // output 0 is RGB
    Glow->ConnectExpression(&Emissive->B, 0);
    Color->ConnectExpression(&MCP_GET_MATERIAL_INPUT(Material, BaseColor), 0);
    Emissive->ConnectExpression(&MCP_GET_MATERIAL_INPUT(Material, EmissiveColor), 0);
    McpSaveNewMaterial(Material);
    return Material;
}

// The instances' parent: the engine's BasicShapeMaterial for a flat colour, the glow material for a glowing one; null
// with Error when it is missing or lacks a parameter the instance sets.
UMaterialInterface* FindParent(bool bGlow, FString& Error)
{
    UMaterialInterface* Parent = bGlow ? McpFindMaterialAsset(ColorFolder, GlowBaseName)
                                       : LoadObject<UMaterialInterface>(nullptr, ParentPath);
    Parent = Parent || !bGlow ? Parent : MakeGlowBase();
    FLinearColor Color;
    float Glow = 0.0f;
    if (!Parent || !Parent->GetVectorParameterValue(NamedParam(ColorParameter), Color) ||
        (bGlow && !Parent->GetScalarParameterValue(NamedParam(GlowParameter), Glow)))
    {
        Error = bGlow
            ? FString::Printf(TEXT("the glow material %s%s could not be made or has no Color and Glow parameters"),
                              ColorFolder, GlowBaseName)
            : TEXT("the engine's /Engine/BasicShapes/BasicShapeMaterial with its Color parameter did not load");
        return nullptr;
    }
    return Parent;
}

// The instance Name of Parent with this colour (and glow strength in tenths, 0 for none), made and saved.
UMaterialInterface* MakeInstance(UMaterialInterface* Parent, const FString& Name, const FColor& Color,
                                 int32 GlowTenths, FString& Error)
{
    UMaterialInstanceConstant* Instance = McpNewMaterialInstance(Parent, ColorFolder, Name);
    if (!Instance)
    {
        Error = FString::Printf(TEXT("could not make the material instance %s%s"), ColorFolder, *Name);
        return nullptr;
    }
    Instance->SetVectorParameterValueEditorOnly(FName(ColorParameter), FLinearColor(Color)); // sRGB to linear
    if (GlowTenths > 0)
    {
        Instance->SetScalarParameterValueEditorOnly(FName(GlowParameter), GlowTenths / 10.0f);
    }
    McpSaveNewMaterial(Instance);
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
    FString Text = TEXT("white"); // a glow with no colour glows white
    double Glow = 0.0;
    const bool bColor = Payload.IsValid() && Payload->TryGetStringField(TEXT("color"), Text);
    const bool bGlow = Payload.IsValid() && Payload->TryGetNumberField(TEXT("glow"), Glow);
    if (!bColor && !bGlow)
    {
        return true;
    }
    ErrorCode = TEXT("INVALID_ARGUMENT");
    if (Payload->HasField(TEXT("materialPath")) || Payload->HasField(TEXT("materials")))
    {
        Error = TEXT("color and glow take the place of materialPath and materials: send one of them");
        return false;
    }
    // The glow is absolute brightness: under a physically bright sun it takes tens of thousands to show.
    if (bGlow && !(Glow >= 0.1 && Glow <= 100000.0))
    {
        Error = TEXT("glow is a strength from 0.1 to 100000");
        return false;
    }
    const int32 GlowTenths = bGlow ? FMath::RoundToInt(Glow * 10.0) : 0;
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
    const FString Hex = FString::Printf(TEXT("%02X%02X%02X"), Color.R, Color.G, Color.B);
    const FString Name = bGlow ? FString::Printf(TEXT("MI_Glow_%s_G%d"), *Hex, GlowTenths) : TEXT("MI_Color_") + Hex;
    UMaterialInterface* Material = McpFindMaterialAsset(ColorFolder, Name);
    if (!Material)
    {
        UMaterialInterface* Parent = FindParent(bGlow, Error);
        Material = Parent ? MakeInstance(Parent, Name, Color, GlowTenths, Error) : nullptr;
    }
    if (!Material)
    {
        ErrorCode = TEXT("CREATE_FAILED");
        return false;
    }
    OutPayload = MakeShared<FJsonObject>();
    OutPayload->Values = Payload->Values;
    OutPayload->RemoveField(TEXT("color"));
    OutPayload->RemoveField(TEXT("glow"));
    OutPayload->SetStringField(TEXT("materialPath"), Material->GetPathName());
    return true;
}
} // namespace McpColorMaterial
