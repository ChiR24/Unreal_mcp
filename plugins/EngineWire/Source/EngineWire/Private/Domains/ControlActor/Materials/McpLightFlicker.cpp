// Copyright (c) 2024 EngineWire Contributors
#include "Domains/ControlActor/Materials/McpLightFlicker.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/LightComponent.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/Package.h"
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif

namespace McpLightFlicker
{
namespace
{
const TCHAR* const Folder = TEXT("/Game/Materials/Lights/");
const TCHAR* const BaseName = TEXT("MLF_Flicker");
const TCHAR* const InstancePrefix = TEXT("MI_LightFlicker_");

template <typename T>
T* AddExpression(UMaterial* Material, int32 X, int32 Y)
{
    T* Expression = NewObject<T>(Material, NAME_None, RF_Transactional);
    Expression->MaterialExpressionEditorX = X;
    Expression->MaterialExpressionEditorY = Y;
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(Expression);
    return Expression;
}

UMaterialExpressionMultiply* Multiply(UMaterial* Material, UMaterialExpression* A, UMaterialExpression* B, float ConstB,
                                      int32 X, int32 Y)
{
    UMaterialExpressionMultiply* Node = AddExpression<UMaterialExpressionMultiply>(Material, X, Y);
    Node->A.Expression = A;
    Node->B.Expression = B;
    Node->ConstB = ConstB;
    return Node;
}

// brightness = 1 - Depth / 2 + Depth / 2 * sin(2 pi t) sin(2 pi 2.31 t), t = Time * FlickerSpeed: two sines beating
// against each other, between 1 - FlickerDepth and 1, so the light dips at uneven moments.
UMaterial* MakeBase(FString& Error)
{
    const FString Name = BaseName;
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UPackage* Package = CreatePackage(*(FString(Folder) + Name));
    UMaterial* Material = Package ? Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn)) : nullptr;
    if (!Material)
    {
        Error = FString::Printf(TEXT("could not make the light function material %s%s"), Folder, *Name);
        return nullptr;
    }
    Material->MaterialDomain = MD_LightFunction;
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(Material, -1200, 0);
    UMaterialExpressionScalarParameter* Speed = AddExpression<UMaterialExpressionScalarParameter>(Material, -1200, 120);
    Speed->ParameterName = TEXT("FlickerSpeed");
    Speed->DefaultValue = 8.0f;
    UMaterialExpressionScalarParameter* Depth = AddExpression<UMaterialExpressionScalarParameter>(Material, -1200, 320);
    Depth->ParameterName = TEXT("FlickerDepth");
    Depth->DefaultValue = 0.6f;
    UMaterialExpressionMultiply* T = Multiply(Material, Time, Speed, 1.0f, -1000, 40);
    UMaterialExpressionSine* First = AddExpression<UMaterialExpressionSine>(Material, -800, 0);
    First->Input.Expression = T;
    UMaterialExpressionSine* Second = AddExpression<UMaterialExpressionSine>(Material, -600, 120);
    Second->Input.Expression = Multiply(Material, T, nullptr, 2.31f, -800, 120);
    UMaterialExpressionMultiply* Half = Multiply(Material, Depth, nullptr, 0.5f, -1000, 320);
    UMaterialExpressionOneMinus* Floor = AddExpression<UMaterialExpressionOneMinus>(Material, -600, 320);
    Floor->Input.Expression = Half;
    UMaterialExpressionMultiply* Swing =
        Multiply(Material, Half, Multiply(Material, First, Second, 1.0f, -400, 60), 1.0f, -200, 200);
    UMaterialExpressionAdd* Brightness = AddExpression<UMaterialExpressionAdd>(Material, 0, 200);
    Brightness->A.Expression = Floor;
    Brightness->B.Expression = Swing;
    MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = Brightness;
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Material);
    McpSafeAssetSave(Material);
    return Material;
}

UMaterialInterface* FindAsset(const FString& Name)
{
    const FString PackagePath = FString(Folder) + Name;
    const FString ObjectPath = PackagePath + TEXT(".") + Name;
    UMaterialInterface* Found = FindObject<UMaterialInterface>(nullptr, *ObjectPath);
    return Found ? Found
                 : FPackageName::DoesPackageExist(PackagePath) ? LoadObject<UMaterialInterface>(nullptr, *ObjectPath)
                                                                : nullptr;
}

