#include "Domains/Play/McpFuzz.h"

#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace McpFuzz
{
namespace FuzzValueParts
{
constexpr int32 MaxEnumValues = 8;

FValue& Add(TArray<FValue>& Out, const TSharedPtr<FJsonValue>& Shown)
{
    FValue& Value = Out.AddDefaulted_GetRef();
    Value.Shown = Shown;
    return Value;
}

const UEnum* EnumOf(const FProperty* Property)
{
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        return Enum->GetEnum();
    }
    if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
    {
        return Byte->Enum;
    }
    return nullptr;
}

// A class value takes the class the parameter names; an object one the target itself when it fits, else the first actor
// of that class in the running game (or the target's first component of it), else none at all.
UObject* FirstObject(const FObjectPropertyBase* Property, UWorld* World, AActor* Target)
{
    if (const FClassProperty* Class = CastField<FClassProperty>(Property))
    {
        return Class->MetaClass;
    }
    if (const FSoftClassProperty* SoftClass = CastField<FSoftClassProperty>(Property))
    {
        return SoftClass->MetaClass;
    }
    UClass* Want = Property->PropertyClass;
    if (!Want)
    {
        return nullptr;
    }
    if (Target && Target->IsA(Want))
    {
        return Target;
    }
    if (Want->IsChildOf(AActor::StaticClass()))
    {
        TActorIterator<AActor> It(World, Want);
        return It ? *It : nullptr;
    }
    return Target && Want->IsChildOf(UActorComponent::StaticClass()) ? Target->FindComponentByClass(Want) : nullptr;
}

TSharedPtr<FJsonValue> ShowVector(double Each, bool bThree)
{
    TSharedPtr<FJsonObject> Shown = MakeShared<FJsonObject>();
    Shown->SetNumberField(TEXT("x"), Each);
    Shown->SetNumberField(TEXT("y"), Each);
    if (bThree)
    {
        Shown->SetNumberField(TEXT("z"), Each);
    }
    return MakeShared<FJsonValueObject>(Shown);
}
} // namespace FuzzValueParts

using namespace FuzzValueParts;

TArray<FValue> ValuesFor(const FProperty* Property, UWorld* World, AActor* Target)
{
    TArray<FValue> Out;
    const auto Numbers = [&Out](std::initializer_list<double> List)
    {
        for (const double Number : List)
        {
            Add(Out, MakeShared<FJsonValueNumber>(Number)).Number = Number;
        }
    };
    if (CastField<FBoolProperty>(Property))
    {
        Add(Out, MakeShared<FJsonValueBoolean>(false));
        Add(Out, MakeShared<FJsonValueBoolean>(true)).bOn = true;
    }
    else if (const UEnum* Enum = EnumOf(Property))
    {
        // The engine's hidden _MAX entry is no value a Blueprint can hold.
        const int32 Last = Enum->NumEnums() - (Enum->ContainsExistingMax() ? 2 : 1);
        for (int32 Index = 0; Index <= Last && Out.Num() < MaxEnumValues; ++Index)
        {
            Add(Out, MakeShared<FJsonValueString>(Enum->GetDisplayNameTextByIndex(Index).ToString())).Number =
                static_cast<double>(Enum->GetValueByIndex(Index));
        }
    }
    else if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
    {
        if (Numeric->IsFloatingPoint())
        {
            Numbers({1.0, 0.0, -1.0, 1e9, -1e9});
        }
        else if (CastField<FByteProperty>(Property))
        {
            Numbers({1.0, 0.0, 255.0});
        }
        else
        {
            Numbers({1.0, 0.0, -1.0, 2147483647.0, -2147483648.0});
        }
    }
    else if (CastField<FStrProperty>(Property) || CastField<FNameProperty>(Property) || CastField<FTextProperty>(Property))
    {
        for (const FString& Text : {FString(TEXT("A")), FString(), FString::ChrN(256, TEXT('x'))})
        {
            Add(Out, MakeShared<FJsonValueString>(Text)).Text = Text;
        }
    }
    else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        if (UObject* First = FirstObject(Object, World, Target))
        {
            const AActor* Actor = Cast<AActor>(First);
            Add(Out, MakeShared<FJsonValueString>(Actor ? Actor->GetActorNameOrLabel() : First->GetName())).Object = First;
        }
        Add(Out, MakeShared<FJsonValueNull>());
    }
    else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        const bool bVector = Struct->Struct == TBaseStructure<FVector>::Get();
        if (bVector || Struct->Struct == TBaseStructure<FVector2D>::Get())
        {
            for (const double Each : {100.0, 0.0, 1e9})
            {
                Add(Out, ShowVector(Each, bVector)).Vector = FVector(Each);
            }
        }
    }
    else if (CastField<FArrayProperty>(Property))
    {
        // Three elements, then none: an empty array is where a Get at index 0 or a Last goes wrong.
        Add(Out, MakeShared<FJsonValueString>(TEXT("3 elements"))).Count = 3;
        Add(Out, MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>()));
    }
    if (Out.Num() == 0)
    {
        Add(Out, MakeShared<FJsonValueString>(TEXT("default"))); // a map, a set, a rotator or another struct: its default only
    }
    return Out;
}

void Write(const FProperty* Property, void* Where, const FValue& Value, UWorld* World, AActor* Target)
{
    if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
    {
        Bool->SetPropertyValue(Where, Value.bOn);
    }
    else if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        Enum->GetUnderlyingProperty()->SetIntPropertyValue(Where, static_cast<int64>(Value.Number));
    }
    else if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
    {
        if (Numeric->IsFloatingPoint())
        {
            Numeric->SetFloatingPointPropertyValue(Where, Value.Number);
        }
        else
        {
            Numeric->SetIntPropertyValue(Where, static_cast<int64>(Value.Number));
        }
    }
    else if (const FStrProperty* Str = CastField<FStrProperty>(Property))
    {
        Str->SetPropertyValue(Where, Value.Text);
    }
    else if (const FNameProperty* Name = CastField<FNameProperty>(Property))
    {
        Name->SetPropertyValue(Where, FName(*Value.Text));
    }
    else if (const FTextProperty* Text = CastField<FTextProperty>(Property))
    {
        Text->SetPropertyValue(Where, FText::FromString(Value.Text));
    }
    else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        Object->SetObjectPropertyValue(Where, Value.Object.Get());
    }
    else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        if (Struct->Struct == TBaseStructure<FVector>::Get())
        {
            *static_cast<FVector*>(Where) = Value.Vector;
        }
        else if (Struct->Struct == TBaseStructure<FVector2D>::Get())
        {
            *static_cast<FVector2D*>(Where) = FVector2D(Value.Vector.X, Value.Vector.Y);
        }
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Elements(Array, Where);
        Elements.EmptyValues();
        if (Value.Count > 0)
        {
            Elements.AddValues(Value.Count);
            const FValue First = ValuesFor(Array->Inner, World, Target)[0];
            for (int32 Index = 0; Index < Value.Count; ++Index)
            {
                Write(Array->Inner, Elements.GetRawPtr(Index), First, World, Target);
            }
        }
    }
}
} // namespace McpFuzz
