#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "EditorAssetLibrary.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#if __has_include("Factories/WidgetBlueprintFactory.h")
#include "Factories/WidgetBlueprintFactory.h"
#define MCP_HAS_WIDGET_FACTORY 1
#else
#define MCP_HAS_WIDGET_FACTORY 0
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleUiAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString LowerAction = Action.ToLower();
    bool bIsSystemControl = LowerAction.Equals(TEXT("system_control"), ESearchCase::IgnoreCase);
    bool bIsManageUi = LowerAction.Equals(TEXT("manage_ui"), ESearchCase::IgnoreCase);

    if (!bIsSystemControl && !bIsManageUi)
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    if (Payload->HasField(TEXT("subAction")))
    {
        SubAction = Payload->GetStringField(TEXT("subAction"));
    }
    else
    {
        Payload->TryGetStringField(TEXT("action"), SubAction);
    }
    const FString LowerSub = SubAction.ToLower();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("action"), LowerSub);

    bool bSuccess = false;
    FString Message;
    FString ErrorCode;

#if WITH_EDITOR
    if (LowerSub == TEXT("create_widget"))
    {
#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
        FString WidgetName;
        if (!Payload->TryGetStringField(TEXT("name"), WidgetName) || WidgetName.IsEmpty())
        {
            Message = TEXT("name field required for create_widget");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (SavePath.IsEmpty())
            {
                SavePath = TEXT("/Game/UI/Widgets");
            }

            FString WidgetType;
            Payload->TryGetStringField(TEXT("widgetType"), WidgetType);

            const FString NormalizedPath = SavePath.TrimStartAndEnd();
            const FString TargetPath = FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);
            if (UEditorAssetLibrary::DoesAssetExist(TargetPath))
            {
                bSuccess = true;
                Message = FString::Printf(TEXT("Widget blueprint already exists at %s"), *TargetPath);
                Resp->SetStringField(TEXT("widgetPath"), TargetPath);
                Resp->SetBoolField(TEXT("exists"), true);
                if (!WidgetType.IsEmpty())
                {
                    Resp->SetStringField(TEXT("widgetType"), WidgetType);
                }
                Resp->SetStringField(TEXT("widgetName"), WidgetName);
            }
            else
            {
                UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
                if (!Factory)
                {
                    Message = TEXT("Failed to create widget blueprint factory");
                    ErrorCode = TEXT("FACTORY_CREATION_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    UObject* NewAsset = Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), 
                        UEditorAssetLibrary::DoesAssetExist(NormalizedPath) ? UEditorAssetLibrary::LoadAsset(NormalizedPath) : nullptr, FName(*WidgetName), 
                        RF_Standalone, nullptr, GWarn);

                    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);

                    if (!WidgetBlueprint)
                    {
                        Message = TEXT("Failed to create widget blueprint asset");
                        ErrorCode = TEXT("ASSET_CREATION_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        UEditorAssetLibrary::SaveAsset(WidgetBlueprint->GetPathName());

                        bSuccess = true;
                        Message = FString::Printf(TEXT("Widget blueprint created at %s"), *WidgetBlueprint->GetPathName());
                        Resp->SetStringField(TEXT("widgetPath"), WidgetBlueprint->GetPathName());
                        Resp->SetStringField(TEXT("widgetName"), WidgetName);
                        if (!WidgetType.IsEmpty())
                        {
                            Resp->SetStringField(TEXT("widgetType"), WidgetType);
                        }
                    }
                }
            }
        }
