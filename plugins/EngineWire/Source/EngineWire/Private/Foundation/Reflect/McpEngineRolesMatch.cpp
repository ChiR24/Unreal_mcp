// McpEngineRolesMatch.cpp — see header.
#include "Foundation/Reflect/McpEngineRolesMatch.h"

#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
bool AllWordsIn(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Field, const FString& Name)
{
    const TArray<TSharedPtr<FJsonValue>>* Wanted = nullptr;
    if (!Spec->TryGetArrayField(Field, Wanted) || !Wanted || Wanted->Num() == 0)
    {
        return false;
    }
    const FString Lower = Name.ToLower();
    for (const TSharedPtr<FJsonValue>& Word : *Wanted)
    {
        if (!Word.IsValid() || !Lower.Contains(Word->AsString().ToLower()))
        {
            return false;
        }
    }
    return true;
}

const UEnum* EnumOf(const FProperty* Property)
{
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        return Enum->GetEnum();
    }
    const FByteProperty* Byte = CastField<FByteProperty>(Property);
    return Byte ? Byte->GetIntPropertyEnum() : nullptr;
}
}

namespace McpEngineRolesMatch
{
FString CppName(const FProperty* Property)
{
    if (!Property)
    {
        return FString();
    }
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        return FString::Printf(TEXT("TArray<%s>"), *CppName(Array->Inner));
    }
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        return Struct->Struct ? FString(TEXT("F")) + Struct->Struct->GetName() : FString();
    }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        return Object->PropertyClass ? FString(Object->PropertyClass->GetPrefixCPP()) + Object->PropertyClass->GetName() : FString();
    }
    if (const UEnum* Enum = EnumOf(Property))
    {
        return Enum->GetName();
    }
    FString Type = Property->GetCPPType();
    Type.ReplaceInline(TEXT(" "), TEXT(""));
    return Type;
}

bool Fits(const TSharedPtr<FJsonObject>& Spec, const FProperty* Property, TFunctionRef<UStruct*(FName)> RoleType)
{
    if (!Spec.IsValid())
    {
        return true;
    }
    if (!Property)
    {
        return false;
    }
    const TSharedPtr<FJsonObject>* Inner = nullptr;
    if (Spec->TryGetObjectField(TEXT("array"), Inner) && Inner)
    {
        const FArrayProperty* Array = CastField<FArrayProperty>(Property);
        return Array && Fits(*Inner, Array->Inner, RoleType);
    }
    FString Text;
    if (Spec->TryGetStringField(TEXT("cpp"), Text))
    {
        return CppName(Property) == Text;
    }
    if (Spec->TryGetStringField(TEXT("role"), Text))
    {
        const UStruct* Wanted = RoleType(FName(*Text));
        const FStructProperty* Struct = CastField<FStructProperty>(Property);
        const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property);
        // static_cast: these members are raw pointers on some engines and TObjectPtr on others.
        return Wanted && ((Struct && static_cast<const UStruct*>(Struct->Struct) == Wanted) ||
                          (Object && static_cast<const UStruct*>(Object->PropertyClass) == Wanted));
    }
    if (Spec->HasField(TEXT("enumWords")))
    {
        const UEnum* Enum = EnumOf(Property);
        return Enum && AllWordsIn(Spec, TEXT("enumWords"), Enum->GetName());
    }
    if (Spec->HasField(TEXT("structWords")))
    {
        const FStructProperty* Struct = CastField<FStructProperty>(Property);
        return Struct && Struct->Struct && AllWordsIn(Spec, TEXT("structWords"), Struct->Struct->GetName());
    }
    const TArray<TSharedPtr<FJsonValue>>* Any = nullptr;
    if (Spec->TryGetArrayField(TEXT("anyOf"), Any) && Any)
    {
        for (const TSharedPtr<FJsonValue>& Each : *Any)
        {
            const TSharedPtr<FJsonObject>* Option = nullptr;
            if (Each.IsValid() && Each->TryGetObject(Option) && Option && Fits(*Option, Property, RoleType))
            {
                return true;
            }
        }
    }
    return false; // an unknown spec fits nothing: fail closed
}

TArray<FString> Words(const FString& Name)
{
    FString Source = Name;
    if (Source.Len() > 1 && Source[0] == TEXT('b') && FChar::IsUpper(Source[1]))
    {
        Source.RightChopInline(1);
    }
    TArray<FString> Out;
    FString Word;
    for (int32 Index = 0; Index < Source.Len(); ++Index)
    {
        const TCHAR Char = Source[Index];
        const bool bBreak = Char == TEXT('_') ||
            (FChar::IsUpper(Char) && Index > 0 && (FChar::IsLower(Source[Index - 1]) || FChar::IsDigit(Source[Index - 1])));
        if (bBreak && !Word.IsEmpty())
        {
            Out.Add(Word.ToLower());
            Word.Reset();
        }
        if (Char != TEXT('_'))
        {
            Word.AppendChar(Char);
        }
    }
    if (!Word.IsEmpty())
    {
        Out.Add(Word.ToLower());
    }
    return Out;
}

float Overlap(const FString& Name, const TArray<FString>& Want)
{
    if (Want.Num() == 0)
    {
        return 0.f;
    }
    const TArray<FString> Have = Words(Name);
    int32 Shared = 0;
    for (const FString& Word : Want)
    {
        Shared += Have.Contains(Word) ? 1 : 0;
    }
    return static_cast<float>(Shared) / static_cast<float>(Want.Num());
}
}
