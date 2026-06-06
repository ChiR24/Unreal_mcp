#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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

bool HandleWidgetAuthoringCreditsTemplate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.16 Additional Template Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("create_credits_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Credits"));
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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create credits widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background
        UImage* Background = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Scrolling credits container
        UScrollBox* CreditsScroll = CreateAndRegisterWidget<UScrollBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CreditsScroll"));
        RootCanvas->AddChild(CreditsScroll);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CreditsScroll->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 1.0f));
            Slot->SetAlignment(FVector2D(0.5f, 0.0f));
            Slot->SetSize(FVector2D(600.0f, 0.0f));
            Slot->SetOffsets(FMargin(0.0f, 50.0f, 0.0f, 50.0f));
        }

        // Credits content
        UVerticalBox* CreditsContent = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CreditsContent"));
        CreditsScroll->AddChild(CreditsContent);

        // Sample credits sections
        TArray<FString> Sections = { TEXT("Lead Developer"), TEXT("Art Director"), TEXT("Sound Design"), TEXT("Special Thanks") };
        for (const FString& Section : Sections)
        {
            UTextBlock* SectionTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Title")));
            SectionTitle->SetText(FText::FromString(Section));
            CreditsContent->AddChild(SectionTitle);

            UTextBlock* SectionName = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Name")));
            SectionName->SetText(FText::FromString(TEXT("Your Name Here")));
            CreditsContent->AddChild(SectionName);
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created credits screen"), ResultJson);
        return true;
    }

    return false;
}
}
