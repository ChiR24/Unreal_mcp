#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/UnrealType.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/Reflection/McpInstancedStructView.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeOperationsAssetSave.h"

// =============================================================================
// instanced_struct handler shard (struct ecosystem, issue #struct-ecosystem)
// -----------------------------------------------------------------------------
// Get/set an FInstancedStruct property on any UObject asset. Both operations are
// driven through the generic reflection layer (McpPropertyReflection) and, on
// write, persisted only via the safe save wrapper (McpSafeAssetSave). This shard
// never touches the raw package save API directly.
//
// Before UE 5.5 FInstancedStruct belongs to the Experimental StructUtils module,
// which the plugin does not link. So this file calls none of its members: it
// reads the value through FMcpInstancedStructView and works through the type's
// UScriptStruct, whose struct ops (construct, destroy, copy, text import) the
// owning module registers when it loads - and a property of the type exists only
// once that module is loaded.
// =============================================================================

namespace McpStructProperty
{

// Locate an FInstancedStruct-typed property by name on the given object, by the
// type's name and package (StructUtils before 5.5, CoreUObject from 5.5) rather
// than FInstancedStruct::StaticStruct().
static FStructProperty* FindInstancedStructProperty(UObject* Object, const FString& PropertyName)
{
    if (!Object || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    FStructProperty* StructProp = CastField<FStructProperty>(Object->GetClass()->FindPropertyByName(*PropertyName));
    if (!StructProp || !StructProp->Struct || StructProp->Struct->GetFName() != FName(TEXT("InstancedStruct")))
    {
        return nullptr;
    }
    const FString Package = StructProp->Struct->GetOutermost()->GetName();
    return Package == TEXT("/Script/StructUtils") || Package == TEXT("/Script/CoreUObject") ? StructProp : nullptr;
}

// An FInstancedStruct value of the property's type, constructed and destroyed by
// that type's struct ops.
struct FScratchInstance
{
    const UScriptStruct* Type;
    uint8* Memory;

    explicit FScratchInstance(const UScriptStruct* InType)
        : Type(InType)
        , Memory(static_cast<uint8*>(FMemory::Malloc(FMath::Max(1, InType->GetStructureSize()), InType->GetMinAlignment())))
    {
        Type->InitializeStruct(Memory);
    }

    ~FScratchInstance()
    {
        Type->DestroyStruct(Memory);
        FMemory::Free(Memory);
    }

    uint8* Inner() const
    {
        return FMcpInstancedStructView(*reinterpret_cast<const FInstancedStruct*>(Memory)).Memory;
    }
};

// Point an FInstancedStruct value at a default Target through the type's own
// text import, "/Script/Pkg.Name()": the format its copy and paste use in every
// UE 5 version (read a struct path, then that struct's text).
static bool InitializeInstanceAs(FStructProperty* Prop, void* Value, UScriptStruct* Target, UObject* Owner)
{
    const FString Text = Target->GetPathName() + TEXT("()");
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    const bool bImported = Prop->ImportText_Direct(*Text, Value, Owner, PPF_None) != nullptr;
#else
    const bool bImported = Prop->ImportText(*Text, Value, PPF_None, Owner) != nullptr;
#endif
    return bImported && FMcpInstancedStructView(*reinterpret_cast<const FInstancedStruct*>(Value)).Type == Target;
}

// Build a throwaway FStructProperty bound to the inner script struct so the
// generic reflection layer can (de)serialize the inner instance memory.
// FStructProperty is non-copyable in UE 5.7, so it is heap-allocated and must
// be deleted by the caller once the reflection call completes.
static FStructProperty* MakeInnerStructProperty(UScriptStruct* InnerStruct)
{
    FStructProperty* TempProp = new FStructProperty(nullptr, FName(TEXT("InstancedValue")), RF_NoFlags);
    TempProp->Struct = InnerStruct;
    return TempProp;
}

bool HandleStructPropertyAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    OutResult = MakeShared<FJsonObject>();

    // Protocol invariant: ProcessAutomationRequest only delivers OutResult to
    // the caller when the handler returns true. A false return leaves the native
    // MCP SSE stream open with no event, so the request hangs instead of
    // returning the error already written into OutResult. Every branch here must
    // therefore return true (the enum handlers follow the same convention).

    if (Action.IsEmpty())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("UNKNOWN_ACTION"));
        OutResult->SetStringField(TEXT("message"), TEXT("Empty action"));
        return true;
    }

    if (!Params.IsValid())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("INVALID_PAYLOAD"));
        OutResult->SetStringField(TEXT("message"), TEXT("instanced_struct payload missing"));
        return true;
    }

    FString AssetPath;
    Params->TryGetStringField(TEXT("assetPath"), AssetPath);
    AssetPath.TrimStartAndEndInline();
    FString PropertyName;
    Params->TryGetStringField(TEXT("propertyName"), PropertyName);
    PropertyName.TrimStartAndEndInline();

    if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("MISSING_PARAMETER"));
        OutResult->SetStringField(TEXT("message"),
            TEXT("instanced_struct requires asset_path and property_name"));
        return true;
    }

    UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
    if (!Asset)
    {
        OutResult->SetBoolField(TEXT("success"), false);
        OutResult->SetStringField(TEXT("error"), TEXT("ASSET_NOT_FOUND"));
        OutResult->SetStringField(TEXT("message"),
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
        return true;
    }

    if (Action.Equals(TEXT("get_instanced_struct_property"), ESearchCase::IgnoreCase))
    {
        FStructProperty* StructProp = FindInstancedStructProperty(Asset, PropertyName);
        if (!StructProp)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                FString::Printf(TEXT("Property '%s' is not an FInstancedStruct"), *PropertyName));
            return true;
        }

        const FMcpInstancedStructView Inst(*StructProp->ContainerPtrToValuePtr<FInstancedStruct>(Asset));
        const UScriptStruct* InnerStruct = Inst.Type;
        if (!InnerStruct)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                TEXT("FInstancedStruct has no inner script struct"));
            return true;
        }

        const uint8* Memory = Inst.Memory;

        TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
        Value->SetStringField(TEXT("structType"), InnerStruct->GetName());
        Value->SetStringField(TEXT("structPath"), InnerStruct->GetPathName());

        FStructProperty* InnerProp = MakeInnerStructProperty(const_cast<UScriptStruct*>(InnerStruct));
        TSharedPtr<FJsonValue> Fields =
            McpPropertyReflection::ExportPropertyToJsonValue(const_cast<uint8*>(Memory), InnerProp);
        Value->SetField(TEXT("fields"), Fields);
        delete InnerProp;

        OutResult->SetBoolField(TEXT("success"), true);
        OutResult->SetStringField(TEXT("propertyName"), PropertyName);
        OutResult->SetObjectField(TEXT("value"), Value);
        return true;
    }

    if (Action.Equals(TEXT("set_instanced_struct_property"), ESearchCase::IgnoreCase))
    {
        FStructProperty* StructProp = FindInstancedStructProperty(Asset, PropertyName);
        if (!StructProp)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                FString::Printf(TEXT("Property '%s' is not an FInstancedStruct"), *PropertyName));
            return true;
        }

        FString StructType;
        Params->TryGetStringField(TEXT("structType"), StructType);
        StructType.TrimStartAndEndInline();
        TSharedPtr<FJsonValue> Values = Params->TryGetField(TEXT("structValues"));

        if (StructType.IsEmpty())
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("MISSING_PARAMETER"));
            OutResult->SetStringField(TEXT("message"),
                TEXT("set_instanced_struct_property requires struct_type"));
            return true;
        }

        UScriptStruct* TargetStruct = FindObject<UScriptStruct>(nullptr, *StructType);
        if (!TargetStruct)
        {
            TargetStruct = LoadObject<UScriptStruct>(nullptr, *StructType);
        }
        if (!TargetStruct)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("ASSET_NOT_FOUND"));
            OutResult->SetStringField(TEXT("message"),
                FString::Printf(TEXT("Struct type not found: %s"), *StructType));
            return true;
        }

        if (Values.IsValid() && Values->Type != EJson::Object)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                TEXT("structValues must be a JSON object of fieldName -> value"));
            return true;
        }

        // Build the new value in a scratch instance and validate structValues there
        // BEFORE touching the live FInstancedStruct (issue #struct-ecosystem [23]):
        // the JSON object goes through the shared reflection importer
        // (McpPropertyReflection::ApplyJsonValueToProperty) bound to the scratch's
        // inner struct memory, so a malformed payload never mutates the asset.
        FScratchInstance Scratch(StructProp->Struct);
        if (!InitializeInstanceAs(StructProp, Scratch.Memory, TargetStruct, Asset))
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
            OutResult->SetStringField(TEXT("message"),
                FString::Printf(TEXT("Could not make an instance of %s"), *TargetStruct->GetPathName()));
            return true;
        }

        if (Values.IsValid())
        {
            FStructProperty* InnerProp = MakeInnerStructProperty(TargetStruct);
            FString ImportError;
            const bool bImported = McpPropertyReflection::ApplyJsonValueToProperty(
                Scratch.Inner(), InnerProp, Values, ImportError);
            delete InnerProp;

            if (!bImported)
            {
                OutResult->SetBoolField(TEXT("success"), false);
                OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
                OutResult->SetStringField(TEXT("message"),
                    FString::Printf(TEXT("Failed to apply structValues: %s"), *ImportError));
                return true;
            }
        }

        // Validation passed: commit through the type's own copy (a deep copy of
        // the inner struct), so the asset is left unchanged when anything above fails.
        StructProp->Struct->CopyScriptStruct(StructProp->ContainerPtrToValuePtr<void>(Asset), Scratch.Memory);

        // Optional persistence opt-out, default true. The read used to sit
        // behind `if (!bSave)` on a bSave that was always true, so save=false
        // never took effect.
        bool bSave = true;
        Params->TryGetBoolField(TEXT("save"), bSave);

        // Persist only through the safe wrapper; never the raw package save API.
        // Skip the save (without erroring) when the opt-out is requested.
        if (bSave)
        {
            if (!McpSafeOperations::McpSafeAssetSave(Asset))
            {
                OutResult->SetBoolField(TEXT("success"), false);
                OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
                OutResult->SetStringField(TEXT("message"),
                    TEXT("Failed to save asset after setting FInstancedStruct"));
                return true;
            }
        }

        OutResult->SetBoolField(TEXT("success"), true);
        OutResult->SetStringField(TEXT("propertyName"), PropertyName);
        OutResult->SetStringField(TEXT("structType"), TargetStruct->GetName());
        OutResult->SetBoolField(TEXT("saved"), bSave);
        return true;
    }

    OutResult->SetBoolField(TEXT("success"), false);
    OutResult->SetStringField(TEXT("error"), TEXT("UNKNOWN_ACTION"));
    OutResult->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Unsupported action: %s"), *Action));
    return true;

}

} // namespace McpStructProperty
