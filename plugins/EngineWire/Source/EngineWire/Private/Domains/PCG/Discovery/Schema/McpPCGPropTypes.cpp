#include "Domains/PCG/Discovery/Schema/McpPCGPropTypes.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace McpPCGHandlers
{
namespace
{
constexpr int32 PCGPropTypesMaxString = 1024;
constexpr int32 PCGPropTypesMaxEnumOptions = 64;
constexpr int32 PCGPropTypesMaxContainerDepth = 3;
constexpr int64 PCGPropTypesMaxSafeJsonInteger = 9007199254740991LL;

void SetPCGPropTypesInt64(TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64 Value)
{
    if (Value >= -PCGPropTypesMaxSafeJsonInteger && Value <= PCGPropTypesMaxSafeJsonInteger)
    {
        Object->SetNumberField(Field, static_cast<double>(Value));
    }
    else
    {
        Object->SetStringField(Field, LexToString(Value));
    }
}

bool AddPCGPropTypesEnum(const UEnum* Enum, FPCGReadBudget& Budget, TSharedPtr<FJsonObject>& Details, FString& OutError)
{
    if (!Enum) return true;
    const FString EnumPath = Enum->GetPathName();
    if (!SetPCGPropSchemaString(Details, TEXT("enumPath"), EnumPath, OutError)) return false;
    const int32 EntryTotal = Enum->NumEnums();
    bool bMaxSentinel = false;
    if (EntryTotal > 0 && Enum->ContainsExistingMax())
    {
        if (!Budget.IsWithinTime())
        {
            OutError = TEXT("Enum max-sentinel inspection exceeded the 100 ms schema budget.");
            return false;
        }
        const FString LastName = Enum->GetNameStringByIndex(EntryTotal - 1);
        bMaxSentinel = LastName == TEXT("MAX") || LastName.EndsWith(TEXT("_MAX"), ESearchCase::CaseSensitive);
    }
    const int32 MaxOptionTotal = bMaxSentinel ? 1 : 0;
    int32 VisibleTotal = 0;
    int32 HiddenTotal = 0;
    TArray<TSharedPtr<FJsonValue>> Options;
    Options.Reserve(PCGPropTypesMaxEnumOptions);
    for (int32 Index = 0; Index < EntryTotal; ++Index)
    {
        if (!Budget.IsWithinTime())
        {
            OutError = TEXT("Enum metadata enumeration exceeded the 100 ms schema budget.");
            return false;
        }
        if (bMaxSentinel && Index == EntryTotal - 1) continue;
        if (Enum->HasMetaData(TEXT("Hidden"), Index))
        {
            ++HiddenTotal;
            continue;
        }
        ++VisibleTotal;
        if (Options.Num() >= PCGPropTypesMaxEnumOptions) continue;
        const FString Name = Enum->GetNameStringByIndex(Index);
        if (Name.Len() > PCGPropTypesMaxString)
        {
            OutError = TEXT("An enum option name exceeds the 1024-character schema bound.");
            return false;
        }
        TSharedPtr<FJsonObject> Option = MakeShared<FJsonObject>();
        Option->SetStringField(TEXT("name"), Name);
        SetPCGPropTypesInt64(Option, TEXT("value"), Enum->GetValueByIndex(Index));
        Options.Add(MakeShared<FJsonValueObject>(Option));
    }
    Details->SetArrayField(TEXT("enumOptions"), Options);
    Details->SetNumberField(TEXT("enumEntryTotal"), EntryTotal);
    Details->SetNumberField(TEXT("enumOptionTotal"), VisibleTotal);
    Details->SetNumberField(TEXT("hiddenEnumOptionTotal"), HiddenTotal);
    Details->SetBoolField(TEXT("hiddenEnumOptionsOmitted"), HiddenTotal > 0);
    Details->SetNumberField(TEXT("maxEnumOptionTotal"), MaxOptionTotal);
    Details->SetBoolField(TEXT("maxEnumOptionsOmitted"), bMaxSentinel);
    Details->SetBoolField(TEXT("enumOptionsTruncated"), VisibleTotal > Options.Num());
    return true;
}
}

bool SetPCGPropSchemaString(TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Value, FString& OutError)
{
    if (Value.Len() > PCGPropTypesMaxString)
    {
        OutError = TEXT("A reflected property schema string exceeds the 1024-character bound.");
        return false;
    }
    Object->SetStringField(Field, Value);
    return true;
}

FString GetPCGPropSchemaTypeName(FProperty* Property)
{
    if (CastField<FSoftClassProperty>(Property)) return TEXT("SoftClass");
    if (CastField<FClassProperty>(Property)) return TEXT("Class");
    return McpPropertyReflection::GetPropertyTypeName(Property);
}

bool BuildPCGPropSchemaType(FProperty* Property, const FString& TypeName, int32 Depth, FPCGReadBudget& Budget,
    TSharedPtr<FJsonObject>& OutDetails, FString& OutError)
{
    OutDetails = MakeShared<FJsonObject>();
    if (!SetPCGPropSchemaString(OutDetails, TEXT("type"), TypeName, OutError)) return false;
    const bool bSoftReference = CastField<FSoftObjectProperty>(Property) || CastField<FSoftClassProperty>(Property);
    OutDetails->SetBoolField(TEXT("softReference"), bSoftReference);
    const TCHAR* ReferenceKind = nullptr;
    if (CastField<FSoftClassProperty>(Property)) ReferenceKind = TEXT("softClass");
    else if (CastField<FClassProperty>(Property)) ReferenceKind = TEXT("class");
    else if (CastField<FSoftObjectProperty>(Property)) ReferenceKind = TEXT("softObject");
    else if (CastField<FObjectPropertyBase>(Property)) ReferenceKind = TEXT("object");
    if (ReferenceKind) OutDetails->SetStringField(TEXT("referenceKind"), ReferenceKind);

    UEnum* Enum = nullptr;
    FByteProperty* ByteProperty = nullptr;
    bool bHasEnumProperty = false;
    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        Enum = EnumProperty->GetEnum();
        bHasEnumProperty = true;
    }
    else if ((ByteProperty = CastField<FByteProperty>(Property)) != nullptr && ByteProperty->Enum)
    {
        Enum = ByteProperty->Enum;
        bHasEnumProperty = true;
    }
    if (bHasEnumProperty)
    {
        OutDetails->SetBoolField(TEXT("enumAvailable"), Enum != nullptr);
        if (Enum && !AddPCGPropTypesEnum(Enum, Budget, OutDetails, OutError)) return false;
    }

    FString ConstraintPath;
    const UClass* ConstraintClass = nullptr;
    if (FSoftClassProperty* SoftClass = CastField<FSoftClassProperty>(Property))
    {
        ConstraintClass = SoftClass->MetaClass.Get();
    }
    else if (FClassProperty* Class = CastField<FClassProperty>(Property))
    {
        ConstraintClass = Class->MetaClass.Get();
    }
    else if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        ConstraintClass = Object->PropertyClass.Get();
    }
    if (ConstraintClass) ConstraintPath = ConstraintClass->GetPathName();
    const bool bConstraintAvailable = !ConstraintPath.IsEmpty();
    if (CastField<FObjectPropertyBase>(Property))
    {
        OutDetails->SetBoolField(TEXT("classConstraintAvailable"), bConstraintAvailable);
        if (!SetPCGPropSchemaString(OutDetails, TEXT("classConstraintPath"), ConstraintPath, OutError)) return false;
    }
    if (FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        OutDetails->SetBoolField(TEXT("structTypeAvailable"), Struct->Struct != nullptr);
        if (Struct->Struct)
        {
            if (!SetPCGPropSchemaString(OutDetails, TEXT("structPath"), Struct->Struct->GetPathName(), OutError)) return false;
        }
        else
        {
            OutDetails->SetStringField(TEXT("structUnavailableReason"), TEXT("Reflected struct type is unavailable."));
        }
    }

    FProperty* Element = nullptr;
    FProperty* Key = nullptr;
    FProperty* Value = nullptr;
    const TCHAR* ContainerName = nullptr;
    if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        ContainerName = TEXT("array");
        Element = Array->Inner;
    }
    else if (FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        ContainerName = TEXT("set");
        Element = Set->ElementProp;
    }
    else if (FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        ContainerName = TEXT("map");
        Key = Map->KeyProp;
        Value = Map->ValueProp;
    }
    if (!ContainerName) return Budget.IsWithinTime();
    if (!SetPCGPropSchemaString(OutDetails, TEXT("container"), ContainerName, OutError)) return false;
    if (Depth >= PCGPropTypesMaxContainerDepth)
    {
        OutDetails->SetBoolField(TEXT("nestedUnsupported"), true);
        OutDetails->SetStringField(TEXT("nestedUnsupportedReason"), TEXT("Container descriptor depth is limited to three."));
        return true;
    }
    const auto AddChild = [&](const TCHAR* Field, FProperty* Child) -> bool
    {
        if (!Child || !Budget.IsWithinTime())
        {
            OutError = TEXT("A reflected container child is unavailable or exceeded the schema time budget.");
            return false;
        }
        TSharedPtr<FJsonObject> ChildDetails;
        if (!BuildPCGPropSchemaType(Child, GetPCGPropSchemaTypeName(Child), Depth + 1, Budget, ChildDetails, OutError)) return false;
        OutDetails->SetObjectField(Field, ChildDetails);
        return true;
    };
    if (Element && !AddChild(TEXT("element"), Element)) return false;
    if (Key && !AddChild(TEXT("key"), Key)) return false;
    if (Value && !AddChild(TEXT("value"), Value)) return false;
    return Budget.IsWithinTime();
}
}
#endif
