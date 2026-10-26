// manage_blueprint audit_widget (see McpWidgetAudit.h): the Widget Blueprint drawn at each screen size asked for, its
// findings merged across the sizes (one row per finding, with the sizes it shows at), the most severe first.
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScreenshotResample.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace McpWidgetAudit
{
bool ParseSizes(const TArray<TSharedPtr<FJsonValue>>* Values, const TCHAR* Field, TArray<FIntPoint>& Out, FString& OutError)
{
    constexpr int32 MaxSizes = 6;
    if (!Values)
    {
        Out = {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(3440, 1440)};
        return true;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString Width;
        FString Height;
        const FString Text = Value.IsValid() && Value->Type == EJson::String ? Value->AsString().ToLower() : FString();
        const bool bPair = Text.Split(TEXT("x"), &Width, &Height) && Width.IsNumeric() && Height.IsNumeric();
        const FIntPoint Size = bPair ? FIntPoint(FCString::Atoi(*Width), FCString::Atoi(*Height)) : FIntPoint::ZeroValue;
        if (Size.X < 64 || Size.X > 3840 || Size.Y < 64 || Size.Y > 2160)
        {
            OutError = FString::Printf(TEXT("%s: '%s' is not \"WxH\" with a width of 64-3840 and a height of 64-2160."), Field, *Text);
            return false;
        }
        Out.AddUnique(Size);
    }
    if (Out.Num() == 0 || Out.Num() > MaxSizes)
    {
        OutError = FString::Printf(TEXT("%s takes 1 to %d sizes; got %d."), Field, MaxSizes, Out.Num());
        return false;
    }
    return true;
}
} // namespace McpWidgetAudit

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
constexpr int32 AuditMaxFindings = 100;

struct FAuditRow
{
    McpWidgetAudit::FFinding Finding;
    TArray<FString> Sizes;
};

TSharedPtr<FJsonObject> AuditRowJson(const FAuditRow& Row)
{
    const McpWidgetAudit::FFinding& Finding = Row.Finding;
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("kind"), Finding.Kind);
    Json->SetStringField(TEXT("widget"), Finding.Widget);
    if (!Finding.Other.IsEmpty())
    {
        Json->SetStringField(TEXT("other"), Finding.Other);
    }
    TArray<TSharedPtr<FJsonValue>> Sizes;
    for (const FString& Size : Row.Sizes)
    {
        Sizes.Add(MakeShared<FJsonValueString>(Size));
    }
    Json->SetArrayField(TEXT("resolutions"), Sizes);
    const FVector2D Min = Finding.Rect.bIsValid ? Finding.Rect.Min : FVector2D::ZeroVector;
    const FVector2D Extent = Finding.Rect.bIsValid ? Finding.Rect.GetSize() : FVector2D::ZeroVector;
    Json->SetArrayField(TEXT("rect"), {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Min.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Min.Y)),
                                       MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Extent.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Extent.Y))});
    Json->SetStringField(TEXT("detail"), Finding.Detail);
    return Json;
}

// The first size's picture over the preview's dark backdrop, each finding outlined (red, orange, yellow, grey by
// severity) where it shows at that size.
void AuditPicture(TArray<FColor>& Pixels, FIntPoint Size, const TArray<McpWidgetAudit::FFinding>& Findings)
{
    for (FColor& Pixel : Pixels)
    {
        const float Alpha = Pixel.A / 255.f;
        Pixel = FColor(FMath::RoundToInt(Pixel.R * Alpha + 24 * (1.f - Alpha)), FMath::RoundToInt(Pixel.G * Alpha + 24 * (1.f - Alpha)),
                       FMath::RoundToInt(Pixel.B * Alpha + 28 * (1.f - Alpha)), 255);
    }
    static const FColor Marks[4] = {FColor(160, 160, 160), FColor(255, 220, 0), FColor(255, 140, 0), FColor(255, 40, 40)};
    for (const McpWidgetAudit::FFinding& Finding : Findings)
    {
        const int32 X0 = FMath::Clamp(FMath::FloorToInt(Finding.Rect.Min.X), 0, Size.X - 1);
        const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Finding.Rect.Min.Y), 0, Size.Y - 1);
        const int32 X1 = FMath::Clamp(FMath::CeilToInt(Finding.Rect.Max.X), 0, Size.X - 1);
        const int32 Y1 = FMath::Clamp(FMath::CeilToInt(Finding.Rect.Max.Y), 0, Size.Y - 1);
        for (int32 Y = Y0; Y <= Y1; ++Y)
        {
            for (int32 X = X0; X <= X1; ++X)
            {
                if (X - X0 < 2 || X1 - X < 2 || Y - Y0 < 2 || Y1 - Y < 2)
                {
                    Pixels[Y * Size.X + X] = Marks[FMath::Clamp(Finding.Severity, 0, 3)];
                }
            }
        }
    }
}
} // namespace

