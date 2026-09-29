#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TextureResource.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
// Draws the widget offscreen the way the game would at Size (the project's DPI curve applied), in
// design mode like its thumbnail: Construct graphs do not run, so a HUD shows its designer texts.
// The preview used to open and focus the Widget Blueprint editor and return no image at all.
bool McpDrawWidgetPreview(UWidgetBlueprint* WidgetBP, FIntPoint Size, TArray<FColor>& OutPixels, FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UClass* Class = WidgetBP->GeneratedClass;
    if (!World || !Class || !Class->IsChildOf(UUserWidget::StaticClass()))
    {
        OutError = TEXT("The Widget Blueprint has no compiled class to draw; compile it first.");
        return false;
    }
    UUserWidget* Widget = NewObject<UUserWidget>(World, Class, NAME_None, RF_Transient);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
    Widget->Initialize();
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->ClearColor = FLinearColor::Transparent;
    Target->SRGB = true;
    Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    FWidgetRenderer* Renderer = new FWidgetRenderer(false);
    const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
    Renderer->DrawWidget(Target, Slate, Scale, FVector2D(Size.X, Size.Y), 0.f);
    FlushRenderingCommands();
    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    const bool bRead = Resource && Resource->ReadPixels(OutPixels) && OutPixels.Num() == Size.X * Size.Y;
    BeginCleanup(Renderer);
    Widget->MarkAsGarbage();
    // Over dark grey, so white HUD text on nothing still reads.
    for (FColor& Pixel : OutPixels)
    {
        const float Alpha = Pixel.A / 255.f;
        Pixel = FColor(FMath::RoundToInt(Pixel.R * Alpha + 24 * (1.f - Alpha)), FMath::RoundToInt(Pixel.G * Alpha + 24 * (1.f - Alpha)),
                       FMath::RoundToInt(Pixel.B * Alpha + 28 * (1.f - Alpha)), 255);
    }
    if (!bRead)
    {
        OutError = TEXT("The widget drew, but its pixels could not be read back.");
    }
    return bRead;
}
}

bool HandleWidgetAuthoringPreview(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (!SubAction.Equals(TEXT("preview_widget"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, WidgetPath.IsEmpty()
            ? TEXT("Missing required parameter: widgetPath") : TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }
    // resolution "WxH" is the screen the widget is drawn for (default 1280x720).
    FString Resolution = GetJsonStringField(Payload, TEXT("resolution"), TEXT("1280x720"));
    FString Width, Height;
    FIntPoint Size(1280, 720);
    if (Resolution.ToLower().Split(TEXT("x"), &Width, &Height))
    {
        Size = FIntPoint(FMath::Clamp(FCString::Atoi(*Width), 64, 3840), FMath::Clamp(FCString::Atoi(*Height), 64, 2160));
    }
    TArray<FColor> Pixels;
    FString Error;
    TArray64<uint8> Png;
    if (!McpDrawWidgetPreview(WidgetBP, Size, Pixels, Error))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PREVIEW_FAILED"));
        return true;
    }
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
    // openEditor also shows it in the Widget Blueprint editor, which takes focus.
    const bool bOpen = GetJsonBoolField(Payload, TEXT("openEditor"), false);
    UAssetEditorSubsystem* AssetEditors = bOpen && GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
    // The preview only draws the widget: without this the receipt lists widgetPath under changes.
    McpHandlerUtils::MarkNoAssetsChanged(ResultJson);
    ResultJson->SetNumberField(TEXT("width"), Size.X);
    ResultJson->SetNumberField(TEXT("height"), Size.Y);
    ResultJson->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    ResultJson->SetNumberField(TEXT("sizeBytes"), static_cast<double>(Png.Num()));
    ResultJson->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
    ResultJson->SetBoolField(TEXT("editorOpened"), AssetEditors && AssetEditors->OpenEditorForAsset(WidgetBP));
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
        TEXT("Widget drawn at %dx%d (design mode: Construct graphs do not run)"), Size.X, Size.Y), ResultJson);
    return true;
}
}
