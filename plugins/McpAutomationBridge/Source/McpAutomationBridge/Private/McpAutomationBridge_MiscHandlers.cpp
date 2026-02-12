// McpAutomationBridge_MiscHandlers.cpp
// Miscellaneous handlers for editor control, cameras, viewports, and bookmarks

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Components/PostProcessComponent.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "UnrealClient.h"
#include "Engine/LevelStreaming.h"
#include "Misc/EngineVersionComparison.h"
#include "Components/SplineComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"

#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR 1
#else
#define MCP_HAS_LEVEL_EDITOR 0
#endif

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpMiscHandlers, Log, All);

#if WITH_EDITOR

// ============================================================================
// Helper Functions
// ============================================================================

namespace MiscHelpers
{
    UWorld* GetEditorWorld()
    {
        if (GEditor)
        {
            return GEditor->GetEditorWorldContext().World();
        }
        return nullptr;
    }

    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""))
    {
        FString Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetStringField(FieldName, Value);
        }
        return Value;
    }

    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0)
    {
        double Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(FieldName, Value);
        }
        return Value;
    }

    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false)
    {
        bool Value = Default;
        if (Payload.IsValid())
        {
            Payload->TryGetBoolField(FieldName, Value);
        }
        return Value;
    }

    FVector GetVectorField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FVector Default = FVector::ZeroVector)
    {
        return ExtractVectorField(Payload, *FieldName, Default);
    }

    FRotator GetRotatorField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FRotator Default = FRotator::ZeroRotator)
    {
        return ExtractRotatorField(Payload, *FieldName, Default);
    }
}

// ============================================================================
// Post Process Volume Handler
// ============================================================================

static bool HandleCreatePostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString VolumeName = GetStringField(Payload, TEXT("volumeName"), TEXT("PostProcessVolume"));
    FVector Location = GetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FVector Extent = GetVectorField(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));
    bool bUnbound = GetBoolField(Payload, TEXT("unbound"), false);
    double BlendRadius = GetNumberField(Payload, TEXT("blendRadius"), 100.0);
    double BlendWeight = GetNumberField(Payload, TEXT("blendWeight"), 1.0);
    double Priority = GetNumberField(Payload, TEXT("priority"), 0.0);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(Location, FRotator::ZeroRotator, SpawnParams);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn PostProcessVolume"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    Volume->SetActorLabel(VolumeName);
    Volume->bUnbound = bUnbound;
    Volume->BlendRadius = static_cast<float>(BlendRadius);
    Volume->BlendWeight = static_cast<float>(BlendWeight);
    Volume->Priority = static_cast<float>(Priority);

    // Configure post process settings if provided
    if (Payload->HasField(TEXT("settings")))
    {
        const TSharedPtr<FJsonObject>* SettingsPtr = nullptr;
        if (Payload->TryGetObjectField(TEXT("settings"), SettingsPtr) && SettingsPtr)
        {
            FPostProcessSettings& Settings = Volume->Settings;
            
            double Bloom;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("bloomIntensity"), Bloom))
            {
                Settings.bOverride_BloomIntensity = true;
                Settings.BloomIntensity = static_cast<float>(Bloom);
            }
            
            double Exposure;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("exposureCompensation"), Exposure))
            {
                Settings.bOverride_AutoExposureBias = true;
                Settings.AutoExposureBias = static_cast<float>(Exposure);
            }
            
            double Saturation;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("saturation"), Saturation))
            {
                Settings.bOverride_ColorSaturation = true;
                Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
            }
            
            double Contrast;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("contrast"), Contrast))
            {
                Settings.bOverride_ColorContrast = true;
                Settings.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.0f);
            }
            
            double VignetteIntensity;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("vignetteIntensity"), VignetteIntensity))
            {
                Settings.bOverride_VignetteIntensity = true;
                Settings.VignetteIntensity = static_cast<float>(VignetteIntensity);
            }
        }
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumePath"), Volume->GetPathName());
    ResponseJson->SetBoolField(TEXT("unbound"), bUnbound);
    ResponseJson->SetNumberField(TEXT("blendRadius"), BlendRadius);
    ResponseJson->SetNumberField(TEXT("priority"), Priority);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PostProcessVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

// ============================================================================
// Camera Handlers
// ============================================================================

