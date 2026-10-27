#include "Domains/Debug/Explain/McpExplain.h"

#include "Dom/JsonValue.h"

namespace McpExplain
{
namespace
{
TSharedPtr<FJsonObject> Row(const TCHAR* Check, const TCHAR* Status, const FString& Detail)
{
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("check"), Check);
    Entry->SetStringField(TEXT("status"), Status);
    Entry->SetStringField(TEXT("detail"), Detail);
    return Entry;
}
} // namespace

void FTree::Pass(const TCHAR* Check, const FString& Detail)
{
    Checks.Add(Row(Check, TEXT("passed"), Detail));
}

void FTree::Fail(const TCHAR* Check, const TCHAR* Code, const FString& Detail, const TSharedPtr<FJsonObject>& NextCall)
{
    TSharedPtr<FJsonObject> Entry = Row(Check, TEXT("failed"), Detail);
    Entry->SetStringField(TEXT("code"), Code);
    if (NextCall.IsValid())
    {
        Entry->SetObjectField(TEXT("nextCall"), NextCall);
    }
    if (FirstFailure == INDEX_NONE)
    {
        FirstFailure = Checks.Num();
    }
    Checks.Add(Entry);
}

void FTree::Unknown(const TCHAR* Check, const FString& Detail)
{
    Checks.Add(Row(Check, TEXT("unknown"), Detail));
    ++Unknowns;
}

void FTree::WriteTo(const TSharedPtr<FJsonObject>& Result) const
{
    TArray<TSharedPtr<FJsonValue>> All;
    TArray<TSharedPtr<FJsonValue>> Causes;
    for (const TSharedPtr<FJsonObject>& Entry : Checks)
    {
        All.Add(MakeShared<FJsonValueObject>(Entry));
        if (Entry->GetStringField(TEXT("status")) == TEXT("failed"))
        {
            Causes.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    Result->SetStringField(TEXT("verdict"), HasFailed() ? TEXT("failed") : Unknowns > 0 ? TEXT("unknown") : TEXT("ok"));
    if (HasFailed())
    {
        Result->SetObjectField(TEXT("cause"), Checks[FirstFailure]);
        Result->SetArrayField(TEXT("causes"), Causes);
    }
    Result->SetArrayField(TEXT("checks"), All);
}

FString FTree::Summary(const FString& Subject) const
{
    if (HasFailed())
    {
        return FString::Printf(TEXT("%s: %s"), *Subject, *Checks[FirstFailure]->GetStringField(TEXT("detail")));
    }
    return Unknowns > 0
        ? FString::Printf(TEXT("%s: nothing found that stops it, but %d check(s) could not be made (see checks)."), *Subject, Unknowns)
        : FString::Printf(TEXT("%s: every check passed; nothing found that stops it."), *Subject);
}

TSharedPtr<FJsonObject> MakeNextCall(const TCHAR* Tool, const TCHAR* Action, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
    Call->SetStringField(TEXT("operation"), TEXT("execute"));
    Call->SetStringField(TEXT("tool"), Tool);
    Call->SetStringField(TEXT("action"), Action);
    Call->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeShared<FJsonObject>());
    return Call;
}

bool Handle(UEngineWireSubsystem& Self, const FString& RequestId, const FString& LowerSubAction,
            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (LowerSubAction == TEXT("explain_collision"))
    {
        return ExplainCollision(Self, RequestId, Payload, Socket);
    }
    if (LowerSubAction == TEXT("explain_visible"))
    {
        return ExplainVisible(Self, RequestId, Payload, Socket);
    }
    if (LowerSubAction == TEXT("explain_input"))
    {
        return ExplainInput(Self, RequestId, Payload, Socket);
    }
    if (LowerSubAction == TEXT("explain_widget"))
    {
        return ExplainWidget(Self, RequestId, Payload, Socket);
    }
    if (LowerSubAction == TEXT("explain_audible"))
    {
        return ExplainAudible(Self, RequestId, Payload, Socket);
    }
    if (LowerSubAction == TEXT("explain_lit"))
    {
        return ExplainLit(Self, RequestId, Payload, Socket);
    }
    return false;
}
} // namespace McpExplain
