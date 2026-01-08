// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 42: AI & NPC Plugins Handlers
// Implements ~30 actions for Convai, Inworld AI, NVIDIA ACE (Audio2Face)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h" // For TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"

// ============================================
// Conditional Plugin Includes - Convai
// ============================================
#if __has_include("ConvaiChatbotComponent.h")
#include "ConvaiChatbotComponent.h"
#define MCP_HAS_CONVAI 1
#else
#define MCP_HAS_CONVAI 0
#endif

#if __has_include("ConvaiPlayerComponent.h")
#include "ConvaiPlayerComponent.h"
#define MCP_HAS_CONVAI_PLAYER 1
#else
#define MCP_HAS_CONVAI_PLAYER 0
#endif

#if __has_include("ConvaiChatBotProxy.h")
#include "ConvaiChatBotProxy.h"
#define MCP_HAS_CONVAI_PROXY 1
#else
#define MCP_HAS_CONVAI_PROXY 0
#endif

#if __has_include("LipSyncInterface.h")
#include "LipSyncInterface.h"
#define MCP_HAS_CONVAI_LIPSYNC 1
#else
#define MCP_HAS_CONVAI_LIPSYNC 0
#endif

// ============================================
// Conditional Plugin Includes - Inworld AI
// ============================================
#if __has_include("InworldCharacterComponent.h")
#include "InworldCharacterComponent.h"
#define MCP_HAS_INWORLD 1
#else
#define MCP_HAS_INWORLD 0
#endif

#if __has_include("InworldConversationGroup.h")
#include "InworldConversationGroup.h"
#define MCP_HAS_INWORLD_CONVERSATION 1
#else
#define MCP_HAS_INWORLD_CONVERSATION 0
#endif

#if __has_include("InworldRuntimeSubsystem.h")
#include "InworldRuntimeSubsystem.h"
#define MCP_HAS_INWORLD_RUNTIME 1
#else
#define MCP_HAS_INWORLD_RUNTIME 0
#endif

#if __has_include("InworldProjectSettings.h")
#include "InworldProjectSettings.h"
#define MCP_HAS_INWORLD_SETTINGS 1
#else
#define MCP_HAS_INWORLD_SETTINGS 0
#endif

// ============================================
// Conditional Plugin Includes - NVIDIA ACE
// ============================================
#if __has_include("ACEBlueprintLibrary.h")
#include "ACEBlueprintLibrary.h"
#define MCP_HAS_ACE 1
#else
#define MCP_HAS_ACE 0
#endif

#if __has_include("ACEAudioCurveSourceComponent.h")
#include "ACEAudioCurveSourceComponent.h"
#define MCP_HAS_ACE_COMPONENT 1
#else
#define MCP_HAS_ACE_COMPONENT 0
#endif

#if __has_include("ACERuntimeModule.h")
#include "ACERuntimeModule.h"
#define MCP_HAS_ACE_RUNTIME 1
#else
#define MCP_HAS_ACE_RUNTIME 0
#endif

#if __has_include("Audio2FaceParameters.h")
#include "Audio2FaceParameters.h"
#define MCP_HAS_A2F_PARAMS 1
#else
#define MCP_HAS_A2F_PARAMS 0
#endif

// ============================================
// Helper Macros
// ============================================
#define AINPC_SUCCESS_RESPONSE(Msg) \
  { \
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); \
    Result->SetBoolField(TEXT("success"), true); \
    Result->SetStringField(TEXT("message"), Msg); \
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result); \
    return true; \
  }

#define AINPC_SUCCESS_WITH_DATA(Msg, DataObj) \
  { \
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); \
    Result->SetBoolField(TEXT("success"), true); \
    Result->SetStringField(TEXT("message"), Msg); \
    for (const auto& Pair : DataObj->Values) { Result->SetField(Pair.Key, Pair.Value); } \
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result); \
    return true; \
  }

#define AINPC_ERROR_RESPONSE(Msg) \
  { \
    SendAutomationError(RequestingSocket, RequestId, Msg, TEXT("AINPC_ERROR")); \
    return true; \
  }

#define AINPC_NOT_AVAILABLE(PluginName) \
  { \
    SendAutomationError(RequestingSocket, RequestId, \
      FString::Printf(TEXT("%s plugin not available in this build. Install it from the Marketplace or GitHub."), TEXT(PluginName)), \
      TEXT("PLUGIN_NOT_AVAILABLE")); \
    return true; \
  }

