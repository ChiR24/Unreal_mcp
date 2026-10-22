#include "Domains/PCG/Data/McpPCGDataActions.h"

#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#endif

namespace McpPCGHandlers
{
bool IsPCGDataAction(const FString& Action)
{
    return Action == TEXT("list_pcg_data") || Action == TEXT("get_pcg_data_schema") ||
        Action == TEXT("create_pcg_data_view") || Action == TEXT("read_pcg_data") || Action == TEXT("release_pcg_data_view");
}

#if MCP_HAS_PCG_EXECUTION
bool ReadPCGDataInt(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key, int32 Default, int32 Minimum,
    int32 Maximum, int32& OutValue, FString& OutError)
{
    OutError.Reset();
    OutValue = Default;
    const auto Value = Payload->TryGetField(Key);
    if (!Value.IsValid()) return true;
    const double Number = Value->Type == EJson::Number ? Value->AsNumber() : -1;
    if (Value->Type != EJson::Number || !FMath::IsFinite(Number) || Number < Minimum || Number > Maximum || FMath::FloorToDouble(Number) != Number)
    {
        OutError = FString::Printf(TEXT("%s must be an integer between %d and %d."), Key, Minimum, Maximum);
        return false;
    }
    OutValue = static_cast<int32>(Number);
    return true;
}

bool ReadPCGDataDomain(const TSharedPtr<FJsonObject>& Payload, FString& OutDomain, FString& OutError)
{
    OutError.Reset();
    OutDomain = TEXT("Elements");
    if ((Payload->HasField(TEXT("domain")) && !Payload->TryGetStringField(TEXT("domain"), OutDomain)) ||
        (OutDomain != TEXT("Elements") && OutDomain != TEXT("Data")))
    {
        OutError = TEXT("domain must be exactly Elements or Data.");
        return false;
    }
    return true;
}

bool ReadPCGDataStrings(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key, int32 MaxLength,
    TArray<FString>& OutValues, FString& OutError)
{
    OutError.Reset();
    OutValues.Reset();
    const auto Field = Payload->TryGetField(Key);
    if (!Field.IsValid()) return true;
    if (Field->Type != EJson::Array || Field->AsArray().Num() > 8)
    {
        OutError = FString::Printf(TEXT("%s must be an array of at most eight explicit column names."), Key);
        return false;
    }
    for (const auto& Value : Field->AsArray())
    {
        if (!Value.IsValid() || Value->Type != EJson::String || Value->AsString().IsEmpty() || Value->AsString().Len() > MaxLength)
        {
            OutValues.Reset();
            OutError = TEXT("Column names must be bounded nonempty strings.");
            return false;
        }
        OutValues.Add(Value->AsString());
    }
    return true;
}

bool HandlePCGData(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, FPCGDataViewStore& Store)
{
    if (!IsPCGDataAction(Action)) return false;
    FPCGReadBudget Budget;
    FString Error, Principal;
    UPCGComponent* Component = ResolveExactPCGComponent(Payload, Error);
    if (!Component || !Component->GetWorld() || Component->GetWorld()->WorldType != EWorldType::Editor)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("Only the exact current editor-world component is supported.") : Error, TEXT("PCG_DATA_UNAVAILABLE"));
        return true;
    }
    if (!ResolvePCGPrincipal(Bridge, RequestId, Socket, Principal))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("A trusted request principal is required for data-view ownership."), TEXT("PCG_DATA_UNAVAILABLE"));
        return true;
    }
    const bool bRelease = Action == TEXT("release_pcg_data_view");
    if (!bRelease && (Component->IsPartitioned() || Component->IsLocalComponent() || Component->IsManagedByRuntimeGenSystem() ||
        Component->IsGenerating() || Component->IsCleaningUp() || HasActivePCGOperation(Component)))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Data reads currently require an idle unpartitioned, non-runtime-managed editor component; no work was scheduled or transferred."), TEXT("PCG_DATA_UNAVAILABLE"));
        return true;
    }
    TArray<FString> NewHandles;
    bool bPublished = false;
    ON_SCOPE_EXIT
    {
        if (!bPublished) for (const FString& Id : NewHandles) Store.Release(Id, Component, Principal);
    };
    auto Result = Action == TEXT("create_pcg_data_view") ? CapturePCGDataView(Component, Principal, Payload, Store, NewHandles, Error) :
        QueryPCGRootData(Component, Principal, Action, Payload, Store, NewHandles, Error);
    if (!Result.IsValid())
    {
        Bridge->SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("The requested data view is unavailable.") : Error, TEXT("PCG_DATA_UNAVAILABLE"));
        return true;
    }
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    FString Encoded;
    auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Result.ToSharedRef(), Writer) || Encoded.Len() > 80000 || !Budget.IsWithinTime())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("The complete data result exceeds its serialized or cooperative time budget; narrow the requested page or projection. No partial view was published."), TEXT("PCG_DATA_BUDGET_EXCEEDED"));
        return true;
    }
    bPublished = true;
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG data read without native generation, capture, transfer or save."), Result);
    return true;
}
#endif
}
