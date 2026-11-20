// system_control propose_recipe (McpRecipeInduce.h): the calls kept per caller, and the handler.

#include "Domains/Scripts/McpRecipeInduce.h"

#include "Core/Security/McpPrequeueGate.h"
#include "Domains/Scripts/McpScript.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpRecipeInduceDetail
{
constexpr int32 MaxKept = 32;
constexpr int32 MaxDataChars = 16384; // a reply larger than this is kept without its data (a long list)

// ponytail: one list per principal identity with no eviction, as the loop guard keeps its streaks; identities are few.
FCriticalSection GMutex;
TMap<FString, TArray<McpRecipeInduce::FCall>> GKept;

// A copy that nothing the reply goes on to do can change; null when it is larger than Max characters.
TSharedPtr<FJsonObject> Copy(const TSharedPtr<FJsonObject>& Object, int32 Max)
{
    FString Text;
    TSharedPtr<FJsonObject> Out;
    if (Object.IsValid() && FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)) && Text.Len() <= Max)
    {
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out);
    }
    return Out;
}

// A recipe's own calls are no step of one.
bool IsRecipeCall(const FString& Capability)
{
    return Capability.EndsWith(TEXT(".propose_recipe")) || Capability.EndsWith(TEXT(".save_recipe")) ||
           Capability.EndsWith(TEXT(".list_recipes"));
}
} // namespace McpRecipeInduceDetail

namespace McpRecipeInduce
{
using namespace McpRecipeInduceDetail;

void Note(const TSharedPtr<FMcpPageOwner>& Owner, bool bSuccess, const TSharedPtr<FJsonObject>& Receipt)
{
    const TSharedPtr<FJsonObject>* Params = nullptr;
    if (!bSuccess || !Owner.IsValid() || !Owner->Call.IsValid() || IsRecipeCall(Owner->CapabilityId))
    {
        return;
    }
    FCall Call;
    Call.Capability = Owner->CapabilityId;
    Call.Params = Owner->Call->TryGetObjectField(TEXT("params"), Params) && Params ? Copy(*Params, MaxDataChars) : nullptr;
    const TSharedPtr<FJsonObject>* Data = nullptr;
    Call.Data = Receipt.IsValid() && Receipt->TryGetObjectField(TEXT("data"), Data) && Data ? Copy(*Data, MaxDataChars) : nullptr;
    if (!Call.Params.IsValid())
    {
        Call.Params = MakeShared<FJsonObject>();
    }
    FScopeLock Lock(&GMutex);
    TArray<FCall>& List = GKept.FindOrAdd(Owner->Principal);
    if (List.Num() >= MaxKept)
    {
        List.RemoveAt(0);
    }
    List.Add(MoveTemp(Call));
}

TArray<FCall> Recent(const FString& Principal)
{
    FScopeLock Lock(&GMutex);
    const TArray<FCall>* List = GKept.Find(Principal);
    return List ? *List : TArray<FCall>();
}

bool HandlePropose(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    FMcpCapabilityPrincipal Principal;
    if (!McpPrequeueGate::FindRequestPrincipal(RequestId, Principal))
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("This request reached the editor with no authorized caller, so it has no calls of its own to read."), TEXT("SCRIPT_NO_PRINCIPAL"));
        return true;
    }
    TArray<FCall> Calls = Recent(Principal.Identity);
    TArray<TSharedPtr<FJsonValue>> Skipped;
    Calls.RemoveAll([&Skipped](const FCall& Call)
    {
        FString Code;
        const bool bRefused = !McpScript::StepRefusal(Call.Capability, {Call.Capability}, Code).IsEmpty();
        if (bRefused)
        {
            Skipped.Add(MakeShared<FJsonValueString>(Call.Capability));
        }
        return bRefused;
    });
    const int32 Steps = FMath::Clamp(GetJsonIntField(Payload, TEXT("steps"), Calls.Num()), 1, MaxKept);
    if (Calls.Num() > Steps)
    {
        Calls.RemoveAt(0, Calls.Num() - Steps);
    }
    if (Calls.Num() == 0)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("No successful call of yours is kept to propose a recipe from: the last 32 calls that succeeded since the editor started are kept, and a run_script, undo or restore point is no step of one."), TEXT("NOTHING_TO_PROPOSE"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = Induce(Calls);
    if (Skipped.Num() > 0)
    {
        Result->SetArrayField(TEXT("skipped"), Skipped);
    }
    // The approval is this call, made by the caller: save_recipe with the proposal as it stands.
    TSharedPtr<FJsonObject> SaveParams = MakeShared<FJsonObject>();
    SaveParams->SetStringField(TEXT("name"), GetJsonStringField(Payload, TEXT("name"), TEXT("proposed_recipe")));
    SaveParams->SetStringField(TEXT("summary"), GetJsonStringField(Payload, TEXT("summary"), TEXT("")));
    for (const TCHAR* Field : {TEXT("script"), TEXT("paramNames"), TEXT("destructive")})
    {
        SaveParams->SetField(Field, Result->TryGetField(Field));
    }
    TSharedPtr<FJsonObject> SaveCall = MakeShared<FJsonObject>();
    SaveCall->SetStringField(TEXT("operation"), TEXT("execute"));
    SaveCall->SetStringField(TEXT("tool"), TEXT("system_control"));
    SaveCall->SetStringField(TEXT("action"), TEXT("save_recipe"));
    SaveCall->SetObjectField(TEXT("params"), SaveParams);
    Result->SetObjectField(TEXT("saveCall"), SaveCall);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(
        TEXT("Proposed a %d-step recipe with %d parameter(s) from your last %d call(s). Nothing is saved: read the script, then send saveCall (with a summary a caller would search for) to keep it."),
        int32(Result->GetNumberField(TEXT("stepCount"))), Result->GetArrayField(TEXT("paramNames")).Num(), Calls.Num()), Result);
    return true;
}
} // namespace McpRecipeInduce
