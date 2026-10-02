#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

#include "Engine/Font.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace McpTextureHandlers
{
namespace
{
// White text scaled to fit Size inside Padding, drawn offscreen by Slate (the editor UI's own
// rasterizer, so any Font asset and face draws as it does in a widget). Only the alpha of
// OutPixels is used: it is each pixel's coverage, whatever Slate does with gamma or premultiply.
bool McpRasterizeText(const FString& Text, const FSlateFontInfo& Font, ETextJustify::Type Justify,
                      FIntPoint Size, float Padding, TArray<FColor>& OutPixels)
{
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->ClearColor = FLinearColor::Transparent;
    Target->SRGB = true;
    Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    const TSharedRef<SWidget> Widget = SNew(SBox).Padding(FMargin(Padding))
    [
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
        [
            SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(FLinearColor::White).Justification(Justify)
        ]
    ];
    FWidgetRenderer* Renderer = new FWidgetRenderer(false);
    Renderer->DrawWidget(Target, Widget, 1.f, FVector2D(Size.X, Size.Y), 0.f);
    FlushRenderingCommands();
    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    const bool bRead = Resource && Resource->ReadPixels(OutPixels) && OutPixels.Num() == Size.X * Size.Y;
    BeginCleanup(Renderer);
    return bRead;
}
}

TSharedPtr<FJsonObject> HandleCreateTextTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Path;
    FString Name;
    FString Error;
    if (!ResolveOutputTarget(Params, TEXT("/Game/Textures"), FString(), Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }
    const FString Text = GetJsonStringField(Params, TEXT("text"));
    if (Text.TrimStartAndEnd().IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("text is required: the words to draw (a newline starts a new line)."));
    }
    int32 Width = 0;
    int32 Height = 0;
    if (!ValidateGeneratedTextureDimensions(GetJsonNumberField(Params, TEXT("width"), 1024),
                                            GetJsonNumberField(Params, TEXT("height"), 256),
                                            TEXT("width"), TEXT("height"), Width, Height, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    FString Family = GetJsonStringField(Params, TEXT("fontFamily"), TEXT("/Engine/EngineFonts/Roboto"));
    if (!Family.Contains(TEXT(".")))
    {
        Family += TEXT(".") + FPackageName::GetShortName(Family);
    }
    UFont* FontAsset = LoadObject<UFont>(nullptr, *Family);
    const FCompositeFont* Composite = FontAsset ? FontAsset->GetCompositeFont() : nullptr;
    if (!Composite)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("fontFamily '%s' is not a Font asset (e.g. /Engine/EngineFonts/Roboto)."), *Family));
    }
    TArray<FString> Faces;
    for (const FTypefaceEntry& Entry : Composite->DefaultTypeface.Fonts)
    {
        Faces.Add(Entry.Name.ToString());
    }
    const FString Typeface = GetJsonStringField(Params, TEXT("typeface"), Faces.Contains(TEXT("Bold")) ? TEXT("Bold") : TEXT(""));
    if (!Typeface.IsEmpty() && !Faces.Contains(Typeface))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("typeface '%s' is not a face of %s; it has %s."),
                                               *Typeface, *Family, *FString::Join(Faces, TEXT(", "))));
    }
    FSlateFontInfo Font;
    Font.FontObject = FontAsset;
    Font.TypefaceFontName = FName(*Typeface);
    Font.Size = 48;
    Font.LetterSpacing = static_cast<int32>(GetJsonNumberField(Params, TEXT("letterSpacing"), 0));

    const FString Justification = GetJsonStringField(Params, TEXT("justification"), TEXT("center"));
    const ETextJustify::Type Justify = Justification.Equals(TEXT("left"), ESearchCase::IgnoreCase) ? ETextJustify::Left
        : Justification.Equals(TEXT("right"), ESearchCase::IgnoreCase) ? ETextJustify::Right : ETextJustify::Center;
    const float Padding = FMath::Max(0.f, static_cast<float>(GetJsonNumberField(Params, TEXT("padding"), FMath::Min(Width, Height) * 0.04)));
    if (Padding * 2.f >= FMath::Min(Width, Height))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("padding %.0f leaves no room in a %dx%d texture."), Padding, Width, Height));
    }

    TArray<FColor> Coverage;
    if (!McpRasterizeText(Text, Font, Justify, FIntPoint(Width, Height), Padding, Coverage))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Slate drew the text, but its pixels could not be read back."));
    }

    // The text colour over the background, straight alpha: a transparent background keeps the
    // text colour under every edge, so the texture has no dark fringe where it is filtered.
    const FLinearColor Ink = ExtractLinearColorField(Params, TEXT("color"), FLinearColor::White);
    const FLinearColor Back = ExtractLinearColorField(Params, TEXT("backgroundColor"), FLinearColor(0, 0, 0, 0));
    TArray<uint8> PixelData;
    PixelData.SetNumUninitialized(Width * Height * 4);
    FIntPoint InkMin(Width, Height);
    FIntPoint InkMax(-1, -1);
    for (int32 Index = 0; Index < Coverage.Num(); ++Index)
    {
        const float Cover = Coverage[Index].A / 255.f;
        const float InkAlpha = Ink.A * Cover;
        const float BackAlpha = Back.A * (1.f - Cover);
        const float Alpha = InkAlpha + BackAlpha;
        FLinearColor Out = Alpha > 0.f ? (Ink * InkAlpha + Back * BackAlpha) / Alpha : Ink;
        Out.A = Alpha;
        const FColor Quantized = Out.GetClamped().QuantizeRound();
        PixelData[Index * 4 + 0] = Quantized.B;
        PixelData[Index * 4 + 1] = Quantized.G;
        PixelData[Index * 4 + 2] = Quantized.R;
        PixelData[Index * 4 + 3] = Quantized.A;
        if (Coverage[Index].A > 0)
        {
            const FIntPoint Pixel(Index % Width, Index / Width);
            InkMin = FIntPoint(FMath::Min(InkMin.X, Pixel.X), FMath::Min(InkMin.Y, Pixel.Y));
            InkMax = FIntPoint(FMath::Max(InkMax.X, Pixel.X), FMath::Max(InkMax.Y, Pixel.Y));
        }
    }
    if (InkMax.X < 0)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Slate drew no pixels for this text with %s %s; try another fontFamily or typeface."),
                                               *Family, *Typeface));
    }

    UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!NewTexture || !UpdateTextureBGRA8(NewTexture, Width, Height, PixelData))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create the texture"));
    }
    const bool bSaved = SaveTextureAsset(NewTexture);

    TArray<TSharedPtr<FJsonValue>> FaceValues;
    for (const FString& Face : Faces)
    {
        FaceValues.Add(MakeShared<FJsonValueString>(Face));
    }
    const TSharedPtr<FJsonObject> Ink2D = MakeShared<FJsonObject>();
    Ink2D->SetNumberField(TEXT("x"), InkMin.X);
    Ink2D->SetNumberField(TEXT("y"), InkMin.Y);
    Ink2D->SetNumberField(TEXT("width"), InkMax.X - InkMin.X + 1);
    Ink2D->SetNumberField(TEXT("height"), InkMax.Y - InkMin.Y + 1);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Text texture '%s' created"), *Name));
    Response->SetNumberField(TEXT("width"), Width);
    Response->SetNumberField(TEXT("height"), Height);
    Response->SetStringField(TEXT("fontFamily"), Family);
    Response->SetStringField(TEXT("typeface"), Typeface);
    Response->SetArrayField(TEXT("typefaces"), FaceValues);
    Response->SetObjectField(TEXT("inkBounds"), Ink2D);
    Response->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}
