// inspect audit_style (S4, first slice): placed copies of one mesh whose scale stands out from the rest of the level's
// copies of it, by a robust z-score (McpStyleOutliers.h): the crate scaled 3x among forty at 1x, or the lamp at 0.2.

#include "Domains/Inspect/Style/McpStyleOutliers.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

namespace McpStyleOutliers
{
bool HandleAuditStyle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    TArray<FSample> Samples;
    for (TActorIterator<AActor> It(World); World && It; ++It)
    {
        const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(It->GetRootComponent());
        if (Mesh && Mesh->GetStaticMesh())
        {
            Samples.Add({Mesh->GetStaticMesh()->GetPathName(), It->GetActorLabel(), It->GetActorScale3D().GetAbsMax()});
        }
    }
    const double Threshold = FMath::Clamp(GetJsonNumberField(Payload, TEXT("threshold"), 3.5), 2.0, 10.0);
    const TArray<FOutlier> Found = Find(Samples, Threshold, FMath::Clamp(GetJsonIntField(Payload, TEXT("minCopies"), 5), 3, 100));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FOutlier& Each : Found)
    {
        if (Rows.Num() >= 50) break;
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Each.Name);
        Row->SetStringField(TEXT("mesh"), Each.Group);
        Row->SetNumberField(TEXT("scale"), FMath::RoundToDouble(Each.Value * 100.0) / 100.0);
        Row->SetNumberField(TEXT("typicalScale"), FMath::RoundToDouble(Each.Median * 100.0) / 100.0);
        Row->SetNumberField(TEXT("score"), FMath::RoundToDouble(Each.Score * 10.0) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("outliers"), Rows);
    Result->SetNumberField(TEXT("outlierCount"), Found.Num());
    Result->SetNumberField(TEXT("actorsRead"), Samples.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, Found.Num() == 0
        ? FString::Printf(TEXT("No copy's scale stands out among %d mesh actors."), Samples.Num())
        : FString::Printf(TEXT("Copies scaled unlike the others of their mesh: %d (largest score first)."), Found.Num()), Result);
    return true;
}
} // namespace McpStyleOutliers
