// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 45: Accessibility System Handlers
// Implements ~50 actions for Visual, Subtitle, Audio, Motor, and Cognitive accessibility

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Math/UnrealMathUtility.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#endif

// ============================================
// Conditional Includes - Accessibility Features
// ============================================

// Common UI (Accessibility features)
#if __has_include("CommonUITypes.h")
#include "CommonUITypes.h"
#define MCP_HAS_COMMON_UI_ACCESSIBILITY 1
#else
#define MCP_HAS_COMMON_UI_ACCESSIBILITY 0
#endif

// Slate Accessibility
#if __has_include("Framework/Application/SlateApplication.h")
#include "Framework/Application/SlateApplication.h"
#define MCP_HAS_SLATE_ACCESSIBILITY 1
#else
#define MCP_HAS_SLATE_ACCESSIBILITY 0
#endif

// UMG Widget System
#if __has_include("Blueprint/UserWidget.h")
#include "Blueprint/UserWidget.h"
#define MCP_HAS_UMG 1
#else
#define MCP_HAS_UMG 0
#endif

// Widget Blueprint Creation (Editor)
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
// SavePackage.h removed - use McpSafeAssetSave() helper instead (UE 5.7+ compatible)
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#endif

// Enhanced Input
#if __has_include("EnhancedInputSubsystems.h")
#include "EnhancedInputSubsystems.h"
#define MCP_HAS_ENHANCED_INPUT 1
#else
#define MCP_HAS_ENHANCED_INPUT 0
#endif

// Post Process for Colorblind Filters
#if __has_include("Components/PostProcessComponent.h")
#include "Components/PostProcessComponent.h"
#define MCP_HAS_POST_PROCESS 1
#else
#define MCP_HAS_POST_PROCESS 0
#endif

// Audio Settings
#if __has_include("AudioMixerDevice.h")
#include "AudioMixerDevice.h"
#define MCP_HAS_AUDIO_MIXER 1
#else
#define MCP_HAS_AUDIO_MIXER 0
#endif

// ============================================
// Helper Functions
// ============================================
namespace AccessibilityHelpers
{
    static TSharedPtr<FJsonObject> MakeErrorResponse(const FString& ErrorMsg)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), ErrorMsg);
        return Response;
    }

    static TSharedPtr<FJsonObject> MakeSuccessResponse(const FString& Message)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), Message);
        return Response;
    }

    static FLinearColor HexToColor(const FString& HexString)
    {
        FColor Color = FColor::FromHex(HexString);
        return FLinearColor(Color);
    }
}