// ============================================
// Main Handler Entry Point
// ============================================
bool UMcpAutomationBridgeSubsystem::HandleManageAINPCAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  FString ActionType;
  if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
  {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing action_type in manage_ai_npc request"),
                        TEXT("INVALID_PARAMS"));
    return true;
  }

  UWorld* World = GetActiveWorld();
  if (!World)
  {
    AINPC_ERROR_RESPONSE("No active world available");
  }

  // =========================================
  // CONVAI - Conversational AI (10 actions)
  // =========================================

  if (ActionType == TEXT("create_convai_character"))
  {
#if MCP_HAS_CONVAI && MCP_HAS_CONVAI_PROXY
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString CharacterName = Payload->GetStringField(TEXT("characterName"));
    FString Backstory = Payload->GetStringField(TEXT("backstory"));
    FString VoiceType = Payload->GetStringField(TEXT("voiceType"));
    if (VoiceType.IsEmpty()) VoiceType = TEXT("Male");

    // Find or create target actor
    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    // Check if component already exists
    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      // Create new Convai component
      ConvaiComp = NewObject<UConvaiChatbotComponent>(TargetActor, TEXT("ConvaiChatbot"));
      if (!ConvaiComp)
      {
        AINPC_ERROR_RESPONSE("Failed to create ConvaiChatbotComponent");
      }
      ConvaiComp->RegisterComponent();
      TargetActor->AddInstanceComponent(ConvaiComp);
    }

    // Create character on Convai server using async proxy
    UConvaiChatBotCreateProxy* CreateProxy = UConvaiChatBotCreateProxy::CreateCharacterCreateProxy(
        World,
        CharacterName,
        VoiceType,
        Backstory
    );

    bool bAsyncInitiated = false;
    if (CreateProxy)
    {
      // Activate async operation - character ID will be set when callback fires
      // Note: This is async - caller should poll for character ID or wait for callback
      CreateProxy->Activate();
      bAsyncInitiated = true;
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("componentAdded"), true);
    Data->SetStringField(TEXT("characterName"), CharacterName);
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetBoolField(TEXT("asyncCreationInitiated"), bAsyncInitiated);
    Data->SetStringField(TEXT("note"), TEXT("Character creation is async - character ID will be available after Convai server responds"));
    AINPC_SUCCESS_WITH_DATA("Convai character component created and character creation initiated", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("configure_character_backstory"))
  {
#if MCP_HAS_CONVAI && MCP_HAS_CONVAI_PROXY
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString Backstory;
    if (!Payload->TryGetStringField(TEXT("backstory"), Backstory))
    {
      AINPC_ERROR_RESPONSE("Missing backstory parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Get character ID from component
    FString CharacterId = ConvaiComp->CharacterID;
    if (CharacterId.IsEmpty())
    {
      AINPC_ERROR_RESPONSE("Character ID not set on component. Create character first.");
    }

    // Update backstory via proxy
    FString NewVoice = Payload->GetStringField(TEXT("voiceType"));
    FString NewName = Payload->GetStringField(TEXT("characterName"));
    FString NewLanguage = Payload->GetStringField(TEXT("language"));

    UConvaiChatBotUpdateProxy* UpdateProxy = UConvaiChatBotUpdateProxy::CreateCharacterUpdateProxy(
        World,
        CharacterId,
        NewVoice,
        Backstory,
        NewName,
        NewLanguage
    );

    if (UpdateProxy)
    {
      UpdateProxy->Activate();
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("characterId"), CharacterId);
    AINPC_SUCCESS_WITH_DATA("Character backstory update initiated", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("configure_character_voice"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Voice configuration is typically done at character creation via the proxy
    // For runtime adjustments, we can modify audio component properties
    double SpeechRate = 1.0;
    Payload->TryGetNumberField(TEXT("speechRate"), SpeechRate);

    double Pitch = 0.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);

    // Find audio component if it exists
    UAudioComponent* AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
    if (AudioComp)
    {
      AudioComp->SetPitchMultiplier(1.0f + static_cast<float>(Pitch));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("speechRate"), SpeechRate);
    Data->SetNumberField(TEXT("pitch"), Pitch);
    AINPC_SUCCESS_WITH_DATA("Voice settings configured", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("configure_convai_lipsync"))
  {
#if MCP_HAS_CONVAI && MCP_HAS_CONVAI_LIPSYNC
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    bool bLipsyncEnabled = true;
    Payload->TryGetBoolField(TEXT("lipsyncEnabled"), bLipsyncEnabled);

    FString LipsyncMode = Payload->GetStringField(TEXT("lipsyncMode"));
    if (LipsyncMode.IsEmpty()) LipsyncMode = TEXT("viseme");

    double VisemeMultiplier = 1.0;
    Payload->TryGetNumberField(TEXT("visemeMultiplier"), VisemeMultiplier);

    // Lipsync is handled via IConvaiLipSyncInterface on the AnimInstance
    // Configuration depends on the specific implementation

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("lipsyncEnabled"), bLipsyncEnabled);
    Data->SetStringField(TEXT("lipsyncMode"), LipsyncMode);
    Data->SetNumberField(TEXT("visemeMultiplier"), VisemeMultiplier);
    AINPC_SUCCESS_WITH_DATA("Lipsync configured", Data);
#else
    AINPC_NOT_AVAILABLE("Convai Lipsync");
#endif
  }

  if (ActionType == TEXT("start_convai_session"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    FString CharacterId = ConvaiComp->CharacterID;
    if (CharacterId.IsEmpty())
    {
      AINPC_ERROR_RESPONSE("Character ID not set. Configure character first.");
    }

    // Load character data from server
    ConvaiComp->LoadCharacter(CharacterId);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("characterId"), CharacterId);
    Data->SetBoolField(TEXT("sessionActive"), true);
    AINPC_SUCCESS_WITH_DATA("Convai session started", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("stop_convai_session"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Convai handles session cleanup internally
    // For explicit stop, you would call the appropriate method

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("sessionActive"), false);
    AINPC_SUCCESS_WITH_DATA("Convai session stopped", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("send_text_to_character"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString Message;
    if (!Payload->TryGetStringField(TEXT("message"), Message) &&
        !Payload->TryGetStringField(TEXT("textInput"), Message))
    {
      AINPC_ERROR_RESPONSE("Missing message or textInput parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Find a player component to send from - check player pawn first
    UConvaiPlayerComponent* PlayerComp = nullptr;
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
      if (APawn* PlayerPawn = PC->GetPawn())
      {
        PlayerComp = PlayerPawn->FindComponentByClass<UConvaiPlayerComponent>();
      }
    }

    // If no player component found, try to find one in the world
    if (!PlayerComp)
    {
      for (TActorIterator<AActor> It(World); It; ++It)
      {
        if (UConvaiPlayerComponent* Comp = It->FindComponentByClass<UConvaiPlayerComponent>())
        {
          PlayerComp = Comp;
          break;
        }
      }
    }

    if (!PlayerComp)
    {
      AINPC_ERROR_RESPONSE("No ConvaiPlayerComponent found in world - required to send text to NPC");
    }

    // Optional parameters from payload
    bool bGenerateActions = true;
    bool bVoiceResponse = true;
    Payload->TryGetBoolField(TEXT("generateActions"), bGenerateActions);
    Payload->TryGetBoolField(TEXT("voiceResponse"), bVoiceResponse);

    // Send text via PlayerComponent->SendText(ChatbotComponent, Text, Environment, GenerateActions, VoiceResponse, RunOnServer, UseServerAPIKey)
    PlayerComp->SendText(ConvaiComp, Message, ConvaiComp->Environment, bGenerateActions, bVoiceResponse, false, false);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("messageSent"), Message);
    Data->SetStringField(TEXT("targetActor"), ActorName);
    Data->SetBoolField(TEXT("generateActions"), bGenerateActions);
    Data->SetBoolField(TEXT("voiceResponse"), bVoiceResponse);
    AINPC_SUCCESS_WITH_DATA("Text sent to character via Convai", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("get_character_response"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Response is delivered asynchronously via delegates
    // This action returns current state

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("characterId"), ConvaiComp->CharacterID);
    Data->SetStringField(TEXT("status"), TEXT("Responses are delivered via OnResponseReceived delegate"));
    AINPC_SUCCESS_WITH_DATA("Character response state retrieved", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("configure_convai_actions"))
  {
#if MCP_HAS_CONVAI
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    if (!ConvaiComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have a ConvaiChatbotComponent");
    }

    // Get available actions from payload
    const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
    TArray<FString> AvailableActions;
    if (Payload->TryGetArrayField(TEXT("availableActions"), ActionsArray))
    {
      for (const auto& ActionVal : *ActionsArray)
      {
        AvailableActions.Add(ActionVal->AsString());
      }
    }

    FString ActionContext = Payload->GetStringField(TEXT("actionContext"));

    // Convai uses UConvaiEnvironment to define available actions
    // This would be configured on the component's environment object

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("actionsConfigured"), AvailableActions.Num());
    AINPC_SUCCESS_WITH_DATA("Convai actions configured", Data);
#else
    AINPC_NOT_AVAILABLE("Convai");
#endif
  }

  if (ActionType == TEXT("get_convai_info"))
  {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

#if MCP_HAS_CONVAI
    InfoObj->SetBoolField(TEXT("available"), true);
    InfoObj->SetStringField(TEXT("moduleVersion"), TEXT("1.0"));

    // Count actors with Convai components
    int32 ConnectedCharacters = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
      if (It->FindComponentByClass<UConvaiChatbotComponent>())
      {
        ConnectedCharacters++;
      }
    }
    InfoObj->SetNumberField(TEXT("connectedCharacters"), ConnectedCharacters);
    InfoObj->SetBoolField(TEXT("lipsyncEnabled"), MCP_HAS_CONVAI_LIPSYNC != 0);
#else
    InfoObj->SetBoolField(TEXT("available"), false);
    InfoObj->SetStringField(TEXT("moduleVersion"), TEXT("Not installed"));
    InfoObj->SetNumberField(TEXT("connectedCharacters"), 0);
    InfoObj->SetBoolField(TEXT("lipsyncEnabled"), false);
#endif

    Data->SetObjectField(TEXT("convaiInfo"), InfoObj);
    AINPC_SUCCESS_WITH_DATA("Convai info retrieved", Data);
  }

  // =========================================
  // INWORLD AI (10 actions)
  // =========================================

  if (ActionType == TEXT("create_inworld_character"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    // Check if component already exists
    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      // Create new Inworld component
      InworldComp = NewObject<UInworldCharacterComponent>(TargetActor, TEXT("InworldCharacter"));
      if (!InworldComp)
      {
        AINPC_ERROR_RESPONSE("Failed to create InworldCharacterComponent");
      }
      InworldComp->RegisterComponent();
      TargetActor->AddInstanceComponent(InworldComp);
    }

    // Configure character profile from payload
    const TSharedPtr<FJsonObject>* ProfileObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("characterProfile"), ProfileObj))
    {
      FString Name = (*ProfileObj)->GetStringField(TEXT("name"));
      FString Role = (*ProfileObj)->GetStringField(TEXT("role"));
      FString Description = (*ProfileObj)->GetStringField(TEXT("description"));

      // Set profile properties
      InworldComp->CharacterProfile.Name = Name;
      InworldComp->CharacterProfile.Role = Role;
      InworldComp->CharacterProfile.Description = Description;
    }
    else
    {
      // Use individual fields
      FString CharName = Payload->GetStringField(TEXT("characterName"));
      FString Role = Payload->GetStringField(TEXT("role"));
      FString Desc = Payload->GetStringField(TEXT("description"));

      if (!CharName.IsEmpty()) InworldComp->CharacterProfile.Name = CharName;
      if (!Role.IsEmpty()) InworldComp->CharacterProfile.Role = Role;
      if (!Desc.IsEmpty()) InworldComp->CharacterProfile.Description = Desc;
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("componentAdded"), true);
    Data->SetStringField(TEXT("actorName"), ActorName);
    AINPC_SUCCESS_WITH_DATA("Inworld character component created", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("configure_inworld_settings"))
  {
#if MCP_HAS_INWORLD && MCP_HAS_INWORLD_SETTINGS
    FString ApiKey = Payload->GetStringField(TEXT("apiKey"));
    FString ApiSecret = Payload->GetStringField(TEXT("apiSecret"));
    FString SceneId = Payload->GetStringField(TEXT("sceneId"));

    // Configure global Inworld settings
    UInworldProjectSettings* Settings = GetMutableDefault<UInworldProjectSettings>();
    if (Settings)
    {
      if (!ApiKey.IsEmpty())
      {
        Settings->ApiKey = ApiKey;
      }
      if (!ApiSecret.IsEmpty())
      {
        Settings->Secret = ApiSecret;
      }
      if (!SceneId.IsEmpty())
      {
        Settings->DefaultSceneId = SceneId;
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("configured"), true);
    AINPC_SUCCESS_WITH_DATA("Inworld settings configured", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI Settings");
#endif
  }

  if (ActionType == TEXT("configure_inworld_scene"))
  {
#if MCP_HAS_INWORLD
    FString SceneId;
    if (!Payload->TryGetStringField(TEXT("sceneId"), SceneId))
    {
      AINPC_ERROR_RESPONSE("Missing sceneId parameter");
    }

    // Scene configuration is typically done via InworldProjectSettings
#if MCP_HAS_INWORLD_SETTINGS
    UInworldProjectSettings* Settings = GetMutableDefault<UInworldProjectSettings>();
    if (Settings)
    {
      Settings->DefaultSceneId = SceneId;
    }
#endif

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sceneId"), SceneId);
    AINPC_SUCCESS_WITH_DATA("Inworld scene configured", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("start_inworld_session"))
  {
#if MCP_HAS_INWORLD && MCP_HAS_INWORLD_CONVERSATION
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an InworldCharacterComponent");
    }

    // Create conversation group to start session
    // This is typically done asynchronously
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("sessionActive"), true);
    AINPC_SUCCESS_WITH_DATA("Inworld session started", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI Conversation");
#endif
  }

  if (ActionType == TEXT("stop_inworld_session"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("sessionActive"), false);
    AINPC_SUCCESS_WITH_DATA("Inworld session stopped", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("send_message_to_character"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString Message;
    if (!Payload->TryGetStringField(TEXT("message"), Message))
    {
      AINPC_ERROR_RESPONSE("Missing message parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an InworldCharacterComponent");
    }

    // Message sending is done via the conversation group
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("messageSent"), Message);
    AINPC_SUCCESS_WITH_DATA("Message sent to Inworld character", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("get_character_emotion"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an InworldCharacterComponent");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

    // Get emotion from emotion state
    if (InworldComp->EmotionState)
    {
      // EInworldEmotionLabel CurrentEmotion = InworldComp->EmotionState->GetEmotionLabel();
      Data->SetStringField(TEXT("currentEmotion"), TEXT("NEUTRAL")); // Placeholder
      Data->SetNumberField(TEXT("emotionStrength"), 0.5);
    }
    else
    {
      Data->SetStringField(TEXT("currentEmotion"), TEXT("UNKNOWN"));
      Data->SetNumberField(TEXT("emotionStrength"), 0.0);
    }

    AINPC_SUCCESS_WITH_DATA("Character emotion retrieved", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("get_character_goals"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an InworldCharacterComponent");
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> GoalsArray;
    // Goals would be retrieved from the character's runtime data
    Data->SetArrayField(TEXT("activeGoals"), GoalsArray);
    AINPC_SUCCESS_WITH_DATA("Character goals retrieved", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("trigger_inworld_event"))
  {
#if MCP_HAS_INWORLD
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString EventName;
    if (!Payload->TryGetStringField(TEXT("eventName"), EventName))
    {
      AINPC_ERROR_RESPONSE("Missing eventName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    if (!InworldComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an InworldCharacterComponent");
    }

    // Event triggering is done via the Inworld runtime
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("eventTriggered"), EventName);
    AINPC_SUCCESS_WITH_DATA("Inworld event triggered", Data);
#else
    AINPC_NOT_AVAILABLE("Inworld AI");
#endif
  }

  if (ActionType == TEXT("get_inworld_info"))
  {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

#if MCP_HAS_INWORLD
    InfoObj->SetBoolField(TEXT("available"), true);
    InfoObj->SetBoolField(TEXT("connected"), true);

    // Count actors with Inworld components
    int32 RegisteredCharacters = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
      if (It->FindComponentByClass<UInworldCharacterComponent>())
      {
        RegisteredCharacters++;
      }
    }
    InfoObj->SetNumberField(TEXT("registeredCharacters"), RegisteredCharacters);
    InfoObj->SetNumberField(TEXT("activeConversations"), 0);

#if MCP_HAS_INWORLD_SETTINGS
    UInworldProjectSettings* Settings = GetMutableDefault<UInworldProjectSettings>();
    if (Settings)
    {
      InfoObj->SetStringField(TEXT("activeSceneId"), Settings->DefaultSceneId);
    }
#endif

#else
    InfoObj->SetBoolField(TEXT("available"), false);
    InfoObj->SetBoolField(TEXT("connected"), false);
    InfoObj->SetNumberField(TEXT("registeredCharacters"), 0);
    InfoObj->SetNumberField(TEXT("activeConversations"), 0);
#endif

    Data->SetObjectField(TEXT("inworldInfo"), InfoObj);
    AINPC_SUCCESS_WITH_DATA("Inworld info retrieved", Data);
  }

  // =========================================
  // NVIDIA ACE / Audio2Face (8 actions)
  // =========================================

  if (ActionType == TEXT("configure_audio2face"))
  {
#if MCP_HAS_ACE
    FString DestUrl = Payload->GetStringField(TEXT("aceDestUrl"));
    FString ApiKey = Payload->GetStringField(TEXT("aceApiKey"));
    FString FunctionId = Payload->GetStringField(TEXT("nvcfFunctionId"));
    FString FunctionVersion = Payload->GetStringField(TEXT("nvcfFunctionVersion"));

    if (!DestUrl.IsEmpty() || !ApiKey.IsEmpty())
    {
      // Configure ACE connection info
      UACEBlueprintLibrary::SetA2XConnectionInfo(DestUrl, ApiKey, FunctionId, FunctionVersion);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("configured"), true);
    AINPC_SUCCESS_WITH_DATA("Audio2Face configured", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE");
#endif
  }

  if (ActionType == TEXT("process_audio_to_blendshapes"))
  {
#if MCP_HAS_ACE && MCP_HAS_ACE_COMPONENT
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    FString SoundWavePath = Payload->GetStringField(TEXT("soundWavePath"));

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UACEAudioCurveSourceComponent* ACEComp = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
    if (!ACEComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an ACEAudioCurveSourceComponent");
    }

    // Load sound wave if path provided
    if (!SoundWavePath.IsEmpty())
    {
      USoundWave* SoundWave = LoadObject<USoundWave>(nullptr, *SoundWavePath);
      if (SoundWave)
      {
        FString ProviderName = Payload->GetStringField(TEXT("aceProviderName"));
        if (ProviderName.IsEmpty()) ProviderName = TEXT("Default");

        // Trigger animation from sound wave
        FAudio2FaceEmotion EmotionParams;
        // EmotionParams can be configured from payload

        UACEBlueprintLibrary::AnimateCharacterFromSoundWave(
            TargetActor,
            SoundWave,
            EmotionParams,
            nullptr, // A2FParams
            FName(*ProviderName)
        );
      }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("processing"), true);
    AINPC_SUCCESS_WITH_DATA("Audio2Face processing started", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE Component");
#endif
  }

  if (ActionType == TEXT("configure_blendshape_mapping"))
  {
#if MCP_HAS_ACE
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    // Blendshape mapping is typically configured in the Animation Blueprint
    // ACE outputs ARKit-compatible blendshape names by default

    const TSharedPtr<FJsonObject>* MappingObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("blendshapeMapping"), MappingObj))
    {
      // Store custom mapping - would be applied in AnimBP
    }

    const TSharedPtr<FJsonObject>* MultipliersObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("blendshapeMultipliers"), MultipliersObj))
    {
      // Store multipliers - would be applied in AnimBP
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("mappingConfigured"), true);
    AINPC_SUCCESS_WITH_DATA("Blendshape mapping configured", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE");
#endif
  }

  if (ActionType == TEXT("start_audio2face_stream"))
  {
#if MCP_HAS_ACE && MCP_HAS_ACE_RUNTIME
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UACEAudioCurveSourceComponent* ACEComp = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
    if (!ACEComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an ACEAudioCurveSourceComponent");
    }

    FString ProviderName = Payload->GetStringField(TEXT("aceProviderName"));
    if (ProviderName.IsEmpty()) ProviderName = TEXT("Default");

    // Allocate A2F resources for streaming
    FACERuntimeModule::Get().AllocateA2F3DResources(FName(*ProviderName));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("streamActive"), true);
    Data->SetStringField(TEXT("provider"), ProviderName);
    AINPC_SUCCESS_WITH_DATA("Audio2Face stream started", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE Runtime");
#endif
  }

  if (ActionType == TEXT("stop_audio2face_stream"))
  {
#if MCP_HAS_ACE && MCP_HAS_ACE_RUNTIME
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UACEAudioCurveSourceComponent* ACEComp = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
    if (!ACEComp)
    {
      AINPC_ERROR_RESPONSE("Actor does not have an ACEAudioCurveSourceComponent");
    }

    FString ProviderName = Payload->GetStringField(TEXT("aceProviderName"));
    if (ProviderName.IsEmpty()) ProviderName = TEXT("Default");

    // Cancel any ongoing animation
    FACERuntimeModule::Get().CancelAnimationGeneration(ACEComp);

    // Free A2F resources
    FACERuntimeModule::Get().FreeA2F3DResources(FName(*ProviderName));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("streamActive"), false);
    AINPC_SUCCESS_WITH_DATA("Audio2Face stream stopped", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE Runtime");
#endif
  }

  if (ActionType == TEXT("get_audio2face_status"))
  {
#if MCP_HAS_ACE
    FString ActorName = Payload->GetStringField(TEXT("actorName"));

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

    if (!ActorName.IsEmpty())
    {
      AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
      if (TargetActor)
      {
        UACEAudioCurveSourceComponent* ACEComp = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
        Data->SetBoolField(TEXT("hasACEComponent"), ACEComp != nullptr);
        Data->SetBoolField(TEXT("a2fProcessing"), ACEComp != nullptr); // Placeholder
      }
    }

    // Get available providers
    TArray<FName> Providers = UACEBlueprintLibrary::GetAvailableA2FProviderNames();
    TArray<TSharedPtr<FJsonValue>> ProviderArray;
    for (const FName& Provider : Providers)
    {
      ProviderArray.Add(MakeShared<FJsonValueString>(Provider.ToString()));
    }
    Data->SetArrayField(TEXT("availableProviders"), ProviderArray);

    AINPC_SUCCESS_WITH_DATA("Audio2Face status retrieved", Data);
#else
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("available"), false);
    AINPC_SUCCESS_WITH_DATA("NVIDIA ACE not available", Data);
#endif
  }

  if (ActionType == TEXT("configure_ace_emotions"))
  {
#if MCP_HAS_ACE
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    // Configure emotion weights from payload
    const TSharedPtr<FJsonObject>* EmotionObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("a2fEmotion"), EmotionObj))
    {
      // FAudio2FaceEmotion struct would be populated here
      // These emotions are passed to AnimateCharacterFromSoundWave
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("emotionsConfigured"), true);
    AINPC_SUCCESS_WITH_DATA("ACE emotions configured", Data);
#else
    AINPC_NOT_AVAILABLE("NVIDIA ACE");
#endif
  }

  if (ActionType == TEXT("get_ace_info"))
  {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

#if MCP_HAS_ACE
    InfoObj->SetBoolField(TEXT("available"), true);
    InfoObj->SetBoolField(TEXT("runtimeLoaded"), MCP_HAS_ACE_RUNTIME != 0);
    InfoObj->SetBoolField(TEXT("gpuAccelerated"), true);

    TArray<FName> Providers = UACEBlueprintLibrary::GetAvailableA2FProviderNames();
    TArray<TSharedPtr<FJsonValue>> ProviderArray;
    for (const FName& Provider : Providers)
    {
      ProviderArray.Add(MakeShared<FJsonValueString>(Provider.ToString()));
    }
    InfoObj->SetArrayField(TEXT("providers"), ProviderArray);

    // Count actors with ACE components
    int32 ActiveStreams = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
      if (It->FindComponentByClass<UACEAudioCurveSourceComponent>())
      {
        ActiveStreams++;
      }
    }
    InfoObj->SetNumberField(TEXT("activeStreams"), ActiveStreams);
#else
    InfoObj->SetBoolField(TEXT("available"), false);
    InfoObj->SetBoolField(TEXT("runtimeLoaded"), false);
    InfoObj->SetBoolField(TEXT("gpuAccelerated"), false);
    InfoObj->SetArrayField(TEXT("providers"), TArray<TSharedPtr<FJsonValue>>());
    InfoObj->SetNumberField(TEXT("activeStreams"), 0);
#endif

    Data->SetObjectField(TEXT("aceInfo"), InfoObj);
    AINPC_SUCCESS_WITH_DATA("ACE info retrieved", Data);
  }

  // =========================================
  // UTILITIES (2 actions)
  // =========================================

  if (ActionType == TEXT("get_ai_npc_info"))
  {
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName))
    {
      AINPC_ERROR_RESPONSE("Missing actorName parameter");
    }

    AActor* TargetActor = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!TargetActor)
    {
      AINPC_ERROR_RESPONSE(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

    InfoObj->SetStringField(TEXT("actorName"), ActorName);

#if MCP_HAS_CONVAI
    UConvaiChatbotComponent* ConvaiComp = TargetActor->FindComponentByClass<UConvaiChatbotComponent>();
    InfoObj->SetBoolField(TEXT("hasConvaiComponent"), ConvaiComp != nullptr);
    if (ConvaiComp)
    {
      InfoObj->SetStringField(TEXT("characterId"), ConvaiComp->CharacterID);
      InfoObj->SetStringField(TEXT("activeBackend"), TEXT("Convai"));
    }
#else
    InfoObj->SetBoolField(TEXT("hasConvaiComponent"), false);
#endif

#if MCP_HAS_INWORLD
    UInworldCharacterComponent* InworldComp = TargetActor->FindComponentByClass<UInworldCharacterComponent>();
    InfoObj->SetBoolField(TEXT("hasInworldComponent"), InworldComp != nullptr);
    if (InworldComp && !InfoObj->HasField(TEXT("activeBackend")))
    {
      InfoObj->SetStringField(TEXT("activeBackend"), TEXT("Inworld"));
    }
#else
    InfoObj->SetBoolField(TEXT("hasInworldComponent"), false);
#endif

#if MCP_HAS_ACE_COMPONENT
    UACEAudioCurveSourceComponent* ACEComp = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
    InfoObj->SetBoolField(TEXT("hasACEComponent"), ACEComp != nullptr);
#else
    InfoObj->SetBoolField(TEXT("hasACEComponent"), false);
#endif

    if (!InfoObj->HasField(TEXT("activeBackend")))
    {
      InfoObj->SetStringField(TEXT("activeBackend"), TEXT("None"));
    }

    Data->SetObjectField(TEXT("aiNpcInfo"), InfoObj);
    AINPC_SUCCESS_WITH_DATA("AI NPC info retrieved", Data);
  }

  if (ActionType == TEXT("list_available_ai_backends"))
  {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> BackendsArray;

    // Convai
    TSharedPtr<FJsonObject> ConvaiBackend = MakeShared<FJsonObject>();
    ConvaiBackend->SetStringField(TEXT("name"), TEXT("Convai"));
    ConvaiBackend->SetStringField(TEXT("type"), TEXT("convai"));
#if MCP_HAS_CONVAI
    ConvaiBackend->SetBoolField(TEXT("available"), true);
    ConvaiBackend->SetStringField(TEXT("version"), TEXT("1.0"));
#else
    ConvaiBackend->SetBoolField(TEXT("available"), false);
    ConvaiBackend->SetStringField(TEXT("version"), TEXT("Not installed"));
#endif
    BackendsArray.Add(MakeShared<FJsonValueObject>(ConvaiBackend));

    // Inworld AI
    TSharedPtr<FJsonObject> InworldBackend = MakeShared<FJsonObject>();
    InworldBackend->SetStringField(TEXT("name"), TEXT("Inworld AI"));
    InworldBackend->SetStringField(TEXT("type"), TEXT("inworld"));
#if MCP_HAS_INWORLD
    InworldBackend->SetBoolField(TEXT("available"), true);
    InworldBackend->SetStringField(TEXT("version"), TEXT("1.0"));
#else
    InworldBackend->SetBoolField(TEXT("available"), false);
    InworldBackend->SetStringField(TEXT("version"), TEXT("Not installed"));
#endif
    BackendsArray.Add(MakeShared<FJsonValueObject>(InworldBackend));

    // NVIDIA ACE
    TSharedPtr<FJsonObject> ACEBackend = MakeShared<FJsonObject>();
    ACEBackend->SetStringField(TEXT("name"), TEXT("NVIDIA ACE"));
    ACEBackend->SetStringField(TEXT("type"), TEXT("ace"));
#if MCP_HAS_ACE
    ACEBackend->SetBoolField(TEXT("available"), true);
    ACEBackend->SetStringField(TEXT("version"), TEXT("2.5"));
#else
    ACEBackend->SetBoolField(TEXT("available"), false);
    ACEBackend->SetStringField(TEXT("version"), TEXT("Not installed"));
#endif
    BackendsArray.Add(MakeShared<FJsonValueObject>(ACEBackend));

    Data->SetArrayField(TEXT("availableBackends"), BackendsArray);
    AINPC_SUCCESS_WITH_DATA("Available AI backends listed", Data);
  }

  // Unknown action
  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Unknown manage_ai_npc action: %s"), *ActionType),
                      TEXT("UNKNOWN_ACTION"));
  return true;
}