bool HandleWidgetAuthoringAudit(UEngineWireSubsystem& Subsystem, const FString& RequestId, const FString& SubAction,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                TSharedPtr<FJsonObject> ResultJson)
{
    if (!SubAction.Equals(TEXT("audit_widget"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    TArray<FIntPoint> Sizes;
    FString Error;
    const TArray<TSharedPtr<FJsonValue>>* SizeJson = nullptr;
    Payload->TryGetArrayField(TEXT("resolutions"), SizeJson);
    if (!WidgetBP || !McpWidgetAudit::ParseSizes(SizeJson, TEXT("resolutions"), Sizes, Error))
    {
        Subsystem.SendAutomationError(Socket, RequestId, WidgetBP ? Error : TEXT("No Widget Blueprint at widgetPath."),
                                      WidgetBP ? TEXT("INVALID_ARGUMENT") : TEXT("NOT_FOUND"));
        return true;
    }
    McpWidgetAudit::FOptions Options;
    Options.SafeZone = FMath::Clamp(GetJsonNumberField(Payload, TEXT("safeZone"), 0.9), 0.5, 1.0);
    Options.MinTarget = GetJsonBoolField(Payload, TEXT("touch"), false) ? 44.0 : 0.0;
    TArray<FAuditRow> Rows;
    TArray<FColor> FirstPixels;
    TArray<McpWidgetAudit::FFinding> FirstFindings;
    int32 Checked = 0;
    for (const FIntPoint& Size : Sizes)
    {
        TArray<McpWidgetAudit::FItem> Items;
        TArray<FColor> Pixels;
        if (!McpDrawWidget(WidgetBP, Size, Pixels, Error, [&Items](UUserWidget& Widget, float) { Items = McpWidgetAudit::Collect(Widget); }))
        {
            Subsystem.SendAutomationError(Socket, RequestId, Error, TEXT("AUDIT_FAILED"));
            return true;
        }
        TArray<McpWidgetAudit::FFinding> Findings;
        McpWidgetAudit::CheckLayout(Items, Size, Options, Findings);
        McpWidgetAudit::CheckContrast(Items, Pixels, Size, Findings);
        const FString Label = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
        for (const McpWidgetAudit::FFinding& Finding : Findings)
        {
            FAuditRow* Row = Rows.FindByPredicate([&Finding](const FAuditRow& Seen)
                { return Seen.Finding.Kind == Finding.Kind && Seen.Finding.Widget == Finding.Widget && Seen.Finding.Other == Finding.Other; });
            Row = Row ? Row : &Rows.Add_GetRef({Finding, {}});
            Row->Sizes.Add(Label);
        }
        if (FirstPixels.Num() == 0)
        {
            FirstPixels = MoveTemp(Pixels);
            FirstFindings = MoveTemp(Findings);
            Checked = Items.Num();
        }
    }
    // A material's UI shaders compile the first time it is drawn, so the drawing above is what starts them: with
    // waitForShaders the whole audit runs again once they are done.
    if (McpDeferForShaderCompile(Payload, [Weak = TWeakObjectPtr<UEngineWireSubsystem>(&Subsystem), RequestId, Socket,
                                           ResultJson](const TSharedPtr<FJsonObject>& Resumed)
        {
            if (UEngineWireSubsystem* Self = Weak.Get())
            {
                HandleWidgetAuthoringAudit(*Self, RequestId, TEXT("audit_widget"), Resumed, Socket, ResultJson);
            }
        }))
    {
        return true;
    }
    Rows.StableSort([](const FAuditRow& L, const FAuditRow& R) { return L.Finding.Severity > R.Finding.Severity; });
    TMap<FString, int32> Counts;
    TArray<TSharedPtr<FJsonValue>> RowsJson;
    for (const FAuditRow& Row : Rows)
    {
        ++Counts.FindOrAdd(Row.Finding.Kind);
        if (RowsJson.Num() < AuditMaxFindings)
        {
            RowsJson.Add(MakeShared<FJsonValueObject>(AuditRowJson(Row)));
        }
    }
    TSharedPtr<FJsonObject> CountsJson = MakeShared<FJsonObject>();
    TArray<FString> Parts;
    for (const TPair<FString, int32>& Count : Counts)
    {
        CountsJson->SetNumberField(Count.Key, Count.Value);
        Parts.Add(FString::Printf(TEXT("%d %s"), Count.Value, *Count.Key));
    }
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetArrayField(TEXT("findings"), RowsJson);
    ResultJson->SetObjectField(TEXT("counts"), CountsJson);
    ResultJson->SetNumberField(TEXT("widgetsChecked"), Checked);
    McpHandlerUtils::MarkNoAssetsChanged(ResultJson);
    if (GetJsonBoolField(Payload, TEXT("image"), false))
    {
        AuditPicture(FirstPixels, Sizes[0], FirstFindings);
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Sizes[0].X, Sizes[0].Y, TArrayView64<const FColor>(FirstPixels.GetData(), FirstPixels.Num()), Png);
        ResultJson->SetStringField(TEXT("mimeType"), TEXT("image/png"));
        ResultJson->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
    }
    McpAddShaderCompileState(ResultJson, Payload);
    Subsystem.SendAutomationResponse(Socket, RequestId, true, Rows.Num() == 0
        ? FString::Printf(TEXT("No findings at %d screen size(s); %d widgets checked."), Sizes.Num(), Checked)
        : FString::Printf(TEXT("%d finding(s) at %d screen size(s): %s."), Rows.Num(), Sizes.Num(), *FString::Join(Parts, TEXT(", "))),
        ResultJson);
    return true;
}
} // namespace WidgetAuthoringHandlers
