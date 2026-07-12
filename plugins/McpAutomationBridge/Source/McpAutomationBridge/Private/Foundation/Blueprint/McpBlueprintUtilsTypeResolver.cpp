#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "EdGraphSchema_K2.h"
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

bool ResolveBaseType(
    const FString& Token,
    FEdGraphPinType& OutPin,
    const FName& InSelfStructPath,
    FString& OutError);

static FTypeResolutionResult ResolvePinTypeRecursive(
    const FString& TypeSpec,
    const FName& InSelfStructPath)
{
    FTypeResolutionResult Result;
    FString Clean = TypeSpec;
    Clean.TrimStartAndEndInline();
    Clean.RemoveFromEnd(TEXT("*"));
    if (Clean.IsEmpty())
    {
        Result.OutError = TEXT("Empty type specification");
        return Result;
    }

    const FString Lower = Clean.ToLower();

    if (Lower.StartsWith(TEXT("array:")))
    {
        FTypeResolutionResult Inner = ResolvePinTypeRecursive(Clean.Mid(6), InSelfStructPath);
        if (!Inner.bSuccess) { Result.OutError = Inner.OutError; return Result; }
        Inner.PinType.ContainerType = EPinContainerType::Array;
        Result.bSuccess = true; Result.PinType = Inner.PinType;
        return Result;
    }
    if (Lower.StartsWith(TEXT("set:")))
    {
        FTypeResolutionResult Inner = ResolvePinTypeRecursive(Clean.Mid(4), InSelfStructPath);
        if (!Inner.bSuccess) { Result.OutError = Inner.OutError; return Result; }
        Inner.PinType.ContainerType = EPinContainerType::Set;
        Result.bSuccess = true; Result.PinType = Inner.PinType;
        return Result;
    }
    if (Lower.StartsWith(TEXT("map:")))
    {
        FString MapInner = Clean.Mid(4);
        FString KeyStr, ValueStr;
        bool bHasComma = false;
        {
            int32 Depth = 0;
            for (int32 i = 0; i < MapInner.Len(); ++i)
            {
                TCHAR Ch = MapInner[i];
                if (Ch == TEXT('<')) { ++Depth; }
                else if (Ch == TEXT('>')) { --Depth; }
                else if (Ch == TEXT(',') && Depth == 0)
                {
                    KeyStr = MapInner.Left(i);
                    ValueStr = MapInner.Mid(i + 1);
                    bHasComma = true;
                    break;
                }
            }
        }
        if (!bHasComma)
        {
            Result.OutError = FString::Printf(
                TEXT("Map type '%s' is missing the key/value comma (expected 'Map:<Key>,<Value>')"), *Clean);
            return Result;
        }
        KeyStr.TrimStartAndEndInline();
        ValueStr.TrimStartAndEndInline();
        if (KeyStr.IsEmpty() || ValueStr.IsEmpty())
        {
            Result.OutError = FString::Printf(
                TEXT("Map type '%s' has an empty key or value half"), *Clean);
            return Result;
        }

        FTypeResolutionResult KeyPin = ResolvePinTypeRecursive(KeyStr, InSelfStructPath);
        if (!KeyPin.bSuccess) { Result.OutError = KeyPin.OutError; return Result; }
        FTypeResolutionResult ValuePin = ResolvePinTypeRecursive(ValueStr, InSelfStructPath);
        if (!ValuePin.bSuccess) { Result.OutError = ValuePin.OutError; return Result; }

        const FName KC = KeyPin.PinType.PinCategory;
        const bool bValidKey = (KC == UEdGraphSchema_K2::PC_Byte ||
                                KC == UEdGraphSchema_K2::PC_Int ||
                                KC == UEdGraphSchema_K2::PC_Int64 ||
                                KC == UEdGraphSchema_K2::PC_Name ||
                                KC == UEdGraphSchema_K2::PC_String);
        if (!bValidKey)
        {
            Result.OutError = FString::Printf(
                TEXT("Unsupported map key type '%s' in '%s' (only Byte, Int, Int64, Name, String and Enum are valid map keys)"),
                *KeyStr, *Clean);
            return Result;
        }

        FEdGraphPinType MapPin;
        MapPin.PinCategory = KeyPin.PinType.PinCategory;
        MapPin.PinSubCategory = KeyPin.PinType.PinSubCategory;
        MapPin.PinSubCategoryObject = KeyPin.PinType.PinSubCategoryObject;
        MapPin.PinValueType.TerminalCategory = ValuePin.PinType.PinCategory;
        MapPin.PinValueType.TerminalSubCategory = ValuePin.PinType.PinSubCategory;
        MapPin.PinValueType.TerminalSubCategoryObject = ValuePin.PinType.PinSubCategoryObject;
        MapPin.ContainerType = EPinContainerType::Map;
        Result.bSuccess = true;
        Result.PinType = MapPin;
        return Result;
    }

    if (Lower.StartsWith(TEXT("array<")) && Clean.EndsWith(TEXT(">")))
    {
        return ResolvePinTypeRecursive(
            FString(TEXT("Array:")) + Clean.Mid(6, Clean.Len() - 7), InSelfStructPath);
    }
    if (Lower.StartsWith(TEXT("set<")) && Clean.EndsWith(TEXT(">")))
    {
        return ResolvePinTypeRecursive(
            FString(TEXT("Set:")) + Clean.Mid(4, Clean.Len() - 5), InSelfStructPath);
    }
    if (Lower.StartsWith(TEXT("map<")) && Clean.EndsWith(TEXT(">")))
    {
        FString Inner = Clean.Mid(4, Clean.Len() - 5);
        return ResolvePinTypeRecursive(
            FString(TEXT("Map:")) + Inner, InSelfStructPath);
    }

    if (!ResolveBaseType(Clean, Result.PinType, InSelfStructPath, Result.OutError))
    {
        Result.PinType = FEdGraphPinType();
    }
    else
    {
        Result.bSuccess = true;
    }
    return Result;
}