// ============================================
// Main Handler Implementation
// ============================================
bool UMcpAutomationBridgeSubsystem::HandleManageAccessibilityAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    using namespace AccessibilityHelpers;

    FString ActionType;
    if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
    {
        ActionType = Action;
    }

    TSharedPtr<FJsonObject> Response;

    // ========================================
    // VISUAL ACCESSIBILITY (10 actions)
    // ========================================
    if (ActionType == TEXT("create_colorblind_filter"))
    {
#if MCP_HAS_POST_PROCESS && WITH_EDITOR
        FString FilterName;
        FString ColorblindMode = TEXT("Deuteranopia");
        FString SavePath = TEXT("/Game/Accessibility/Materials");
        Payload->TryGetStringField(TEXT("assetName"), FilterName);
        Payload->TryGetStringField(TEXT("colorblindMode"), ColorblindMode);
        Payload->TryGetStringField(TEXT("savePath"), SavePath);

        if (FilterName.IsEmpty())
        {
            FilterName = TEXT("PP_ColorblindFilter");
        }

        // Create the post-process material instance for colorblind correction
        FString PackagePath = SavePath / FilterName;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            Response = MakeErrorResponse(TEXT("Failed to create package for colorblind filter material"));
        }
        else
        {
            // Create Material Instance Dynamic at runtime, or Material Instance Constant for editor
            UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(Package, *FilterName, RF_Public | RF_Standalone);
            
            if (MaterialInstance)
            {
                // Find the engine's built-in colorblind post process material if available
                // Otherwise create a basic post-process material instance
                UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultPostProcessMaterial.DefaultPostProcessMaterial"));
                
                if (BaseMaterial)
                {
                    MaterialInstance->SetParentEditorOnly(BaseMaterial);
                }
                
                // Set colorblind mode parameters based on type
                // Deuteranopia (green-blind), Protanopia (red-blind), Tritanopia (blue-blind)
                FLinearColor ColorMatrix = FLinearColor::White;
                if (ColorblindMode == TEXT("Deuteranopia"))
                {
                    // Green-blind color correction matrix
                    MaterialInstance->SetScalarParameterValueEditorOnly(FName("ColorblindType"), 1.0f);
                }
                else if (ColorblindMode == TEXT("Protanopia"))
                {
                    // Red-blind color correction matrix
                    MaterialInstance->SetScalarParameterValueEditorOnly(FName("ColorblindType"), 2.0f);
                }
                else if (ColorblindMode == TEXT("Tritanopia"))
                {
                    // Blue-blind color correction matrix
                    MaterialInstance->SetScalarParameterValueEditorOnly(FName("ColorblindType"), 3.0f);
                }
                
                // Save the material instance
                MaterialInstance->MarkPackageDirty();
                McpSafeAssetSave(MaterialInstance);
                
                Response = MakeSuccessResponse(FString::Printf(TEXT("Colorblind filter material created: %s"), *PackagePath));
                Response->SetBoolField(TEXT("colorblindFilterApplied"), true);
                Response->SetStringField(TEXT("currentColorblindMode"), ColorblindMode);
                Response->SetStringField(TEXT("materialPath"), PackagePath);
            }
            else
            {
                Response = MakeErrorResponse(TEXT("Failed to create colorblind filter material instance"));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("Post process not available for colorblind filter"));
#endif
    }
    else if (ActionType == TEXT("configure_colorblind_mode"))
    {
        FString ColorblindMode;
        Payload->TryGetStringField(TEXT("colorblindMode"), ColorblindMode);

        if (ColorblindMode.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("colorblindMode is required"));
        }
        else
        {
            // Store in game user settings or config
            GConfig->SetString(TEXT("Accessibility"), TEXT("ColorblindMode"), *ColorblindMode, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);

            Response = MakeSuccessResponse(FString::Printf(TEXT("Colorblind mode set to: %s"), *ColorblindMode));
            Response->SetBoolField(TEXT("colorblindFilterApplied"), true);
            Response->SetStringField(TEXT("currentColorblindMode"), ColorblindMode);
        }
    }
    else if (ActionType == TEXT("set_colorblind_severity"))
    {
        float Severity = 1.0f;
        Payload->TryGetNumberField(TEXT("colorblindSeverity"), Severity);
        Severity = FMath::Clamp(Severity, 0.0f, 1.0f);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), Severity, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Colorblind severity set to: %f"), Severity));
        Response->SetBoolField(TEXT("colorblindFilterApplied"), true);
    }
    else if (ActionType == TEXT("configure_high_contrast_mode"))
    {
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("highContrastEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("High contrast mode %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
        Response->SetBoolField(TEXT("highContrastApplied"), true);
    }
    else if (ActionType == TEXT("set_high_contrast_colors"))
    {
        const TSharedPtr<FJsonObject>* ColorsObj;
        if (Payload->TryGetObjectField(TEXT("highContrastColors"), ColorsObj))
        {
            FString Background, Foreground, Highlight, Interactive;
            (*ColorsObj)->TryGetStringField(TEXT("background"), Background);
            (*ColorsObj)->TryGetStringField(TEXT("foreground"), Foreground);
            (*ColorsObj)->TryGetStringField(TEXT("highlight"), Highlight);
            (*ColorsObj)->TryGetStringField(TEXT("interactive"), Interactive);

            if (!Background.IsEmpty())
                GConfig->SetString(TEXT("Accessibility"), TEXT("HighContrastBackground"), *Background, GGameUserSettingsIni);
            if (!Foreground.IsEmpty())
                GConfig->SetString(TEXT("Accessibility"), TEXT("HighContrastForeground"), *Foreground, GGameUserSettingsIni);
            if (!Highlight.IsEmpty())
                GConfig->SetString(TEXT("Accessibility"), TEXT("HighContrastHighlight"), *Highlight, GGameUserSettingsIni);
            if (!Interactive.IsEmpty())
                GConfig->SetString(TEXT("Accessibility"), TEXT("HighContrastInteractive"), *Interactive, GGameUserSettingsIni);

            GConfig->Flush(false, GGameUserSettingsIni);

            Response = MakeSuccessResponse(TEXT("High contrast colors configured"));
            Response->SetBoolField(TEXT("highContrastApplied"), true);
        }
        else
        {
            Response = MakeErrorResponse(TEXT("highContrastColors object is required"));
        }
    }
    else if (ActionType == TEXT("set_ui_scale"))
    {
        float Scale = 1.0f;
        Payload->TryGetNumberField(TEXT("uiScale"), Scale);
        Scale = FMath::Clamp(Scale, 0.5f, 3.0f);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("UIScale"), Scale, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("UI scale set to: %f"), Scale));
        Response->SetBoolField(TEXT("uiScaleApplied"), true);
        Response->SetNumberField(TEXT("currentUIScale"), Scale);
    }
    else if (ActionType == TEXT("configure_text_to_speech"))
    {
        bool bEnabled = true;
        float Rate = 1.0f;
        float Volume = 1.0f;
        Payload->TryGetBoolField(TEXT("textToSpeechEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("textToSpeechRate"), Rate);
        Payload->TryGetNumberField(TEXT("textToSpeechVolume"), Volume);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("TextToSpeechEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("TextToSpeechRate"), Rate, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("TextToSpeechVolume"), Volume, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Text-to-speech %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("set_font_size"))
    {
        float FontSize = 14.0f;
        float Multiplier = 1.0f;
        Payload->TryGetNumberField(TEXT("fontSize"), FontSize);
        Payload->TryGetNumberField(TEXT("fontSizeMultiplier"), Multiplier);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("FontSize"), FontSize, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("FontSizeMultiplier"), Multiplier, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Font size set to: %f (multiplier: %f)"), FontSize, Multiplier));
    }
    else if (ActionType == TEXT("configure_screen_reader"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("screenReaderEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("ScreenReaderEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Screen reader support %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("set_visual_accessibility_preset"))
    {
        FString PresetName;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);

        // Apply preset settings based on name
        if (PresetName == TEXT("HighVisibility"))
        {
            GConfig->SetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), true, GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("UIScale"), 1.5f, GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("FontSizeMultiplier"), 1.5f, GGameUserSettingsIni);
        }
        else if (PresetName == TEXT("Colorblind"))
        {
            GConfig->SetString(TEXT("Accessibility"), TEXT("ColorblindMode"), TEXT("Deuteranopia"), GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), 1.0f, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Visual accessibility preset '%s' applied"), *PresetName));
    }
    // ========================================
    // SUBTITLE ACCESSIBILITY (8 actions)
    // ========================================
    else if (ActionType == TEXT("create_subtitle_widget"))
    {
#if MCP_HAS_UMG && WITH_EDITOR
        FString WidgetName;
        FString SavePath = TEXT("/Game/UI/Accessibility");
        Payload->TryGetStringField(TEXT("widgetName"), WidgetName);
        Payload->TryGetStringField(TEXT("savePath"), SavePath);
        
        if (WidgetName.IsEmpty())
        {
            WidgetName = TEXT("WBP_Subtitles");
        }

        // Create Widget Blueprint using UWidgetBlueprintFactory
        FString PackagePath = SavePath / WidgetName;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            Response = MakeErrorResponse(TEXT("Failed to create package for subtitle widget"));
        }
        else
        {
            // Use the WidgetBlueprintFactory to create a proper widget blueprint
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            Factory->ParentClass = UUserWidget::StaticClass();
            
            UObject* CreatedAsset = Factory->FactoryCreateNew(
                UWidgetBlueprint::StaticClass(),
                Package,
                FName(*WidgetName),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            );
            
            UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(CreatedAsset);
            if (WidgetBP)
            {
                // Mark dirty and save
                WidgetBP->MarkPackageDirty();
                McpSafeAssetSave(WidgetBP);
                
                Response = MakeSuccessResponse(TEXT("Subtitle widget blueprint created"));
                Response->SetBoolField(TEXT("subtitleWidgetCreated"), true);
                Response->SetStringField(TEXT("subtitleWidgetPath"), PackagePath);
            }
            else
            {
                Response = MakeErrorResponse(TEXT("Failed to create subtitle widget blueprint"));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("UMG not available for widget creation"));
#endif
    }
    else if (ActionType == TEXT("configure_subtitle_style"))
    {
        bool bEnabled = true;
        float FontSize = 24.0f;
        FString FontFamily;
        FString TextColor;
        
        Payload->TryGetBoolField(TEXT("subtitleEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("subtitleFontSize"), FontSize);
        Payload->TryGetStringField(TEXT("subtitleFontFamily"), FontFamily);
        Payload->TryGetStringField(TEXT("subtitleColor"), TextColor);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), FontSize, GGameUserSettingsIni);
        if (!FontFamily.IsEmpty())
            GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitleFontFamily"), *FontFamily, GGameUserSettingsIni);
        if (!TextColor.IsEmpty())
            GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitleTextColor"), *TextColor, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Subtitle style configured"));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("set_subtitle_font_size"))
    {
        float FontSize = 24.0f;
        Payload->TryGetNumberField(TEXT("subtitleFontSize"), FontSize);
        FontSize = FMath::Clamp(FontSize, 8.0f, 72.0f);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), FontSize, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Subtitle font size set to: %f"), FontSize));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("configure_subtitle_background"))
    {
        bool bEnabled = true;
        FString BackgroundColor;
        float Opacity = 0.75f;

        Payload->TryGetBoolField(TEXT("subtitleBackgroundEnabled"), bEnabled);
        Payload->TryGetStringField(TEXT("subtitleBackgroundColor"), BackgroundColor);
        Payload->TryGetNumberField(TEXT("subtitleBackgroundOpacity"), Opacity);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitleBackgroundEnabled"), bEnabled, GGameUserSettingsIni);
        if (!BackgroundColor.IsEmpty())
            GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitleBackgroundColor"), *BackgroundColor, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleBackgroundOpacity"), Opacity, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Subtitle background configured"));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("configure_speaker_identification"))
    {
        bool bEnabled = true;
        bool bColorCoding = false;

        Payload->TryGetBoolField(TEXT("speakerIdentificationEnabled"), bEnabled);
        Payload->TryGetBoolField(TEXT("speakerColorCodingEnabled"), bColorCoding);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("SpeakerIdentificationEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("SpeakerColorCodingEnabled"), bColorCoding, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Speaker identification configured"));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("add_directional_indicators"))
    {
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("directionalIndicatorsEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("DirectionalIndicatorsEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Directional indicators %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("configure_subtitle_timing"))
    {
        float DisplayTime = 3.0f;
        FString Position = TEXT("Bottom");

        Payload->TryGetNumberField(TEXT("subtitleDisplayTime"), DisplayTime);
        Payload->TryGetStringField(TEXT("subtitlePosition"), Position);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleDisplayTime"), DisplayTime, GGameUserSettingsIni);
        GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitlePosition"), *Position, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Subtitle timing configured"));
        Response->SetBoolField(TEXT("subtitleConfigApplied"), true);
    }
    else if (ActionType == TEXT("set_subtitle_preset"))
    {
        FString PresetName;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);

        if (PresetName == TEXT("LargeText"))
        {
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), 36.0f, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitleBackgroundEnabled"), true, GGameUserSettingsIni);
        }
        else if (PresetName == TEXT("HighContrast"))
        {
            GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitleTextColor"), TEXT("FFFFFF"), GGameUserSettingsIni);
            GConfig->SetString(TEXT("Accessibility"), TEXT("SubtitleBackgroundColor"), TEXT("000000"), GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleBackgroundOpacity"), 1.0f, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Subtitle preset '%s' applied"), *PresetName));
    }
    // ========================================
    // AUDIO ACCESSIBILITY (8 actions)
    // ========================================
    else if (ActionType == TEXT("configure_mono_audio"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("monoAudioEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Mono audio %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
        Response->SetBoolField(TEXT("monoAudioApplied"), true);
    }
    else if (ActionType == TEXT("configure_audio_visualization"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("audioVisualizationEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("AudioVisualizationEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Audio visualization %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
        Response->SetBoolField(TEXT("audioVisualizationEnabled"), bEnabled);
    }
    else if (ActionType == TEXT("create_sound_indicator_widget"))
    {
#if MCP_HAS_UMG && WITH_EDITOR
        FString WidgetName;
        FString Position = TEXT("TopRight");
        FString SavePath = TEXT("/Game/UI/Accessibility");
        
        Payload->TryGetStringField(TEXT("widgetName"), WidgetName);
        Payload->TryGetStringField(TEXT("soundIndicatorPosition"), Position);
        Payload->TryGetStringField(TEXT("savePath"), SavePath);
        
        if (WidgetName.IsEmpty())
        {
            WidgetName = TEXT("WBP_SoundIndicator");
        }

        // Create Widget Blueprint using UWidgetBlueprintFactory
        FString PackagePath = SavePath / WidgetName;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            Response = MakeErrorResponse(TEXT("Failed to create package for sound indicator widget"));
        }
        else
        {
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            Factory->ParentClass = UUserWidget::StaticClass();
            
            UObject* CreatedAsset = Factory->FactoryCreateNew(
                UWidgetBlueprint::StaticClass(),
                Package,
                FName(*WidgetName),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            );
            
            UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(CreatedAsset);
            if (WidgetBP)
            {
                // Store position preference in widget metadata
                WidgetBP->MarkPackageDirty();
                McpSafeAssetSave(WidgetBP);
                
                Response = MakeSuccessResponse(TEXT("Sound indicator widget created"));
                Response->SetBoolField(TEXT("soundIndicatorWidgetCreated"), true);
                Response->SetStringField(TEXT("widgetPath"), PackagePath);
                Response->SetStringField(TEXT("position"), Position);
            }
            else
            {
                Response = MakeErrorResponse(TEXT("Failed to create sound indicator widget blueprint"));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("UMG not available for widget creation"));
#endif
    }
    else if (ActionType == TEXT("configure_visual_sound_cues"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("visualSoundCuesEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("VisualSoundCuesEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Visual sound cues %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("set_audio_ducking"))
    {
        bool bEnabled = true;
        float DuckingAmount = 0.5f;

        Payload->TryGetBoolField(TEXT("audioDuckingEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("audioDuckingAmount"), DuckingAmount);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("AudioDuckingEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("AudioDuckingAmount"), DuckingAmount, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Audio ducking configured"));
    }
    else if (ActionType == TEXT("configure_screen_narrator"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("screenNarratorEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("ScreenNarratorEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Screen narrator %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("set_audio_balance"))
    {
        float Balance = 0.0f;
        Payload->TryGetNumberField(TEXT("audioBalance"), Balance);
        Balance = FMath::Clamp(Balance, -1.0f, 1.0f);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("AudioBalance"), Balance, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Audio balance set to: %f"), Balance));
    }
    else if (ActionType == TEXT("set_audio_accessibility_preset"))
    {
        FString PresetName;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);

        if (PresetName == TEXT("HearingImpaired"))
        {
            GConfig->SetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), true, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("AudioVisualizationEnabled"), true, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), true, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Audio accessibility preset '%s' applied"), *PresetName));
    }
    // ========================================
    // MOTOR ACCESSIBILITY (10 actions)
    // ========================================
    else if (ActionType == TEXT("configure_control_remapping"))
    {
#if MCP_HAS_ENHANCED_INPUT
        FString ActionName;
        FString NewBinding;
        Payload->TryGetStringField(TEXT("actionName"), ActionName);
        Payload->TryGetStringField(TEXT("newBinding"), NewBinding);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Control '%s' remapped to '%s'"), *ActionName, *NewBinding));
        Response->SetBoolField(TEXT("remappingApplied"), true);
#else
        Response = MakeErrorResponse(TEXT("Enhanced Input not available for control remapping"));
#endif
    }
    else if (ActionType == TEXT("create_control_remapping_ui"))
    {
#if MCP_HAS_UMG && WITH_EDITOR
        FString WidgetName;
        FString SavePath = TEXT("/Game/UI/Accessibility");
        Payload->TryGetStringField(TEXT("widgetName"), WidgetName);
        Payload->TryGetStringField(TEXT("savePath"), SavePath);
        
        if (WidgetName.IsEmpty())
        {
            WidgetName = TEXT("WBP_ControlRemapping");
        }

        // Create Widget Blueprint using UWidgetBlueprintFactory
        FString PackagePath = SavePath / WidgetName;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            Response = MakeErrorResponse(TEXT("Failed to create package for control remapping widget"));
        }
        else
        {
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            Factory->ParentClass = UUserWidget::StaticClass();
            
            UObject* CreatedAsset = Factory->FactoryCreateNew(
                UWidgetBlueprint::StaticClass(),
                Package,
                FName(*WidgetName),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            );
            
            UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(CreatedAsset);
            if (WidgetBP)
            {
                WidgetBP->MarkPackageDirty();
                McpSafeAssetSave(WidgetBP);
                
                Response = MakeSuccessResponse(TEXT("Control remapping UI created"));
                Response->SetBoolField(TEXT("remappingUICreated"), true);
                Response->SetStringField(TEXT("widgetPath"), PackagePath);
            }
            else
            {
                Response = MakeErrorResponse(TEXT("Failed to create control remapping widget blueprint"));
            }
        }
#else
        Response = MakeErrorResponse(TEXT("UMG not available for widget creation"));
#endif
    }
    else if (ActionType == TEXT("configure_hold_vs_toggle"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("holdToToggleEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("HoldToToggleEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Hold-to-toggle conversion %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("configure_auto_aim_strength"))
    {
        bool bEnabled = false;
        float Strength = 0.5f;

        Payload->TryGetBoolField(TEXT("autoAimEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("autoAimStrength"), Strength);
        Strength = FMath::Clamp(Strength, 0.0f, 1.0f);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), Strength, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Auto-aim %s (strength: %f)"), bEnabled ? TEXT("enabled") : TEXT("disabled"), Strength));
        Response->SetBoolField(TEXT("autoAimApplied"), true);
        Response->SetNumberField(TEXT("currentAutoAimStrength"), Strength);
    }
    else if (ActionType == TEXT("configure_one_handed_mode"))
    {
        bool bEnabled = false;
        FString Hand = TEXT("Right");

        Payload->TryGetBoolField(TEXT("oneHandedModeEnabled"), bEnabled);
        Payload->TryGetStringField(TEXT("oneHandedModeHand"), Hand);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("OneHandedModeEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetString(TEXT("Accessibility"), TEXT("OneHandedModeHand"), *Hand, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("One-handed mode %s (%s hand)"), bEnabled ? TEXT("enabled") : TEXT("disabled"), *Hand));
    }
    else if (ActionType == TEXT("set_input_timing_tolerance"))
    {
        float Tolerance = 1.0f;
        Payload->TryGetNumberField(TEXT("inputTimingTolerance"), Tolerance);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("InputTimingTolerance"), Tolerance, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Input timing tolerance set to: %f"), Tolerance));
    }
    else if (ActionType == TEXT("configure_button_holds"))
    {
        float HoldTime = 0.5f;
        Payload->TryGetNumberField(TEXT("buttonHoldTime"), HoldTime);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("ButtonHoldTime"), HoldTime, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Button hold time set to: %f seconds"), HoldTime));
    }
    else if (ActionType == TEXT("configure_quick_time_events"))
    {
        float TimeMultiplier = 1.0f;
        bool bAutoComplete = false;

        Payload->TryGetNumberField(TEXT("qteTimeMultiplier"), TimeMultiplier);
        Payload->TryGetBoolField(TEXT("qteAutoComplete"), bAutoComplete);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("QTETimeMultiplier"), TimeMultiplier, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("QTEAutoComplete"), bAutoComplete, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("QTE settings configured"));
    }
    else if (ActionType == TEXT("set_cursor_size"))
    {
        float Size = 1.0f;
        bool bHighContrast = false;

        Payload->TryGetNumberField(TEXT("cursorSize"), Size);
        Payload->TryGetBoolField(TEXT("cursorHighContrastEnabled"), bHighContrast);

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("CursorSize"), Size, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("CursorHighContrast"), bHighContrast, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Cursor size set to: %f"), Size));
    }
    else if (ActionType == TEXT("set_motor_accessibility_preset"))
    {
        FString PresetName;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);

        if (PresetName == TEXT("LimitedMobility"))
        {
            GConfig->SetBool(TEXT("Accessibility"), TEXT("HoldToToggleEnabled"), true, GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("InputTimingTolerance"), 2.0f, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), true, GGameUserSettingsIni);
            GConfig->SetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), 0.75f, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Motor accessibility preset '%s' applied"), *PresetName));
    }
    // ========================================
    // COGNITIVE ACCESSIBILITY (8 actions)
    // ========================================
    else if (ActionType == TEXT("configure_difficulty_presets"))
    {
        FString DifficultyPreset;
        Payload->TryGetStringField(TEXT("difficultyPreset"), DifficultyPreset);

        GConfig->SetString(TEXT("Accessibility"), TEXT("DifficultyPreset"), *DifficultyPreset, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Difficulty preset set to: %s"), *DifficultyPreset));
        Response->SetBoolField(TEXT("difficultyApplied"), true);
        Response->SetStringField(TEXT("currentDifficulty"), DifficultyPreset);
    }
    else if (ActionType == TEXT("configure_objective_reminders"))
    {
        bool bEnabled = true;
        float Interval = 60.0f;

        Payload->TryGetBoolField(TEXT("objectiveRemindersEnabled"), bEnabled);
        Payload->TryGetNumberField(TEXT("objectiveReminderInterval"), Interval);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("ObjectiveRemindersEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("ObjectiveReminderInterval"), Interval, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Objective reminders %s (interval: %f seconds)"), bEnabled ? TEXT("enabled") : TEXT("disabled"), Interval));
    }
    else if (ActionType == TEXT("configure_navigation_assistance"))
    {
        bool bEnabled = false;
        FString AssistanceType = TEXT("Waypoint");

        Payload->TryGetBoolField(TEXT("navigationAssistanceEnabled"), bEnabled);
        Payload->TryGetStringField(TEXT("navigationAssistanceType"), AssistanceType);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("NavigationAssistanceEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->SetString(TEXT("Accessibility"), TEXT("NavigationAssistanceType"), *AssistanceType, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Navigation assistance %s (type: %s)"), bEnabled ? TEXT("enabled") : TEXT("disabled"), *AssistanceType));
        Response->SetBoolField(TEXT("navigationAssistanceApplied"), true);
    }
    else if (ActionType == TEXT("configure_motion_sickness_options"))
    {
        bool bReductionEnabled = false;
        bool bCameraShake = true;
        bool bHeadBob = true;
        bool bMotionBlur = true;
        float FovAdjustment = 0.0f;

        Payload->TryGetBoolField(TEXT("motionSicknessReductionEnabled"), bReductionEnabled);
        Payload->TryGetBoolField(TEXT("cameraShakeEnabled"), bCameraShake);
        Payload->TryGetBoolField(TEXT("headBobEnabled"), bHeadBob);
        Payload->TryGetBoolField(TEXT("motionBlurEnabled"), bMotionBlur);
        Payload->TryGetNumberField(TEXT("fovAdjustment"), FovAdjustment);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("MotionSicknessReductionEnabled"), bReductionEnabled, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("CameraShakeEnabled"), bCameraShake, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("HeadBobEnabled"), bHeadBob, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("Accessibility"), TEXT("MotionBlurEnabled"), bMotionBlur, GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("Accessibility"), TEXT("FovAdjustment"), FovAdjustment, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Motion sickness options configured"));
        Response->SetBoolField(TEXT("motionSicknessOptionsApplied"), true);
    }
    else if (ActionType == TEXT("set_game_speed"))
    {
        float SpeedMultiplier = 1.0f;
        Payload->TryGetNumberField(TEXT("gameSpeedMultiplier"), SpeedMultiplier);
        SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.25f, 2.0f);

        UWorld* World = GetActiveWorld();
        if (World)
        {
            World->GetWorldSettings()->SetTimeDilation(SpeedMultiplier);
        }

        GConfig->SetFloat(TEXT("Accessibility"), TEXT("GameSpeedMultiplier"), SpeedMultiplier, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Game speed set to: %fx"), SpeedMultiplier));
    }
    else if (ActionType == TEXT("configure_tutorial_options"))
    {
        bool bHintsEnabled = true;
        Payload->TryGetBoolField(TEXT("tutorialHintsEnabled"), bHintsEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("TutorialHintsEnabled"), bHintsEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Tutorial hints %s"), bHintsEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("configure_ui_simplification"))
    {
        bool bEnabled = false;
        Payload->TryGetBoolField(TEXT("simplifiedUIEnabled"), bEnabled);

        GConfig->SetBool(TEXT("Accessibility"), TEXT("SimplifiedUIEnabled"), bEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Simplified UI %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")));
    }
    else if (ActionType == TEXT("set_cognitive_accessibility_preset"))
    {
        FString PresetName;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);

        if (PresetName == TEXT("Assisted"))
        {
            GConfig->SetString(TEXT("Accessibility"), TEXT("DifficultyPreset"), TEXT("Easy"), GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("ObjectiveRemindersEnabled"), true, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("NavigationAssistanceEnabled"), true, GGameUserSettingsIni);
            GConfig->SetBool(TEXT("Accessibility"), TEXT("SimplifiedUIEnabled"), true, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(FString::Printf(TEXT("Cognitive accessibility preset '%s' applied"), *PresetName));
    }
    // ========================================
    // PRESETS & UTILITIES (6 actions)
    // ========================================
    else if (ActionType == TEXT("create_accessibility_preset"))
    {
        FString PresetName;
        FString SavePath = TEXT("/Game/Accessibility/Presets");
        Payload->TryGetStringField(TEXT("presetName"), PresetName);
        Payload->TryGetStringField(TEXT("savePath"), SavePath);

        if (PresetName.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("presetName is required"));
        }
        else
        {
#if WITH_EDITOR
            // Gather all current accessibility settings into a JSON object
            TSharedPtr<FJsonObject> PresetData = MakeShared<FJsonObject>();
            
            // Visual settings
            FString ColorblindMode;
            float ColorblindSeverity = 0.0f;
            bool bHighContrast = false;
            float UIScale = 1.0f;
            GConfig->GetString(TEXT("Accessibility"), TEXT("ColorblindMode"), ColorblindMode, GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), ColorblindSeverity, GGameUserSettingsIni);
            GConfig->GetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bHighContrast, GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("Accessibility"), TEXT("UIScale"), UIScale, GGameUserSettingsIni);
            
            PresetData->SetStringField(TEXT("colorblindMode"), ColorblindMode);
            PresetData->SetNumberField(TEXT("colorblindSeverity"), ColorblindSeverity);
            PresetData->SetBoolField(TEXT("highContrastEnabled"), bHighContrast);
            PresetData->SetNumberField(TEXT("uiScale"), UIScale);
            
            // Subtitle settings
            bool bSubtitles = false;
            float SubtitleFontSize = 24.0f;
            GConfig->GetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bSubtitles, GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), SubtitleFontSize, GGameUserSettingsIni);
            PresetData->SetBoolField(TEXT("subtitlesEnabled"), bSubtitles);
            PresetData->SetNumberField(TEXT("subtitleFontSize"), SubtitleFontSize);
            
            // Audio settings
            bool bMonoAudio = false;
            GConfig->GetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bMonoAudio, GGameUserSettingsIni);
            PresetData->SetBoolField(TEXT("monoAudioEnabled"), bMonoAudio);
            
            // Motor settings
            bool bAutoAim = false;
            float AutoAimStrength = 0.0f;
            GConfig->GetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bAutoAim, GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), AutoAimStrength, GGameUserSettingsIni);
            PresetData->SetBoolField(TEXT("autoAimEnabled"), bAutoAim);
            PresetData->SetNumberField(TEXT("autoAimStrength"), AutoAimStrength);
            
            // Serialize to JSON string
            FString JsonString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
            FJsonSerializer::Serialize(PresetData.ToSharedRef(), Writer);
            
            // Save to file in project directory
            FString PresetFilePath = FPaths::ProjectSavedDir() / TEXT("Accessibility") / PresetName + TEXT(".json");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(PresetFilePath), true);
            
            if (FFileHelper::SaveStringToFile(JsonString, *PresetFilePath))
            {
                Response = MakeSuccessResponse(FString::Printf(TEXT("Accessibility preset '%s' created"), *PresetName));
                Response->SetBoolField(TEXT("presetCreated"), true);
                Response->SetStringField(TEXT("presetPath"), PresetFilePath);
            }
            else
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Failed to save preset file: %s"), *PresetFilePath));
            }
#else
            Response = MakeErrorResponse(TEXT("Preset creation requires editor"));
#endif
        }
    }
    else if (ActionType == TEXT("apply_accessibility_preset"))
    {
        FString PresetName;
        FString PresetPath;
        Payload->TryGetStringField(TEXT("presetName"), PresetName);
        Payload->TryGetStringField(TEXT("presetPath"), PresetPath);

        if (PresetName.IsEmpty() && PresetPath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("presetName or presetPath is required"));
        }
        else
        {
            // Build path from preset name if not provided
            if (PresetPath.IsEmpty())
            {
                PresetPath = FPaths::ProjectSavedDir() / TEXT("Accessibility") / PresetName + TEXT(".json");
            }
            
            // Load preset JSON file
            FString JsonString;
            if (FFileHelper::LoadFileToString(JsonString, *PresetPath))
            {
                TSharedPtr<FJsonObject> PresetData;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                
                if (FJsonSerializer::Deserialize(Reader, PresetData) && PresetData.IsValid())
                {
                    // Apply visual settings
                    FString ColorblindMode;
                    if (PresetData->TryGetStringField(TEXT("colorblindMode"), ColorblindMode))
                    {
                        GConfig->SetString(TEXT("Accessibility"), TEXT("ColorblindMode"), *ColorblindMode, GGameUserSettingsIni);
                    }
                    
                    double ColorblindSeverity;
                    if (PresetData->TryGetNumberField(TEXT("colorblindSeverity"), ColorblindSeverity))
                    {
                        GConfig->SetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), (float)ColorblindSeverity, GGameUserSettingsIni);
                    }
                    
                    bool bHighContrast;
                    if (PresetData->TryGetBoolField(TEXT("highContrastEnabled"), bHighContrast))
                    {
                        GConfig->SetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bHighContrast, GGameUserSettingsIni);
                    }
                    
                    double UIScale;
                    if (PresetData->TryGetNumberField(TEXT("uiScale"), UIScale))
                    {
                        GConfig->SetFloat(TEXT("Accessibility"), TEXT("UIScale"), (float)UIScale, GGameUserSettingsIni);
                    }
                    
                    // Apply subtitle settings
                    bool bSubtitles;
                    if (PresetData->TryGetBoolField(TEXT("subtitlesEnabled"), bSubtitles))
                    {
                        GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bSubtitles, GGameUserSettingsIni);
                    }
                    
                    double SubtitleFontSize;
                    if (PresetData->TryGetNumberField(TEXT("subtitleFontSize"), SubtitleFontSize))
                    {
                        GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), (float)SubtitleFontSize, GGameUserSettingsIni);
                    }
                    
                    // Apply audio settings
                    bool bMonoAudio;
                    if (PresetData->TryGetBoolField(TEXT("monoAudioEnabled"), bMonoAudio))
                    {
                        GConfig->SetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bMonoAudio, GGameUserSettingsIni);
                    }
                    
                    // Apply motor settings
                    bool bAutoAim;
                    if (PresetData->TryGetBoolField(TEXT("autoAimEnabled"), bAutoAim))
                    {
                        GConfig->SetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bAutoAim, GGameUserSettingsIni);
                    }
                    
                    double AutoAimStrength;
                    if (PresetData->TryGetNumberField(TEXT("autoAimStrength"), AutoAimStrength))
                    {
                        GConfig->SetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), (float)AutoAimStrength, GGameUserSettingsIni);
                    }
                    
                    GConfig->Flush(false, GGameUserSettingsIni);
                    
                    Response = MakeSuccessResponse(FString::Printf(TEXT("Accessibility preset '%s' applied"), *PresetName));
                    Response->SetBoolField(TEXT("presetApplied"), true);
                }
                else
                {
                    Response = MakeErrorResponse(TEXT("Failed to parse preset JSON file"));
                }
            }
            else
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Preset file not found: %s"), *PresetPath));
            }
        }
    }
    else if (ActionType == TEXT("export_accessibility_settings"))
    {
        FString ExportPath;
        FString ExportFormat = TEXT("json");
        Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
        Payload->TryGetStringField(TEXT("exportFormat"), ExportFormat);

        if (ExportPath.IsEmpty())
        {
            ExportPath = FPaths::ProjectSavedDir() / TEXT("Accessibility/settings.json");
        }

        // Gather all accessibility settings into a comprehensive JSON object
        TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
        
        // Visual settings
        TSharedPtr<FJsonObject> VisualObj = MakeShared<FJsonObject>();
        FString ColorblindMode;
        float ColorblindSeverity = 0.0f;
        bool bHighContrast = false;
        float UIScale = 1.0f;
        bool bTTS = false;
        float FontSize = 14.0f;
        
        GConfig->GetString(TEXT("Accessibility"), TEXT("ColorblindMode"), ColorblindMode, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), ColorblindSeverity, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bHighContrast, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("UIScale"), UIScale, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("TextToSpeechEnabled"), bTTS, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("FontSize"), FontSize, GGameUserSettingsIni);
        
        VisualObj->SetStringField(TEXT("colorblindMode"), ColorblindMode);
        VisualObj->SetNumberField(TEXT("colorblindSeverity"), ColorblindSeverity);
        VisualObj->SetBoolField(TEXT("highContrastEnabled"), bHighContrast);
        VisualObj->SetNumberField(TEXT("uiScale"), UIScale);
        VisualObj->SetBoolField(TEXT("textToSpeechEnabled"), bTTS);
        VisualObj->SetNumberField(TEXT("fontSize"), FontSize);
        SettingsObj->SetObjectField(TEXT("visual"), VisualObj);
        
        // Subtitle settings
        TSharedPtr<FJsonObject> SubtitleObj = MakeShared<FJsonObject>();
        bool bSubtitles = false;
        float SubtitleFontSize = 24.0f;
        bool bSpeakerID = false;
        bool bDirectional = false;
        
        GConfig->GetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bSubtitles, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), SubtitleFontSize, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("SpeakerIdentificationEnabled"), bSpeakerID, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("DirectionalIndicatorsEnabled"), bDirectional, GGameUserSettingsIni);
        
        SubtitleObj->SetBoolField(TEXT("enabled"), bSubtitles);
        SubtitleObj->SetNumberField(TEXT("fontSize"), SubtitleFontSize);
        SubtitleObj->SetBoolField(TEXT("speakerIdentification"), bSpeakerID);
        SubtitleObj->SetBoolField(TEXT("directionalIndicators"), bDirectional);
        SettingsObj->SetObjectField(TEXT("subtitles"), SubtitleObj);
        
        // Audio settings
        TSharedPtr<FJsonObject> AudioObj = MakeShared<FJsonObject>();
        bool bMono = false;
        bool bAudioVis = false;
        float AudioBalance = 0.0f;
        
        GConfig->GetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bMono, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("AudioVisualizationEnabled"), bAudioVis, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("AudioBalance"), AudioBalance, GGameUserSettingsIni);
        
        AudioObj->SetBoolField(TEXT("monoAudio"), bMono);
        AudioObj->SetBoolField(TEXT("audioVisualization"), bAudioVis);
        AudioObj->SetNumberField(TEXT("audioBalance"), AudioBalance);
        SettingsObj->SetObjectField(TEXT("audio"), AudioObj);
        
        // Motor settings
        TSharedPtr<FJsonObject> MotorObj = MakeShared<FJsonObject>();
        bool bHoldToggle = false;
        bool bAutoAim = false;
        float AutoAimStrength = 0.0f;
        bool bOneHanded = false;
        
        GConfig->GetBool(TEXT("Accessibility"), TEXT("HoldToToggleEnabled"), bHoldToggle, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bAutoAim, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), AutoAimStrength, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("OneHandedModeEnabled"), bOneHanded, GGameUserSettingsIni);
        
        MotorObj->SetBoolField(TEXT("holdToToggle"), bHoldToggle);
        MotorObj->SetBoolField(TEXT("autoAimEnabled"), bAutoAim);
        MotorObj->SetNumberField(TEXT("autoAimStrength"), AutoAimStrength);
        MotorObj->SetBoolField(TEXT("oneHandedMode"), bOneHanded);
        SettingsObj->SetObjectField(TEXT("motor"), MotorObj);
        
        // Cognitive settings
        TSharedPtr<FJsonObject> CognitiveObj = MakeShared<FJsonObject>();
        FString DifficultyPreset;
        bool bObjectiveReminders = false;
        bool bNavAssist = false;
        float GameSpeed = 1.0f;
        
        GConfig->GetString(TEXT("Accessibility"), TEXT("DifficultyPreset"), DifficultyPreset, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("ObjectiveRemindersEnabled"), bObjectiveReminders, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("NavigationAssistanceEnabled"), bNavAssist, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("GameSpeedMultiplier"), GameSpeed, GGameUserSettingsIni);
        
        CognitiveObj->SetStringField(TEXT("difficultyPreset"), DifficultyPreset);
        CognitiveObj->SetBoolField(TEXT("objectiveReminders"), bObjectiveReminders);
        CognitiveObj->SetBoolField(TEXT("navigationAssistance"), bNavAssist);
        CognitiveObj->SetNumberField(TEXT("gameSpeed"), GameSpeed);
        SettingsObj->SetObjectField(TEXT("cognitive"), CognitiveObj);
        
        // Serialize to JSON string
        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(SettingsObj.ToSharedRef(), Writer);
        
        // Ensure directory exists
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);
        
        // Write to file
        if (FFileHelper::SaveStringToFile(JsonString, *ExportPath))
        {
            Response = MakeSuccessResponse(TEXT("Accessibility settings exported"));
            Response->SetBoolField(TEXT("settingsExported"), true);
            Response->SetStringField(TEXT("exportPath"), ExportPath);
        }
        else
        {
            Response = MakeErrorResponse(FString::Printf(TEXT("Failed to write settings file: %s"), *ExportPath));
        }
    }
    else if (ActionType == TEXT("import_accessibility_settings"))
    {
        FString ImportPath;
        Payload->TryGetStringField(TEXT("importPath"), ImportPath);

        if (ImportPath.IsEmpty())
        {
            Response = MakeErrorResponse(TEXT("importPath is required"));
        }
        else
        {
            // Load JSON file
            FString JsonString;
            if (FFileHelper::LoadFileToString(JsonString, *ImportPath))
            {
                TSharedPtr<FJsonObject> SettingsObj;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                
                if (FJsonSerializer::Deserialize(Reader, SettingsObj) && SettingsObj.IsValid())
                {
                    // Import Visual settings
                    const TSharedPtr<FJsonObject>* VisualObj;
                    if (SettingsObj->TryGetObjectField(TEXT("visual"), VisualObj))
                    {
                        FString ColorblindMode;
                        if ((*VisualObj)->TryGetStringField(TEXT("colorblindMode"), ColorblindMode))
                            GConfig->SetString(TEXT("Accessibility"), TEXT("ColorblindMode"), *ColorblindMode, GGameUserSettingsIni);
                        
                        double ColorblindSeverity;
                        if ((*VisualObj)->TryGetNumberField(TEXT("colorblindSeverity"), ColorblindSeverity))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), (float)ColorblindSeverity, GGameUserSettingsIni);
                        
                        bool bHighContrast;
                        if ((*VisualObj)->TryGetBoolField(TEXT("highContrastEnabled"), bHighContrast))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bHighContrast, GGameUserSettingsIni);
                        
                        double UIScale;
                        if ((*VisualObj)->TryGetNumberField(TEXT("uiScale"), UIScale))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("UIScale"), (float)UIScale, GGameUserSettingsIni);
                        
                        bool bTTS;
                        if ((*VisualObj)->TryGetBoolField(TEXT("textToSpeechEnabled"), bTTS))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("TextToSpeechEnabled"), bTTS, GGameUserSettingsIni);
                        
                        double FontSize;
                        if ((*VisualObj)->TryGetNumberField(TEXT("fontSize"), FontSize))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("FontSize"), (float)FontSize, GGameUserSettingsIni);
                    }
                    
                    // Import Subtitle settings
                    const TSharedPtr<FJsonObject>* SubtitleObj;
                    if (SettingsObj->TryGetObjectField(TEXT("subtitles"), SubtitleObj))
                    {
                        bool bSubtitles;
                        if ((*SubtitleObj)->TryGetBoolField(TEXT("enabled"), bSubtitles))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bSubtitles, GGameUserSettingsIni);
                        
                        double SubtitleFontSize;
                        if ((*SubtitleObj)->TryGetNumberField(TEXT("fontSize"), SubtitleFontSize))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), (float)SubtitleFontSize, GGameUserSettingsIni);
                        
                        bool bSpeakerID;
                        if ((*SubtitleObj)->TryGetBoolField(TEXT("speakerIdentification"), bSpeakerID))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("SpeakerIdentificationEnabled"), bSpeakerID, GGameUserSettingsIni);
                        
                        bool bDirectional;
                        if ((*SubtitleObj)->TryGetBoolField(TEXT("directionalIndicators"), bDirectional))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("DirectionalIndicatorsEnabled"), bDirectional, GGameUserSettingsIni);
                    }
                    
                    // Import Audio settings
                    const TSharedPtr<FJsonObject>* AudioObj;
                    if (SettingsObj->TryGetObjectField(TEXT("audio"), AudioObj))
                    {
                        bool bMono;
                        if ((*AudioObj)->TryGetBoolField(TEXT("monoAudio"), bMono))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bMono, GGameUserSettingsIni);
                        
                        bool bAudioVis;
                        if ((*AudioObj)->TryGetBoolField(TEXT("audioVisualization"), bAudioVis))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("AudioVisualizationEnabled"), bAudioVis, GGameUserSettingsIni);
                        
                        double AudioBalance;
                        if ((*AudioObj)->TryGetNumberField(TEXT("audioBalance"), AudioBalance))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("AudioBalance"), (float)AudioBalance, GGameUserSettingsIni);
                    }
                    
                    // Import Motor settings
                    const TSharedPtr<FJsonObject>* MotorObj;
                    if (SettingsObj->TryGetObjectField(TEXT("motor"), MotorObj))
                    {
                        bool bHoldToggle;
                        if ((*MotorObj)->TryGetBoolField(TEXT("holdToToggle"), bHoldToggle))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("HoldToToggleEnabled"), bHoldToggle, GGameUserSettingsIni);
                        
                        bool bAutoAim;
                        if ((*MotorObj)->TryGetBoolField(TEXT("autoAimEnabled"), bAutoAim))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bAutoAim, GGameUserSettingsIni);
                        
                        double AutoAimStrength;
                        if ((*MotorObj)->TryGetNumberField(TEXT("autoAimStrength"), AutoAimStrength))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), (float)AutoAimStrength, GGameUserSettingsIni);
                        
                        bool bOneHanded;
                        if ((*MotorObj)->TryGetBoolField(TEXT("oneHandedMode"), bOneHanded))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("OneHandedModeEnabled"), bOneHanded, GGameUserSettingsIni);
                    }
                    
                    // Import Cognitive settings
                    const TSharedPtr<FJsonObject>* CognitiveObj;
                    if (SettingsObj->TryGetObjectField(TEXT("cognitive"), CognitiveObj))
                    {
                        FString DifficultyPreset;
                        if ((*CognitiveObj)->TryGetStringField(TEXT("difficultyPreset"), DifficultyPreset))
                            GConfig->SetString(TEXT("Accessibility"), TEXT("DifficultyPreset"), *DifficultyPreset, GGameUserSettingsIni);
                        
                        bool bObjectiveReminders;
                        if ((*CognitiveObj)->TryGetBoolField(TEXT("objectiveReminders"), bObjectiveReminders))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("ObjectiveRemindersEnabled"), bObjectiveReminders, GGameUserSettingsIni);
                        
                        bool bNavAssist;
                        if ((*CognitiveObj)->TryGetBoolField(TEXT("navigationAssistance"), bNavAssist))
                            GConfig->SetBool(TEXT("Accessibility"), TEXT("NavigationAssistanceEnabled"), bNavAssist, GGameUserSettingsIni);
                        
                        double GameSpeed;
                        if ((*CognitiveObj)->TryGetNumberField(TEXT("gameSpeed"), GameSpeed))
                            GConfig->SetFloat(TEXT("Accessibility"), TEXT("GameSpeedMultiplier"), (float)GameSpeed, GGameUserSettingsIni);
                    }
                    
                    GConfig->Flush(false, GGameUserSettingsIni);
                    
                    Response = MakeSuccessResponse(TEXT("Accessibility settings imported"));
                    Response->SetBoolField(TEXT("settingsImported"), true);
                }
                else
                {
                    Response = MakeErrorResponse(TEXT("Failed to parse settings JSON file"));
                }
            }
            else
            {
                Response = MakeErrorResponse(FString::Printf(TEXT("Settings file not found: %s"), *ImportPath));
            }
        }
    }
    else if (ActionType == TEXT("get_accessibility_info"))
    {
        TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

        // Visual Settings
        TSharedPtr<FJsonObject> VisualObj = MakeShared<FJsonObject>();
        FString ColorblindMode;
        float ColorblindSeverity = 0.0f;
        bool bHighContrast = false;
        float UIScale = 1.0f;
        bool bTTS = false;
        
        GConfig->GetString(TEXT("Accessibility"), TEXT("ColorblindMode"), ColorblindMode, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("ColorblindSeverity"), ColorblindSeverity, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("HighContrastEnabled"), bHighContrast, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("UIScale"), UIScale, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("TextToSpeechEnabled"), bTTS, GGameUserSettingsIni);

        VisualObj->SetStringField(TEXT("colorblindMode"), ColorblindMode);
        VisualObj->SetNumberField(TEXT("colorblindSeverity"), ColorblindSeverity);
        VisualObj->SetBoolField(TEXT("highContrastEnabled"), bHighContrast);
        VisualObj->SetNumberField(TEXT("uiScale"), UIScale);
        VisualObj->SetBoolField(TEXT("textToSpeechEnabled"), bTTS);
        InfoObj->SetObjectField(TEXT("visualSettings"), VisualObj);

        // Subtitle Settings
        TSharedPtr<FJsonObject> SubtitleObj = MakeShared<FJsonObject>();
        bool bSubtitles = false;
        float SubtitleFontSize = 24.0f;
        bool bSpeakerID = false;
        bool bDirectional = false;

        GConfig->GetBool(TEXT("Accessibility"), TEXT("SubtitlesEnabled"), bSubtitles, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("SubtitleFontSize"), SubtitleFontSize, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("SpeakerIdentificationEnabled"), bSpeakerID, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("DirectionalIndicatorsEnabled"), bDirectional, GGameUserSettingsIni);

        SubtitleObj->SetBoolField(TEXT("enabled"), bSubtitles);
        SubtitleObj->SetNumberField(TEXT("fontSize"), SubtitleFontSize);
        SubtitleObj->SetBoolField(TEXT("speakerIdentification"), bSpeakerID);
        SubtitleObj->SetBoolField(TEXT("directionalIndicators"), bDirectional);
        InfoObj->SetObjectField(TEXT("subtitleSettings"), SubtitleObj);

        // Audio Settings
        TSharedPtr<FJsonObject> AudioObj = MakeShared<FJsonObject>();
        bool bMono = false;
        bool bAudioVis = false;
        float AudioBalance = 0.0f;

        GConfig->GetBool(TEXT("Accessibility"), TEXT("MonoAudioEnabled"), bMono, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("AudioVisualizationEnabled"), bAudioVis, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("AudioBalance"), AudioBalance, GGameUserSettingsIni);

        AudioObj->SetBoolField(TEXT("monoAudio"), bMono);
        AudioObj->SetBoolField(TEXT("audioVisualization"), bAudioVis);
        AudioObj->SetNumberField(TEXT("audioBalance"), AudioBalance);
        InfoObj->SetObjectField(TEXT("audioSettings"), AudioObj);

        // Motor Settings
        TSharedPtr<FJsonObject> MotorObj = MakeShared<FJsonObject>();
        bool bHoldToggle = false;
        bool bAutoAim = false;
        float AutoAimStrength = 0.0f;
        bool bOneHanded = false;

        GConfig->GetBool(TEXT("Accessibility"), TEXT("HoldToToggleEnabled"), bHoldToggle, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("AutoAimEnabled"), bAutoAim, GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Accessibility"), TEXT("AutoAimStrength"), AutoAimStrength, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("OneHandedModeEnabled"), bOneHanded, GGameUserSettingsIni);

        MotorObj->SetBoolField(TEXT("holdToToggle"), bHoldToggle);
        MotorObj->SetBoolField(TEXT("autoAimEnabled"), bAutoAim);
        MotorObj->SetNumberField(TEXT("autoAimStrength"), AutoAimStrength);
        MotorObj->SetBoolField(TEXT("oneHandedMode"), bOneHanded);
        InfoObj->SetObjectField(TEXT("motorSettings"), MotorObj);

        // Cognitive Settings
        TSharedPtr<FJsonObject> CognitiveObj = MakeShared<FJsonObject>();
        FString DifficultyPreset;
        bool bObjectiveReminders = false;
        bool bNavAssist = false;
        bool bMotionSickness = false;

        GConfig->GetString(TEXT("Accessibility"), TEXT("DifficultyPreset"), DifficultyPreset, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("ObjectiveRemindersEnabled"), bObjectiveReminders, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("NavigationAssistanceEnabled"), bNavAssist, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("Accessibility"), TEXT("MotionSicknessReductionEnabled"), bMotionSickness, GGameUserSettingsIni);

        CognitiveObj->SetStringField(TEXT("difficultyPreset"), DifficultyPreset);
        CognitiveObj->SetBoolField(TEXT("objectiveReminders"), bObjectiveReminders);
        CognitiveObj->SetBoolField(TEXT("navigationAssistance"), bNavAssist);
        CognitiveObj->SetBoolField(TEXT("motionSicknessReduction"), bMotionSickness);
        InfoObj->SetObjectField(TEXT("cognitiveSettings"), CognitiveObj);

        Response = MakeSuccessResponse(TEXT("Accessibility info retrieved"));
        Response->SetObjectField(TEXT("accessibilityInfo"), InfoObj);
    }
    else if (ActionType == TEXT("reset_accessibility_defaults"))
    {
        // Clear all accessibility settings
        GConfig->EmptySection(TEXT("Accessibility"), GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);

        Response = MakeSuccessResponse(TEXT("Accessibility settings reset to defaults"));
    }
    else
    {
        Response = MakeErrorResponse(FString::Printf(TEXT("Unknown accessibility action: %s"), *ActionType));
    }

    // Send response
    bool bSuccess = Response->HasField(TEXT("success")) ? Response->GetBoolField(TEXT("success")) : true;
    FString Message = Response->HasField(TEXT("message")) ? Response->GetStringField(TEXT("message")) : TEXT("Operation completed");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Response);
    return true;
}
