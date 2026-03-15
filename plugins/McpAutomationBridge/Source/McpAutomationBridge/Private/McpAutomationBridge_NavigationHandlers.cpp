// =============================================================================
// McpAutomationBridge_NavigationHandlers.cpp
// =============================================================================
// Navigation System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: NavMesh Configuration
//   - HandleConfigureNavMeshSettings    : Configure NavMesh tile size, cell size/height
//   - HandleSetNavAgentProperties       : Set agent radius, height, max slope, step height
//   - HandleRebuildNavigation           : Trigger full navigation rebuild
//
// Section 2: Nav Modifiers
//   - HandleCreateNavModifierComponent  : Add NavModifierComponent to Blueprint
//   - HandleSetNavAreaClass             : Set nav area class on NavModifierComponent
//   - HandleConfigureNavAreaCost        : Configure nav area default cost
//
// Section 3: Nav Links
//   - HandleCreateNavLinkProxy          : Create NavLinkProxy actor with point links
//   - HandleConfigureNavLink            : Configure nav link start/end points
//   - HandleSetNavLinkType              : Set link type (simple/smart)
//   - HandleCreateSmartLink             : Create NavLinkProxy with smart link enabled
//   - HandleConfigureSmartLinkBehavior  : Configure smart link behavior
//
// Section 4: Utility Handlers
//   - HandleGetNavigationInfo           : Get navigation system status and settings
//   - HandleManageNavigationAction      : Main dispatcher for navigation actions
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// configure_nav_mesh_settings:
//   Payload: { "blueprintPath"?: string, "tileSizeUU"?: number, "cellSize"?: number,
//              "cellHeight"?: number, "minRegionArea"?: number, "mergeRegionSize"?: number,
//              "maxSimplificationError"?: number, "agentStepHeight"?: number }
//   Response: { "success": bool, "navMeshName": string, "tileSizeUU": number, "modified": bool }
//
// set_nav_agent_properties:
//   Payload: { "blueprintPath"?: string, "agentRadius"?: number, "agentHeight"?: number,
//              "agentMaxSlope"?: number, "agentStepHeight"?: number }
//   Response: { "success": bool, "agentRadius": number, "agentHeight": number, "agentMaxSlope": number }
//
// create_nav_link_proxy:
//   Payload: { "actorName"?: string, "location": {x,y,z}, "rotation"?: {pitch,yaw,roll},
//              "startPoint": {x,y,z}, "endPoint": {x,y,z}, "direction"?: string }
//   Response: { "success": bool, "actorName": string, "actorPath": string }
//
// get_navigation_info:
//   Payload: { "blueprintPath"?: string }
//   Response: { "success": bool, "navMeshInfo": { agentRadius, agentHeight, cellSize, ... } }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.1: Uses deprecated direct NavMesh properties (CellSize, CellHeight, AgentMaxStepHeight)
// UE 5.2: Uses NavMeshResolutionParams for CellSize/CellHeight, direct property for AgentMaxStepHeight
// UE 5.3+: Uses NavMeshResolutionParams for all resolution params including AgentMaxStepHeight
//
// REFACTORING NOTES:
// ------------------
// - Use PRAGMA_DISABLE_DEPRECATION_WARNINGS for UE 5.0-5.1 compatibility
// - NavMeshResolutionParams indexed by ENavigationDataResolution::Default
// - NavLinkProxy uses NameMode::Requested for unique name generation
// - Security validation via IsValidNavigationPath() and IsValidActorName()
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"

// =============================================================================
// Navigation System Includes
// =============================================================================
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierComponent.h"
#include "NavLinkCustomComponent.h"
#include "Navigation/NavLinkProxy.h"
#include "AI/NavigationSystemBase.h"

// =============================================================================
// Nav Area Includes
// =============================================================================
#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpNavigationHandlers, Log, All);

// =============================================================================
// Section 0: Helper Functions
// =============================================================================

#if WITH_EDITOR

/**
 * GetJsonStringFieldNav - Extract string field from JSON with default value
 */
static FString GetJsonStringFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FString& Default = TEXT(""))
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    FString Value;
    if (Payload->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

/**
 * GetJsonNumberFieldNav - Extract number field from JSON with default value
 */
static double GetJsonNumberFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, double Default = 0.0)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    double value;
    if (Payload->TryGetNumberField(FieldName, value))
    {
        return value;
    }
    return Default;
}