FTypeResolutionResult ResolvePinType(
    const FString& TypeSpec,
    const FName& InSelfStructPath)
{
    return ResolvePinTypeRecursive(TypeSpec, InSelfStructPath);
}

FEdGraphPinType MakePinType(const FString& InType)
{
    FTypeResolutionResult Result = ResolvePinType(InType);
    if (!Result.bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("McpBlueprintUtils::MakePinType: %s"), *Result.OutError);
        FEdGraphPinType Wildcard;
        Wildcard.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        return Wildcard;
    }
    return Result.PinType;
}

FString DescribePinType(const FEdGraphPinType& PinType)
{
    FString BaseType = PinType.PinCategory.ToString();

    if (PinType.PinSubCategoryObject.IsValid())
    {
        if (const UObject* SubObj = PinType.PinSubCategoryObject.Get())
        {
            BaseType = SubObj->GetName();
        }
    }
    else if (PinType.PinSubCategory != NAME_None)
    {
        BaseType = PinType.PinSubCategory.ToString();
    }

    FString ContainerWrappedType = BaseType;
    switch (PinType.ContainerType)
    {
    case EPinContainerType::Array:
        ContainerWrappedType = FString::Printf(TEXT("Array<%s>"), *BaseType);
        break;
    case EPinContainerType::Set:
        ContainerWrappedType = FString::Printf(TEXT("Set<%s>"), *BaseType);
        break;
    case EPinContainerType::Map:
    {
        FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
        if (PinType.PinValueType.TerminalSubCategoryObject.IsValid())
        {
            if (const UObject* ValueObj = PinType.PinValueType.TerminalSubCategoryObject.Get())
            {
                ValueType = ValueObj->GetName();
            }
        }
        else if (PinType.PinValueType.TerminalSubCategory != NAME_None)
        {
            ValueType = PinType.PinValueType.TerminalSubCategory.ToString();
        }
        ContainerWrappedType = FString::Printf(TEXT("Map<%s,%s>"), *BaseType, *ValueType);
        break;
    }
    default:
        break;
    }

    return ContainerWrappedType;
}

}

#endif
