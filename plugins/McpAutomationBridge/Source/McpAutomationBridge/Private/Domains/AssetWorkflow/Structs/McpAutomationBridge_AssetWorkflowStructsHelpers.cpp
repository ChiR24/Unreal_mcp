#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"

#if WITH_EDITOR


FGuid ResolveMemberGuid(UUserDefinedStruct* S, const FString& VarGuidStr, const FString& MemberName)
{
    FGuid G;
    if (!VarGuidStr.IsEmpty() && FGuid::Parse(VarGuidStr, G))
    {
        if (FStructureEditorUtils::GetVarDescByGuid(S, G))
        {
            return G;
        }
    }

    if (!MemberName.IsEmpty())
    {
        for (FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            if (Var.FriendlyName == MemberName)
            {
                return Var.VarGuid;
            }
        }
    }

    return FGuid();
}

TSharedPtr<FJsonObject> VariableDescriptionToJson(const FStructVariableDescription& Var)
{
    TSharedPtr<FJsonObject> Member = MakeShared<FJsonObject>();
    Member->SetStringField(TEXT("guid"), Var.VarGuid.ToString());
    Member->SetStringField(TEXT("name"), Var.FriendlyName);
    Member->SetStringField(TEXT("type"), PinTypeToSummary(Var.ToPinType()));
    Member->SetStringField(TEXT("default"), Var.DefaultValue);
    Member->SetStringField(TEXT("tooltip"), Var.ToolTip);
    Member->SetStringField(TEXT("containerType"),
        Var.ContainerType == EPinContainerType::Array ? TEXT("Array")
        : Var.ContainerType == EPinContainerType::Set ? TEXT("Set")
        : Var.ContainerType == EPinContainerType::Map ? TEXT("Map")
        : TEXT("None"));

    TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
    for (const TPair<FName, FString>& Meta : Var.MetaData)
    {
        MetaObj->SetStringField(Meta.Key.ToString(), Meta.Value);
    }
    Member->SetObjectField(TEXT("metaData"), MetaObj);

    return Member;
}

FString UserDefinedStructureStatusToString(EUserDefinedStructureStatus Status)
{
    switch (Status)
    {
    case UDSS_UpToDate:
        return TEXT("UpToDate");
    case UDSS_Dirty:
        return TEXT("Dirty");
    case UDSS_Error:
        return TEXT("Error");
    default:
        return TEXT("Unknown");
    }
}

FString BuildDefaultExportText(UUserDefinedStruct* S, FProperty* Prop, const TSharedPtr<FJsonValue>& JsonValue)
{
    const uint8* DefaultInstance = S->GetDefaultInstance();
    void* Container = const_cast<uint8*>(DefaultInstance);

    FString ApplyError;
    if (McpPropertyReflection::ApplyJsonValueToProperty(Container, Prop, JsonValue, ApplyError))
    {
        FString OutStr;
        Prop->ExportTextItem_Direct(OutStr, Container, nullptr, nullptr, PPF_None);
        return OutStr;
    }

    UE_LOG(LogTemp, Warning, TEXT("McpStructHandlers: failed to apply default value: %s"), *ApplyError);
    return TEXT("");
}

void ForEachReferencingBlueprint(UUserDefinedStruct* S, TFunction<void(UBlueprint*)> Callback)
{
    IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();

    TArray<FAssetIdentifier> Refs;
    AR.GetReferencers(
        FAssetIdentifier(S->GetOutermost()->GetFName()),
        Refs);

    for (const FAssetIdentifier& Ref : Refs)
    {
        // UE 5.1+ returns FAssetData by value from GetAssetByObjectPath(FSoftObjectPath(...));
        // UE 5.0 takes an FName. Guard for the 5.0-5.8 source-compatibility target.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        const FAssetData AssetData = AR.GetAssetByObjectPath(FSoftObjectPath(Ref.ToString()));
#else
        const FAssetData AssetData = AR.GetAssetByObjectPath(FName(*Ref.ToString()));
#endif
        if (AssetData.IsValid())
        {
            UObject* Asset = AssetData.GetAsset();
            if (Asset && Asset->IsA<UBlueprint>())
            {
                if (UBlueprint* BP = Cast<UBlueprint>(Asset))
                {
                    Callback(BP);
                }
            }
        }
    }
}

#endif // WITH_EDITOR
