// =============================================================================
// McpAutomationBridge_DebugHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Gameplay Debugger Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_debug
//   - spawn_category: Toggle gameplay debugger category visibility
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: GameplayDebugger module (optional)
//
// Notes:
//   - Uses console command "GameplayDebuggerCategory [name]" for robustness
//   - Alternative: IGameplayDebugger::Get().ToggleCategory() (requires module)
//   - Works with PIE (Play In Editor) for runtime debugging
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameplayDebuggerCategory.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayDebuggerConfig.h"
#if __has_include("GameplayDebuggerModule.h")
#include "GameplayDebuggerModule.h"
#define MCP_HAS_GAMEPLAY_DEBUGGER_MODULE_HEADER 1
#else
#define MCP_HAS_GAMEPLAY_DEBUGGER_MODULE_HEADER 0
#endif
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleDebugAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    const FString LowerAction = Action.ToLower();
    if (LowerAction != TEXT("manage_debug") && LowerAction != TEXT("spawn_category"))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction")).ToLower();
    if (SubAction.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("action"), SubAction);
        SubAction = SubAction.ToLower();
    }
    if (SubAction.IsEmpty() && LowerAction == TEXT("spawn_category"))
    {
        SubAction = TEXT("spawn_category");
    }

    // -------------------------------------------------------------------------
    // spawn_category: Toggle gameplay debugger category
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("spawn_category"))
    {
        // Accept both 'categoryName' and 'category' for flexibility
        FString CategoryName;
        if (!Payload->TryGetStringField(TEXT("categoryName"), CategoryName))
        {
            // Fallback to 'category' field if 'categoryName' not found
            Payload->TryGetStringField(TEXT("category"), CategoryName);
        }

        if (CategoryName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing or empty category/categoryName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Sanitize categoryName to prevent command injection
        // Only allow alphanumeric characters and underscores
        CategoryName = CategoryName.TrimStartAndEnd();
        bool bHasInvalidChars = false;
        for (int32 i = 0; i < CategoryName.Len(); i++)
        {
            TCHAR Ch = CategoryName[i];
            if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-'))
            {
                bHasInvalidChars = true;
                break;
            }
        }

        if (bHasInvalidChars)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid categoryName. Only alphanumeric characters, underscores, and hyphens are allowed."),
                TEXT("INVALID_CATEGORY_NAME"));
            return true;
        }

        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

#if MCP_HAS_GAMEPLAY_DEBUGGER_MODULE_HEADER
        FGameplayDebuggerModule* GameplayDebuggerModule =
            FModuleManager::LoadModulePtr<FGameplayDebuggerModule>(TEXT("GameplayDebugger"));
#else
        const bool bGameplayDebuggerModuleLoaded = FModuleManager::Get().LoadModule(TEXT("GameplayDebugger")) != nullptr;
#endif

        bool bConfigUpdated = false;
        bool bReplicatorUpdated = false;
        int32 UpdatedReplicatorCount = 0;
        int32 UpdatedCategoryIndex = INDEX_NONE;

        if (UGameplayDebuggerConfig* Config = GetMutableDefault<UGameplayDebuggerConfig>())
        {
            FGameplayDebuggerCategoryConfig* CategoryConfig = Config->Categories.FindByPredicate(
                [&CategoryName](const FGameplayDebuggerCategoryConfig& Entry) {
                    return Entry.CategoryName.Equals(CategoryName, ESearchCase::IgnoreCase);
                });
            if (!CategoryConfig)
            {
                CategoryConfig = &Config->Categories.AddDefaulted_GetRef();
                CategoryConfig->CategoryName = CategoryName;
            }

            CategoryConfig->ActiveInGame = bEnabled
                ? EGameplayDebuggerOverrideMode::Enable
                : EGameplayDebuggerOverrideMode::Disable;
            CategoryConfig->ActiveInSimulate = bEnabled
                ? EGameplayDebuggerOverrideMode::Enable
                : EGameplayDebuggerOverrideMode::Disable;
            CategoryConfig->Hidden = EGameplayDebuggerOverrideMode::Disable;
            bConfigUpdated = true;
        }

#if MCP_HAS_GAMEPLAY_DEBUGGER_MODULE_HEADER
        if (GameplayDebuggerModule)
        {
            GameplayDebuggerModule->NotifyCategoriesChanged();
        }
#endif

        UWorld* World = nullptr;
#if WITH_EDITOR
        if (GEditor)
        {
            World = GEditor->PlayWorld.Get();
            if (!World)
            {
                World = GEditor->GetEditorWorldContext().World();
            }
        }
#endif
        if (!World)
        {
            World = GetWorld();
        }

        if (World)
        {
            for (TActorIterator<AGameplayDebuggerCategoryReplicator> It(World); It; ++It)
            {
                AGameplayDebuggerCategoryReplicator* Replicator = *It;
                if (!IsValid(Replicator))
                {
                    continue;
                }

                for (int32 CategoryIndex = 0; CategoryIndex < Replicator->GetNumCategories(); ++CategoryIndex)
                {
                    const FString ReplicatorCategoryName = Replicator->GetCategory(CategoryIndex)->GetCategoryName().ToString();
                    if (ReplicatorCategoryName.Equals(CategoryName, ESearchCase::IgnoreCase))
                    {
                        Replicator->SetEnabled(true);
                        Replicator->SetCategoryEnabled(CategoryIndex, bEnabled);
                        Replicator->CollectCategoryData(false);
                        bReplicatorUpdated = true;
                        ++UpdatedReplicatorCount;
                        UpdatedCategoryIndex = CategoryIndex;
                        break;
                    }
                }
            }
        }

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("categoryName"), CategoryName);
        Result->SetBoolField(TEXT("enabled"), bEnabled);
#if MCP_HAS_GAMEPLAY_DEBUGGER_MODULE_HEADER
        Result->SetBoolField(TEXT("moduleLoaded"), GameplayDebuggerModule != nullptr);
#else
        Result->SetBoolField(TEXT("moduleLoaded"), bGameplayDebuggerModuleLoaded);
#endif
        Result->SetBoolField(TEXT("configUpdated"), bConfigUpdated);
        Result->SetBoolField(TEXT("replicatorUpdated"), bReplicatorUpdated);
        Result->SetNumberField(TEXT("updatedReplicatorCount"), UpdatedReplicatorCount);
        if (UpdatedCategoryIndex != INDEX_NONE)
        {
            Result->SetNumberField(TEXT("categoryIndex"), UpdatedCategoryIndex);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Gameplay debugger category %s: %s"),
                bEnabled ? TEXT("enabled") : TEXT("disabled"), *CategoryName),
            Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
