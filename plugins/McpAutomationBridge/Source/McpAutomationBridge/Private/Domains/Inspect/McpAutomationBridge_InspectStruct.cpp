#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Engine/DataTable.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "UObject/UnrealType.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

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
        return true;
    }

    if (!Params.IsValid())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("INVALID_PAYLOAD"));
        OutResult->SetStringField(TEXT("message"), TEXT("inspect_struct payload missing"));
        return true;
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
        return true;
    }

    UScriptStruct* Struct = ResolveStruct(StructPath);
    if (!Struct)
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("ASSET_NOT_FOUND"));
        OutResult->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Struct not found: %s"), *StructPath));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("structName"), Struct->GetName());
    Result->SetStringField(TEXT("structPath"), Struct->GetPathName());

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

    const bool bIsRowStruct = Struct->IsChildOf(FTableRowBase::StaticStruct());
    Result->SetBoolField(TEXT("isRowStruct"), bIsRowStruct);
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

    // For UUserDefinedStruct, build a name→VarDesc lookup so we can attach
    // the stable GUID and the full metadata map per member (reflection-only
    // FProperty lacks both).
    TMap<FName, const FStructVariableDescription*> UDSVarMap;
    if (Struct->IsA<UUserDefinedStruct>())
    {
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(Cast<UUserDefinedStruct>(Struct)))
        {
            UDSVarMap.Add(FName(*Var.FriendlyName), &Var);
        }
    }

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

        FString Tooltip = Prop->GetMetaData(TEXT("Tooltip"));
        FString Category = Prop->GetMetaData(TEXT("Category"));
        Member->SetStringField(TEXT("tooltip"), Tooltip);
        Member->SetStringField(TEXT("category"), Category);

        if (const FStructVariableDescription** FoundVar = UDSVarMap.Find(FName(*Prop->GetName())))
        {
            const FStructVariableDescription& VarDesc = **FoundVar;
            Member->SetStringField(TEXT("guid"), VarDesc.VarGuid.ToString());
            TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
            for (const TPair<FName, FString>& Meta : VarDesc.MetaData)
            {
                MetaObj->SetStringField(Meta.Key.ToString(), Meta.Value);
            }
            Member->SetObjectField(TEXT("metadata"), MetaObj);
        }
        else
        {
            Member->SetStringField(TEXT("guid"), FString());
            Member->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
        }

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
    return true;
#endif
}

} // namespace McpInspectStruct
