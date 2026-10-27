// Copyright (c) 2024 EngineWire Contributors
//
// inspect audit_materials (McpPerception.h): the level viewport read as analyze_view reads it, then the checks asked:
// pbr (McpPerceptionPbr: base colour, metallic and roughness as the G-buffer holds them, per actor in view),
// texel_density and instancing (McpPerceptionTexel). The camera moves only when location or rotation is given.

#include "Domains/Perception/McpPerception.h"

#include "Algo/Find.h"
#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Domains/Environment/McpEnvShared.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Foundation/McpScreenshotResample.h"

namespace McpPerception
{
namespace
{
const TCHAR* const AuditChecks[] = {TEXT("pbr"), TEXT("texel_density"), TEXT("instancing")};

int32 AuditCount(const TSharedPtr<FJsonObject>& Part, const TCHAR* Field)
{
    double Value = 0.0;
    return Part.IsValid() && Part->TryGetNumberField(Field, Value) ? int32(Value) : 0;
}
} // namespace

bool HandleAuditMaterials(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSet<FString> Checks;
    for (const FString& Asked : McpHandlerUtils::GetStringArrayField(Payload, TEXT("checks")))
    {
        const TCHAR* const* Known = Algo::FindByPredicate(AuditChecks, [&Asked](const TCHAR* Name) { return Asked.Equals(Name, ESearchCase::IgnoreCase); });
        if (!Known)
        {
            Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("checks takes pbr, texel_density and instancing; '%s' is "
                "none of them."), *Asked), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Checks.Add(*Known);
    }
    if (Checks.Num() == 0)
    {
        Checks = {TEXT("pbr"), TEXT("texel_density"), TEXT("instancing")};
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<FString> Said;
    if (Checks.Contains(TEXT("pbr")) || Checks.Contains(TEXT("texel_density")))
    {
        McpEnvironmentHandlers::FMcpViewCapture View;
        FString Error, Code;
        if (!McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("audit_materials"), true, View, Error, Code))
        {
            Bridge.SendAutomationError(Socket, RequestId, Error, Code);
            return true;
        }
        const FFrame Frame = BuildFrame(View);
        if (Checks.Contains(TEXT("pbr")))
        {
            const TSharedPtr<FJsonObject> Pbr = PbrAsRendered(*View.Client, Frame);
            Result->SetObjectField(TEXT("pbr"), Pbr);
            Said.Add(FString::Printf(TEXT("%d of %d actor(s) in view off physically based ranges"), AuditCount(Pbr, TEXT("flagged")), AuditCount(Pbr, TEXT("judged"))));
        }
        if (Checks.Contains(TEXT("texel_density")))
        {
            const TSharedPtr<FJsonObject> Texel = TexelDensity(Frame);
            Result->SetObjectField(TEXT("texelDensity"), Texel);
            Said.Add(FString::Printf(TEXT("%d of %d material slot(s) 4x off the view's texel density"), AuditCount(Texel, TEXT("outliers")), AuditCount(Texel, TEXT("slots"))));
        }
        Result->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(View.Client->GetViewLocation()));
        Result->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(View.Client->GetViewRotation()));
    }
    if (Checks.Contains(TEXT("instancing")))
    {
        const TSharedPtr<FJsonObject> Instancing = InstancingGroups(GEditor->GetEditorWorldContext().World());
        Result->SetObjectField(TEXT("instancing"), Instancing);
        const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
        Said.Add(FString::Printf(TEXT("%d group(s) of repeated meshes to instance"), Instancing->TryGetArrayField(TEXT("groups"), Groups) ? Groups->Num() : 0));
    }
    // Until a material's shaders finish compiling it draws as the engine's stand-in, and its buffers read the stand-in's.
    if (const int32 Jobs = McpShaderJobsRemaining())
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d shader job(s) were still compiling: what they cover drew "
            "with the engine's stand-in material; call again when they finish."), Jobs)));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Join(Said, TEXT("; ")) + TEXT("."), Result, FString());
    return true;
}
} // namespace McpPerception
