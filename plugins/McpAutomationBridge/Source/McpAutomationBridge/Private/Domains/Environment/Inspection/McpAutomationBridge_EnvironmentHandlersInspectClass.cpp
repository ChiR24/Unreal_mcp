#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

namespace {
// Mirrors the editor's inheritance rule for IsBlueprintBase / BlueprintType:
// the nearest class in the chain that declares the key decides.
bool McpResolveInheritedBoolMetadata(const UClass *Class, const TCHAR *TrueKey, const TCHAR *FalseKey)
{
    for (const UClass *Current = Class; Current; Current = Current->GetSuperClass())
    {
        if (FalseKey && Current->HasMetaData(FalseKey))
        {
            return false;
        }
        if (Current->HasMetaData(TrueKey))
        {
            return Current->GetBoolMetaData(TrueKey);
        }
    }
    return false;
}
} // namespace

// inspect_class payload: identity, module/package, flags, the declared
// properties (name/type/category, capped at 200) and the CDO values as text.
void McpDescribeClass(UClass *Class, TSharedPtr<FJsonObject> Resp)
{
    if (!Class || !Resp.IsValid())
    {
        return;
    }
    UClass *Super = Class->GetSuperClass();
    const FString Package = Class->GetOutermost()->GetName();
    Resp->SetStringField(TEXT("className"), Class->GetName());
    Resp->SetStringField(TEXT("classPath"), Class->GetPathName());
    Resp->SetStringField(TEXT("parentClass"), Super ? Super->GetName() : TEXT("None"));
    Resp->SetStringField(TEXT("parentClassPath"), Super ? Super->GetPathName() : TEXT(""));
    Resp->SetStringField(TEXT("package"), Package);
    Resp->SetStringField(TEXT("module"), Package.StartsWith(TEXT("/Script/")) ? Package.RightChop(8) : FString());
    Resp->SetBoolField(TEXT("isNative"), Class->HasAnyClassFlags(CLASS_Native));
    Resp->SetBoolField(TEXT("isBlueprintGenerated"), Class->ClassGeneratedBy != nullptr);
    if (Class->ClassGeneratedBy)
    {
        Resp->SetStringField(TEXT("generatedBy"), Class->ClassGeneratedBy->GetPathName());
    }
    Resp->SetStringField(TEXT("classFlags"), FString::Printf(TEXT("0x%08X"), static_cast<uint32>(Class->GetClassFlags())));
    TArray<TSharedPtr<FJsonValue>> Ancestors;
    for (const UClass *Ancestor = Super; Ancestor; Ancestor = Ancestor->GetSuperClass())
    {
        Ancestors.Add(MakeShared<FJsonValueString>(Ancestor->GetName()));
    }
    Resp->SetArrayField(TEXT("ancestors"), Ancestors);

    TSharedPtr<FJsonObject> Flags = McpHandlerUtils::CreateResultObject();
    Flags->SetBoolField(TEXT("abstract"), Class->HasAnyClassFlags(CLASS_Abstract));
    Flags->SetBoolField(TEXT("blueprintable"), McpResolveInheritedBoolMetadata(Class, TEXT("IsBlueprintBase"), nullptr));
    Flags->SetBoolField(TEXT("blueprintType"), McpResolveInheritedBoolMetadata(Class, TEXT("BlueprintType"), TEXT("NotBlueprintType")));
    Flags->SetBoolField(TEXT("deprecated"), Class->HasAnyClassFlags(CLASS_Deprecated));
    Flags->SetBoolField(TEXT("transient"), Class->HasAnyClassFlags(CLASS_Transient));
    Flags->SetBoolField(TEXT("config"), Class->HasAnyClassFlags(CLASS_Config));
    Flags->SetBoolField(TEXT("interface"), Class->HasAnyClassFlags(CLASS_Interface));
    Flags->SetBoolField(TEXT("isActor"), Class->IsChildOf(AActor::StaticClass()));
    Flags->SetBoolField(TEXT("isComponent"), Class->IsChildOf(UActorComponent::StaticClass()));
    Resp->SetObjectField(TEXT("flags"), Flags);

    constexpr int32 MaxProperties = 200;
    TArray<TSharedPtr<FJsonValue>> Properties;
    TSharedPtr<FJsonObject> Defaults = McpHandlerUtils::CreateResultObject();
    UObject *Cdo = Class->GetDefaultObject();
    int32 Total = 0;
    for (TFieldIterator<FProperty> It(Class); It; ++It)
    {
        FProperty *Property = *It;
        if (!Property)
        {
            continue;
        }
        ++Total;
        if (Properties.Num() >= MaxProperties)
        {
            continue;
        }
        const UClass *Owner = Property->GetOwnerClass();
        TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
        Entry->SetStringField(TEXT("name"), Property->GetName());
        Entry->SetStringField(TEXT("type"), McpPropertyReflection::GetPropertyTypeName(Property));
        Entry->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
        Entry->SetStringField(TEXT("declaredIn"), Owner ? Owner->GetName() : TEXT(""));
        Entry->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
        Entry->SetBoolField(TEXT("blueprintVisible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
        Entry->SetBoolField(TEXT("deprecated"), Property->HasAnyPropertyFlags(CPF_Deprecated));
        Properties.Add(MakeShared<FJsonValueObject>(Entry));
        if (Cdo && !Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            Defaults->SetStringField(Property->GetName(), McpPropertyReflection::GetPropertyValueAsString(Cdo, Property));
        }
    }
    Resp->SetArrayField(TEXT("properties"), Properties);
    Resp->SetNumberField(TEXT("propertyCount"), Total);
    Resp->SetBoolField(TEXT("propertiesTruncated"), Total > Properties.Num());
    Resp->SetObjectField(TEXT("defaultProperties"), Defaults);
    Resp->SetStringField(TEXT("defaultObjectPath"), Cdo ? Cdo->GetPathName() : TEXT(""));
    int32 FunctionCount = 0;
    for (TFieldIterator<UFunction> It(Class); It; ++It)
    {
        ++FunctionCount;
    }
    Resp->SetNumberField(TEXT("functionCount"), FunctionCount);
}

} // namespace McpEnvironmentHandlers
#endif
