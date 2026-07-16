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
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

bool ResolveBaseType(
    const FString& Token,
    FEdGraphPinType& OutPin,
    const FName& InSelfStructPath,
    FString& OutError)
{
    const FString Lower = Token.ToLower();

    if (Lower == TEXT("bool") || Lower == TEXT("boolean"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Boolean; return true; }
    if (Lower == TEXT("byte") || Lower == TEXT("uint8"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Byte; return true; }
    if (Lower == TEXT("int") || Lower == TEXT("int32") || Lower == TEXT("integer"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Int; return true; }
    if (Lower == TEXT("int64"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Int64; return true; }
    if (Lower == TEXT("float"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPin.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        return true;
    }
    if (Lower == TEXT("double") || Lower == TEXT("real"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPin.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        return true;
    }
    if (Lower == TEXT("string") || Lower == TEXT("fstring"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_String; return true; }
    if (Lower == TEXT("name") || Lower == TEXT("fname"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Name; return true; }
    if (Lower == TEXT("text") || Lower == TEXT("ftext"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Text; return true; }

    if (Lower == TEXT("vector") || Lower == TEXT("fvector"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        return true;
    }
    if (Lower == TEXT("vector2d") || Lower == TEXT("fvector2d"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
        return true;
    }
    if (Lower == TEXT("vector4") || Lower == TEXT("fvector4"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FVector4>::Get();
        return true;
    }
    if (Lower == TEXT("rotator") || Lower == TEXT("frotator"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
        return true;
    }
    if (Lower == TEXT("transform"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
        return true;
    }
    if (Lower == TEXT("color") || Lower == TEXT("fcolor"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FColor>::Get();
        return true;
    }
    if (Lower == TEXT("linearcolor") || Lower == TEXT("flinearcolor"))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
        return true;
    }

    if (Lower.StartsWith(TEXT("softobject:")))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        const FString Sub = Token.Mid(11);
        if (!Sub.IsEmpty())
        {
            if (UClass* ClassResolve = ResolveClassByName(Sub))
            { OutPin.PinSubCategoryObject = ClassResolve; }
            else
            { OutError = FString::Printf(TEXT("Unresolved soft object class '%s' in '%s'"), *Sub, *Token); return false; }
        }
        else
        { OutPin.PinSubCategoryObject = UObject::StaticClass(); }
        return true;
    }
    if (Lower.StartsWith(TEXT("softclass:")))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        const FString Sub = Token.Mid(10);
        if (!Sub.IsEmpty())
        {
            if (UClass* ClassResolve = ResolveClassByName(Sub))
            { OutPin.PinSubCategoryObject = ClassResolve; }
            else
            { OutError = FString::Printf(TEXT("Unresolved soft class class '%s' in '%s'"), *Sub, *Token); return false; }
        }
        else
        { OutPin.PinSubCategoryObject = UObject::StaticClass(); }
        return true;
    }
    if (Lower.StartsWith(TEXT("object:")))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Object;
        const FString Sub = Token.Mid(7);
        if (!Sub.IsEmpty())
        {
            if (UClass* ClassResolve = ResolveClassByName(Sub))
            { OutPin.PinSubCategoryObject = ClassResolve; }
            else
            { OutError = FString::Printf(TEXT("Unresolved object class '%s' in '%s'"), *Sub, *Token); return false; }
        }
        else
        { OutPin.PinSubCategoryObject = UObject::StaticClass(); }
        return true;
    }
    if (Lower.StartsWith(TEXT("class:")))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Class;
        const FString Sub = Token.Mid(6);
        if (!Sub.IsEmpty())
        {
            if (UClass* ClassResolve = ResolveClassByName(Sub))
            { OutPin.PinSubCategoryObject = ClassResolve; }
            else
            { OutError = FString::Printf(TEXT("Unresolved class '%s' in '%s'"), *Sub, *Token); return false; }
        }
        else
        { OutPin.PinSubCategoryObject = UObject::StaticClass(); }
        return true;
    }
    if (Lower == TEXT("object"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Object; OutPin.PinSubCategoryObject = UObject::StaticClass(); return true; }
    if (Lower == TEXT("class"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Class; OutPin.PinSubCategoryObject = UObject::StaticClass(); return true; }
    if (Lower == TEXT("softobject"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_SoftObject; return true; }
    if (Lower == TEXT("softclass"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_SoftClass; return true; }
    if (Lower == TEXT("interface"))
    { OutPin.PinCategory = UEdGraphSchema_K2::PC_Interface; return true; }

    if (Lower.StartsWith(TEXT("enum:")))
    {
        const FString EnumPath = Token.Mid(5);
        UEnum* Enum = FindObject<UEnum>(nullptr, *EnumPath);
        if (!Enum) { Enum = LoadObject<UEnum>(nullptr, *EnumPath); }
        if (!Enum)
        {
            OutError = FString::Printf(TEXT("Unresolved enum '%s'"), *EnumPath);
            return false;
        }
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Enum;
        OutPin.PinSubCategoryObject = Enum;
        return true;
    }

    if (Lower.StartsWith(TEXT("struct:")))
    {
        FString StructPath = Token.Mid(7);
        if (!StructPath.Contains(TEXT(".")))
        {
            int32 LastSlash = INDEX_NONE;
            if (StructPath.FindLastChar(TEXT('/'), LastSlash))
            {
                StructPath = StructPath + TEXT(".") + StructPath.Mid(LastSlash + 1);
            }
        }

        if (InSelfStructPath != NAME_None && !InSelfStructPath.IsNone())
        {
            if (StructPath.Equals(InSelfStructPath.ToString(), ESearchCase::IgnoreCase))
            {
                OutError = FString::Printf(
                    TEXT("Recursive by-value struct self-reference '%s' is rejected; use Object:%s for a self-referencing member"),
                    *Token, *StructPath);
                return false;
            }
        }

        UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *StructPath);
        if (!Struct) { Struct = LoadObject<UScriptStruct>(nullptr, *StructPath); }
        if (!Struct)
        {
            OutError = FString::Printf(TEXT("Unresolved struct '%s'"), *StructPath);
            return false;
        }
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPin.PinSubCategoryObject = Struct;
        return true;
    }

    if (Token.Contains(TEXT("/Script/")))
    {
        if (UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, *Token))
        {
            OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
            OutPin.PinSubCategoryObject = Struct;
            return true;
        }
    }
    {
        FString StructName = Token;
        if (StructName.StartsWith(TEXT("F"))) { StructName = StructName.Mid(1); }
        if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *StructName))
        {
            OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
            OutPin.PinSubCategoryObject = Struct;
            return true;
        }
        for (TObjectIterator<UScriptStruct> It; It; ++It)
        {
            if (It->GetName().Equals(StructName, ESearchCase::IgnoreCase))
            {
                OutPin.PinCategory = UEdGraphSchema_K2::PC_Struct;
                OutPin.PinSubCategoryObject = *It;
                return true;
            }
        }
    }
    if (UEnum* Enum = FindObject<UEnum>(nullptr, *Token))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Enum;
        OutPin.PinSubCategoryObject = Enum;
        return true;
    }
    if (UClass* ClassResolve = ResolveClassByName(Token))
    {
        OutPin.PinCategory = UEdGraphSchema_K2::PC_Object;
        OutPin.PinSubCategoryObject = ClassResolve;
        return true;
    }

    OutError = FString::Printf(TEXT("Unknown type '%s'"), *Token);
    return false;
}

}

#endif