#else
        Message = TEXT("create_widget requires editor build with widget factory support");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
    }
    else if (LowerSub == TEXT("screenshot"))
    {
        // Take a screenshot of the viewport
        FString ScreenshotPath;
        Payload->TryGetStringField(TEXT("path"), ScreenshotPath);
        if (ScreenshotPath.IsEmpty())
        {
            ScreenshotPath = TEXT("../../../Saved/Screenshots/WindowsEditor");
        }
        
        FString Filename;
        Payload->TryGetStringField(TEXT("filename"), Filename);
        if (Filename.IsEmpty())
        {
            Filename = FString::Printf(TEXT("Screenshot_%lld"), FDateTime::Now().ToUnixTimestamp());
        }

        // Execute screenshot command
        FString FullCommand = FString::Printf(TEXT("shot %s/%s"), *ScreenshotPath, *Filename);
        bool bCommandSuccess = GEditor->Exec(nullptr, *FullCommand);
        
        if (bCommandSuccess)
        {
            bSuccess = true;
            Message = FString::Printf(TEXT("Screenshot saved to %s/%s.png"), *ScreenshotPath, *Filename);
            Resp->SetStringField(TEXT("screenshotPath"), ScreenshotPath);
            Resp->SetStringField(TEXT("filename"), Filename);
        }
        else
        {
            Message = TEXT("Failed to take screenshot");
            ErrorCode = TEXT("SCREENSHOT_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
        }
    }
    else if (LowerSub == TEXT("play_in_editor"))
    {
        // Start play in editor
        if (GEditor && GEditor->PlayWorld)
        {
            Message = TEXT("Already playing in editor");
            ErrorCode = TEXT("ALREADY_PLAYING");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            // Execute play command
            bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Play In Editor"));
            if (bCommandSuccess)
            {
                bSuccess = true;
                Message = TEXT("Started play in editor");
                Resp->SetStringField(TEXT("status"), TEXT("playing"));
            }
            else
            {
                Message = TEXT("Failed to start play in editor");
                ErrorCode = TEXT("PLAY_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
            }
        }
    }
    else if (LowerSub == TEXT("stop_play"))
    {
        // Stop play in editor
        if (GEditor && GEditor->PlayWorld)
        {
            // Execute stop command
            bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Stop Play In Editor"));
            if (bCommandSuccess)
            {
                bSuccess = true;
                Message = TEXT("Stopped play in editor");
                Resp->SetStringField(TEXT("status"), TEXT("stopped"));
            }
            else
            {
                Message = TEXT("Failed to stop play in editor");
                ErrorCode = TEXT("STOP_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
            }
        }
        else
        {
            Message = TEXT("Not currently playing in editor");
            ErrorCode = TEXT("NOT_PLAYING");
            Resp->SetStringField(TEXT("error"), Message);
        }
    }
    else if (LowerSub == TEXT("save_all"))
    {
        // Save all assets and levels
        bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Asset Save All"));
        if (bCommandSuccess)
        {
            bSuccess = true;
            Message = TEXT("Saved all assets");
            Resp->SetStringField(TEXT("status"), TEXT("saved"));
        }
        else
        {
            Message = TEXT("Failed to save all assets");
            ErrorCode = TEXT("SAVE_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
        }
    }
    else if (LowerSub == TEXT("simulate_input"))
    {
        FString KeyName;
        Payload->TryGetStringField(TEXT("keyName"), KeyName); // Changed to keyName to match schema
        if (KeyName.IsEmpty()) Payload->TryGetStringField(TEXT("key"), KeyName); // Fallback

        FString EventType;
        Payload->TryGetStringField(TEXT("eventType"), EventType);
        
        FKey Key = FKey(FName(*KeyName));
        if (Key.IsValid())
        {
            const uint32 CharacterCode = 0;
            const uint32 KeyCode = 0;
            const bool bIsRepeat = false;
            FModifierKeysState ModifierState; 

            if (EventType == TEXT("KeyDown"))
            {
                FKeyEvent KeyEvent(Key, ModifierState, FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat, CharacterCode, KeyCode);
                FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
            }
            else if (EventType == TEXT("KeyUp"))
            {
                FKeyEvent KeyEvent(Key, ModifierState, FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat, CharacterCode, KeyCode);
                FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
            }
            else
            {
                // Press and Release
                FKeyEvent KeyDownEvent(Key, ModifierState, FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat, CharacterCode, KeyCode);
                FSlateApplication::Get().ProcessKeyDownEvent(KeyDownEvent);
                
                FKeyEvent KeyUpEvent(Key, ModifierState, FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat, CharacterCode, KeyCode);
                FSlateApplication::Get().ProcessKeyUpEvent(KeyUpEvent);
            }
            
            bSuccess = true;
            Message = FString::Printf(TEXT("Simulated input for key: %s"), *KeyName);
        }
        else
        {
            Message = FString::Printf(TEXT("Invalid key name: %s"), *KeyName);
            ErrorCode = TEXT("INVALID_KEY");
            Resp->SetStringField(TEXT("error"), Message);
        }
    }
    else
    {
        Message = FString::Printf(TEXT("System control action '%s' not implemented"), *LowerSub);
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
    }
#else
    Message = TEXT("System control actions require editor build.");
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
#endif

    Resp->SetBoolField(TEXT("success"), bSuccess);
    if (Message.IsEmpty())
    {
        Message = bSuccess ? TEXT("System control action completed") : TEXT("System control action failed");
    }

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
}
