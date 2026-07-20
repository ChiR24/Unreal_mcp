#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/UnrealType.h"
#include "StructUtils/InstancedStruct.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeOperationsAssetSave.h"

// =============================================================================
// instanced_struct handler shard (struct ecosystem, issue #struct-ecosystem)
// -----------------------------------------------------------------------------
// Get/set an FInstancedStruct property on any UObject asset. Both operations are
// driven through the generic reflection layer (McpPropertyReflection) and, on
// write, persisted only via the safe save wrapper (McpSafeAssetSave). This shard
// never touches the raw package save API directly.
// =============================================================================

namespace McpStructProperty
{

// Locate an FInstancedStruct-typed property by name on the given object.
static FProperty* FindInstancedStructProperty(UObject* Object, const FString& PropertyName)
{
    if (!Object || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    FProperty* Prop = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Prop || !Prop->IsA<FStructProperty>())
    {
        return nullptr;
    }

    FStructProperty* StructProp = CastField<FStructProperty>(Prop);
    if (StructProp->Struct && StructProp->Struct == FInstancedStruct::StaticStruct())
    {
        return Prop;
    }
    return nullptr;
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

#if WITH_EDITOR
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
        FProperty* Prop = FindInstancedStructProperty(Asset, PropertyName);
        if (!Prop)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                FString::Printf(TEXT("Property '%s' is not an FInstancedStruct"), *PropertyName));
            return true;
        }

        FStructProperty* StructProp = CastField<FStructProperty>(Prop);
        FInstancedStruct* Inst = StructProp->ContainerPtrToValuePtr<FInstancedStruct>(Asset);
        const UScriptStruct* InnerStruct = Inst->GetScriptStruct();
        if (!InnerStruct)
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
            OutResult->SetStringField(TEXT("message"),
                TEXT("FInstancedStruct has no inner script struct"));
            return true;
        }

        const uint8* Memory = Inst->GetMemory();

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
        FProperty* Prop = FindInstancedStructProperty(Asset, PropertyName);
        if (!Prop)
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

        FStructProperty* StructProp = CastField<FStructProperty>(Prop);
        FInstancedStruct* Inst = StructProp->ContainerPtrToValuePtr<FInstancedStruct>(Asset);

        if (Values.IsValid())
        {
            // Validate against a throwaway instance BEFORE touching the live
            // FInstancedStruct (issue #struct-ecosystem [23]). Route the JSON
            // object through the shared reflection importer
            // (McpPropertyReflection::ApplyJsonValueToProperty) bound to scratch
            // struct memory via a temporary FStructProperty, so a malformed
            // structValues payload never mutates the in-memory asset. Only after
            // validation succeeds do we commit the instance via MoveTemp.
            if (Values->Type != EJson::Object)
            {
                OutResult->SetBoolField(TEXT("success"), false);
                OutResult->SetStringField(TEXT("error"), TEXT("INVALID_OPERATION"));
                OutResult->SetStringField(TEXT("message"),
                    TEXT("structValues must be a JSON object of fieldName -> value"));
                return true;
            }

            FInstancedStruct Scratch;
            Scratch.InitializeAs(TargetStruct);

            FStructProperty* InnerProp = MakeInnerStructProperty(TargetStruct);
            FString ImportError;
            const bool bImported = McpPropertyReflection::ApplyJsonValueToProperty(
                Scratch.GetMutableMemory(), InnerProp, Values, ImportError);
            delete InnerProp;

            if (!bImported)
            {
                OutResult->SetBoolField(TEXT("success"), false);
                OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
                OutResult->SetStringField(TEXT("message"),
                    FString::Printf(TEXT("Failed to apply structValues: %s"), *ImportError));
                return true;
            }

            // Validation passed: commit the validated instance into the live
            // property. MoveTemp transfers ownership without re-applying JSON, so
            // the asset is left unchanged when validation above fails.
            *Inst = MoveTemp(Scratch);
        }
        else
        {
            // No values supplied: just (re)initialize the instance type.
            Inst->InitializeAs(TargetStruct);
        }

        // Optional persistence opt-out. Default true to preserve the historical
        // always-save contract; accepted as `bSave` (priority) or `save`.
        bool bSave = true;
        Params->TryGetBoolField(TEXT("bSave"), bSave);
        if (!bSave)
        {
            Params->TryGetBoolField(TEXT("save"), bSave);
        }

        // Persist only through the safe wrapper; never the raw package save API.
        // Skip the save (without erroring) when the opt-out is requested.
        if (bSave)
        {
            if (!McpSafeAssetSave(Asset))
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

#else
    OutResult->SetBoolField(TEXT("success"), false);
    OutResult->SetStringField(TEXT("error"), TEXT("NOT_IMPLEMENTED"));
    OutResult->SetStringField(TEXT("message"), TEXT("instanced_struct requires editor build"));
    return true;
#endif
}

} // namespace McpStructProperty
