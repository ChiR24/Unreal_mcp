#include "MCP/Protocol/McpModernConsent.h"

#include "Foundation/BridgeHelpers/Security/McpCapToken.h"
#include "Foundation/McpSecureTokenCompare.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Protocol/McpJsonRpc.h"
#include "MCP/Protocol/McpModernRequest.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpModernConsent
{
namespace
{
// Long enough for a person to read the question and answer it.
constexpr int64 AskLifetimeSeconds = 600;
const TCHAR* const InputKey = TEXT("consent");

FString Sign(const FString& Principal, const FString& Digest, const FString& Payload)
{
    // A fresh key per editor run: a state from an earlier run, or one a client wrote itself, never verifies.
    static const FString Key = McpCapabilityTokenStore::GenerateRandomToken();
    const FTCHARToUTF8 KeyBytes(*Key);
    const FTCHARToUTF8 Data(*FString::Printf(TEXT("%s\n%s\n%s"), *Principal, *Digest, *Payload));
    uint8 Hash[20];
    FSHA1::HMACBuffer(KeyBytes.Get(), KeyBytes.Length(), Data.Get(), Data.Length(), Hash);
    FString Hex;
    for (const uint8 Byte : Hash)
    {
        Hex.Appendf(TEXT("%02x"), Byte);
    }
    return Hex;
}

// What the call is about to touch, for the question: its scalar arguments, shortened.
FString DescribeArguments(const TSharedPtr<FJsonObject>& Arguments)
{
    TArray<FString> Parts;
    if (Arguments.IsValid())
    {
        for (const auto& Pair : Arguments->Values)
        {
            const FString Key(Pair.Key.Len(), *Pair.Key);
            FString Text;
            if (Key == TEXT("action") || Key == TEXT("subAction") || !Pair.Value.IsValid() ||
                !Pair.Value->TryGetString(Text))
            {
                continue;
            }
            Parts.Add(FString::Printf(TEXT("%s=%s"), *Key, *Text.Left(80)));
            if (Parts.Num() == 6)
            {
                break;
            }
        }
    }
    return Parts.Num() == 0 ? FString() : FString::Printf(TEXT(" (%s)"), *FString::Join(Parts, TEXT(", ")));
}

TSharedPtr<FJsonObject> MakeAsk(const FString& Principal, const FString& Digest, const FString& CapabilityId,
                                const FString& ConsentMode, const FString& Target,
                                const TSharedPtr<FJsonObject>& Arguments)
{
    TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
    State->SetStringField(TEXT("capability"), CapabilityId);
    State->SetStringField(TEXT("acknowledge"), ConsentMode);
    State->SetNumberField(TEXT("expires"), static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp() + AskLifetimeSeconds));
    State->SetStringField(TEXT("nonce"), FGuid::NewGuid().ToString());
    const FTCHARToUTF8 StateBytes(*FMcpJsonRpc::JsonToString(State));
    const FString Payload = FBase64::Encode(reinterpret_cast<const uint8*>(StateBytes.Get()), StateBytes.Length());

    TSharedPtr<FJsonObject> Confirm = MakeShared<FJsonObject>();
    Confirm->SetStringField(TEXT("type"), TEXT("boolean"));
    Confirm->SetStringField(TEXT("title"), TEXT("Allow this call"));
    Confirm->SetBoolField(TEXT("default"), false);
    TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
    Properties->SetObjectField(TEXT("confirm"), Confirm);
    TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
    Schema->SetStringField(TEXT("type"), TEXT("object"));
    Schema->SetObjectField(TEXT("properties"), Properties);
    Schema->SetArrayField(TEXT("required"), {MakeShared<FJsonValueString>(TEXT("confirm"))});

    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("mode"), TEXT("form"));
    Params->SetStringField(TEXT("message"), FString::Printf(
        TEXT("Unreal Engine asks before it runs %s%s: this capability needs %s consent. Allow it?"),
        *Target, *DescribeArguments(Arguments), *ConsentMode));
    Params->SetObjectField(TEXT("requestedSchema"), Schema);
    TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
    Request->SetStringField(TEXT("method"), TEXT("elicitation/create"));
    Request->SetObjectField(TEXT("params"), Params);
    TSharedPtr<FJsonObject> Requests = MakeShared<FJsonObject>();
    Requests->SetObjectField(InputKey, Request);

    TSharedPtr<FJsonObject> Ask = MakeShared<FJsonObject>();
    Ask->SetStringField(TEXT("resultType"), TEXT("input_required"));
    Ask->SetObjectField(TEXT("inputRequests"), Requests);
    Ask->SetStringField(TEXT("requestState"), Payload + TEXT(".") + Sign(Principal, Digest, Payload));
    return Ask;
}

