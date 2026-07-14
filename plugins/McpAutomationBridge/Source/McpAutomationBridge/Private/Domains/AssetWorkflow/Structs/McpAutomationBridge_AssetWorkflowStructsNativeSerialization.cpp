#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"

#if WITH_EDITOR

// Native-USTRUCT counterpart to VariableDescriptionToJson (see Helpers.cpp).
// Must keep the same member JSON shape so that get_struct can return a uniform
// member list regardless of whether the struct is a UserDefinedStruct or a
// native UScriptStruct.
TSharedPtr<FJsonObject> NativePropertyToMemberJson(FProperty* Prop)
{
    TSharedPtr<FJsonObject> Member = MakeShared<FJsonObject>();
    if (!Prop)
    {
        return Member;
    }

    FEdGraphPinType PinType;
    FString TypeStr;
    FString ContainerStr = TEXT("None");
    if (const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>())
    {
        if (Schema->ConvertPropertyToPinType(Prop, PinType))
        {
            TypeStr = PinTypeToSummary(PinType);
            ContainerStr = PinType.ContainerType == EPinContainerType::Array ? TEXT("Array")
                        : PinType.ContainerType == EPinContainerType::Set ? TEXT("Set")
                        : PinType.ContainerType == EPinContainerType::Map ? TEXT("Map")
                        : TEXT("None");
        }
    }
    if (TypeStr.IsEmpty())
    {
        FString Ext;
        TypeStr = Prop->GetCPPType(&Ext);
    }

    Member->SetStringField(TEXT("guid"), Prop->GetName());
    Member->SetStringField(TEXT("name"), Prop->GetName());
    Member->SetStringField(TEXT("type"), TypeStr);
    Member->SetStringField(TEXT("default"), TEXT(""));
    Member->SetStringField(TEXT("tooltip"), Prop->GetToolTipText().ToString());
    Member->SetStringField(TEXT("containerType"), ContainerStr);
    Member->SetObjectField(TEXT("metaData"), MakeShared<FJsonObject>());
    return Member;
}

#endif // WITH_EDITOR
