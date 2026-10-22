#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Misc/SecureHash.h"

namespace McpPropertyReflection
{
namespace
{
bool AppendExactNumbers(const FProperty* Property, void* Value, FArchive& Writer, int32 Depth)
{
    if (Depth > 32) return false;
    if (Property->IsA<FNumericProperty>() || Property->IsA<FBoolProperty>() || Property->IsA<FEnumProperty>())
    {
        Property->SerializeItem(FStructuredArchiveFromArchive(Writer).GetSlot(), Value, nullptr);
        return !Writer.IsError();
    }
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
        {
            if (!AppendExactNumbers(*It, It->ContainerPtrToValuePtr<void>(Value), Writer, Depth + 1)) return false;
        }
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(Array, Value);
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            if (!AppendExactNumbers(Array->Inner, Helper.GetRawPtr(Index), Writer, Depth + 1)) return false;
        }
    }
    else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Helper(Map, Value);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (!Helper.IsValidIndex(Index)) continue;
            if (!AppendExactNumbers(Map->KeyProp, Helper.GetKeyPtr(Index), Writer, Depth + 1) ||
                !AppendExactNumbers(Map->ValueProp, Helper.GetValuePtr(Index), Writer, Depth + 1)) return false;
        }
    }
    else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Helper(Set, Value);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index) && !AppendExactNumbers(Set->ElementProp, Helper.GetElementPtr(Index), Writer, Depth + 1)) return false;
        }
    }
    return true;
}

bool AppendProperty(const FProperty* Property, void* Container, UObject* Owner, FArchive& Writer, const FString* TextOverride = nullptr)
{
    FString Name = Property->GetName(), Text;
    // Callers may canonicalize presentation-only text; numeric bits remain exact in either case.
    if (TextOverride) Text = *TextOverride;
    else Property->ExportText_InContainer(0, Text, Container, nullptr, Owner, PPF_None);
    Writer << Name;
    Writer << Text;
    for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
    {
        if (!AppendExactNumbers(Property, Property->ContainerPtrToValuePtr<void>(Container, Index), Writer, 0)) return false;
    }
    return !Writer.IsError();
}
FString FinishHash(const TArray<uint8>& Bytes)
{
    FMD5 Digest;
    if (!Bytes.IsEmpty()) Digest.Update(Bytes.GetData(), Bytes.Num());
    uint8 Hash[16];
    Digest.Final(Hash);
    return BytesToHex(Hash, UE_ARRAY_COUNT(Hash));
}
}

FString FingerprintEditableProperties(UObject* Object, const TArray<FName>& AdditionalProperties,
    const TMap<FName, FString>& PropertyTextOverrides)
{
    if (!Object) return FString();
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if ((!It->HasAnyPropertyFlags(CPF_Edit) && !AdditionalProperties.Contains(It->GetFName())) ||
            It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
        if (!AppendProperty(*It, Object, Object, Writer, PropertyTextOverrides.Find(It->GetFName()))) return FString();
    }
    return FinishHash(Bytes);
}

FString FingerprintReflectedValue(const FProperty* Property, void* Container)
{
    if (!Property || !Container) return FString();
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    return AppendProperty(Property, Container, nullptr, Writer) ? FinishHash(Bytes) : FString();
}
}
