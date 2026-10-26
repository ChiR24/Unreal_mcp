#include "Domains/Scripts/McpScript.h"

#include "Core/Security/McpPrequeueGate.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"

// Which capabilities a script may call: asked by the check before anything runs, and again by each step for the
// capability the gateway resolved its name to.
namespace McpScript
{
FString ResolveCapability(const FString& Name)
{
    const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
    FString Target;
    if (Index.FindById(Name))
    {
        return Name;
    }
    return Index.ResolveAlias(Name, Target) == EMcpAliasResolution::Unique && Index.FindById(Target) ? Target : FString();
}

bool AsksConsent(const FString& CapabilityId)
{
    FMcpPrequeueRequest Request;
    Request.CapabilityId = CapabilityId;
    const FMcpCapabilityDemand Demand = McpPrequeueGate::ResolveDemand(Request);
    // A capability the gate cannot resolve is held to consent too: fail closed.
    return Demand.CapabilityId.IsEmpty() || !Demand.ConsentMode.Equals(TEXT("none"), ESearchCase::IgnoreCase);
}

FString StepRefusal(const FString& CapabilityId, const TSet<FString>& Allowed, FString& OutCode)
{
    OutCode = TEXT("SCRIPT_STEP_REFUSED");
    if (CapabilityId == TEXT("system_control.run_script") || CapabilityId == TEXT("system_control.run_recipe"))
    {
        return TEXT("a script cannot run another script (a recipe runs as one).");
    }
    if (CapabilityId == TEXT("control_editor.undo"))
    {
        return TEXT("a script rolls back through the undo history, so its steps cannot move it (undo, redo, restore_point).");
    }
    OutCode = TEXT("CONSENT_NOT_LISTED");
    return AsksConsent(CapabilityId) && !Allowed.Contains(CapabilityId)
        ? FString::Printf(TEXT("%s asks for consent: list it in destructive, and this run's consent covers it."), *CapabilityId)
        : FString();
}
} // namespace McpScript
