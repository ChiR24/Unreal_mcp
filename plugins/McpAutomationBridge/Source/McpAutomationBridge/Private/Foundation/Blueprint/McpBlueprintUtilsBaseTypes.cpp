#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "EditorAssetLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Color.h"


namespace McpBlueprintUtils
{
namespace
{
using K2 = UEdGraphSchema_K2;

FEdGraphPinType MakePin(const FName Category, const FName SubCategory = NAME_None, UObject* SubObject = nullptr)
{
    FEdGraphPinType Pin;
    Pin.PinCategory = Category;
    Pin.PinSubCategory = SubCategory;
    Pin.PinSubCategoryObject = SubObject;
    return Pin;
}

// Lower-cased spelling -> pin type, for every type that needs no lookup.
const TMap<FString, FEdGraphPinType>& SimpleTypes()
{
    static const TMap<FString, FEdGraphPinType> Types = []
    {
        TMap<FString, FEdGraphPinType> Map;
        auto Add = [&Map](std::initializer_list<const TCHAR*> Names, const FEdGraphPinType& Pin)
        {
            for (const TCHAR* Name : Names) { Map.Add(Name, Pin); }
        };
        Add({TEXT("bool"), TEXT("boolean")}, MakePin(K2::PC_Boolean));
        Add({TEXT("byte"), TEXT("uint8")}, MakePin(K2::PC_Byte));
        Add({TEXT("int"), TEXT("int32"), TEXT("integer")}, MakePin(K2::PC_Int));
        Add({TEXT("int64")}, MakePin(K2::PC_Int64));
        Add({TEXT("float")}, MakePin(K2::PC_Real, K2::PC_Float));
        Add({TEXT("double"), TEXT("real")}, MakePin(K2::PC_Real, K2::PC_Double));
        Add({TEXT("string"), TEXT("fstring")}, MakePin(K2::PC_String));
        Add({TEXT("name"), TEXT("fname")}, MakePin(K2::PC_Name));
        Add({TEXT("text"), TEXT("ftext")}, MakePin(K2::PC_Text));
        Add({TEXT("vector"), TEXT("fvector")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FVector>::Get()));
        Add({TEXT("vector2d"), TEXT("fvector2d")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FVector2D>::Get()));
        Add({TEXT("vector4"), TEXT("fvector4")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FVector4>::Get()));
        Add({TEXT("rotator"), TEXT("frotator")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FRotator>::Get()));
        Add({TEXT("transform")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FTransform>::Get()));
        Add({TEXT("color"), TEXT("fcolor")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FColor>::Get()));
        Add({TEXT("linearcolor"), TEXT("flinearcolor")}, MakePin(K2::PC_Struct, NAME_None, TBaseStructure<FLinearColor>::Get()));
        Add({TEXT("object")}, MakePin(K2::PC_Object, NAME_None, UObject::StaticClass()));
        Add({TEXT("class")}, MakePin(K2::PC_Class, NAME_None, UObject::StaticClass()));
        Add({TEXT("softobject")}, MakePin(K2::PC_SoftObject));
        Add({TEXT("softclass")}, MakePin(K2::PC_SoftClass));
        Add({TEXT("interface")}, MakePin(K2::PC_Interface));
        return Map;
    }();
    return Types;
}

bool SetStructPin(UScriptStruct* Struct, FEdGraphPinType& OutPin)
{
    if (!Struct) { return false; }
    OutPin = MakePin(K2::PC_Struct, NAME_None, Struct);
    return true;
}
}

bool ResolveBaseType(
    const FString& Token,
    FEdGraphPinType& OutPin,
    const FName& InSelfStructPath,
    FString& OutError)
{
    const FString Lower = Token.ToLower();
    if (const FEdGraphPinType* Simple = SimpleTypes().Find(Lower))
    {
        OutPin = *Simple;
        return true;
    }

    // Category:ClassName; an empty class name means UObject.
    struct FClassPrefix { const TCHAR* Prefix; FName Category; };
    const FClassPrefix ClassPrefixes[] = {
        {TEXT("softobject:"), K2::PC_SoftObject}, {TEXT("softclass:"), K2::PC_SoftClass},
        {TEXT("object:"), K2::PC_Object}, {TEXT("class:"), K2::PC_Class}};
    for (const FClassPrefix& Entry : ClassPrefixes)
    {
        if (!Lower.StartsWith(Entry.Prefix)) { continue; }
        const FString Sub = Token.Mid(FCString::Strlen(Entry.Prefix));
        UClass* Class = Sub.IsEmpty() ? UObject::StaticClass() : ResolveClassByName(Sub);
        if (!Class)
        {
            OutError = FString::Printf(TEXT("Unresolved class '%s' in '%s'"), *Sub, *Token);
            return false;
        }
        OutPin = MakePin(Entry.Category, NAME_None, Class);
        return true;
    }

    if (Lower.StartsWith(TEXT("enum:")))
    {
        const FString EnumPath = Token.Mid(5);
        UEnum* Enum = LoadObject<UEnum>(nullptr, *EnumPath);
        if (!Enum)
        {
            OutError = FString::Printf(TEXT("Unresolved enum '%s'"), *EnumPath);
            return false;
        }
        OutPin = MakePin(K2::PC_Enum, NAME_None, Enum);
        return true;
    }

    if (Lower.StartsWith(TEXT("struct:")))
    {
        FString StructPath = Token.Mid(7);
        int32 LastSlash = INDEX_NONE;
        if (!StructPath.Contains(TEXT(".")) && StructPath.FindLastChar(TEXT('/'), LastSlash))
        {
            StructPath = StructPath + TEXT(".") + StructPath.Mid(LastSlash + 1);
        }
        if (!InSelfStructPath.IsNone() && StructPath.Equals(InSelfStructPath.ToString(), ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("Recursive by-value struct self-reference '%s' is rejected; use Object:%s for a self-referencing member"),
                *Token, *StructPath);
            return false;
        }
        if (!SetStructPin(LoadObject<UScriptStruct>(nullptr, *StructPath), OutPin))
        {
            OutError = FString::Printf(TEXT("Unresolved struct '%s'"), *StructPath);
            return false;
        }
        return true;
    }

    if (Token.Contains(TEXT("/Script/")) && SetStructPin(LoadObject<UScriptStruct>(nullptr, *Token), OutPin))
    {
        return true;
    }
    // A bare struct name, with or without its F prefix.
    const FString StructName = Token.StartsWith(TEXT("F")) ? Token.Mid(1) : Token;
    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        if (It->GetName().Equals(StructName, ESearchCase::IgnoreCase))
        {
            return SetStructPin(*It, OutPin);
        }
    }
    if (UEnum* Enum = FindObject<UEnum>(nullptr, *Token))
    {
        OutPin = MakePin(K2::PC_Enum, NAME_None, Enum);
        return true;
    }
    if (UClass* ClassResolve = ResolveClassByName(Token))
    {
        OutPin = MakePin(K2::PC_Object, NAME_None, ClassResolve);
        return true;
    }

    // The container spellings a C++ author reaches for first - TArray<Text>,
    // Text[] - are not the ones this resolver takes, and a bare "Unknown type"
    // left no way to discover that short of reading the plugin source.
    if (Lower.EndsWith(TEXT("[]")) || Lower.StartsWith(TEXT("tarray<")) ||
        Lower.StartsWith(TEXT("tset<")) || Lower.StartsWith(TEXT("tmap<")))
    {
        OutError = FString::Printf(
            TEXT("Unknown type '%s'. Containers are spelled Array<T>, Set<T> "
                 "and Map<Key,Value> - no leading T, no trailing []."),
            *Token);
        return false;
    }

    OutError = FString::Printf(TEXT("Unknown type '%s'"), *Token);
    return false;
}

}
