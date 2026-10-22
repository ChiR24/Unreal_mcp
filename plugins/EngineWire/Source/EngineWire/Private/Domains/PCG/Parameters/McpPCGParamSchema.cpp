#include "Domains/PCG/Parameters/McpPCGParameters.h"

#if MCP_HAS_PCG_PARAMETERS
namespace McpPCGHandlers
{
bool BuildPCGParameterDesc(const TSharedPtr<FJsonObject>& Payload, FPropertyBagPropertyDesc& Desc, FString& OutError)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    bool bCharactersValid = true;
    for (TCHAR Ch : Name) bCharactersValid &= FChar::IsAlnum(Ch) || Ch == TEXT('_');
    if (Name.IsEmpty() || Name.Len() > 128 || (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_')) || !bCharactersValid)
    {
        OutError = TEXT("Parameter names must be 1-128 letters, digits or underscores and cannot start with a digit.");
        return false;
    }
    const FString TypeName = GetJsonStringField(Payload, TEXT("type"));
    const TSet<FString> Supported = {TEXT("Bool"), TEXT("Byte"), TEXT("Int32"), TEXT("Int64"), TEXT("Float"), TEXT("Double"),
        TEXT("Name"), TEXT("String"), TEXT("Enum"), TEXT("Struct"), TEXT("Object"), TEXT("Class"), TEXT("SoftObject"), TEXT("SoftClass")};
    if (!Supported.Contains(TypeName))
    {
        OutError = TEXT("Unsupported parameter type; use an explicitly supported native property bag type.");
        return false;
    }
    const int64 EnumValue = StaticEnum<EPropertyBagPropertyType>()->GetValueByNameString(TypeName);
    const EPropertyBagPropertyType Type = static_cast<EPropertyBagPropertyType>(EnumValue);
    UObject* TypeObject = nullptr;
    const FString TypePath = GetJsonStringField(Payload, TEXT("typeObject"));
    if (Type == EPropertyBagPropertyType::Enum) TypeObject = LoadObject<UEnum>(nullptr, *TypePath);
    else if (Type == EPropertyBagPropertyType::Struct)
    {
        TypeObject = LoadObject<UScriptStruct>(nullptr, *TypePath);
        if (!TypeObject || (TypeObject != TBaseStructure<FVector>::Get() && TypeObject != TBaseStructure<FRotator>::Get() &&
            TypeObject != TBaseStructure<FTransform>::Get()))
        {
            OutError = TEXT("Struct parameters currently support native Vector, Rotator and Transform types only.");
            return false;
        }
    }
    else if (Type == EPropertyBagPropertyType::Object || Type == EPropertyBagPropertyType::Class ||
             Type == EPropertyBagPropertyType::SoftObject || Type == EPropertyBagPropertyType::SoftClass)
    {
        TypeObject = ResolveClassByName(TypePath.IsEmpty() ? TEXT("/Script/CoreUObject.Object") : TypePath);
    }
    if ((Type == EPropertyBagPropertyType::Enum || Type == EPropertyBagPropertyType::Struct || Type == EPropertyBagPropertyType::Object ||
         Type == EPropertyBagPropertyType::Class || Type == EPropertyBagPropertyType::SoftObject || Type == EPropertyBagPropertyType::SoftClass) && !TypeObject)
    {
        OutError = TEXT("typeObject must resolve to the enum, struct or class required by the selected parameter type.");
        return false;
    }
    const FString Container = GetJsonStringField(Payload, TEXT("container"), TEXT("none"));
    if (Container != TEXT("none") && Container != TEXT("array"))
    {
        OutError = TEXT("Only scalar parameters and a single array container are supported.");
        return false;
    }
    Desc = FPropertyBagPropertyDesc(FName(*Name), Container == TEXT("array") ? EPropertyBagContainerType::Array : EPropertyBagContainerType::None, Type, TypeObject);
    Desc.ID = FGuid::NewGuid();
    return true;
}

TArray<TSharedPtr<FJsonValue>> FindPCGParameterReferences(UPCGGraph* Graph, FName Name, const FGuid& Id)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (TObjectIterator<UPCGGraph> It; It; ++It)
    {
        if (*It != Graph && (It->HasAnyFlags(RF_ClassDefaultObject | RF_Transient) || It->GetOutermost() == GetTransientPackage())) continue;
        for (UPCGNode* Node : It->GetNodes())
        {
            UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
            if (!Settings) continue;
            const FStructProperty* GuidProperty = FindFProperty<FStructProperty>(Settings->GetClass(), TEXT("PropertyGuid"));
            const FStrProperty* PathProperty = FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("PropertyPath"));
            const FString Path = PathProperty ? PathProperty->GetPropertyValue_InContainer(Settings) : FString();
            const bool bGuidReference = GuidProperty && GuidProperty->Struct == TBaseStructure<FGuid>::Get() && *GuidProperty->ContainerPtrToValuePtr<FGuid>(Settings) == Id;
            const bool bPathReference = *It == Graph && PathProperty && (Path == Name.ToString() || Path.StartsWith(Name.ToString() + TEXT(".")));
            if (!bGuidReference && !bPathReference) continue;
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("graphPath"), It->GetPathName());
            Row->SetStringField(TEXT("nodeId"), Node->GetName());
            Row->SetStringField(TEXT("kind"), bGuidReference ? TEXT("parameter_guid") : TEXT("parameter_path"));
            Result.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    return Result;
}
}
#endif