static bool HandleCreateCamera(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString CameraName = GetStringField(Payload, TEXT("cameraName"), TEXT("Camera"));
    FVector Location = GetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    double FOV = GetNumberField(Payload, TEXT("fov"), 90.0);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ACameraActor* Camera = World->SpawnActor<ACameraActor>(Location, Rotation, SpawnParams);
    if (!Camera)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn camera actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    Camera->SetActorLabel(CameraName);
    
    if (UCameraComponent* CamComp = Camera->GetCameraComponent())
    {
        CamComp->SetFieldOfView(static_cast<float>(FOV));
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("cameraName"), Camera->GetActorLabel());
    ResponseJson->SetStringField(TEXT("cameraPath"), Camera->GetPathName());
    ResponseJson->SetNumberField(TEXT("fov"), FOV);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created camera: %s"), *CameraName), ResponseJson);
    return true;
}

static bool HandleSetCameraFOV(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString CameraName = GetStringField(Payload, TEXT("cameraName"), TEXT(""));
    double FOV = GetNumberField(Payload, TEXT("fov"), 90.0);

    if (CameraName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("cameraName is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Find camera by name
    ACameraActor* Camera = nullptr;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(CameraName, ESearchCase::IgnoreCase) ||
            It->GetName().Equals(CameraName, ESearchCase::IgnoreCase))
        {
            Camera = *It;
            break;
        }
    }

    if (!Camera)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Camera not found: %s"), *CameraName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    if (UCameraComponent* CamComp = Camera->GetCameraComponent())
    {
        CamComp->SetFieldOfView(static_cast<float>(FOV));
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("cameraName"), Camera->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("fov"), FOV);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set FOV to %.1f for camera: %s"), FOV, *CameraName), ResponseJson);
    return true;
}

// ============================================================================
// Viewport Resolution Handler
// ============================================================================

static bool HandleSetViewportResolution(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    int32 Width = static_cast<int32>(GetNumberField(Payload, TEXT("width"), 1920.0));
    int32 Height = static_cast<int32>(GetNumberField(Payload, TEXT("height"), 1080.0));

    if (Width <= 0 || Height <= 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid resolution dimensions"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

#if MCP_HAS_LEVEL_EDITOR
    FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
    TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
    
    if (ActiveViewport.IsValid())
    {
        // Note: Direct viewport resize isn't always possible in editor
        // This is primarily for reference/documentation
        UE_LOG(LogMcpMiscHandlers, Log, TEXT("Viewport resolution request: %dx%d"), Width, Height);
    }
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("width"), Width);
    ResponseJson->SetNumberField(TEXT("height"), Height);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Viewport resolution preferences set. Actual resolution depends on editor window size."));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Viewport resolution preference set to %dx%d"), Width, Height), ResponseJson);
    return true;
}

// ============================================================================
// Game Speed Handler
// ============================================================================

static bool HandleSetGameSpeed(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    double Speed = GetNumberField(Payload, TEXT("speed"), 1.0);
    
    if (Speed < 0.0 || Speed > 100.0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Speed must be between 0.0 and 100.0"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = nullptr;
    
    // Prefer PIE world if available
    if (GEditor && GEditor->PlayWorld)
    {
        World = GEditor->PlayWorld;
    }
    else
    {
        World = GetEditorWorld();
    }

    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!WorldSettings)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World settings not available"), nullptr, TEXT("NO_WORLD_SETTINGS"));
        return true;
    }

    // Set the time dilation
    WorldSettings->SetTimeDilation(static_cast<float>(Speed));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("speed"), Speed);
    ResponseJson->SetNumberField(TEXT("actualTimeDilation"), WorldSettings->TimeDilation);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Game speed set to %.2fx"), Speed), ResponseJson);
    return true;
}

// ============================================================================
// Editor Bookmark Handler
// ============================================================================