/**
 * GetJsonBoolFieldNav - Extract bool field from JSON with default value
 */
static bool GetJsonBoolFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, bool Default = false)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    bool value;
    if (Payload->TryGetBoolField(FieldName, value))
    {
        return value;
    }
    return Default;
}

/**
 * GetJsonVectorFieldNav - Extract FVector from JSON object field
 */
static FVector GetJsonVectorFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FVector& Default = FVector::ZeroVector)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonObject>* VecObj;
    if (Payload->TryGetObjectField(FieldName, VecObj) && VecObj->IsValid())
    {
        return FVector(
            GetJsonNumberFieldNav(*VecObj, TEXT("x"), Default.X),
            GetJsonNumberFieldNav(*VecObj, TEXT("y"), Default.Y),
            GetJsonNumberFieldNav(*VecObj, TEXT("z"), Default.Z)
        );
    }
    return Default;
}

/**
 * GetJsonRotatorFieldNav - Extract FRotator from JSON object field
 */
static FRotator GetJsonRotatorFieldNav(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FRotator& Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(FieldName, RotObj) && RotObj->IsValid())
    {
        return FRotator(
            GetJsonNumberFieldNav(*RotObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberFieldNav(*RotObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberFieldNav(*RotObj, TEXT("roll"), Default.Roll)
        );
    }
    return Default;
}

/**
 * IsValidActorName - Validate actor name (reject path traversal and separators)
 */
static bool IsValidActorName(const FString& Name)
{
    if (Name.IsEmpty())
    {
        return false;
    }
    // Reject path traversal
    if (Name.Contains(TEXT("..")))
    {
        return false;
    }
    // Reject path separators
    if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")))
    {
        return false;
    }
    // Reject Windows drive letters
    if (Name.Contains(TEXT(":")))
    {
        return false;
    }
    return true;
}

/**
 * IsValidNavigationPath - Validate asset/class path (reject path traversal)
 */
static bool IsValidNavigationPath(const FString& Path)
{
    if (Path.IsEmpty())
    {
        return false;
    }
    // Use the existing validation helper
    return IsValidAssetPath(Path);
}

#endif // WITH_EDITOR

// =============================================================================
// Section 1: NavMesh Configuration Handlers
// =============================================================================

#if WITH_EDITOR

/**
 * HandleConfigureNavMeshSettings
 * -------------------------------
 * Configure NavMesh settings like tile size, cell size/height, and region parameters.
 * 
 * Version Compatibility:
 * - UE 5.0-5.1: Uses deprecated direct properties (CellSize, CellHeight)
 * - UE 5.2+: Uses NavMeshResolutionParams for cell size/height
 * - UE 5.3+: Uses NavMeshResolutionParams for AgentMaxStepHeight
 */
static bool HandleConfigureNavMeshSettings(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Validate optional blueprintPath parameter
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Navigation system not available"), nullptr, TEXT("NO_NAV_SYS"));
        return true;
    }

    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No RecastNavMesh found in level"), nullptr, TEXT("NO_NAVMESH"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Apply settings from payload
    // -------------------------------------------------------------------------
    bool bModified = false;

    // TileSizeUU - available in all UE 5.x versions
    if (Payload->HasField(TEXT("tileSizeUU")))
    {
        NavMesh->TileSizeUU = GetJsonNumberFieldNav(Payload, TEXT("tileSizeUU"), 1000.0f);
        bModified = true;
    }

    // MinRegionArea - available in all UE 5.x versions
    if (Payload->HasField(TEXT("minRegionArea")))
    {
        NavMesh->MinRegionArea = GetJsonNumberFieldNav(Payload, TEXT("minRegionArea"), 0.0f);
        bModified = true;
    }

    // MergeRegionSize - available in all UE 5.x versions
    if (Payload->HasField(TEXT("mergeRegionSize")))
    {
        NavMesh->MergeRegionSize = GetJsonNumberFieldNav(Payload, TEXT("mergeRegionSize"), 400.0f);
        bModified = true;
    }

    // MaxSimplificationError - available in all UE 5.x versions
    if (Payload->HasField(TEXT("maxSimplificationError")))
    {
        NavMesh->MaxSimplificationError = GetJsonNumberFieldNav(Payload, TEXT("maxSimplificationError"), 1.3f);
        bModified = true;
    }

    // -------------------------------------------------------------------------
    // Cell Size/Height - Version-specific handling
    // UE 5.2+: Uses NavMeshResolutionParams array
    // UE 5.0-5.1: Uses deprecated direct properties
    // -------------------------------------------------------------------------
    if (Payload->HasField(TEXT("cellSize")) || Payload->HasField(TEXT("cellHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
        // UE 5.2+: Use NavMeshResolutionParams array
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];

        if (Payload->HasField(TEXT("cellSize")))
        {
            DefaultParams.CellSize = GetJsonNumberFieldNav(Payload, TEXT("cellSize"), 19.0f);
            bModified = true;
        }
        if (Payload->HasField(TEXT("cellHeight")))
        {
            DefaultParams.CellHeight = GetJsonNumberFieldNav(Payload, TEXT("cellHeight"), 10.0f);
            bModified = true;
        }
#else
        // UE 5.0-5.1: Use deprecated direct properties
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        if (Payload->HasField(TEXT("cellSize")))
        {
            NavMesh->CellSize = GetJsonNumberFieldNav(Payload, TEXT("cellSize"), 19.0f);
            bModified = true;
        }
        if (Payload->HasField(TEXT("cellHeight")))
        {
            NavMesh->CellHeight = GetJsonNumberFieldNav(Payload, TEXT("cellHeight"), 10.0f);
            bModified = true;
        }
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
    }

    // -------------------------------------------------------------------------
    // AgentMaxStepHeight - Version-specific handling
    // UE 5.3+: Uses NavMeshResolutionParams
    // UE 5.0-5.2: Uses direct property
    // -------------------------------------------------------------------------
    if (Payload->HasField(TEXT("agentStepHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
        DefaultParams.AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
#else
        // UE 5.0-5.2: Use direct property
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NavMesh->AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        bModified = true;
    }

    if (bModified)
    {
        NavMesh->MarkPackageDirty();
    }

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("navMeshName"), NavMesh->GetName());
    Result->SetNumberField(TEXT("tileSizeUU"), NavMesh->TileSizeUU);
    Result->SetBoolField(TEXT("modified"), bModified);
    Result->SetBoolField(TEXT("navMeshPresent"), true);

    // Add verification data
    Result->SetStringField(TEXT("navMeshPath"), NavMesh->GetPathName());
    Result->SetStringField(TEXT("navMeshClass"), NavMesh->GetClass()->GetName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bModified ? TEXT("NavMesh settings configured") : TEXT("No settings modified"), Result);
    return true;
}

/**
 * HandleSetNavAgentProperties
 * ----------------------------
 * Set navigation agent properties (radius, height, slope, step height).
 */
static bool HandleSetNavAgentProperties(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Validate optional blueprintPath parameter
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Navigation system not available"), nullptr, TEXT("NO_NAV_SYS"));
        return true;
    }

    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No RecastNavMesh found in level"), nullptr, TEXT("NO_NAVMESH"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Set agent properties
    // -------------------------------------------------------------------------
    bool bModified = false;

    // AgentRadius - available in all UE 5.x versions
    if (Payload->HasField(TEXT("agentRadius")))
    {
        NavMesh->AgentRadius = GetJsonNumberFieldNav(Payload, TEXT("agentRadius"), 35.0f);
        bModified = true;
    }

    // AgentHeight - available in all UE 5.x versions
    if (Payload->HasField(TEXT("agentHeight")))
    {
        NavMesh->AgentHeight = GetJsonNumberFieldNav(Payload, TEXT("agentHeight"), 144.0f);
        bModified = true;
    }

    // AgentMaxSlope - available in all UE 5.x versions
    if (Payload->HasField(TEXT("agentMaxSlope")))
    {
        NavMesh->AgentMaxSlope = GetJsonNumberFieldNav(Payload, TEXT("agentMaxSlope"), 44.0f);
        bModified = true;
    }

    // AgentMaxStepHeight - Version-specific handling
    if (Payload->HasField(TEXT("agentStepHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
        DefaultParams.AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NavMesh->AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        bModified = true;
    }

    if (bModified)
    {
        NavMesh->MarkPackageDirty();
    }

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("agentRadius"), NavMesh->AgentRadius);
    Result->SetNumberField(TEXT("agentHeight"), NavMesh->AgentHeight);
    Result->SetNumberField(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
    Result->SetBoolField(TEXT("navMeshPresent"), true);

    // Add verification data
    Result->SetStringField(TEXT("navMeshPath"), NavMesh->GetPathName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Nav agent properties set"), Result);
    return true;
}

/**
 * HandleRebuildNavigation
 * ------------------------
 * Trigger a full navigation rebuild for the current level.
 */
static bool HandleRebuildNavigation(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Validate optional blueprintPath parameter
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Navigation system not available"), nullptr, TEXT("NO_NAV_SYS"));
        return true;
    }

    // Check for RecastNavMesh - warn if missing but still allow rebuild attempt
    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    bool bHasNavMesh = (NavMesh != nullptr);

    // Trigger full navigation rebuild
    NavSys->Build();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("rebuilding"), NavSys->IsNavigationBuildInProgress());
    Result->SetBoolField(TEXT("hasNavMesh"), bHasNavMesh);
    Result->SetBoolField(TEXT("navMeshPresent"), bHasNavMesh);
    Result->SetBoolField(TEXT("bHasNavMesh"), bHasNavMesh);

    // Add verification data
    Result->SetStringField(TEXT("navigationSystemPath"), NavSys->GetPathName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bHasNavMesh ? TEXT("Navigation rebuild initiated") : TEXT("Navigation rebuild initiated (no existing NavMesh - ensure NavMeshBoundsVolume is present)"), Result);
    return true;
}

#endif // WITH_EDITOR

// =============================================================================
// Section 2: Nav Modifier Handlers
// =============================================================================

#if WITH_EDITOR

/**
 * HandleCreateNavModifierComponent
 * ----------------------------------
 * Create a NavModifierComponent on a Blueprint.
 * 
 * Uses SCS (Simple Construction Script) for proper component template ownership.
 */
static bool HandleCreateNavModifierComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringFieldNav(Payload, TEXT("componentName"), TEXT("NavModifier"));
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));
    FVector FailsafeExtent = GetJsonVectorFieldNav(Payload, TEXT("failsafeExtent"), FVector(100, 100, 100));

    // Validate required parameters
    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate blueprint path
    if (!IsValidNavigationPath(BlueprintPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    // Validate area class path if provided
    if (!AreaClassPath.IsEmpty() && !IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    // Load the Blueprint
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    // Check if component already exists
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    // Create the SCS node for NavModifierComponent
    USCS_Node* NewNode = SCS->CreateNode(UNavModifierComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }

    // Configure the component template
    UNavModifierComponent* ModComp = Cast<UNavModifierComponent>(NewNode->ComponentTemplate);
    if (ModComp)
    {
        ModComp->FailsafeExtent = FailsafeExtent;

        // Set area class if provided
        if (!AreaClassPath.IsEmpty())
        {
            UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
            if (AreaClass)
            {
                ModComp->AreaClass = AreaClass;
            }
        }
    }

    // Add node to SCS
    SCS->AddNode(NewNode);

    // Mark Blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    // Save if requested
    if (GetJsonBoolFieldNav(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("existsAfter"), true);

    // Add verification data for blueprint
    McpHandlerUtils::AddVerification(Result, Blueprint);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavModifierComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}

/**
 * HandleSetNavAreaClass
 * ----------------------
 * Set the nav area class on a NavModifierComponent.
 */
static bool HandleSetNavAreaClass(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldNav(Payload, TEXT("componentName"));
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));

    // Validate required parameters
    if (ActorName.IsEmpty() || AreaClassPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and areaClass are required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    // Validate area class path
    if (!IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Find NavModifierComponent
    UNavModifierComponent* ModComp = nullptr;
    TArray<UNavModifierComponent*> Components;
    TargetActor->GetComponents<UNavModifierComponent>(Components);

    if (!ComponentName.IsEmpty())
    {
        // Find component by name
        for (UNavModifierComponent* Comp : Components)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                ModComp = Comp;
                break;
            }
        }

        if (!ModComp)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("NavModifierComponent '%s' not found on actor"), *ComponentName), nullptr, TEXT("NO_COMPONENT"));
            return true;
        }
    }
    else
    {
        // Use first NavModifierComponent if no name specified
        if (Components.Num() > 0)
        {
            ModComp = Components[0];
        }
    }

    if (!ModComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No NavModifierComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    // Load and set area class
    UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    ModComp->SetAreaClass(AreaClass);

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    McpHandlerUtils::AddVerification(Result, TargetActor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Nav area class set"), Result);
    return true;
}

/**
 * HandleConfigureNavAreaCost
 * ----------------------------
 * Configure the default cost for a nav area class.
 */
static bool HandleConfigureNavAreaCost(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("areaClass"));
    double AreaCost = GetJsonNumberFieldNav(Payload, TEXT("areaCost"), 1.0);
    double FixedCost = GetJsonNumberFieldNav(Payload, TEXT("fixedAreaEnteringCost"), 0.0);

    // Validate required parameters
    if (AreaClassPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("areaClass is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate area class path
    if (!IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    // Load area class
    UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    // Get the CDO and modify it
    UNavArea* AreaCDO = AreaClass->GetDefaultObject<UNavArea>();
    if (!AreaCDO)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Could not get NavArea CDO"), nullptr, TEXT("CDO_FAILED"));
        return true;
    }

    AreaCDO->DefaultCost = AreaCost;
    // Note: FixedAreaEnteringCost is protected, can only read via GetFixedAreaEnteringCost()

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    Result->SetNumberField(TEXT("areaCost"), AreaCost);
    Result->SetNumberField(TEXT("fixedAreaEnteringCost"), AreaCDO->GetFixedAreaEnteringCost());
    Result->SetBoolField(TEXT("existsAfter"), true);

    // Warn if user tried to set fixedAreaEnteringCost (it's read-only)
    FString Message = TEXT("Nav area cost configured");
    if (Payload->HasField(TEXT("fixedAreaEnteringCost")))
    {
        Message = TEXT("Nav area cost configured (note: fixedAreaEnteringCost is read-only and was not modified)");
        Result->SetBoolField(TEXT("fixedAreaEnteringCostIgnored"), true);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}

#endif // WITH_EDITOR

// =============================================================================
// Section 3: Nav Link Handlers
// =============================================================================

#if WITH_EDITOR

/**
 * HandleCreateNavLinkProxy
 * -------------------------
 * Create a NavLinkProxy actor with point links.
 */
static bool HandleCreateNavLinkProxy(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"), TEXT("NavLinkProxy"));
    FVector Location = GetJsonVectorFieldNav(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldNav(Payload, TEXT("rotation"));
    FVector StartPoint = GetJsonVectorFieldNav(Payload, TEXT("startPoint"), FVector(-100, 0, 0));
    FVector EndPoint = GetJsonVectorFieldNav(Payload, TEXT("endPoint"), FVector(100, 0, 0));

    // Validate required parameters
    if (!Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("location is required for create_nav_link_proxy"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate that link geometry is provided
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("startPoint and endPoint are required for create_nav_link_proxy to define the navigation link"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Spawn the NavLinkProxy actor
    // Use NameMode::Requested to auto-generate unique name if collision occurs
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavLinkProxy* NavLink = World->SpawnActor<ANavLinkProxy>(Location, Rotation, SpawnParams);
    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn NavLinkProxy"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NavLink->SetActorLabel(*ActorName);

    // Add a point link
    FNavigationLink NewLink;
    NewLink.Left = StartPoint;
    NewLink.Right = EndPoint;

    // Parse direction
    FString DirectionStr = GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays"));
    if (DirectionStr == TEXT("LeftToRight"))
    {
        NewLink.Direction = ENavLinkDirection::LeftToRight;
    }
    else if (DirectionStr == TEXT("RightToLeft"))
    {
        NewLink.Direction = ENavLinkDirection::RightToLeft;
    }
    else
    {
        NewLink.Direction = ENavLinkDirection::BothWays;
    }

    NavLink->PointLinks.Add(NewLink);

    // Mark level dirty
    World->MarkPackageDirty();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NavLink->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavLinkProxy '%s' created"), *ActorName), Result);
    return true;
}

/**
 * HandleConfigureNavLink
 * -----------------------
 * Configure an existing NavLinkProxy's point links.
 */
static bool HandleConfigureNavLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Find the NavLinkProxy
    ANavLinkProxy* NavLink = nullptr;
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            NavLink = *It;
            break;
        }
    }

    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavLinkProxy not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    bool bModified = false;

    // Update point links if start/end points provided
    if (Payload->HasField(TEXT("startPoint")) || Payload->HasField(TEXT("endPoint")))
    {
        if (NavLink->PointLinks.Num() == 0)
        {
            NavLink->PointLinks.Add(FNavigationLink());
        }

        FNavigationLink& Link = NavLink->PointLinks[0];

        if (Payload->HasField(TEXT("startPoint")))
        {
            Link.Left = GetJsonVectorFieldNav(Payload, TEXT("startPoint"));
            bModified = true;
        }
        if (Payload->HasField(TEXT("endPoint")))
        {
            Link.Right = GetJsonVectorFieldNav(Payload, TEXT("endPoint"));
            bModified = true;
        }
        if (Payload->HasField(TEXT("direction")))
        {
            FString DirectionStr = GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays"));
            if (DirectionStr == TEXT("LeftToRight"))
            {
                Link.Direction = ENavLinkDirection::LeftToRight;
            }
            else if (DirectionStr == TEXT("RightToLeft"))
            {
                Link.Direction = ENavLinkDirection::RightToLeft;
            }
            else
            {
                Link.Direction = ENavLinkDirection::BothWays;
            }
            bModified = true;
        }
        if (Payload->HasField(TEXT("snapRadius")))
        {
            Link.SnapRadius = GetJsonNumberFieldNav(Payload, TEXT("snapRadius"), 30.0f);
            bModified = true;
        }
    }

    if (bModified)
    {
        World->MarkPackageDirty();
    }

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("modified"), bModified);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("NavLink configured"), Result);
    return true;
}

/**
 * HandleSetNavLinkType
 * ---------------------
 * Set the link type (simple or smart) on a NavLinkProxy.
 */
static bool HandleSetNavLinkType(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));
    FString LinkType = GetJsonStringFieldNav(Payload, TEXT("linkType"), TEXT("simple"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Find the NavLinkProxy
    ANavLinkProxy* NavLink = nullptr;
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            NavLink = *It;
            break;
        }
    }

    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavLinkProxy not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Toggle smart link relevancy
    bool bSmartLink = (LinkType == TEXT("smart"));
    NavLink->bSmartLinkIsRelevant = bSmartLink;

    if (bSmartLink)
    {
        // Enable the smart link component
        UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
        if (SmartComp)
        {
            SmartComp->SetEnabled(true);
        }
    }

    World->MarkPackageDirty();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("linkType"), LinkType);
    Result->SetBoolField(TEXT("bSmartLinkIsRelevant"), NavLink->bSmartLinkIsRelevant);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("NavLink type set to %s"), *LinkType), Result);
    return true;
}

