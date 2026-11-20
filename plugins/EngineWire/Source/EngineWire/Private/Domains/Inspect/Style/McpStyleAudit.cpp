// inspect audit_style (S4): placed static mesh copies that stand out in the open level, by a robust z-score
// (McpStyleOutliers.h): a copy scaled unlike the other copies of its mesh (the crate at 3x among forty at 1x), and a
// prop whose own material colour is far more or less saturated, or far lighter or darker, than the level's other
// props (the colour its material instance sets, in OKLab, as the eye sees the difference).

#include "Domains/Inspect/Style/McpStyleOutliers.h"

#include "Components/StaticMeshComponent.h"
#include "Domains/Perception/McpPerception.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Materials/MaterialInstance.h"

namespace McpStyleOutliers
{
namespace AuditDetail
{
// The colour a copy's first material instance sets for itself: its first overridden vector parameter named like a
// colour (BaseColor, Tint, Albedo). A material with no such override gives none.
bool MaterialColour(const UStaticMeshComponent& Mesh, FLinearColor& Out)
{
    const UMaterialInstance* Instance = Cast<UMaterialInstance>(Mesh.GetMaterial(0));
    for (int32 Index = 0; Instance && Index < Instance->VectorParameterValues.Num(); ++Index)
    {
        const FString Name = Instance->VectorParameterValues[Index].ParameterInfo.Name.ToString();
        if (Name.Contains(TEXT("Color")) || Name.Contains(TEXT("Colour")) || Name.Contains(TEXT("Albedo")) || Name.Contains(TEXT("Tint")))
        {
            Out = Instance->VectorParameterValues[Index].ParameterValue;
            return true;
        }
    }
    return false;
}

double Round(double Value, double Steps) { return FMath::RoundToDouble(Value * Steps) / Steps; }
} // namespace AuditDetail

bool HandleAuditStyle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace AuditDetail;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    TArray<FSample> Scale, Saturation, Lightness;
    TMap<FString, FString> MeshOf;
    for (TActorIterator<AActor> It(World); World && It; ++It)
    {
        const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(It->GetRootComponent());
        if (!Mesh || !Mesh->GetStaticMesh())
        {
            continue;
        }
        const FString Label = It->GetActorLabel();
        MeshOf.Add(Label, Mesh->GetStaticMesh()->GetPathName());
        Scale.Add({Mesh->GetStaticMesh()->GetPathName(), Label, It->GetActorScale3D().GetAbsMax()});
        FLinearColor Linear;
        if (MaterialColour(*Mesh, Linear))
        {
            const FVector3f Lab = McpPerception::OkLab(FLinearColor(McpPerception::LinearToSrgb(Linear.R),
                McpPerception::LinearToSrgb(Linear.G), McpPerception::LinearToSrgb(Linear.B)));
            Saturation.Add({TEXT("saturation"), Label, FMath::Sqrt(Lab.Y * Lab.Y + Lab.Z * Lab.Z)});
            Lightness.Add({TEXT("lightness"), Label, Lab.X});
        }
    }
    const double Threshold = FMath::Clamp(GetJsonNumberField(Payload, TEXT("threshold"), 3.5), 2.0, 10.0);
    const int32 MinCopies = FMath::Clamp(GetJsonIntField(Payload, TEXT("minCopies"), 5), 3, 100);
    TArray<TPair<FString, FOutlier>> Found;
    for (const FOutlier& Each : Find(Scale, Threshold, MinCopies)) Found.Add({TEXT("scale"), Each});
    for (const TArray<FSample>* Colour : {&Saturation, &Lightness})
    {
        for (const FOutlier& Each : Find(*Colour, Threshold, MinCopies)) Found.Add({Each.Group, Each});
    }
    Found.Sort([](const TPair<FString, FOutlier>& A, const TPair<FString, FOutlier>& B) { return A.Value.Score > B.Value.Score; });
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < FMath::Min(Found.Num(), 50); ++Index)
    {
        const FOutlier& Each = Found[Index].Value;
        const bool bScale = Found[Index].Key == TEXT("scale");
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"), Each.Name);
        Row->SetStringField(TEXT("mesh"), MeshOf.FindRef(Each.Name));
        Row->SetStringField(TEXT("statistic"), Found[Index].Key);
        Row->SetNumberField(TEXT("value"), Round(Each.Value, bScale ? 100.0 : 1000.0));
        Row->SetNumberField(TEXT("typical"), Round(Each.Median, bScale ? 100.0 : 1000.0));
        Row->SetNumberField(TEXT("score"), Round(Each.Score, 10.0));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("outliers"), Rows);
    Result->SetNumberField(TEXT("outlierCount"), Found.Num());
    Result->SetNumberField(TEXT("actorsRead"), Scale.Num());
    Result->SetNumberField(TEXT("coloursRead"), Saturation.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, Found.Num() == 0
        ? FString::Printf(TEXT("Nothing stands out among %d mesh actors (%d with a material colour of their own)."), Scale.Num(), Saturation.Num())
        : FString::Printf(TEXT("Props unlike the rest of the level: %d (largest score first)."), Found.Num()), Result);
    return true;
}
} // namespace McpStyleOutliers