// The instance for this speed and depth, made (with the base material) when missing.
UMaterialInterface* FindOrMakeInstance(double Speed, double Depth, FString& Error)
{
    const FString Name = FString::Printf(TEXT("%sS%d_D%d"), InstancePrefix, FMath::RoundToInt(Speed * 10.0),
                                         FMath::RoundToInt(Depth * 100.0));
    if (UMaterialInterface* Existing = FindAsset(Name))
    {
        return Existing;
    }
    UMaterialInterface* Base = FindAsset(BaseName);
    Base = Base ? Base : MakeBase(Error);
    if (!Base)
    {
        return nullptr;
    }
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Base;
    UPackage* Package = CreatePackage(*(FString(Folder) + Name));
    UMaterialInstanceConstant* Instance = Package ? Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
        UMaterialInstanceConstant::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn))
                                                  : nullptr;
    if (!Instance)
    {
        Error = FString::Printf(TEXT("could not make the material instance %s%s"), Folder, *Name);
        return nullptr;
    }
    Instance->SetScalarParameterValueEditorOnly(FName(TEXT("FlickerSpeed")), static_cast<float>(Speed));
    Instance->SetScalarParameterValueEditorOnly(FName(TEXT("FlickerDepth")), static_cast<float>(Depth));
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Instance);
    McpSafeAssetSave(Instance);
    return Instance;
}

bool IsOurs(const UMaterialInterface* Material)
{
    return Material && Material->GetPathName().StartsWith(FString(Folder) + InstancePrefix);
}
} // namespace

bool Handle(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    const bool bOn = GetJsonBoolField(Payload, TEXT("enabled"), true);
    const double Speed = GetJsonNumberField(Payload, TEXT("speed"), 8.0);
    const double Depth = GetJsonNumberField(Payload, TEXT("depth"), 0.6);
    AActor* Actor = ActorName.IsEmpty() ? nullptr : Find(ActorName);
    if (!Actor)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Actor not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    TArray<ULightComponent*> Lights;
    Actor->GetComponents<ULightComponent>(Lights);
    Lights.RemoveAll([&ComponentName](const ULightComponent* Light)
                     { return !ComponentName.IsEmpty() && !Light->GetName().Equals(ComponentName, ESearchCase::IgnoreCase); });
    if (Lights.Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId,
                                    ComponentName.IsEmpty() ? FString(TEXT("The actor has no light component"))
                                                            : FString::Printf(TEXT("The actor has no light component named %s"), *ComponentName),
                                    TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }
    for (const ULightComponent* Light : Lights)
    {
        if (bOn && Light->Mobility == EComponentMobility::Static)
        {
            // A Static light is baked: a light function never reaches it.
            Bridge->SendAutomationError(Socket, RequestId,
                                        FString::Printf(TEXT("%s is Static, and a baked light cannot flicker: make it Movable first "
                                                             "(edit_component set_property, propertyName Mobility, value Movable)"),
                                                        *Light->GetName()),
                                        TEXT("LIGHT_IS_STATIC"));
            return true;
        }
    }
    FString Error;
    UMaterialInterface* Flicker = bOn ? FindOrMakeInstance(Speed, Depth, Error) : nullptr;
    if (bOn && !Flicker)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("CREATE_FAILED"));
        return true;
    }
    // Off takes away only a flicker this made: a light function of the project's own stays.
    Lights.RemoveAll([bOn, Flicker](const ULightComponent* Light)
                     { return Light->LightFunctionMaterial == Flicker || (!bOn && !IsOurs(Light->LightFunctionMaterial)); });
    TArray<TSharedPtr<FJsonValue>> Changed;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (Lights.Num() > 0)
    {
        // Only when something changes: the transaction Modifies what it is given, and a no-op left the level unsaved.
        TArray<UObject*> Undoable{Actor};
        for (ULightComponent* Light : Lights)
        {
            Light->SetFlags(RF_Transactional);
            Undoable.Add(Light);
        }
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Light Flicker")),
                                                EMcpMutationDurability::EditorStateOnly, Undoable);
        for (ULightComponent* Light : Lights)
        {
            Light->Modify();
            Light->SetLightFunctionMaterial(Flicker);
            Changed.Add(MakeShared<FJsonValueString>(Light->GetName()));
        }
        Transaction.DescribeInto(Result);
        Actor->MarkPackageDirty();
    }
    // Low shadow quality turns light functions off: a check of the light at that scalability sees it steady.
    const IConsoleVariable* Quality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LightFunctionQuality"));
    if (bOn && Quality && Quality->GetInt() == 0)
    {
        Result->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray({TEXT(
            "Light functions are off at this scalability (r.LightFunctionQuality 0, Low shadow quality), so the editor draws "
            "the light steady; the flicker shows from Medium shadow quality up.")}));
    }
    Result->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Result->SetArrayField(TEXT("changed"), Changed);
    if (Flicker)
    {
        Result->SetStringField(TEXT("materialPath"), Flicker->GetPathName());
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   FString::Printf(TEXT("%s on %d light component(s)"),
                                                   bOn ? TEXT("Flicker set") : TEXT("Flicker removed"), Changed.Num()),
                                   Result);
    return true;
}
} // namespace McpLightFlicker
