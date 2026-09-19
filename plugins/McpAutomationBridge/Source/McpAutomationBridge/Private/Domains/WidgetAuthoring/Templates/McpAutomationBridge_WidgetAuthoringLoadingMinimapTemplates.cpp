#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringLoadingMinimapTemplates(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_loading_screen"), ESearchCase::IgnoreCase))
    {
        // Accept widgetPath as well as (name + path/folder) — see the dialog note.
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_LoadingScreen"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (!WidgetPath.IsEmpty())
        {
            FString PathFolder;
            FString PathName;
            WidgetPath.Split(TEXT("/"), &PathFolder, &PathName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
            if (!PathName.IsEmpty()) { Name = PathName; }
            if (!PathFolder.IsEmpty()) { Folder = PathFolder; }
        }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name),
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create loading screen widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background image
        UImage* Background = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Loading text
        UTextBlock* LoadingText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingText"));
        LoadingText->SetText(FText::FromString(TEXT("Loading...")));
        RootCanvas->AddChild(LoadingText);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingText->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
        }

        // Progress bar. The contract declares includeProgressBar; the handler used
        // to always create it, silently ignoring includeProgressBar:false.
        const bool bIncludeProgressBar = GetJsonBoolField(Payload, TEXT("includeProgressBar"), true);
        if (bIncludeProgressBar)
        {
            UProgressBar* LoadingBar = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LoadingProgressBar"));
            LoadingBar->SetPercent(0.0f);
            RootCanvas->AddChild(LoadingBar);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingBar->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetSize(FVector2D(400.0f, 20.0f));
            }
        }

        // fadeTime: the contract declares it, but nothing read it. Turn it into a
        // real FadeIn animation (RenderOpacity 0 -> 1 over fadeTime seconds) so the
        // value has an effect instead of being silently dropped.
        const double FadeTime = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.0);
        bool bFadeAnimationCreated = false;
        if (FadeTime > 0.0 && WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
        {
            UWidgetAnimation* FadeIn = NewObject<UWidgetAnimation>(WidgetBP, FName(TEXT("FadeIn")), RF_Transactional);
            if (FadeIn)
            {
                FadeIn->MovieScene = NewObject<UMovieScene>(FadeIn, FName(TEXT("FadeIn")), RF_Transactional);
            }
            if (FadeIn && FadeIn->GetMovieScene())
            {
                UMovieScene* FadeScene = FadeIn->GetMovieScene();
                FadeScene->SetDisplayRate(FFrameRate(20, 1));
                const FFrameTime FadeOutFrame = FadeTime * FadeScene->GetTickResolution();
                FadeScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), FadeOutFrame.FrameNumber + 1));
                RegisterAnimationGuid(WidgetBP, FadeIn);

                auto KeyOpacity = [&](double AtTime, double Opacity)
                {
                    TSharedPtr<FJsonObject> KeyPayload = MakeShared<FJsonObject>();
                    KeyPayload->SetStringField(TEXT("trackType"), TEXT("opacity"));
                    KeyPayload->SetNumberField(TEXT("time"), AtTime);
                    KeyPayload->SetNumberField(TEXT("propertyValue"), Opacity);
                    FMcpWidgetKeyResult KeyResult;
                    FString KeyError;
                    FString KeyErrorCode;
                    McpAuthorWidgetAnimationKey(WidgetBP, FadeIn, WidgetBP->WidgetTree->RootWidget,
                                                KeyPayload, KeyResult, KeyError, KeyErrorCode);
                };
                KeyOpacity(0.0, 0.0);
                KeyOpacity(FadeTime, 1.0);
                bFadeAnimationCreated = true;
            }
        }

        // CRITICAL: Register all widget GUIDs and mark as user-created
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 794
        RegisterAllWidgetGuids(WidgetBP);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetBoolField(TEXT("includeProgressBar"), bIncludeProgressBar);
        ResultJson->SetBoolField(TEXT("fadeAnimationCreated"), bFadeAnimationCreated);
        ResultJson->SetNumberField(TEXT("fadeTime"), FadeTime);
        ResultJson->SetStringField(TEXT("message"), TEXT("Created loading screen template"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created loading screen template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_minimap"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Minimap"));
        float Size = GetJsonNumberField(Payload, TEXT("size"), 200.0f);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadOrCreateWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Create minimap container (overlay for stacking)
        UOverlay* MinimapContainer = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *SlotName);

        // Create border for minimap frame
        UBorder* MinimapBorder = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(SlotName + TEXT("_Border")));
        MinimapContainer->AddChild(MinimapBorder);

        // Create image for map content
        UImage* MapImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_MapImage")));
        MinimapBorder->AddChild(MapImage);

        // Create player indicator
        UImage* PlayerIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_PlayerIndicator")));
        MinimapContainer->AddChild(PlayerIndicator);

        // Add to root or parent
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            // The widget was built and then never parented, and the reply still
            // said "Added ...": a root that is a leaf (or absent) left it
            // orphaned in the tree where nothing renders it.
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' has no panel at its root, so the minimap has nowhere to attach. Add a CanvasPanel first."), *WidgetPath),
                TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        {
            Parent->AddChild(MinimapContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(MinimapContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
                Slot->SetAlignment(FVector2D(1.0f, 0.0f));
                Slot->SetSize(FVector2D(Size, Size));
                Slot->SetPosition(FVector2D(-20.0f, 20.0f)); // Offset from corner
            }
        }

        // CRITICAL: Register all widget GUIDs to prevent ensure failures during compilation
        RegisterAllWidgetGuids(WidgetBP);

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("size"), Size);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added minimap widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_compass"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Compass"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadOrCreateWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create compass container
        UHorizontalBox* CompassContainer = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Create compass image (scrolling texture)
        UImage* CompassImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Image")));
        CompassContainer->AddChild(CompassImage);

        // Create direction indicator
        UImage* DirectionIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Indicator")));
        CompassContainer->AddChild(DirectionIndicator);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            // The widget was built and then never parented, and the reply still
            // said "Added ...": a root that is a leaf (or absent) left it
            // orphaned in the tree where nothing renders it.
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' has no panel at its root, so the compass has nowhere to attach. Add a CanvasPanel first."), *WidgetPath),
                TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        {
            Parent->AddChild(CompassContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CompassContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f)); // Top-center
                Slot->SetAlignment(FVector2D(0.5f, 0.0f));
                Slot->SetSize(FVector2D(400.0f, 40.0f));
                Slot->SetPosition(FVector2D(0.0f, 20.0f));
            }
        }

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added compass widget"), ResultJson);
        return true;
    }

    return false;
}
}
