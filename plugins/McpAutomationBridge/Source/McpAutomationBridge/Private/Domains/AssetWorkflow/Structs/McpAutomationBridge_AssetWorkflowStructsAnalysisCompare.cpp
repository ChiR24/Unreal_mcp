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

    auto BuildMap = [](UUserDefinedStruct* S)
    {
        TMap<FString, const FStructVariableDescription*> Map;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Map.Add(Var.FriendlyName, &Var);
        }
        return Map;
    };

    auto BuildOrder = [](UUserDefinedStruct* S)
    {
        TArray<FString> Order;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
        {
            Order.Add(Var.FriendlyName);
        }
        return Order;
    };

    const TMap<FString, const FStructVariableDescription*> MapA = BuildMap(SA);
    const TMap<FString, const FStructVariableDescription*> MapB = BuildMap(SB);
    const TArray<FString> OrderA = BuildOrder(SA);
    const TArray<FString> OrderB = BuildOrder(SB);

    TArray<TSharedPtr<FJsonValue>> DiffArr;
    TSet<FString> Visited;

    // Standard add/remove/changed diff entry. A paired member that is fully
    // identical (type + default) is intentionally suppressed so that an
    // identical compare yields an EMPTY diff array.
    auto AddDiff = [&DiffArr](const FString& Name, const FStructVariableDescription* A, const FStructVariableDescription* B)
    {
        if (A && B)
        {
            const bool bSame = PinTypeToSummary(A->ToPinType()) == PinTypeToSummary(B->ToPinType()) &&
                A->DefaultValue == B->DefaultValue;
            if (bSame)
            {
                return;
            }
        }

        TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
        Diff->SetStringField(TEXT("field"), Name);

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
    auto AddAttributeDiffs = [&DiffArr](const FString& Name, const FStructVariableDescription& A, const FStructVariableDescription& B)
    {
        if (A.VarGuid != B.VarGuid)
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

    for (const auto& Pair : MapA)
    {
        Visited.Add(Pair.Key);
        const FStructVariableDescription* B = MapB.FindRef(Pair.Key);
        AddDiff(Pair.Key, Pair.Value, B);
        if (B)
        {
            AddAttributeDiffs(Pair.Key, *Pair.Value, *B);
        }
    }
    for (const auto& Pair : MapB)
    {
        if (!Visited.Contains(Pair.Key))
        {
            AddDiff(Pair.Key, nullptr, Pair.Value);
        }
    }

    // Order comparison: identical member SET but a different sequence.
    if (MapA.Num() == MapB.Num())
    {
        bool bSameSet = true;
        for (const auto& Pair : MapA)
        {
            if (!MapB.Contains(Pair.Key))
            {
                bSameSet = false;
                break;
            }
        }
        if (bSameSet && OrderA != OrderB)
        {
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("type"), TEXT("order_mismatch"));
            TArray<TSharedPtr<FJsonValue>> ArrA, ArrB;
            for (const FString& N : OrderA) ArrA.Add(MakeShared<FJsonValueString>(N));
            for (const FString& N : OrderB) ArrB.Add(MakeShared<FJsonValueString>(N));
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
