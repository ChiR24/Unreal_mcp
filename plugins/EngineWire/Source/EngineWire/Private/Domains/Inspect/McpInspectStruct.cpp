#include "Domains/Inspect/McpInspectShared.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

#include "Engine/DataTable.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "UObject/UnrealType.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

// inspect_struct: read-only struct layout introspection. Given a struct asset path, struct path, or plain struct
// name, enumerate its fields (name, type, default, tooltip) plus the parent struct and row-struct / instanced-struct
// compatibility. Reflection only: nothing is changed or saved.

namespace McpInspectHandlers
{

bool HandleInspectStructAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString StructPath = McpGetFirstStringField(Payload, {TEXT("structPath")}).TrimStartAndEnd();
    if (StructPath.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("inspect_struct requires structPath"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    UScriptStruct* Struct = McpResolveStructByName(StructPath);
    if (!Struct)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct);

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
    Result->SetBoolField(TEXT("isUserDefined"), UserStruct != nullptr);

    // Enumerate members via reflection. UScriptStruct no longer exposes a public
    // GetDefaultInstance() in UE 5.7; UUserDefinedStruct keeps one, while native
    // script structs fall back to an owned default-initialized instance.
    const uint8* DefaultInstance = nullptr;
    TArray<uint8> OwnedDefaultMemory;
    if (UserStruct)
    {
        DefaultInstance = UserStruct->GetDefaultInstance();
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
    if (UserStruct)
    {
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(UserStruct))
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
#if MCP_HAS_STRUCT_VARIABLE_METADATA
            for (const TPair<FName, FString>& Meta : VarDesc.MetaData)
            {
                MetaObj->SetStringField(Meta.Key.ToString(), Meta.Value);
            }
#endif
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

    TSharedPtr<FJsonObject> Reply = MakeShared<FJsonObject>();
    Reply->SetObjectField(TEXT("result"), Result);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Struct inspected"), Reply);
    return true;
}

} // namespace McpInspectHandlers
