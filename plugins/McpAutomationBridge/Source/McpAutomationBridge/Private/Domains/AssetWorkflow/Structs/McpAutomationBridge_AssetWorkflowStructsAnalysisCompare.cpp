#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsAnalysis.h"

#if WITH_EDITOR

bool HandleStructAnalysisCompare(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString StructPathA = GetPayloadString(Payload, TEXT("structPath"));
    FString StructPathB = GetPayloadString(Payload, TEXT("otherStructPath"));

    if (StructPathA.IsEmpty() || StructPathB.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing required parameter: structPath or otherStructPath"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    UUserDefinedStruct* SA = LoadObject<UUserDefinedStruct>(nullptr, *StructPathA);
    UUserDefinedStruct* SB = LoadObject<UUserDefinedStruct>(nullptr, *StructPathB);
    if (!SA)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Struct not found: %s"), *StructPathA), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    if (!SB)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Struct not found: %s"), *StructPathB), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // Pair members by their stable VarGuid so renamed fields stay matched as
    // modifications instead of being reported as removed+added. Members without
    // a valid GUID are excluded here and resolved later by FriendlyName fallback.
    auto BuildMap = [](UUserDefinedStruct* S)
    {
        TMap<FGuid, const FStructVariableDescription*> Map;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            if (Var.VarGuid.IsValid())
            {
                Map.Add(Var.VarGuid, &Var);
            }
        }
        return Map;
    };

    // Stable ordering key (GUID) for sequence comparison.
    auto BuildGuidOrder = [](UUserDefinedStruct* S)
    {
        TArray<FGuid> Order;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Order.Add(Var.VarGuid);
        }
        return Order;
    };

    // User-facing ordering (FriendlyName) used only for diff output.
    auto BuildNameOrder = [](UUserDefinedStruct* S)
    {
        TArray<FString> Order;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Order.Add(Var.FriendlyName);
        }
        return Order;
    };

    const TMap<FGuid, const FStructVariableDescription*> MapA = BuildMap(SA);
    const TMap<FGuid, const FStructVariableDescription*> MapB = BuildMap(SB);
    const TArray<FGuid> GuidOrderA = BuildGuidOrder(SA);
    const TArray<FGuid> GuidOrderB = BuildGuidOrder(SB);
    const TArray<FString> NameOrderA = BuildNameOrder(SA);
    const TArray<FString> NameOrderB = BuildNameOrder(SB);

    TArray<TSharedPtr<FJsonValue>> DiffArr;

    // Standard add/remove/changed diff entry. A paired member that is fully
    // identical (type + default) is intentionally suppressed so that an
    // identical compare yields an EMPTY diff array.
    auto AddDiff = [&DiffArr](const FString& Name, const FStructVariableDescription* A, const FStructVariableDescription* B)
    {
        if (A && B)
        {
            // A rename (same type/default, different FriendlyName) is a
            // modification, not an identical match, so it must not be suppressed.
            const bool bSameType = PinTypeToSummary(A->ToPinType()) == PinTypeToSummary(B->ToPinType());
            const bool bSameDefault = A->DefaultValue == B->DefaultValue;
            const bool bSameName = A->FriendlyName == B->FriendlyName;
            if (bSameType && bSameDefault && bSameName)
            {
                return;
            }
        }

        TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
        Diff->SetStringField(TEXT("field"), B ? B->FriendlyName : (A ? A->FriendlyName : Name));

        if (A && B && A->FriendlyName != B->FriendlyName)
        {
            Diff->SetStringField(TEXT("renamedFrom"), A->FriendlyName);
            Diff->SetStringField(TEXT("renamedTo"), B->FriendlyName);
        }

        if (A)
        {
            TSharedPtr<FJsonObject> InA = MakeShared<FJsonObject>();
            InA->SetStringField(TEXT("type"), PinTypeToSummary(A->ToPinType()));
            InA->SetStringField(TEXT("default"), A->DefaultValue);
            Diff->SetObjectField(TEXT("inA"), InA);
        }
        if (B)
        {
            TSharedPtr<FJsonObject> InB = MakeShared<FJsonObject>();
            InB->SetStringField(TEXT("type"), PinTypeToSummary(B->ToPinType()));
            InB->SetStringField(TEXT("default"), B->DefaultValue);
            Diff->SetObjectField(TEXT("inB"), InB);
        }

        if (A && !B)
        {
            Diff->SetStringField(TEXT("status"), TEXT("removed"));
        }
        else if (!A && B)
        {
            Diff->SetStringField(TEXT("status"), TEXT("added"));
        }
        else
        {
            Diff->SetStringField(TEXT("status"), TEXT("changed"));
        }

        DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
    };

    // Attribute-level diffs (VarGuid / ToolTip / MetaData) for a paired member.
    // Fallback-matched pairs (matched by FriendlyName, not VarGuid) always have
    // different GUIDs by construction; suppress the spurious guid_mismatch.
    auto AddAttributeDiffs = [&DiffArr](const FString& Name, const FStructVariableDescription& A, const FStructVariableDescription& B, bool bSkipGuidMismatch = false)
    {
        if (!bSkipGuidMismatch && A.VarGuid != B.VarGuid)
        {
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("type"), TEXT("guid_mismatch"));
            Diff->SetStringField(TEXT("name"), Name);
            Diff->SetStringField(TEXT("guidA"), A.VarGuid.ToString());
            Diff->SetStringField(TEXT("guidB"), B.VarGuid.ToString());
            DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
        }

        if (A.ToolTip != B.ToolTip)
        {
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("type"), TEXT("tooltip_mismatch"));
            Diff->SetStringField(TEXT("name"), Name);
            Diff->SetStringField(TEXT("tooltipA"), A.ToolTip);
            Diff->SetStringField(TEXT("tooltipB"), B.ToolTip);
            DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
        }

        if (!A.MetaData.OrderIndependentCompareEqual(B.MetaData))
        {
            auto MapToJson = [](const TMap<FName, FString>& M)
            {
                TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                for (const TPair<FName, FString>& Pair : M)
                {
                    Obj->SetStringField(Pair.Key.ToString(), Pair.Value);
                }
                return Obj;
            };
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("type"), TEXT("metadata_mismatch"));
            Diff->SetStringField(TEXT("name"), Name);
            Diff->SetObjectField(TEXT("metadataA"), MapToJson(A.MetaData));
            Diff->SetObjectField(TEXT("metadataB"), MapToJson(B.MetaData));
            DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
        }
    };

    TSet<FGuid> VisitedB;

    // Pass 1: pair members by stable VarGuid (handles renames as modifications).
    for (const auto& Pair : MapA)
    {
        const FStructVariableDescription* B = MapB.FindRef(Pair.Key);
        if (B)
        {
            VisitedB.Add(Pair.Key);
            AddDiff(Pair.Value->FriendlyName, Pair.Value, B);
            AddAttributeDiffs(Pair.Value->FriendlyName, *Pair.Value, *B);
        }
    }

    // Pass 2: fallback — match unpaired members by FriendlyName so members that
    // lost their GUID pairing are still treated as modifications, not remove/add.
    for (const auto& Pair : MapA)
    {
        if (MapB.Contains(Pair.Key))
        {
            continue;
        }
        const FStructVariableDescription* B = nullptr;
        for (const auto& PairB : MapB)
        {
            if (VisitedB.Contains(PairB.Key))
            {
                continue;
            }
            if (PairB.Value->FriendlyName == Pair.Value->FriendlyName)
            {
                B = PairB.Value;
                VisitedB.Add(PairB.Key);
                break;
            }
        }
        if (B)
        {
            AddDiff(Pair.Value->FriendlyName, Pair.Value, B);
            AddAttributeDiffs(Pair.Value->FriendlyName, *Pair.Value, *B, /*bSkipGuidMismatch=*/true);
        }
        else
        {
            AddDiff(Pair.Value->FriendlyName, Pair.Value, nullptr);
        }
    }

    // Remaining B members with no A counterpart are additions.
    for (const auto& Pair : MapB)
    {
        if (!VisitedB.Contains(Pair.Key))
        {
            AddDiff(Pair.Value->FriendlyName, nullptr, Pair.Value);
        }
    }

    // Order comparison: identical member SET (by GUID) but a different sequence.
    if (MapA.Num() == MapB.Num())
    {
        bool bSameSet = true;
        for (const FGuid& G : GuidOrderA)
        {
            if (!MapB.Contains(G))
            {
                bSameSet = false;
                break;
            }
        }
        if (bSameSet && GuidOrderA != GuidOrderB)
        {
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("type"), TEXT("order_mismatch"));
            TArray<TSharedPtr<FJsonValue>> ArrA, ArrB;
            for (const FString& N : NameOrderA) ArrA.Add(MakeShared<FJsonValueString>(N));
            for (const FString& N : NameOrderB) ArrB.Add(MakeShared<FJsonValueString>(N));
            Diff->SetArrayField(TEXT("orderA"), ArrA);
            Diff->SetArrayField(TEXT("orderB"), ArrB);
            DiffArr.Add(MakeShared<FJsonValueObject>(Diff));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("structPathA"), StructPathA);
    Result->SetStringField(TEXT("structPathB"), StructPathB);
    Result->SetArrayField(TEXT("diff"), DiffArr);
    Result->SetBoolField(TEXT("equal"), DiffArr.Num() == 0);
    Result->SetStringField(TEXT("summary"), DiffArr.Num() == 0 ? TEXT("Structs are identical") : FString::Printf(TEXT("%d difference(s)"), DiffArr.Num()));
    McpHandlerUtils::AddVerification(Result, SA);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Structs compared"), Result);
    return true;
}

#endif // WITH_EDITOR