/**
 * HandleCreateSmartLink
 * ----------------------
 * Create a NavLinkProxy with smart link enabled.
 */
static bool HandleCreateSmartLink(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"), TEXT("SmartNavLink"));
    FVector Location = GetJsonVectorFieldNav(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldNav(Payload, TEXT("rotation"));
    FVector StartPoint = GetJsonVectorFieldNav(Payload, TEXT("startPoint"), FVector(-100, 0, 0));
    FVector EndPoint = GetJsonVectorFieldNav(Payload, TEXT("endPoint"), FVector(100, 0, 0));

    // Validate required parameters
    if (!Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("location is required for create_smart_link"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate that link geometry is provided
    if (!Payload->HasField(TEXT("startPoint")) || !Payload->HasField(TEXT("endPoint")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("startPoint and endPoint are required for create_smart_link to define the navigation link"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Spawn NavLinkProxy with smart link enabled
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavLinkProxy* NavLink = World->SpawnActor<ANavLinkProxy>(Location, Rotation, SpawnParams);
    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn NavLinkProxy"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NavLink->SetActorLabel(*ActorName);
    NavLink->bSmartLinkIsRelevant = true;

    // Configure the smart link component
    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    if (SmartComp)
    {
        // Parse direction
        FString DirectionStr = GetJsonStringFieldNav(Payload, TEXT("direction"), TEXT("BothWays"));
        ENavLinkDirection::Type Direction = ENavLinkDirection::BothWays;
        if (DirectionStr == TEXT("LeftToRight"))
        {
            Direction = ENavLinkDirection::LeftToRight;
        }
        else if (DirectionStr == TEXT("RightToLeft"))
        {
            Direction = ENavLinkDirection::RightToLeft;
        }

        SmartComp->SetLinkData(StartPoint, EndPoint, Direction);
        SmartComp->SetEnabled(true);
    }

    World->MarkPackageDirty();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NavLink->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NavLink->GetPathName());
    Result->SetBoolField(TEXT("bSmartLinkIsRelevant"), true);
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Smart NavLink '%s' created"), *ActorName), Result);
    return true;
}

/**
 * HandleConfigureSmartLinkBehavior
 * ----------------------------------
 * Configure smart link behavior settings.
 */
static bool HandleConfigureSmartLinkBehavior(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldNav(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate actor name
    if (!IsValidActorName(ActorName))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid actorName: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Find the NavLinkProxy
    ANavLinkProxy* NavLink = nullptr;
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            NavLink = *It;
            break;
        }
    }

    if (!NavLink)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavLinkProxy not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UNavLinkCustomComponent* SmartComp = NavLink->GetSmartLinkComp();
    if (!SmartComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("NavLinkProxy has no smart link component"), nullptr, TEXT("NO_SMART_LINK"));
        return true;
    }

    bool bModified = false;

    // Enable/disable smart link
    if (Payload->HasField(TEXT("linkEnabled")))
    {
        SmartComp->SetEnabled(GetJsonBoolFieldNav(Payload, TEXT("linkEnabled"), true));
        bModified = true;
    }

    // Set enabled area class
    if (Payload->HasField(TEXT("enabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("enabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetEnabledArea(AreaClass);
            bModified = true;
        }
    }

    // Set disabled area class
    if (Payload->HasField(TEXT("disabledAreaClass")))
    {
        FString AreaClassPath = GetJsonStringFieldNav(Payload, TEXT("disabledAreaClass"));
        UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
        if (AreaClass)
        {
            SmartComp->SetDisabledArea(AreaClass);
            bModified = true;
        }
    }

    // Configure broadcast settings
    if (Payload->HasField(TEXT("broadcastRadius")) || Payload->HasField(TEXT("broadcastInterval")))
    {
        float Radius = GetJsonNumberFieldNav(Payload, TEXT("broadcastRadius"), 1000.0f);
        float Interval = GetJsonNumberFieldNav(Payload, TEXT("broadcastInterval"), 0.0f);
        SmartComp->SetBroadcastData(Radius, ECC_Pawn, Interval);
        bModified = true;
    }

    // Configure obstacle
    if (GetJsonBoolFieldNav(Payload, TEXT("bCreateBoxObstacle"), false))
    {
        FString ObstacleAreaPath = GetJsonStringFieldNav(Payload, TEXT("obstacleAreaClass"), TEXT("/Script/NavigationSystem.NavArea_Null"));
        UClass* ObstacleArea = LoadClass<UNavArea>(nullptr, *ObstacleAreaPath);
        FVector Extent = GetJsonVectorFieldNav(Payload, TEXT("obstacleExtent"), FVector(100, 100, 100));
        FVector Offset = GetJsonVectorFieldNav(Payload, TEXT("obstacleOffset"));

        if (ObstacleArea)
        {
            SmartComp->AddNavigationObstacle(ObstacleArea, Extent, Offset);
            bModified = true;
        }
    }

    if (bModified)
    {
        World->MarkPackageDirty();
    }

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("linkEnabled"), SmartComp->IsEnabled());
    Result->SetBoolField(TEXT("modified"), bModified);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NavLink);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Smart link behavior configured"), Result);
    return true;
}

#endif // WITH_EDITOR

// =============================================================================
// Section 4: Utility Handlers
// =============================================================================

#if WITH_EDITOR

/**
 * HandleGetNavigationInfo
 * ------------------------
 * Get navigation system status and settings.
 */
static bool HandleGetNavigationInfo(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // Validate optional blueprintPath parameter
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> NavInfo = McpHandlerUtils::CreateResultObject();

    if (NavSys)
    {
        ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
        if (NavMesh)
        {
            NavInfo->SetNumberField(TEXT("agentRadius"), NavMesh->AgentRadius);
            NavInfo->SetNumberField(TEXT("agentHeight"), NavMesh->AgentHeight);
            NavInfo->SetNumberField(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
            NavInfo->SetNumberField(TEXT("tileSizeUU"), NavMesh->TileSizeUU);

            // Get resolution params - UE 5.2+ uses NavMeshResolutionParams
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
            const FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
            NavInfo->SetNumberField(TEXT("cellSize"), DefaultParams.CellSize);
            NavInfo->SetNumberField(TEXT("cellHeight"), DefaultParams.CellHeight);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
            NavInfo->SetNumberField(TEXT("agentStepHeight"), DefaultParams.AgentMaxStepHeight);
#else
            // UE 5.2: AgentMaxStepHeight is not in NavMeshResolutionParam
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            NavInfo->SetNumberField(TEXT("agentStepHeight"), NavMesh->AgentMaxStepHeight);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
#else
            // UE 5.0-5.1: Use deprecated direct properties
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            NavInfo->SetNumberField(TEXT("cellSize"), NavMesh->CellSize);
            NavInfo->SetNumberField(TEXT("cellHeight"), NavMesh->CellHeight);
            NavInfo->SetNumberField(TEXT("agentStepHeight"), NavMesh->AgentMaxStepHeight);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        }

        NavInfo->SetBoolField(TEXT("isNavigationBuildInProgress"), NavSys->IsNavigationBuildInProgress());
    }

    // Count NavLinkProxies
    int32 NavLinkCount = 0;
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        NavLinkCount++;
    }
    NavInfo->SetNumberField(TEXT("navLinkCount"), NavLinkCount);

    // Count NavMeshBoundsVolumes
    int32 BoundsVolumeCount = 0;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
    {
        BoundsVolumeCount++;
    }
    NavInfo->SetNumberField(TEXT("boundsVolumes"), BoundsVolumeCount);

    Result->SetObjectField(TEXT("navMeshInfo"), NavInfo);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Navigation info retrieved"), Result);
    return true;
}

#endif // WITH_EDITOR

// =============================================================================
// Section 5: Main Dispatcher
// =============================================================================

/**
 * HandleManageNavigationAction
 * -----------------------------
 * Main dispatcher for all navigation-related actions.
 * 
 * Dispatches to appropriate handlers based on subAction field.
 * 
 * Supported subActions:
 *   - configure_nav_mesh_settings
 *   - set_nav_agent_properties
 *   - rebuild_navigation
 *   - create_nav_modifier_component
 *   - set_nav_area_class
 *   - configure_nav_area_cost
 *   - create_nav_link_proxy
 *   - configure_nav_link
 *   - set_nav_link_type
 *   - create_smart_link
 *   - configure_smart_link_behavior
 *   - get_navigation_info
 */
bool UMcpAutomationBridgeSubsystem::HandleManageNavigationAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringFieldNav(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpNavigationHandlers, Verbose, TEXT("HandleManageNavigationAction: SubAction=%s"), *SubAction);

    // =========================================================================
    // NavMesh Configuration
    // =========================================================================
    if (SubAction == TEXT("configure_nav_mesh_settings"))
        return HandleConfigureNavMeshSettings(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_agent_properties"))
        return HandleSetNavAgentProperties(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("rebuild_navigation"))
        return HandleRebuildNavigation(this, RequestId, Payload, Socket);

    // =========================================================================
    // Nav Modifiers
    // =========================================================================
    if (SubAction == TEXT("create_nav_modifier_component"))
        return HandleCreateNavModifierComponent(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_area_class"))
        return HandleSetNavAreaClass(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_nav_area_cost"))
        return HandleConfigureNavAreaCost(this, RequestId, Payload, Socket);

    // =========================================================================
    // Nav Links
    // =========================================================================
    if (SubAction == TEXT("create_nav_link_proxy"))
        return HandleCreateNavLinkProxy(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_nav_link"))
        return HandleConfigureNavLink(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_link_type"))
        return HandleSetNavLinkType(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_smart_link"))
        return HandleCreateSmartLink(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_smart_link_behavior"))
        return HandleConfigureSmartLinkBehavior(this, RequestId, Payload, Socket);

    // =========================================================================
    // Utility
    // =========================================================================
    if (SubAction == TEXT("get_navigation_info"))
        return HandleGetNavigationInfo(this, RequestId, Payload, Socket);

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown navigation subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Navigation operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