// True with the grant's mode and nonce when the state was minted in this editor run, for this principal and this
// exact call, and has not expired. Anything else (another call's or principal's state, an edited or stale one)
// is false.
bool Verify(const FString& RequestState, const FString& Principal, const FString& Digest,
            const FString& CapabilityId, FString& OutMode, FString& OutNonce)
{
    FString Payload;
    FString Signature;
    TArray<uint8> Bytes;
    if (!RequestState.Split(TEXT("."), &Payload, &Signature) ||
        !McpConstantTimeTokenEquals(Signature, Sign(Principal, Digest, Payload)) || !FBase64::Decode(Payload, Bytes))
    {
        return false;
    }
    const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FString(Text.Length(), Text.Get()));
    TSharedPtr<FJsonObject> State;
    FString Capability;
    double Expires = 0.0;
    return FJsonSerializer::Deserialize(Reader, State) && State.IsValid() &&
        State->TryGetStringField(TEXT("capability"), Capability) &&
        Capability.Equals(CapabilityId, ESearchCase::CaseSensitive) &&
        State->TryGetNumberField(TEXT("expires"), Expires) &&
        Expires >= static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()) &&
        State->TryGetStringField(TEXT("acknowledge"), OutMode) &&
        State->TryGetStringField(TEXT("nonce"), OutNonce) && !OutNonce.IsEmpty();
}
} // namespace

FOutcome Resolve(const FString& Principal, const FString& CapabilityId, const FString& ConsentMode,
                 const FString& Target, const TSharedPtr<FJsonObject>& Arguments)
{
    FOutcome Out;
    const McpModern::FCall& Call = McpModern::Current();
    if (!Call.bActive || Call.Method != TEXT("tools/call"))
    {
        return Out;
    }
    const FString Digest = McpCanonicalFingerprint(CapabilityId, Arguments);
    FString Mode;
    FString Nonce;
    // Another call's, principal's or editor run's state, or an edited or stale one, refuses the request outright.
    if (!Call.RequestState.IsEmpty() && !Verify(Call.RequestState, Principal, Digest, CapabilityId, Mode, Nonce))
    {
        Out.bInvalidState = true;
        return Out;
    }
    const TSharedPtr<FJsonObject>* Answer = nullptr;
    if (!Call.RequestState.IsEmpty() && Call.InputResponses.IsValid() &&
        Call.InputResponses->TryGetObjectField(InputKey, Answer) && Answer)
    {
        FString Action;
        bool bConfirm = false;
        const TSharedPtr<FJsonObject>* Content = nullptr;
        (*Answer)->TryGetStringField(TEXT("action"), Action);
        if (Action == TEXT("accept") && (*Answer)->TryGetObjectField(TEXT("content"), Content) && Content &&
            (*Content)->TryGetBoolField(TEXT("confirm"), bConfirm) && bConfirm)
        {
            Out.Grant = MakeShared<FJsonObject>();
            Out.Grant->SetStringField(TEXT("capability"), CapabilityId);
            Out.Grant->SetStringField(TEXT("acknowledge"), Mode);
            Out.Grant->SetStringField(TEXT("nonce"), Nonce);
            return Out;
        }
        Out.Declined = FString::Printf(
            TEXT("The user did not allow %s when the client asked, so it did not run. Do not run it again unless they ask for it."),
            *Target);
        return Out;
    }
    // No answer yet: ask (again) when the client can be asked.
    if (Call.bElicitationForm)
    {
        Out.Ask = MakeAsk(Principal, Digest, CapabilityId, ConsentMode, Target, Arguments);
    }
    return Out;
}

FString InvalidStateError(const TSharedPtr<FJsonValue>& Id)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("reason"), TEXT("invalid_request_state"));
    return FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Invalid or expired requestState"), Data);
}
} // namespace McpModernConsent
