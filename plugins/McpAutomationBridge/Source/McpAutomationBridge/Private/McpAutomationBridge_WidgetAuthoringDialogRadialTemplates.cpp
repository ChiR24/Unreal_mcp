#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringDialogRadialTemplates(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_dialog_widget"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_DialogBox"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create dialog widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Dialog background
        UBorder* DialogBg = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogBackground"));
        RootCanvas->AddChild(DialogBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DialogBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
            Slot->SetAlignment(FVector2D(0.5f, 1.0f));
            Slot->SetSize(FVector2D(800.0f, 200.0f));
        }

        UVerticalBox* DialogContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogContainer"));
        DialogBg->AddChild(DialogContainer);

        // Speaker name
        UTextBlock* SpeakerName = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("SpeakerName"));
        SpeakerName->SetText(FText::FromString(TEXT("Speaker")));
        DialogContainer->AddChild(SpeakerName);

        // Dialog text
        URichTextBlock* DialogText = CreateAndRegisterWidget<URichTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogText"));
        DialogContainer->AddChild(DialogText);

        // Response options container
        UVerticalBox* ResponseBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResponseOptions"));
        DialogContainer->AddChild(ResponseBox);

        // Sample response buttons
        for (int32 i = 1; i <= 3; ++i)
        {
            FString ResponseName = FString::Printf(TEXT("Response_%d"), i);
            UButton* ResponseBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *ResponseName);
            UTextBlock* ResponseText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ResponseName + TEXT("_Text")));
            ResponseText->SetText(FText::FromString(FString::Printf(TEXT("Response Option %d"), i)));
            ResponseBtn->AddChild(ResponseText);
            ResponseBox->AddChild(ResponseBtn);
        }

        // Continue indicator
        UTextBlock* ContinueIndicator = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ContinueIndicator"));
        ContinueIndicator->SetText(FText::FromString(TEXT("Press Space to continue...")));
        DialogContainer->AddChild(ContinueIndicator);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created dialog widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_radial_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_RadialMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 SegmentCount = GetJsonIntField(Payload, TEXT("segments"), 8);

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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create radial menu"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Radial menu container (centered)
        UOverlay* RadialContainer = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, TEXT("RadialMenuContainer"));
        RootCanvas->AddChild(RadialContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RadialContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(400.0f, 400.0f));
        }

        // Background ring
        UImage* BackgroundRing = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("RadialBackground"));
        RadialContainer->AddChild(BackgroundRing);

        // Selection indicator
        UImage* SelectionIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("SelectionIndicator"));
        RadialContainer->AddChild(SelectionIndicator);

        // Create segment buttons (arranged in circle via canvas positions)
        UCanvasPanel* SegmentCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("SegmentCanvas"));
        RadialContainer->AddChild(SegmentCanvas);

        float Radius = 150.0f;
        for (int32 i = 0; i < SegmentCount; ++i)
        {
            float Angle = (360.0f / SegmentCount) * i - 90.0f; // Start from top
            float RadAngle = FMath::DegreesToRadians(Angle);
            float X = FMath::Cos(RadAngle) * Radius;
            float Y = FMath::Sin(RadAngle) * Radius;

            FString SegmentName = FString::Printf(TEXT("Segment_%d"), i);
            UButton* SegmentBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *SegmentName);
            SegmentCanvas->AddChild(SegmentBtn);

            if (UCanvasPanelSlot* SegSlot = Cast<UCanvasPanelSlot>(SegmentBtn->Slot))
            {
                SegSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                SegSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                SegSlot->SetPosition(FVector2D(X, Y));
                SegSlot->SetSize(FVector2D(60.0f, 60.0f));
            }

            UImage* SegmentIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SegmentName + TEXT("_Icon")));
            SegmentBtn->AddChild(SegmentIcon);
        }

        // Center button
        UButton* CenterButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("CenterButton"));
        RadialContainer->AddChild(CenterButton);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("segments"), SegmentCount);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created radial menu"), ResultJson);
        return true;
    }

    return false;
}
}