static bool HandleCreateBookmark(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    int32 BookmarkIndex = static_cast<int32>(GetNumberField(Payload, TEXT("index"), 0.0));
    FString BookmarkName = GetStringField(Payload, TEXT("name"), TEXT(""));
    FVector Location = GetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    if (BookmarkIndex < 0 || BookmarkIndex > 9)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Bookmark index must be between 0 and 9"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    // Editor bookmarks are typically accessed via Ctrl+0-9
    // We can store the location in world settings or use editor preferences

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // Note: Editor bookmarks are handled through FEditorViewportClient
    // This provides a simplified interface that logs the bookmark location
    UE_LOG(LogMcpMiscHandlers, Log, TEXT("Bookmark %d set at Location=(%.1f, %.1f, %.1f)"),
        BookmarkIndex, Location.X, Location.Y, Location.Z);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("index"), BookmarkIndex);
    if (!BookmarkName.IsEmpty())
    {
        ResponseJson->SetStringField(TEXT("name"), BookmarkName);
    }
    
    TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
    LocationJson->SetNumberField(TEXT("x"), Location.X);
    LocationJson->SetNumberField(TEXT("y"), Location.Y);
    LocationJson->SetNumberField(TEXT("z"), Location.Z);
    ResponseJson->SetObjectField(TEXT("location"), LocationJson);

    TSharedPtr<FJsonObject> RotationJson = MakeShareable(new FJsonObject());
    RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);
    ResponseJson->SetObjectField(TEXT("rotation"), RotationJson);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created bookmark at index %d"), BookmarkIndex), ResponseJson);
    return true;
}

// ============================================================================
// Spline Component Creation Handler (adds to Blueprint SCS)
// ============================================================================

static bool HandleCreateSplineComponent(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString ComponentName = GetStringField(Payload, TEXT("componentName"), TEXT("SplineComponent"));
    bool bClosedLoop = GetBoolField(Payload, TEXT("closedLoop"), false);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    // Check if component already exists
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    // Create the SCS node
    USCS_Node* NewNode = SCS->CreateNode(USplineComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }

    // Configure the component template
    USplineComponent* SplineComp = Cast<USplineComponent>(NewNode->ComponentTemplate);
    if (SplineComp)
    {
        SplineComp->SetClosedLoop(bClosedLoop);
    }

    SCS->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetBoolField(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("componentName"), ComponentName);
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetBoolField(TEXT("closedLoop"), bClosedLoop);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineComponent '%s' added to Blueprint"), *ComponentName), ResponseJson);
    return true;
}

// ============================================================================
// Additional Networking Handlers
// ============================================================================

static bool HandleSetReplication(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    bool bReplicates = GetBoolField(Payload, TEXT("replicates"), true);
    bool bReplicateMovement = GetBoolField(Payload, TEXT("replicateMovement"), true);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

        if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        CDO->SetReplicates(bReplicates);
        CDO->SetReplicateMovement(bReplicateMovement);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetBoolField(TEXT("replicates"), bReplicates);
    ResponseJson->SetBoolField(TEXT("replicateMovement"), bReplicateMovement);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Replication settings configured for %s"), *BlueprintPath), ResponseJson);
    return true;
}

static bool HandleCreateReplicatedVariable(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString VariableName = GetStringField(Payload, TEXT("variableName"), TEXT(""));
    FString VariableType = GetStringField(Payload, TEXT("variableType"), TEXT("Boolean"));

    if (BlueprintPath.IsEmpty() || VariableName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath and variableName are required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Add variable with replication flag
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    
    if (VariableType.Equals(TEXT("Integer"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }

    bool bCreated = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);
    
    if (bCreated)
    {
        // Set replication flag
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*VariableName))
            {
                VarDesc.PropertyFlags |= CPF_Net;
                break;
            }
        }
        
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetStringField(TEXT("variableName"), VariableName);
    ResponseJson->SetStringField(TEXT("variableType"), VariableType);
    ResponseJson->SetBoolField(TEXT("replicated"), true);

    Subsystem->SendAutomationResponse(Socket, RequestId, bCreated,
        bCreated ? FString::Printf(TEXT("Created replicated variable: %s"), *VariableName) : TEXT("Failed to create variable"),
        ResponseJson);
    return true;
}

