#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Engine/DataTable.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/UnrealType.h"

// =============================================================================
// inspect_struct handler shard (struct ecosystem, issue #struct-ecosystem)
// -----------------------------------------------------------------------------
// Read-only struct layout introspection. Given a struct asset path, struct
// path, or plain struct name, enumerate its fields (name, type, default,
// tooltip) plus the parent struct and row-struct / instanced-struct
// compatibility. This shard performs reflection only and never mutates or
// persists any asset.
// =============================================================================

namespace McpInspectStruct
{

// Resolve a UScriptStruct by object path, asset path, or bare struct name.
static UScriptStruct* ResolveStruct(const FString& Identifier)
{
    if (Identifier.IsEmpty())
    {
        return nullptr;
    }

    if (UScriptStruct* Found = FindObject<UScriptStruct>(nullptr, *Identifier))
    {
        return Found;
    }
    if (UScriptStruct* Loaded = LoadObject<UScriptStruct>(nullptr, *Identifier))
    {
        return Loaded;
    }
    return nullptr;
}

bool HandleInspectStructAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    OutResult = MakeShared<FJsonObject>();

#if WITH_EDITOR
    if (!Action.Equals(TEXT("inspect_struct"), ESearchCase::IgnoreCase))
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("UNKNOWN_ACTION"));
        OutResult->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Unsupported action: %s"), *Action));
        return false;
    }

    if (!Params.IsValid())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("INVALID_PAYLOAD"));
        OutResult->SetStringField(TEXT("message"), TEXT("inspect_struct payload missing"));
        return false;
    }

    // Accept structPath / structName / struct aliases for the target identifier.
    FString StructPath;
    Params->TryGetStringField(TEXT("structPath"), StructPath);
    StructPath.TrimStartAndEndInline();
    if (StructPath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("structName"), StructPath);
        StructPath.TrimStartAndEndInline();
    }
    if (StructPath.IsEmpty())
    {
        Params->TryGetStringField(TEXT("struct"), StructPath);
        StructPath.TrimStartAndEndInline();
    }

    if (StructPath.IsEmpty())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("MISSING_PARAMETER"));
        OutResult->SetStringField(TEXT("message"), TEXT("inspect_struct requires structPath"));
        return false;
    }

    UScriptStruct* Struct = ResolveStruct(StructPath);
    if (!Struct)
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("ASSET_NOT_FOUND"));
        OutResult->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Struct not found: %s"), *StructPath));
        return false;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("structName"), Struct->GetName());
    Result->SetStringField(TEXT("structPath"), Struct->GetPathName());

    // Parent struct chain.
    UStruct* Super = Struct->GetSuperStruct();
    if (Super && Super != UObject::StaticClass())
    {
        Result->SetStringField(TEXT("parentStruct"), Super->GetName());
        Result->SetStringField(TEXT("parentStructPath"), Super->GetPathName());
    }
    else
    {
        Result->SetStringField(TEXT("parentStruct"), TEXT(""));
        Result->SetStringField(TEXT("parentStructPath"), TEXT(""));
    }

    // Row-struct / instanced-struct compatibility.
    const bool bIsRowStruct = Struct->IsChildOf(FTableRowBase::StaticStruct());
    Result->SetBoolField(TEXT("isRowStruct"), bIsRowStruct);
    Result->SetBoolField(TEXT("instancedCompatible"), Struct->IsA<UScriptStruct>());
    Result->SetBoolField(TEXT("isUserDefined"), Struct->IsA<UUserDefinedStruct>());

    // Enumerate members via reflection. UScriptStruct no longer exposes a public
    // GetDefaultInstance() in UE 5.7; UUserDefinedStruct keeps one, while native
    // script structs fall back to an owned default-initialized instance.
    const uint8* DefaultInstance = nullptr;
    TArray<uint8> OwnedDefaultMemory;
    if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Struct))
    {
        DefaultInstance = UDS->GetDefaultInstance();
    }
    else
    {
        OwnedDefaultMemory.AddZeroed(Struct->GetStructureSize());
        Struct->InitializeStruct(OwnedDefaultMemory.GetData());
        DefaultInstance = OwnedDefaultMemory.GetData();
    }
    TArray<TSharedPtr<FJsonValue>> Members;
    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        FProperty* Prop = *It;
        if (!Prop)
        {
            continue;
        }

        TSharedPtr<FJsonObject> Member = MakeShared<FJsonObject>();
        Member->SetStringField(TEXT("name"), Prop->GetName());
        Member->SetStringField(TEXT("type"), McpPropertyReflection::GetPropertyTypeName(Prop));

        // Property-level metadata (Tooltip / Category).
        FString Tooltip = Prop->GetMetaData(TEXT("Tooltip"));
        FString Category = Prop->GetMetaData(TEXT("Category"));
        Member->SetStringField(TEXT("tooltip"), Tooltip);
        Member->SetStringField(TEXT("category"), Category);

        // Nested struct member detection.
        FString InnerStructName;
        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
        {
            if (StructProp->Struct)
            {
                InnerStructName = StructProp->Struct->GetName();
            }
        }
        Member->SetStringField(TEXT("innerStruct"), InnerStructName);
        Member->SetBoolField(TEXT("isStruct"), !InnerStructName.IsEmpty());

        // Default value sourced from the struct default instance.
        if (DefaultInstance)
        {
            TSharedPtr<FJsonValue> DefaultValue =
                McpPropertyReflection::ExportPropertyToJsonValue(
                    const_cast<uint8*>(DefaultInstance), Prop);
            Member->SetField(TEXT("default"), DefaultValue);
        }
        else
        {
            Member->SetField(TEXT("default"), MakeShared<FJsonValueNull>());
        }

        Members.Add(MakeShared<FJsonValueObject>(Member));
    }

    Result->SetArrayField(TEXT("members"), Members);
    Result->SetNumberField(TEXT("memberCount"), Members.Num());

    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetObjectField(TEXT("result"), Result);
    return true;

#else
    OutResult->SetBoolField(TEXT("success"), false);
    OutResult->SetStringField(TEXT("error"), TEXT("NOT_IMPLEMENTED"));
    OutResult->SetStringField(TEXT("message"), TEXT("inspect_struct requires editor build"));
    return false;
#endif
}

} // namespace McpInspectStruct
