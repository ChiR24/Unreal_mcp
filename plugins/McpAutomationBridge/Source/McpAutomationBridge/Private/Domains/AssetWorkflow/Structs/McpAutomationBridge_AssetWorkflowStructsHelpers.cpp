#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

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

bool ValidateStructMembers(
    const TArray<TSharedPtr<FJsonValue>>& Members,
    const FName& SelfStructPath,
    TArray<FParsedMember>& OutParsed,
    TArray<FString>& OutFailures)
{
    OutParsed.Reset();
    OutFailures.Reset();
    OutParsed.Reserve(Members.Num());

    for (const TSharedPtr<FJsonValue>& MemberVal : Members)
    {
        const TSharedPtr<FJsonObject>* MemberObj = nullptr;
        if (!MemberVal->TryGetObject(MemberObj) || !MemberObj || !(*MemberObj).IsValid())
        {
            OutFailures.Add(TEXT("member entry is not a JSON object"));
            continue;
        }
        // Accept both field-name conventions for parity:
        //   - short form: "name" / "type" (used by import_struct payloads)
        //   - long form:  "memberName" / "memberType" (matches add_struct_member schema)
        FString MemberName, MemberType;
        const bool bHasShort = (*MemberObj)->TryGetStringField(TEXT("name"), MemberName) &&
                               (*MemberObj)->TryGetStringField(TEXT("type"), MemberType);
        if (!bHasShort)
        {
            const bool bHasLong = (*MemberObj)->TryGetStringField(TEXT("memberName"), MemberName) &&
                                  (*MemberObj)->TryGetStringField(TEXT("memberType"), MemberType);
            if (!bHasLong)
            {
                OutFailures.Add(TEXT("member missing required 'name'/'type' or 'memberName'/'memberType' field"));
                continue;
            }
        }
        if (MemberName.IsEmpty() || MemberType.IsEmpty())
        {
            OutFailures.Add(FString::Printf(TEXT("member has empty name or type (name=%s)"), *MemberName));
            continue;
        }

        const McpBlueprintUtils::FTypeResolutionResult Resolved =
            McpBlueprintUtils::ResolvePinType(MemberType, SelfStructPath);
        if (!Resolved.bSuccess)
        {
            OutFailures.Add(FString::Printf(TEXT("member '%s' type '%s' rejected: %s"),
                *MemberName, *MemberType, *Resolved.OutError));
            continue;
        }

        FParsedMember V;
        V.Name = MemberName;
        V.PinType = Resolved.PinType;
        V.TypeStr = MemberType;
        FString DefaultStr;
        if ((*MemberObj)->TryGetStringField(TEXT("defaultValue"), DefaultStr) && !DefaultStr.IsEmpty())
        {
            V.Default = DefaultStr;
        }
        else if ((*MemberObj)->TryGetStringField(TEXT("default"), DefaultStr))
        {
            V.Default = DefaultStr;
        }
        (*MemberObj)->TryGetStringField(TEXT("tooltip"), V.Tooltip);
        const TSharedPtr<FJsonObject>* Meta = nullptr;
        if ((*MemberObj)->TryGetObjectField(TEXT("metadata"), Meta) && Meta && (*Meta).IsValid())
        {
            V.Metadata = *Meta;
        }
        OutParsed.Add(MoveTemp(V));
    }

    return OutFailures.Num() == 0;
}

int32 ApplyParsedStructMembers(
    UUserDefinedStruct* S,
    const TArray<FParsedMember>& Parsed,
    TArray<FString>& OutFailures)
{
    OutFailures.Reset();
    int32 Applied = 0;
    if (!S)
    {
        OutFailures.Add(TEXT("ApplyParsedStructMembers: null struct"));
        return 0;
    }
    for (const FParsedMember& V : Parsed)
    {
        if (!FStructureEditorUtils::AddVariable(S, V.PinType))
        {
            OutFailures.Add(FString::Printf(
                TEXT("member '%s' could not be added (invalid or unsupported type '%s')"),
                *V.Name, *V.TypeStr));
            continue;
        }
        const FGuid G = FStructureEditorUtils::GetVarDesc(S).Last().VarGuid;
        FStructureEditorUtils::RenameVariable(S, G, V.Name);

        if (FStructVariableDescription* NewVar = FStructureEditorUtils::GetVarDescByGuid(S, G))
        {
            if (!V.Default.IsEmpty())
            {
                FStructureEditorUtils::ChangeVariableDefaultValue(S, G, V.Default);
            }
            if (!V.Tooltip.IsEmpty())
            {
                FStructureEditorUtils::ChangeVariableTooltip(S, G, V.Tooltip);
            }
            if (V.Metadata.IsValid())
            {
                for (const auto& Pair : V.Metadata->Values)
                {
                    FStructureEditorUtils::SetMetaData(S, G, *Pair.Key, Pair.Value->AsString());
                }
            }
        }
        ++Applied;
    }
    return Applied;
}

bool ApplyStructMembers(
    UUserDefinedStruct* S,
    const TArray<TSharedPtr<FJsonValue>>& Members,
    const FName& SelfStructPath,
    int32& OutImported,
    TArray<FString>& Failures)
{
    OutImported = 0;
    Failures.Reset();

    if (!S)
    {
        Failures.Add(TEXT("ApplyStructMembers: null struct"));
        return false;
    }

    TArray<FParsedMember> Parsed;
    if (!ValidateStructMembers(Members, SelfStructPath, Parsed, Failures))
    {
        return false;
    }

    OutImported = ApplyParsedStructMembers(S, Parsed, Failures);
    return Failures.Num() == 0;
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

void ForEachReferencingAsset(UUserDefinedStruct* S, TFunction<void(UObject*)> Callback)
{
    if (!S || !Callback) return;

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
            if (Asset)
            {
                Callback(Asset);
            }
        }
    }
}

void ForEachReferencingBlueprint(UUserDefinedStruct* S, TFunction<void(UBlueprint*)> Callback)
{
    ForEachReferencingAsset(S, [Callback](UObject* Asset)
    {
        if (UBlueprint* BP = Cast<UBlueprint>(Asset))
        {
            Callback(BP);
        }
    });
}

#endif // WITH_EDITOR
