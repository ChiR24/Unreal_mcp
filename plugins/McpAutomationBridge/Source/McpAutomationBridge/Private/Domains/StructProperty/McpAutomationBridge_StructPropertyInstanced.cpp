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

        // (Re)initialize the instance with the requested inner struct type.
        Inst->InitializeAs(TargetStruct);

        if (Values.IsValid())
        {
            uint8* Memory = Inst->GetMutableMemory();
            FStructProperty* InnerProp = MakeInnerStructProperty(TargetStruct);
            FString ApplyError;
            if (!McpPropertyReflection::ApplyJsonValueToProperty(Memory, InnerProp, Values, ApplyError))
            {
                delete InnerProp;
                OutResult->SetBoolField(TEXT("success"), false);
                OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
                OutResult->SetStringField(TEXT("message"),
                    FString::Printf(TEXT("Failed to apply values: %s"), *ApplyError));
                return true;
            }
            delete InnerProp;
        }

        // Persist only through the safe wrapper; never the raw package save API.
        if (!McpSafeAssetSave(Asset))
        {
            OutResult->SetBoolField(TEXT("success"), false);
            OutResult->SetStringField(TEXT("error"), TEXT("OPERATION_FAILED"));
            OutResult->SetStringField(TEXT("message"),
                TEXT("Failed to save asset after setting FInstancedStruct"));
            return true;
        }

        OutResult->SetBoolField(TEXT("success"), true);
        OutResult->SetStringField(TEXT("propertyName"), PropertyName);
        OutResult->SetStringField(TEXT("structType"), TargetStruct->GetName());
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