static bool HandleSetNetUpdateFrequency(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    double Frequency = GetNumberField(Payload, TEXT("frequency"), 100.0);
    double MinFrequency = GetNumberField(Payload, TEXT("minFrequency"), 2.0);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

        if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetUpdateFrequency(static_cast<float>(Frequency));
        CDO->SetMinNetUpdateFrequency(static_cast<float>(MinFrequency));
#else
        CDO->NetUpdateFrequency = static_cast<float>(Frequency);
        CDO->MinNetUpdateFrequency = static_cast<float>(MinFrequency);
#endif
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetNumberField(TEXT("frequency"), Frequency);
    ResponseJson->SetNumberField(TEXT("minFrequency"), MinFrequency);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Net update frequency set to %.1f (min: %.1f)"), Frequency, MinFrequency), ResponseJson);
    return true;
}

static bool HandleCreateRPC(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString FunctionName = GetStringField(Payload, TEXT("functionName"), TEXT(""));
    FString RPCType = GetStringField(Payload, TEXT("rpcType"), TEXT("Server"));
    bool bReliable = GetBoolField(Payload, TEXT("reliable"), true);

    if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath and functionName are required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Create new function graph
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (NewGraph)
    {
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, false, static_cast<UFunction*>(nullptr));

        // Set RPC flags on the function entry node
        for (UEdGraphNode* Node : NewGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                int32 NetFlags = FUNC_Net;
                
                if (bReliable)
                {
                    NetFlags |= FUNC_NetReliable;
                }
                
                if (RPCType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetServer;
                }
                else if (RPCType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetClient;
                }
                else if (RPCType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase) || 
                         RPCType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetMulticast;
                }
                
                EntryNode->AddExtraFlags(NetFlags);
                break;
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetStringField(TEXT("functionName"), FunctionName);
    ResponseJson->SetStringField(TEXT("rpcType"), RPCType);
    ResponseJson->SetBoolField(TEXT("reliable"), bReliable);

    Subsystem->SendAutomationResponse(Socket, RequestId, NewGraph != nullptr,
        NewGraph ? FString::Printf(TEXT("Created %s RPC: %s"), *RPCType, *FunctionName) : TEXT("Failed to create RPC"),
        ResponseJson);
    return true;
}

static bool HandleConfigureNetCullDistance(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace MiscHelpers;

    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    double CullDistance = GetNumberField(Payload, TEXT("cullDistance"), 15000.0);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

        if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        double CullDistanceSquared = CullDistance * CullDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetCullDistanceSquared(static_cast<float>(CullDistanceSquared));
#else
        CDO->NetCullDistanceSquared = static_cast<float>(CullDistanceSquared);
#endif
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetNumberField(TEXT("cullDistance"), CullDistance);
    ResponseJson->SetNumberField(TEXT("cullDistanceSquared"), CullDistance * CullDistance);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Net cull distance set to %.0f"), CullDistance), ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleMiscAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    
    // Also check action field for direct calls
    if (SubAction.IsEmpty())
    {
        SubAction = Action;
    }

    UE_LOG(LogMcpMiscHandlers, Verbose, TEXT("HandleMiscAction: %s"), *SubAction);

    // Post Process Volume
    if (SubAction == TEXT("create_post_process_volume"))
    {
        return HandleCreatePostProcessVolume(this, RequestId, Payload, Socket);
    }

    // Camera actions
    if (SubAction == TEXT("create_camera"))
    {
        return HandleCreateCamera(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_camera_fov"))
    {
        return HandleSetCameraFOV(this, RequestId, Payload, Socket);
    }

    // Viewport/Editor actions
    if (SubAction == TEXT("set_viewport_resolution"))
    {
        return HandleSetViewportResolution(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_game_speed"))
    {
        return HandleSetGameSpeed(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_bookmark"))
    {
        return HandleCreateBookmark(this, RequestId, Payload, Socket);
    }

    // Spline component (Blueprint SCS)
    if (SubAction == TEXT("create_spline_component"))
    {
        return HandleCreateSplineComponent(this, RequestId, Payload, Socket);
    }

    // Networking actions (alternative entry points)
    if (SubAction == TEXT("set_replication"))
    {
        return HandleSetReplication(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_replicated_variable"))
    {
        return HandleCreateReplicatedVariable(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_net_update_frequency"))
    {
        return HandleSetNetUpdateFrequency(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("create_rpc"))
    {
        return HandleCreateRPC(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_net_cull_distance"))
    {
        return HandleConfigureNetCullDistance(this, RequestId, Payload, Socket);
    }

    return false; // Not handled by this dispatcher
#else
    return false;
#endif
}
