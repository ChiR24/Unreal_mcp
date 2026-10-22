#include "Domains/PCG/Execution/McpPCGExecution.h"
#if MCP_HAS_PCG_EXECUTION
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Foundation/BridgeHelpers/Security/McpSubobjectPath.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "Misc/SecureHash.h"

namespace McpPCGHandlers
{
UPCGComponent* ResolveExactPCGComponent(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    FString Path, Package;
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World || !McpSubobjectPath::TryCanonicalize(GetJsonStringField(Payload, TEXT("componentPath")), Path, Package))
    {
        OutError = TEXT("An exact absolute componentPath in the current editor world is required.");
        return nullptr;
    }
    AActor* Actor = nullptr;
    UPCGComponent* Component = FindPCGComponent(World, FString(), Path, Actor);
    if (!Component || !Component->GetPathName().Equals(Path, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("componentPath did not resolve exactly to a current editor-world PCG component.");
        return nullptr;
    }
    return Component;
}

bool ResolvePCGPrincipal(UEngineWireSubsystem* Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutPrincipal)
{
    (void)Bridge;
    (void)Socket;
    return FMcpRequestOriginRegistry::Get().ResolvePrincipal(RequestId, OutPrincipal);
}

FString GetPCGInstanceRevision(UPCGComponent* Component)
{
    UPCGGraphInstance* Instance = Component ? Component->GetGraphInstance() : nullptr;
    const FInstancedPropertyBag* Bag = Instance ? Instance->GetUserParametersStruct() : nullptr;
    auto Values = MakeShared<FJsonObject>();
    FPCGReadBudget Budget;
    if (!Component || !Budget.CheckParameters(Bag)) return FString();
    Values->SetNumberField(TEXT("componentSeed"), Component->Seed);
    Values->SetNumberField(TEXT("generationGridSize"), Component->GetGenerationGridSize());
    if (!Bag || !Bag->GetPropertyBagStruct())
    {
        FString Json;
        return McpCanonicalJsonObject(Values, Json, true) && Budget.IsWithinTime() ? FMD5::HashAnsiString(*Json) : FString();
    }
    for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyBagStruct()->GetPropertyDescs())
    {
        if (!Desc.CachedProperty) return FString();
        const FString Hash = McpPropertyReflection::FingerprintReflectedValue(Desc.CachedProperty, const_cast<uint8*>(Bag->GetValue().GetMemory()));
        if (Hash.IsEmpty() || !Budget.IsWithinTime()) return FString();
        auto Parameter = MakeShared<FJsonObject>();
        Parameter->SetStringField(TEXT("valueHash"), Hash);
        Parameter->SetBoolField(TEXT("overridden"), Instance->IsPropertyOverridden(Desc.CachedProperty));
        Values->SetObjectField(Desc.ID.ToString(), Parameter);
    }
    FString Json;
    return McpCanonicalJsonObject(Values, Json, true) && Budget.IsWithinTime() ? FMD5::HashAnsiString(*Json) : FString();
}
}
#endif
