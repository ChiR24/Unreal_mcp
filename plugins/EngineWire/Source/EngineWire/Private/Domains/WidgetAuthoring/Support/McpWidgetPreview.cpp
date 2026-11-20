#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TextureResource.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScreenshotResample.h"
#include "EngineWireSubsystem.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
// The preview used to open and focus the Widget Blueprint editor and return no image at all.
bool McpDrawWidget(UWidgetBlueprint* WidgetBP, FIntPoint Size, TArray<FColor>& OutPixels, FString& OutError,
                   TFunctionRef<void(UUserWidget& Widget, float Scale)> Inspect)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
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
    // Design mode draws every widget (UWidget::GetVisibilityInDesigner), so a Collapsed card the game shows later,
    // or a NEW RECORD pill, appeared in the preview. Each widget gets the visibility it is saved with, read off the
    // property itself: GetVisibility() reports the forced designer state once the Slate widget exists.
    const FProperty* VisibilityProperty = UWidget::StaticClass()->FindPropertyByName(TEXT("Visibility"));
    if (Widget->WidgetTree && VisibilityProperty)
    {
        Widget->WidgetTree->ForEachWidget([VisibilityProperty](UWidget* Child)
        {
            const TSharedPtr<SWidget> Cached = Child ? Child->GetCachedWidget() : nullptr;
            if (Cached.IsValid())
            {
                const ESlateVisibility Saved = *VisibilityProperty->ContainerPtrToValuePtr<ESlateVisibility>(Child);
                Cached->SetVisibility(UWidget::ConvertSerializedVisibilityToRuntime(Saved));
            }
        });
    }
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->ClearColor = FLinearColor::Transparent;
    Target->SRGB = true;
    Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    FWidgetRenderer* Renderer = new FWidgetRenderer(false);
    const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
    // Twice: a wrapping text sizes itself from the width its first arrangement gave it, as in the game's second frame.
    Renderer->DrawWidget(Target, Slate, Scale, FVector2D(Size.X, Size.Y), 0.f);
    Renderer->DrawWidget(Target, Slate, Scale, FVector2D(Size.X, Size.Y), 0.f);
    FlushRenderingCommands();
    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    const bool bRead = Resource && Resource->ReadPixels(OutPixels) && OutPixels.Num() == Size.X * Size.Y;
    Inspect(*Widget, Scale);
    BeginCleanup(Renderer);
    Widget->MarkAsGarbage();
    if (!bRead)
    {
        OutError = TEXT("The widget drew, but its pixels could not be read back.");
    }
    return bRead;
}
}

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringPreview(
    UEngineWireSubsystem& Subsystem,
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
    if (WidgetPath.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
        return true;
    }
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprintOrReply(Subsystem, RequestId, RequestingSocket, WidgetPath);
    if (!WidgetBP)
    {
        return true;
    }
    // resolution "WxH" is the screen the widget is drawn for (default 1280x720), read as arrange_widgets reads its
    // own: a size it cannot read or out of 64-3840 by 64-2160 is refused, where it was clamped or replaced unsaid.
    const TArray<TSharedPtr<FJsonValue>> ResolutionJson = {
        MakeShared<FJsonValueString>(GetJsonStringField(Payload, TEXT("resolution"), TEXT("1280x720")))};
    TArray<FIntPoint> Sizes;
    FString Error;
    if (!McpWidgetAudit::ParseSizes(&ResolutionJson, TEXT("resolution"), Sizes, Error))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FIntPoint Size = Sizes[0];
    TArray<FColor> Pixels;
    TArray64<uint8> Png;
    if (!McpDrawWidget(WidgetBP, Size, Pixels, Error, [](UUserWidget&, float) {}))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PREVIEW_FAILED"));
        return true;
    }
    // background [r, g, b] (0-1, sRGB as the picture shows it) is what the widget is drawn over: a dark panel or a
    // shadow could not be judged on the near-black default.
    FColor Backdrop(24, 24, 28);
    const TArray<TSharedPtr<FJsonValue>>* Background = nullptr;
    if (Payload->TryGetArrayField(TEXT("background"), Background) && Background->Num() == 3)
    {
        const auto Channel = [&Background](int32 Index) { return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp((*Background)[Index]->AsNumber(), 0.0, 1.0) * 255.0)); };
        Backdrop = FColor(Channel(0), Channel(1), Channel(2));
    }
    for (FColor& Pixel : Pixels)
    {
        Pixel = McpOverPreviewBackdrop(Pixel, Backdrop);
    }
    // The editor compiles a material's Slate shaders when it is first drawn, so the first preview after the editor
    // starts (or after a material edit) drew panels and button faces blank. waitForShaders waits for that compile and
    // draws again; without it the reply says shaders were compiling.
    if (McpDeferForShaderCompile(Payload, [Weak = TWeakObjectPtr<UEngineWireSubsystem>(&Subsystem), RequestId,
                                           RequestingSocket, ResultJson](const TSharedPtr<FJsonObject>& Resumed)
        {
            if (UEngineWireSubsystem* Self = Weak.Get())
            {
                HandleWidgetAuthoringPreview(*Self, RequestId, TEXT("preview_widget"), Resumed, RequestingSocket, ResultJson);
            }
        }))
    {
        return true;
    }
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
    // openEditor also shows it in the Widget Blueprint editor, which takes focus.
    const bool bOpen = GetJsonBoolField(Payload, TEXT("openEditor"), false);
    UAssetEditorSubsystem* AssetEditors = bOpen && GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetAuthoringHelpers::WidgetBlueprintPackagePath(WidgetBP));
    // The preview only draws the widget: without this the receipt lists widgetPath under changes.
    McpHandlerUtils::MarkNoAssetsChanged(ResultJson);
    ResultJson->SetNumberField(TEXT("width"), Size.X);
    ResultJson->SetNumberField(TEXT("height"), Size.Y);
    ResultJson->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    ResultJson->SetNumberField(TEXT("sizeBytes"), static_cast<double>(Png.Num()));
    ResultJson->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
    ResultJson->SetBoolField(TEXT("editorOpened"), AssetEditors && AssetEditors->OpenEditorForAsset(WidgetBP));
    McpAddShaderCompileState(ResultJson, Payload);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
        TEXT("Widget drawn at %dx%d as the game creates it (Construct graphs do not run)"), Size.X, Size.Y), ResultJson);
    return true;
}
}
