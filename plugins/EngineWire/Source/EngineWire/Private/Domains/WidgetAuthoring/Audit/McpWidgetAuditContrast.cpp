// low_contrast findings of audit_widget (see McpWidgetAudit.h): the WCAG 2.x contrast of each text against what is
// drawn behind it.
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

#include "Foundation/Solvers/McpKMeans.h"

namespace McpWidgetAudit
{
namespace
{
constexpr double AuditLargeText = 18.0;     // points: from here (14 when bold) WCAG asks 3:1, below that 4.5:1
constexpr double AuditLargeBoldText = 14.0;
constexpr int32 AuditMaxSamples = 4096;     // pixels clustered per text
const FColor AuditStandIn(24, 24, 28);      // the preview's backdrop where nothing drew: the game behind is not known here

double AuditLuminance(const FColor& Color)
{
    const FLinearColor Linear = FLinearColor::FromSRGBColor(Color);
    return 0.2126 * Linear.R + 0.7152 * Linear.G + 0.0722 * Linear.B;
}

FColor AuditOver(const FColor& Pixel, const FColor& Back)
{
    const float Alpha = Pixel.A / 255.f;
    return FColor(FMath::RoundToInt(Pixel.R * Alpha + Back.R * (1.f - Alpha)), FMath::RoundToInt(Pixel.G * Alpha + Back.G * (1.f - Alpha)),
                  FMath::RoundToInt(Pixel.B * Alpha + Back.B * (1.f - Alpha)), 255);
}

FColor AuditColor(const FVector3f& Center)
{
    return FColor(FMath::Clamp(FMath::RoundToInt(Center.X), 0, 255), FMath::Clamp(FMath::RoundToInt(Center.Y), 0, 255),
                  FMath::Clamp(FMath::RoundToInt(Center.Z), 0, 255), 255);
}

FString AuditHex(const FColor& Color)
{
    return FString::Printf(TEXT("#%02X%02X%02X"), Color.R, Color.G, Color.B);
}
} // namespace

double ContrastRatio(const FColor& A, const FColor& B)
{
    const double LA = AuditLuminance(A);
    const double LB = AuditLuminance(B);
    return (FMath::Max(LA, LB) + 0.05) / (FMath::Min(LA, LB) + 0.05);
}

void CheckContrast(const TArray<FItem>& Items, const TArray<FColor>& Pixels, FIntPoint Size, TArray<FFinding>& Out)
{
    for (const FItem& Item : Items)
    {
        const int32 X0 = FMath::Clamp(FMath::FloorToInt(Item.Rect.Min.X), 0, Size.X);
        const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Item.Rect.Min.Y), 0, Size.Y);
        const int32 X1 = FMath::Clamp(FMath::CeilToInt(Item.Rect.Max.X), 0, Size.X);
        const int32 Y1 = FMath::Clamp(FMath::CeilToInt(Item.Rect.Max.Y), 0, Size.Y);
        if (Item.Kind != EKind::Text || Item.Text.TrimStartAndEnd().IsEmpty() || X1 - X0 < 2 || Y1 - Y0 < 2 ||
            Pixels.Num() != Size.X * Size.Y)
        {
            continue;
        }
        const int32 Step = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(double(X1 - X0) * (Y1 - Y0) / AuditMaxSamples)));
        TArray<FVector3f> Samples;
        for (int32 Y = Y0; Y < Y1; Y += Step)
        {
            for (int32 X = X0; X < X1; X += Step)
            {
                const FColor Pixel = AuditOver(Pixels[Y * Size.X + X], AuditStandIn);
                Samples.Add(FVector3f(Pixel.R, Pixel.G, Pixel.B));
            }
        }
        // Glyphs cover the lesser part of their box: the heavier cluster is what is behind them. The text is its own
        // colour when it sets one (over that backdrop by its alpha), else the other cluster.
        const TArray<McpKMeans::FCluster> Clusters = McpKMeans::Cluster(Samples, nullptr, 2, 12, 1);
        if (Clusters.Num() == 0)
        {
            continue;
        }
        const FColor Back = AuditColor(Clusters[0].Center);
        const FColor Fore = Item.bColorSet ? AuditOver(Item.Color.ToFColor(true), Back)
                                           : (Clusters.Num() > 1 ? AuditColor(Clusters[1].Center) : Back);
        const double Ratio = ContrastRatio(Fore, Back);
        const bool bLarge = Item.FontSize >= AuditLargeText || (Item.bBold && Item.FontSize >= AuditLargeBoldText);
        const double Need = bLarge ? 3.0 : 4.5;
        if (Ratio < Need)
        {
            FFinding Finding;
            Finding.Kind = TEXT("low_contrast");
            Finding.Severity = 2;
            Finding.Widget = Item.Name;
            Finding.Rect = Item.Rect;
            Finding.Detail = FString::Printf(TEXT("%.1f:1 (%s on %s), under the %.1f:1 %s text needs"), Ratio, *AuditHex(Fore),
                                             *AuditHex(Back), Need, bLarge ? TEXT("large") : TEXT("normal"));
            Out.Add(Finding);
        }
    }
}
} // namespace McpWidgetAudit
