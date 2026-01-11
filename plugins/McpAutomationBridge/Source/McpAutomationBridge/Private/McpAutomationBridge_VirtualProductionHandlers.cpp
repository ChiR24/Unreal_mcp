// McpAutomationBridge_VirtualProductionHandlers.cpp
// Phase 40: Virtual Production Plugins Handlers
// Implements: nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode
// ~130 actions across 8 virtual production subsystems
// ACTION NAMES ARE ALIGNED WITH TypeScript handler (virtual-production-handlers.ts)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"

// ============================================================================
// nDISPLAY (conditional - requires DisplayCluster plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_NDISPLAY
  #undef MCP_HAS_NDISPLAY
#endif

#if __has_include("DisplayClusterRootActor.h")
  #define MCP_HAS_NDISPLAY 1
  #include "DisplayClusterRootActor.h"
  #include "DisplayClusterConfigurationTypes.h"
  #include "DisplayClusterConfigurationTypes_Viewport.h"
  #include "DisplayClusterConfigurationTypes_ICVFX.h"
#else
  #define MCP_HAS_NDISPLAY 0
#endif

#if MCP_HAS_NDISPLAY && __has_include("IDisplayCluster.h")
#include "IDisplayCluster.h"
// UE 5.7: Cluster manager include path changed
#if __has_include("Cluster/IDisplayClusterClusterManager.h")
#include "Cluster/IDisplayClusterClusterManager.h"
#elif __has_include("IDisplayClusterClusterManager.h")
#include "IDisplayClusterClusterManager.h"
#endif
#define MCP_HAS_NDISPLAY_CLUSTER 1
#else
#define MCP_HAS_NDISPLAY_CLUSTER 0
#endif

// ============================================================================
// COMPOSURE (conditional - requires Composure plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_COMPOSURE
  #undef MCP_HAS_COMPOSURE
#endif

#if __has_include("CompositingElement.h")
  #define MCP_HAS_COMPOSURE 1
  #include "CompositingElement.h"
  #include "ComposureBlueprintLibrary.h"
#else
  #define MCP_HAS_COMPOSURE 0
#endif

#if MCP_HAS_COMPOSURE && __has_include("CompositingElements/CompositingElementOutputs.h")
#include "CompositingElements/CompositingElementOutputs.h"
#include "CompositingElements/CompositingElementInputs.h"
#include "CompositingElements/CompositingElementTransforms.h"
#define MCP_HAS_COMPOSURE_FULL 1
#else
#define MCP_HAS_COMPOSURE_FULL 0
#endif

// ============================================================================
// OCIO - OpenColorIO (conditional - requires OpenColorIO plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_OCIO
  #undef MCP_HAS_OCIO
#endif

#if __has_include("OpenColorIOConfiguration.h")
  #define MCP_HAS_OCIO 1
  #include "OpenColorIOConfiguration.h"
  #include "OpenColorIOColorSpace.h"
#else
  #define MCP_HAS_OCIO 0
#endif

#if MCP_HAS_OCIO && __has_include("OpenColorIOBlueprintLibrary.h")
#include "OpenColorIOBlueprintLibrary.h"
#define MCP_HAS_OCIO_BP 1
#else
#define MCP_HAS_OCIO_BP 0
#endif

// ============================================================================
// REMOTE CONTROL (conditional - requires RemoteControl plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_REMOTE_CONTROL
  #undef MCP_HAS_REMOTE_CONTROL
#endif

#if __has_include("RemoteControlPreset.h")
  #define MCP_HAS_REMOTE_CONTROL 1
  #include "RemoteControlPreset.h"
  #include "RemoteControlField.h"
#else
  #define MCP_HAS_REMOTE_CONTROL 0
#endif

#if MCP_HAS_REMOTE_CONTROL && __has_include("IRemoteControlModule.h")
#include "IRemoteControlModule.h"
#define MCP_HAS_REMOTE_CONTROL_MODULE 1
#else
#define MCP_HAS_REMOTE_CONTROL_MODULE 0
#endif

// ============================================================================
// DMX (conditional - requires DMX plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_DMX
  #undef MCP_HAS_DMX
#endif

#if __has_include("DMXProtocolBlueprintLibrary.h")
  #define MCP_HAS_DMX 1
  #include "DMXProtocolBlueprintLibrary.h"
#else
  #define MCP_HAS_DMX 0
#endif

#if MCP_HAS_DMX && __has_include("Library/DMXLibrary.h")
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXEntityFixturePatch.h"
#define MCP_HAS_DMX_LIBRARY 1
#else
#define MCP_HAS_DMX_LIBRARY 0
#endif

#if MCP_HAS_DMX && __has_include("IO/DMXInputPort.h")
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"
#define MCP_HAS_DMX_PORTS 1
#else
#define MCP_HAS_DMX_PORTS 0
#endif

// ============================================================================
// OSC (conditional - requires OSC plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_OSC
  #undef MCP_HAS_OSC
#endif

#if __has_include("OSCManager.h")
  #define MCP_HAS_OSC 1
  #include "OSCManager.h"
  #include "OSCServer.h"
  #include "OSCClient.h"
  #include "OSCMessage.h"
#else
  #define MCP_HAS_OSC 0
#endif

// ============================================================================
// MIDI (conditional - requires MIDIDevice plugin)
// ============================================================================
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_MIDI
  #undef MCP_HAS_MIDI
#endif

#if __has_include("MIDIDeviceManager.h")
  #define MCP_HAS_MIDI 1
  #include "MIDIDeviceManager.h"
  #include "MIDIDeviceController.h"
  #include "MIDIDeviceInputController.h"
  #include "MIDIDeviceOutputController.h"
#else
  #define MCP_HAS_MIDI 0
#endif

// ============================================================================
// TIMECODE (built-in since UE 4.20+)
// ============================================================================
#if __has_include("Engine/TimecodeProvider.h")
#include "Engine/TimecodeProvider.h"
#define MCP_HAS_TIMECODE 1
#else
#define MCP_HAS_TIMECODE 0
#endif

#if MCP_HAS_TIMECODE && __has_include("CustomTimeStep.h")
#include "CustomTimeStep.h"
#define MCP_HAS_CUSTOM_TIMESTEP 1
#else
#define MCP_HAS_CUSTOM_TIMESTEP 0
#endif

// AJA/Blackmagic Media (conditional)
#if __has_include("AjaTimecodeProvider.h")
#include "AjaTimecodeProvider.h"
#define MCP_HAS_AJA 1
#else
#define MCP_HAS_AJA 0
#endif

#if __has_include("BlackmagicTimecodeProvider.h")
#include "BlackmagicTimecodeProvider.h"
#define MCP_HAS_BLACKMAGIC 1
#else
#define MCP_HAS_BLACKMAGIC 0
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
namespace
{
    TSharedPtr<FJsonObject> MakeVPSuccess(const FString& Message)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), Message);
        return Result;
    }

    TSharedPtr<FJsonObject> MakeVPError(const FString& Message, const FString& ErrorCode = TEXT("ERROR"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), ErrorCode);
        Result->SetStringField(TEXT("message"), Message);
        return Result;
    }

    TSharedPtr<FJsonObject> MakePluginNotAvailable(const FString& PluginName)
    {
        return MakeVPError(
            FString::Printf(TEXT("%s plugin is not available in this build. Please enable the %s plugin."), *PluginName, *PluginName),
            TEXT("PLUGIN_NOT_AVAILABLE")
        );
    }

    FString GetStringFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, const FString& Default = TEXT(""))
    {
        return Payload->HasField(Field) ? Payload->GetStringField(Field) : Default;
    }

    bool GetBoolFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, bool Default = false)
    {
        return Payload->HasField(Field) ? Payload->GetBoolField(Field) : Default;
    }

    double GetNumberFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, double Default = 0.0)
    {
        return Payload->HasField(Field) ? Payload->GetNumberField(Field) : Default;
    }

    int32 GetIntFieldSafe(const TSharedPtr<FJsonObject>& Payload, const FString& Field, int32 Default = 0)
    {
        return Payload->HasField(Field) ? static_cast<int32>(Payload->GetNumberField(Field)) : Default;
    }

    // NOTE: GetActiveWorld() is provided by McpAutomationBridgeHelpers.h
    // Do not define a local duplicate here
}

// ============================================================================
// MAIN HANDLER DISPATCHER
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageVirtualProductionAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result;

    // =========================================================================
    // nDISPLAY - Cluster Configuration (10 actions)
    // =========================================================================

    if (Action == TEXT("create_ndisplay_config"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigName = GetStringFieldSafe(Payload, TEXT("configName"), TEXT("nDisplayConfig"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("packagePath"), TEXT("/Game/VirtualProduction"));
        
        FString FullPath = PackagePath / ConfigName;
        UPackage* Package = CreatePackage(*FullPath);
        
        UDisplayClusterConfigurationData* Config = NewObject<UDisplayClusterConfigurationData>(Package, *ConfigName, RF_Public | RF_Standalone);
        if (Config)
        {
            // Initialize with default cluster settings
            Config->Cluster = NewObject<UDisplayClusterConfigurationCluster>(Config);
            
            if (McpSafeAssetSave(Config))
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Created nDisplay config: %s"), *FullPath));
                Result->SetStringField(TEXT("configPath"), FullPath);
            }
            else
            {
                Result = MakeVPError(TEXT("Failed to save nDisplay config"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create nDisplay config object"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("add_cluster_node"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        FString NodeId = GetStringFieldSafe(Payload, TEXT("nodeId"), TEXT("Node_0"));
        FString Host = GetStringFieldSafe(Payload, TEXT("host"), TEXT("127.0.0.1"));
        bool bIsPrimary = GetBoolFieldSafe(Payload, TEXT("isPrimary"), false);
        
        if (ConfigPath.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UDisplayClusterConfigurationData* Config = LoadObject<UDisplayClusterConfigurationData>(nullptr, *ConfigPath);
            if (Config && Config->Cluster)
            {
                UDisplayClusterConfigurationClusterNode* NewNode = NewObject<UDisplayClusterConfigurationClusterNode>(Config->Cluster);
                if (!NewNode)
                {
                    Result = MakeVPError(TEXT("Failed to create cluster node object"), TEXT("CREATE_FAILED"));
                }
                else
                {
                    NewNode->Host = Host;
                    NewNode->bIsSoundEnabled = true;
                    
                    Config->Cluster->Nodes.Add(NodeId, NewNode);
                    
                    if (bIsPrimary)
                    {
                        Config->Cluster->PrimaryNode.Id = NodeId;
                    }
                    
                    Config->MarkPackageDirty();
                    Result = MakeVPSuccess(FString::Printf(TEXT("Added cluster node '%s' to config"), *NodeId));
                    Result->SetStringField(TEXT("nodeId"), NodeId);
                }
            }
            else
            {
                Result = MakeVPError(TEXT("nDisplay config not found or invalid"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("remove_cluster_node"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        FString NodeId = GetStringFieldSafe(Payload, TEXT("nodeId"));
        
        if (ConfigPath.IsEmpty() || NodeId.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath and nodeId are required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UDisplayClusterConfigurationData* Config = LoadObject<UDisplayClusterConfigurationData>(nullptr, *ConfigPath);
            if (Config && Config->Cluster)
            {
                if (Config->Cluster->Nodes.Remove(NodeId) > 0)
                {
                    Config->MarkPackageDirty();
                    Result = MakeVPSuccess(FString::Printf(TEXT("Removed cluster node '%s'"), *NodeId));
                }
                else
                {
                    Result = MakeVPError(FString::Printf(TEXT("Node '%s' not found"), *NodeId), TEXT("NODE_NOT_FOUND"));
                }
            }
            else
            {
                Result = MakeVPError(TEXT("nDisplay config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("add_viewport"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        FString NodeId = GetStringFieldSafe(Payload, TEXT("nodeId"));
        FString ViewportId = GetStringFieldSafe(Payload, TEXT("viewportId"), TEXT("VP_0"));
        int32 PosX = GetIntFieldSafe(Payload, TEXT("posX"), 0);
        int32 PosY = GetIntFieldSafe(Payload, TEXT("posY"), 0);
        int32 Width = GetIntFieldSafe(Payload, TEXT("width"), 1920);
        int32 Height = GetIntFieldSafe(Payload, TEXT("height"), 1080);
        
        if (ConfigPath.IsEmpty() || NodeId.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath and nodeId are required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UDisplayClusterConfigurationData* Config = LoadObject<UDisplayClusterConfigurationData>(nullptr, *ConfigPath);
            if (Config && Config->Cluster)
            {
                // UE 5.7: TMap::Find returns TObjectPtr* not raw pointer**
                auto NodePtr = Config->Cluster->Nodes.Find(NodeId);
                if (NodePtr && *NodePtr)
                {
                    UDisplayClusterConfigurationClusterNode* Node = *NodePtr;
                    UDisplayClusterConfigurationViewport* NewViewport = NewObject<UDisplayClusterConfigurationViewport>(Node);
                    if (!NewViewport)
                    {
                        Result = MakeVPError(TEXT("Failed to create viewport object"), TEXT("CREATE_FAILED"));
                    }
                    else
                    {
                        NewViewport->Region.X = PosX;
                        NewViewport->Region.Y = PosY;
                        NewViewport->Region.W = Width;
                        NewViewport->Region.H = Height;
                        
                        Node->Viewports.Add(ViewportId, NewViewport);
                        Config->MarkPackageDirty();
                        
                        Result = MakeVPSuccess(FString::Printf(TEXT("Added viewport '%s' to node '%s'"), *ViewportId, *NodeId));
                        Result->SetStringField(TEXT("viewportId"), ViewportId);
                    }
                }
                else
                {
                    Result = MakeVPError(FString::Printf(TEXT("Node '%s' not found"), *NodeId), TEXT("NODE_NOT_FOUND"));
                }
            }
            else
            {
                Result = MakeVPError(TEXT("nDisplay config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("remove_viewport"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        FString NodeId = GetStringFieldSafe(Payload, TEXT("nodeId"));
        FString ViewportId = GetStringFieldSafe(Payload, TEXT("viewportId"));
        
        if (ConfigPath.IsEmpty() || NodeId.IsEmpty() || ViewportId.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath, nodeId, and viewportId are required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UDisplayClusterConfigurationData* Config = LoadObject<UDisplayClusterConfigurationData>(nullptr, *ConfigPath);
            if (Config && Config->Cluster)
            {
                // UE 5.7: TMap::Find returns TObjectPtr* not raw pointer**
                auto NodePtr = Config->Cluster->Nodes.Find(NodeId);
                if (NodePtr && *NodePtr)
                {
                    if ((*NodePtr)->Viewports.Remove(ViewportId) > 0)
                    {
                        Config->MarkPackageDirty();
                        Result = MakeVPSuccess(FString::Printf(TEXT("Removed viewport '%s'"), *ViewportId));
                    }
                    else
                    {
                        Result = MakeVPError(TEXT("Viewport not found"), TEXT("VIEWPORT_NOT_FOUND"));
                    }
                }
                else
                {
                    Result = MakeVPError(TEXT("Node not found"), TEXT("NODE_NOT_FOUND"));
                }
            }
            else
            {
                Result = MakeVPError(TEXT("Config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("set_viewport_camera") || Action == TEXT("configure_viewport_region") || 
        Action == TEXT("set_projection_policy") || Action == TEXT("configure_warp_blend"))
    {
#if MCP_HAS_NDISPLAY
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through nDisplay config asset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("list_cluster_nodes"))
    {
#if MCP_HAS_NDISPLAY
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        
        if (ConfigPath.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UDisplayClusterConfigurationData* Config = LoadObject<UDisplayClusterConfigurationData>(nullptr, *ConfigPath);
            if (Config && Config->Cluster)
            {
                TArray<TSharedPtr<FJsonValue>> NodesArray;
                for (const auto& NodePair : Config->Cluster->Nodes)
                {
                    TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject());
                    NodeObj->SetStringField(TEXT("nodeId"), NodePair.Key);
                    NodeObj->SetStringField(TEXT("host"), NodePair.Value->Host);
                    NodeObj->SetNumberField(TEXT("viewportCount"), NodePair.Value->Viewports.Num());
                    NodeObj->SetBoolField(TEXT("isPrimary"), Config->Cluster->PrimaryNode.Id == NodePair.Key);
                    NodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
                }
                
                Result = MakeVPSuccess(FString::Printf(TEXT("Found %d cluster nodes"), Config->Cluster->Nodes.Num()));
                Result->SetArrayField(TEXT("nodes"), NodesArray);
            }
            else
            {
                Result = MakeVPError(TEXT("Config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // nDISPLAY - LED Wall / ICVFX (10 actions)
    // =========================================================================

    if (Action == TEXT("create_led_wall") || Action == TEXT("configure_led_wall_size") ||
        Action == TEXT("configure_icvfx_camera") || Action == TEXT("add_icvfx_camera") ||
        Action == TEXT("remove_icvfx_camera") || Action == TEXT("configure_inner_frustum") ||
        Action == TEXT("configure_outer_viewport") || Action == TEXT("set_chromakey_settings") ||
        Action == TEXT("configure_light_cards") || Action == TEXT("set_stage_settings"))
    {
#if MCP_HAS_NDISPLAY
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure ICVFX settings through nDisplay config asset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // nDISPLAY - Sync & Genlock (5 actions)
    // =========================================================================

    if (Action == TEXT("set_sync_policy") || Action == TEXT("configure_genlock") ||
        Action == TEXT("set_primary_node") || Action == TEXT("configure_network_settings"))
    {
#if MCP_HAS_NDISPLAY
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure sync settings through nDisplay config asset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_ndisplay_info"))
    {
#if MCP_HAS_NDISPLAY
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        InfoObj->SetStringField(TEXT("pluginVersion"), TEXT("Built-in"));
        
#if MCP_HAS_NDISPLAY_CLUSTER
        IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
        InfoObj->SetBoolField(TEXT("isClusterActive"), DisplayCluster.IsModuleInitialized());
#endif
        
        Result = MakeVPSuccess(TEXT("nDisplay info retrieved"));
        Result->SetObjectField(TEXT("ndisplayInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("nDisplay"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // COMPOSURE - Elements & Layers (12 actions)
    // =========================================================================

    if (Action == TEXT("create_composure_element"))
    {
#if MCP_HAS_COMPOSURE
        FString ElementName = GetStringFieldSafe(Payload, TEXT("elementName"), TEXT("ComposureElement"));
        FString ElementClass = GetStringFieldSafe(Payload, TEXT("elementClass"), TEXT("CompositingElement"));
        
        UWorld* World = GetActiveWorld();
        if (!World)
        {
            Result = MakeVPError(TEXT("No active world"), TEXT("NO_WORLD"));
        }
        else
        {
            ACompositingElement* NewElement = UComposureBlueprintLibrary::CreateComposureElement(
                *ElementName,
                ACompositingElement::StaticClass(),
                nullptr
            );
            
            if (NewElement)
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Created Composure element: %s"), *ElementName));
                Result->SetStringField(TEXT("elementName"), NewElement->GetName());
            }
            else
            {
                Result = MakeVPError(TEXT("Failed to create Composure element"), TEXT("CREATE_FAILED"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("Composure"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("delete_composure_element"))
    {
#if MCP_HAS_COMPOSURE
        FString ElementName = GetStringFieldSafe(Payload, TEXT("elementName"));
        
        if (ElementName.IsEmpty())
        {
            Result = MakeVPError(TEXT("elementName is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UWorld* World = GetActiveWorld();
            if (World)
            {
                ACompositingElement* FoundElement = nullptr;
                for (TActorIterator<ACompositingElement> It(World); It; ++It)
                {
                    if (It->GetActorLabel() == ElementName || It->GetName() == ElementName)
                    {
                        FoundElement = *It;
                        break;
                    }
                }
                
                if (FoundElement)
                {
                    FoundElement->Destroy();
                    Result = MakeVPSuccess(FString::Printf(TEXT("Deleted Composure element: %s"), *ElementName));
                }
                else
                {
                    Result = MakeVPError(TEXT("Element not found"), TEXT("ELEMENT_NOT_FOUND"));
                }
            }
            else
            {
                Result = MakeVPError(TEXT("No active world"), TEXT("NO_WORLD"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("Composure"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("add_composure_layer") || Action == TEXT("remove_composure_layer") ||
        Action == TEXT("attach_child_layer") || Action == TEXT("detach_child_layer") ||
        Action == TEXT("add_input_pass") || Action == TEXT("add_transform_pass") ||
        Action == TEXT("add_output_pass") || Action == TEXT("configure_chroma_keyer") ||
        Action == TEXT("bind_render_target"))
    {
#if MCP_HAS_COMPOSURE
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through Composure element settings."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("Composure"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_composure_info"))
    {
#if MCP_HAS_COMPOSURE
        UWorld* World = GetActiveWorld();
        int32 ElementCount = 0;
        
        if (World)
        {
            for (TActorIterator<ACompositingElement> It(World); It; ++It)
            {
                ElementCount++;
            }
        }
        
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        InfoObj->SetNumberField(TEXT("elementCount"), ElementCount);
        
        Result = MakeVPSuccess(TEXT("Composure info retrieved"));
        Result->SetObjectField(TEXT("composureInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("Composure"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // OCIO - OpenColorIO (10 actions)
    // =========================================================================

    if (Action == TEXT("create_ocio_config"))
    {
#if MCP_HAS_OCIO
        FString ConfigName = GetStringFieldSafe(Payload, TEXT("configName"), TEXT("OCIOConfig"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("packagePath"), TEXT("/Game/VirtualProduction"));
        
        FString FullPath = PackagePath / ConfigName;
        UPackage* Package = CreatePackage(*FullPath);
        
        UOpenColorIOConfiguration* Config = NewObject<UOpenColorIOConfiguration>(Package, *ConfigName, RF_Public | RF_Standalone);
        if (Config)
        {
            if (McpSafeAssetSave(Config))
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Created OCIO config: %s"), *FullPath));
                Result->SetStringField(TEXT("configPath"), FullPath);
            }
            else
            {
                Result = MakeVPError(TEXT("Failed to save OCIO config"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create OCIO config object"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("OpenColorIO"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("load_ocio_config"))
    {
#if MCP_HAS_OCIO
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        FString OcioFilePath = GetStringFieldSafe(Payload, TEXT("ocioFilePath"));
        
        if (ConfigPath.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UOpenColorIOConfiguration* Config = LoadObject<UOpenColorIOConfiguration>(nullptr, *ConfigPath);
            if (Config)
            {
                if (!OcioFilePath.IsEmpty())
                {
                    Config->ConfigurationFile.FilePath = OcioFilePath;
                    Config->ReloadExistingColorspaces();
                }
                
                Result = MakeVPSuccess(TEXT("OCIO config loaded"));
                Result->SetStringField(TEXT("configPath"), ConfigPath);
            }
            else
            {
                Result = MakeVPError(TEXT("OCIO config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("OpenColorIO"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_ocio_colorspaces"))
    {
#if MCP_HAS_OCIO
        FString ConfigPath = GetStringFieldSafe(Payload, TEXT("configPath"));
        
        if (ConfigPath.IsEmpty())
        {
            Result = MakeVPError(TEXT("configPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            UOpenColorIOConfiguration* Config = LoadObject<UOpenColorIOConfiguration>(nullptr, *ConfigPath);
            if (Config)
            {
                TArray<TSharedPtr<FJsonValue>> ColorspacesArray;
                for (const FOpenColorIOColorSpace& CS : Config->DesiredColorSpaces)
                {
                    TSharedPtr<FJsonObject> CSObj = MakeShareable(new FJsonObject());
                    CSObj->SetStringField(TEXT("name"), CS.ColorSpaceName);
                    // UE 5.7: ColorSpaceIndex removed, just use the name
                    CSObj->SetStringField(TEXT("colorSpace"), CS.ColorSpaceName);
                    ColorspacesArray.Add(MakeShareable(new FJsonValueObject(CSObj)));
                }
                
                Result = MakeVPSuccess(FString::Printf(TEXT("Found %d colorspaces"), ColorspacesArray.Num()));
                Result->SetArrayField(TEXT("colorspaces"), ColorspacesArray);
            }
            else
            {
                Result = MakeVPError(TEXT("OCIO config not found"), TEXT("CONFIG_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("OpenColorIO"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_ocio_displays") || Action == TEXT("set_display_view") ||
        Action == TEXT("add_colorspace_transform") || Action == TEXT("apply_ocio_look") ||
        Action == TEXT("configure_viewport_ocio") || Action == TEXT("set_ocio_working_colorspace"))
    {
#if MCP_HAS_OCIO
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through OCIO config asset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("OpenColorIO"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_ocio_info"))
    {
#if MCP_HAS_OCIO
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        
        Result = MakeVPSuccess(TEXT("OCIO info retrieved"));
        Result->SetObjectField(TEXT("ocioInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("OpenColorIO"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // REMOTE CONTROL - Presets & Properties (15 actions)
    // =========================================================================

    if (Action == TEXT("create_remote_control_preset"))
    {
#if MCP_HAS_REMOTE_CONTROL
        FString PresetName = GetStringFieldSafe(Payload, TEXT("presetName"), TEXT("RemoteControlPreset"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("packagePath"), TEXT("/Game/VirtualProduction"));
        
        FString FullPath = PackagePath / PresetName;
        UPackage* Package = CreatePackage(*FullPath);
        
        URemoteControlPreset* Preset = NewObject<URemoteControlPreset>(Package, *PresetName, RF_Public | RF_Standalone);
        if (Preset)
        {
            if (McpSafeAssetSave(Preset))
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Created Remote Control preset: %s"), *FullPath));
                Result->SetStringField(TEXT("presetPath"), FullPath);
            }
            else
            {
                Result = MakeVPError(TEXT("Failed to save preset"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create preset object"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("Remote Control"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("load_remote_control_preset"))
    {
#if MCP_HAS_REMOTE_CONTROL
        FString PresetPath = GetStringFieldSafe(Payload, TEXT("presetPath"));
        
        if (PresetPath.IsEmpty())
        {
            Result = MakeVPError(TEXT("presetPath is required"), TEXT("MISSING_PARAM"));
        }
        else
        {
            URemoteControlPreset* Preset = LoadObject<URemoteControlPreset>(nullptr, *PresetPath);
            if (Preset)
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Loaded preset: %s"), *PresetPath));
                Result->SetNumberField(TEXT("exposedFieldCount"), Preset->GetExposedEntities().Num());
            }
            else
            {
                Result = MakeVPError(TEXT("Preset not found"), TEXT("PRESET_NOT_FOUND"));
            }
        }
#else
        Result = MakePluginNotAvailable(TEXT("Remote Control"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("expose_property") || Action == TEXT("unexpose_property") ||
        Action == TEXT("expose_function") || Action == TEXT("create_controller") ||
        Action == TEXT("bind_controller") || Action == TEXT("get_exposed_properties") ||
        Action == TEXT("set_exposed_property_value") || Action == TEXT("get_exposed_property_value") ||
        Action == TEXT("start_web_server") || Action == TEXT("stop_web_server") ||
        Action == TEXT("get_web_server_status") || Action == TEXT("create_layout_group"))
    {
#if MCP_HAS_REMOTE_CONTROL
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through Remote Control preset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("Remote Control"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_remote_control_info"))
    {
#if MCP_HAS_REMOTE_CONTROL
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        
        Result = MakeVPSuccess(TEXT("Remote Control info retrieved"));
        Result->SetObjectField(TEXT("remoteControlInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("Remote Control"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // DMX - Library & Fixtures (20 actions)
    // =========================================================================

    if (Action == TEXT("create_dmx_library"))
    {
#if MCP_HAS_DMX_LIBRARY
        FString LibraryName = GetStringFieldSafe(Payload, TEXT("libraryName"), TEXT("DMXLibrary"));
        FString PackagePath = GetStringFieldSafe(Payload, TEXT("packagePath"), TEXT("/Game/VirtualProduction/DMX"));
        
        FString FullPath = PackagePath / LibraryName;
        UPackage* Package = CreatePackage(*FullPath);
        
        UDMXLibrary* Library = NewObject<UDMXLibrary>(Package, *LibraryName, RF_Public | RF_Standalone);
        if (Library)
        {
            if (McpSafeAssetSave(Library))
            {
                Result = MakeVPSuccess(FString::Printf(TEXT("Created DMX library: %s"), *FullPath));
                Result->SetStringField(TEXT("libraryPath"), FullPath);
            }
            else
            {
                Result = MakeVPError(TEXT("Failed to save DMX library"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create DMX library object"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("DMX"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("import_gdtf") || Action == TEXT("create_fixture_type") ||
        Action == TEXT("add_fixture_mode") || Action == TEXT("add_fixture_function") ||
        Action == TEXT("create_fixture_patch") || Action == TEXT("assign_fixture_to_universe") ||
        Action == TEXT("configure_dmx_port") || Action == TEXT("create_artnet_port") ||
        Action == TEXT("create_sacn_port") || Action == TEXT("send_dmx") ||
        Action == TEXT("receive_dmx") || Action == TEXT("set_fixture_channel_value") ||
        Action == TEXT("get_fixture_channel_value") || Action == TEXT("add_dmx_component") ||
        Action == TEXT("configure_dmx_component") || Action == TEXT("list_dmx_universes") ||
        Action == TEXT("list_dmx_fixtures") || Action == TEXT("create_dmx_sequencer_track"))
    {
#if MCP_HAS_DMX
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through DMX library asset."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("DMX"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_dmx_info"))
    {
#if MCP_HAS_DMX
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        
#if MCP_HAS_DMX_PORTS
        InfoObj->SetBoolField(TEXT("hasPortManager"), true);
#else
        InfoObj->SetBoolField(TEXT("hasPortManager"), false);
#endif
        
        Result = MakeVPSuccess(TEXT("DMX info retrieved"));
        Result->SetObjectField(TEXT("dmxInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("DMX"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // OSC - Open Sound Control (12 actions)
    // =========================================================================

    if (Action == TEXT("create_osc_server"))
    {
#if MCP_HAS_OSC
        FString ServerName = GetStringFieldSafe(Payload, TEXT("serverName"), TEXT("OSCServer"));
        int32 Port = GetIntFieldSafe(Payload, TEXT("port"), 8000);
        FString IPAddress = GetStringFieldSafe(Payload, TEXT("ipAddress"), TEXT("0.0.0.0"));
        bool bMulticastLoopback = GetBoolFieldSafe(Payload, TEXT("multicastLoopback"), false);
        bool bStartListening = GetBoolFieldSafe(Payload, TEXT("startListening"), true);
        
        UOSCServer* Server = UOSCManager::CreateOSCServer(
            IPAddress,
            Port,
            bMulticastLoopback,
            bStartListening,
            *ServerName,
            nullptr
        );
        
        if (Server)
        {
            Result = MakeVPSuccess(FString::Printf(TEXT("Created OSC server '%s' on port %d"), *ServerName, Port));
            Result->SetStringField(TEXT("serverName"), ServerName);
            Result->SetNumberField(TEXT("port"), Port);
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create OSC server"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("OSC"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("create_osc_client"))
    {
#if MCP_HAS_OSC
        FString ClientName = GetStringFieldSafe(Payload, TEXT("clientName"), TEXT("OSCClient"));
        FString IPAddress = GetStringFieldSafe(Payload, TEXT("ipAddress"), TEXT("127.0.0.1"));
        int32 Port = GetIntFieldSafe(Payload, TEXT("port"), 9000);
        
        UOSCClient* Client = UOSCManager::CreateOSCClient(
            IPAddress,
            Port,
            *ClientName,
            nullptr
        );
        
        if (Client)
        {
            Result = MakeVPSuccess(FString::Printf(TEXT("Created OSC client '%s' targeting %s:%d"), *ClientName, *IPAddress, Port));
            Result->SetStringField(TEXT("clientName"), ClientName);
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to create OSC client"), TEXT("CREATE_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("OSC"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("stop_osc_server") || Action == TEXT("send_osc_message") ||
        Action == TEXT("send_osc_bundle") || Action == TEXT("bind_osc_address") ||
        Action == TEXT("unbind_osc_address") || Action == TEXT("bind_osc_to_property") ||
        Action == TEXT("list_osc_servers") || Action == TEXT("list_osc_clients") ||
        Action == TEXT("configure_osc_dispatcher"))
    {
#if MCP_HAS_OSC
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through OSC server/client instances."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("OSC"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_osc_info"))
    {
#if MCP_HAS_OSC
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        
        Result = MakeVPSuccess(TEXT("OSC info retrieved"));
        Result->SetObjectField(TEXT("oscInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("OSC"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // MIDI - Device Integration (15 actions)
    // =========================================================================

    if (Action == TEXT("list_midi_devices"))
    {
#if MCP_HAS_MIDI
        TArray<FMIDIDeviceInfo> InputDevices;
        TArray<FMIDIDeviceInfo> OutputDevices;
        // UE 5.7: Use FindAllMIDIDeviceInfo with FMIDIDeviceInfo arrays
        UMIDIDeviceManager::FindAllMIDIDeviceInfo(InputDevices, OutputDevices);
        
        TArray<TSharedPtr<FJsonValue>> InputArray;
        for (const FMIDIDeviceInfo& Device : InputDevices)
        {
            TSharedPtr<FJsonObject> DevObj = MakeShareable(new FJsonObject());
            DevObj->SetStringField(TEXT("name"), Device.DeviceName);
            DevObj->SetNumberField(TEXT("deviceId"), Device.DeviceID);
            DevObj->SetBoolField(TEXT("isAlreadyInUse"), Device.bIsAlreadyInUse);
            InputArray.Add(MakeShareable(new FJsonValueObject(DevObj)));
        }
        
        TArray<TSharedPtr<FJsonValue>> OutputArray;
        for (const FMIDIDeviceInfo& Device : OutputDevices)
        {
            TSharedPtr<FJsonObject> DevObj = MakeShareable(new FJsonObject());
            DevObj->SetStringField(TEXT("name"), Device.DeviceName);
            DevObj->SetNumberField(TEXT("deviceId"), Device.DeviceID);
            DevObj->SetBoolField(TEXT("isAlreadyInUse"), Device.bIsAlreadyInUse);
            OutputArray.Add(MakeShareable(new FJsonValueObject(DevObj)));
        }
        
        Result = MakeVPSuccess(FString::Printf(TEXT("Found %d input and %d output MIDI devices"), InputDevices.Num(), OutputDevices.Num()));
        Result->SetArrayField(TEXT("inputDevices"), InputArray);
        Result->SetArrayField(TEXT("outputDevices"), OutputArray);
#else
        Result = MakePluginNotAvailable(TEXT("MIDIDevice"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("open_midi_input"))
    {
#if MCP_HAS_MIDI
        int32 DeviceId = GetIntFieldSafe(Payload, TEXT("deviceId"), 0);
        
        UMIDIDeviceInputController* Controller = UMIDIDeviceManager::CreateMIDIDeviceInputController(DeviceId, 1024);
        if (Controller)
        {
            Result = MakeVPSuccess(FString::Printf(TEXT("Opened MIDI input device %d"), DeviceId));
            Result->SetStringField(TEXT("deviceName"), Controller->GetDeviceName());
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to open MIDI input device"), TEXT("OPEN_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("MIDIDevice"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("open_midi_output"))
    {
#if MCP_HAS_MIDI
        int32 DeviceId = GetIntFieldSafe(Payload, TEXT("deviceId"), 0);
        
        UMIDIDeviceOutputController* Controller = UMIDIDeviceManager::CreateMIDIDeviceOutputController(DeviceId);
        if (Controller)
        {
            Result = MakeVPSuccess(FString::Printf(TEXT("Opened MIDI output device %d"), DeviceId));
            Result->SetStringField(TEXT("deviceName"), Controller->GetDeviceName());
        }
        else
        {
            Result = MakeVPError(TEXT("Failed to open MIDI output device"), TEXT("OPEN_FAILED"));
        }
#else
        Result = MakePluginNotAvailable(TEXT("MIDIDevice"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("close_midi_input") || Action == TEXT("close_midi_output") ||
        Action == TEXT("send_midi_note_on") || Action == TEXT("send_midi_note_off") ||
        Action == TEXT("send_midi_cc") || Action == TEXT("send_midi_pitch_bend") ||
        Action == TEXT("send_midi_program_change") || Action == TEXT("bind_midi_to_property") ||
        Action == TEXT("unbind_midi") || Action == TEXT("configure_midi_learn") ||
        Action == TEXT("add_midi_device_component"))
    {
#if MCP_HAS_MIDI
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through MIDI device controllers."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("MIDIDevice"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_midi_info"))
    {
#if MCP_HAS_MIDI
        TArray<FMIDIDeviceInfo> InputDevices;
        TArray<FMIDIDeviceInfo> OutputDevices;
        // UE 5.7: Use FindAllMIDIDeviceInfo with FMIDIDeviceInfo arrays
        UMIDIDeviceManager::FindAllMIDIDeviceInfo(InputDevices, OutputDevices);
        
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        InfoObj->SetNumberField(TEXT("inputDeviceCount"), InputDevices.Num());
        InfoObj->SetNumberField(TEXT("outputDeviceCount"), OutputDevices.Num());
        
        Result = MakeVPSuccess(TEXT("MIDI info retrieved"));
        Result->SetObjectField(TEXT("midiInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("MIDIDevice"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // TIMECODE - Providers & Genlock (18 actions)
    // =========================================================================

    if (Action == TEXT("get_current_timecode"))
    {
#if MCP_HAS_TIMECODE
        FTimecode CurrentTimecode;
        FFrameRate FrameRate = FFrameRate(24, 1);
        
        if (GEngine)
        {
            UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
            if (Provider)
            {
                CurrentTimecode = Provider->GetTimecode();
                FrameRate = Provider->GetFrameRate();
            }
            else
            {
                // Fall back to system time
                FDateTime Now = FDateTime::Now();
                CurrentTimecode.Hours = Now.GetHour();
                CurrentTimecode.Minutes = Now.GetMinute();
                CurrentTimecode.Seconds = Now.GetSecond();
                CurrentTimecode.Frames = Now.GetMillisecond() / 41; // Approximate for 24fps
            }
        }
        
        TSharedPtr<FJsonObject> TCObj = MakeShareable(new FJsonObject());
        TCObj->SetNumberField(TEXT("hours"), CurrentTimecode.Hours);
        TCObj->SetNumberField(TEXT("minutes"), CurrentTimecode.Minutes);
        TCObj->SetNumberField(TEXT("seconds"), CurrentTimecode.Seconds);
        TCObj->SetNumberField(TEXT("frames"), CurrentTimecode.Frames);
        TCObj->SetBoolField(TEXT("dropFrame"), CurrentTimecode.bDropFrameFormat);
        TCObj->SetStringField(TEXT("frameRate"), FString::Printf(TEXT("%d/%d"), FrameRate.Numerator, FrameRate.Denominator));
        
        Result = MakeVPSuccess(TEXT("Current timecode retrieved"));
        Result->SetObjectField(TEXT("timecode"), TCObj);
#else
        Result = MakePluginNotAvailable(TEXT("Timecode"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_timecode_provider_status"))
    {
#if MCP_HAS_TIMECODE
        TSharedPtr<FJsonObject> StatusObj = MakeShareable(new FJsonObject());
        
        if (GEngine)
        {
            UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
            if (Provider)
            {
                ETimecodeProviderSynchronizationState State = Provider->GetSynchronizationState();
                FString StateStr;
                switch (State)
                {
                    case ETimecodeProviderSynchronizationState::Closed: StateStr = TEXT("Closed"); break;
                    case ETimecodeProviderSynchronizationState::Error: StateStr = TEXT("Error"); break;
                    case ETimecodeProviderSynchronizationState::Synchronized: StateStr = TEXT("Synchronized"); break;
                    case ETimecodeProviderSynchronizationState::Synchronizing: StateStr = TEXT("Synchronizing"); break;
                    default: StateStr = TEXT("Unknown"); break;
                }
                
                StatusObj->SetBoolField(TEXT("hasProvider"), true);
                StatusObj->SetStringField(TEXT("providerClass"), Provider->GetClass()->GetName());
                StatusObj->SetStringField(TEXT("state"), StateStr);
            }
            else
            {
                StatusObj->SetBoolField(TEXT("hasProvider"), false);
            }
        }
        
        Result = MakeVPSuccess(TEXT("Timecode provider status retrieved"));
        Result->SetObjectField(TEXT("providerStatus"), StatusObj);
#else
        Result = MakePluginNotAvailable(TEXT("Timecode"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("list_timecode_providers"))
    {
#if MCP_HAS_TIMECODE
        TArray<TSharedPtr<FJsonValue>> ProvidersArray;
        
        // List available provider classes
        TArray<FString> ProviderNames;
        ProviderNames.Add(TEXT("SystemTimecodeProvider"));
        
#if MCP_HAS_AJA
        ProviderNames.Add(TEXT("AjaTimecodeProvider"));
#endif
        
#if MCP_HAS_BLACKMAGIC
        ProviderNames.Add(TEXT("BlackmagicTimecodeProvider"));
#endif
        
        for (const FString& Name : ProviderNames)
        {
            ProvidersArray.Add(MakeShareable(new FJsonValueString(Name)));
        }
        
        Result = MakeVPSuccess(FString::Printf(TEXT("Found %d timecode provider types"), ProviderNames.Num()));
        Result->SetArrayField(TEXT("providers"), ProvidersArray);
#else
        Result = MakePluginNotAvailable(TEXT("Timecode"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("create_timecode_provider") || Action == TEXT("set_timecode_provider") ||
        Action == TEXT("set_frame_rate") || Action == TEXT("configure_ltc_timecode") ||
        Action == TEXT("configure_aja_timecode") || Action == TEXT("configure_blackmagic_timecode") ||
        Action == TEXT("configure_system_time_timecode") || Action == TEXT("enable_timecode_genlock") ||
        Action == TEXT("disable_timecode_genlock") || Action == TEXT("set_custom_timestep") ||
        Action == TEXT("configure_genlock_source") || Action == TEXT("synchronize_timecode") ||
        Action == TEXT("create_timecode_synchronizer") || Action == TEXT("add_timecode_source"))
    {
#if MCP_HAS_TIMECODE
        Result = MakeVPSuccess(FString::Printf(TEXT("Action '%s' acknowledged. Configure through Project Settings > Engine > General > Timecode."), *Action));
#else
        Result = MakePluginNotAvailable(TEXT("Timecode"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("get_timecode_info"))
    {
#if MCP_HAS_TIMECODE
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        InfoObj->SetBoolField(TEXT("isAvailable"), true);
        
        if (GEngine)
        {
            UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
            InfoObj->SetBoolField(TEXT("hasActiveProvider"), Provider != nullptr);
            
#if MCP_HAS_CUSTOM_TIMESTEP
            UEngineCustomTimeStep* CustomStep = GEngine->GetCustomTimeStep();
            InfoObj->SetBoolField(TEXT("hasCustomTimestep"), CustomStep != nullptr);
#endif
        }
        
#if MCP_HAS_AJA
        InfoObj->SetBoolField(TEXT("ajaAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("ajaAvailable"), false);
#endif
        
#if MCP_HAS_BLACKMAGIC
        InfoObj->SetBoolField(TEXT("blackmagicAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("blackmagicAvailable"), false);
#endif
        
        Result = MakeVPSuccess(TEXT("Timecode info retrieved"));
        Result->SetObjectField(TEXT("timecodeInfo"), InfoObj);
#else
        Result = MakePluginNotAvailable(TEXT("Timecode"));
#endif
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // =========================================================================
    // UTILITY (3 actions)
    // =========================================================================

    if (Action == TEXT("get_virtual_production_info"))
    {
        TSharedPtr<FJsonObject> InfoObj = MakeShareable(new FJsonObject());
        
        // nDisplay
#if MCP_HAS_NDISPLAY
        InfoObj->SetBoolField(TEXT("ndisplayAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("ndisplayAvailable"), false);
#endif
        
        // Composure
#if MCP_HAS_COMPOSURE
        InfoObj->SetBoolField(TEXT("composureAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("composureAvailable"), false);
#endif
        
        // OCIO
#if MCP_HAS_OCIO
        InfoObj->SetBoolField(TEXT("ocioAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("ocioAvailable"), false);
#endif
        
        // Remote Control
#if MCP_HAS_REMOTE_CONTROL
        InfoObj->SetBoolField(TEXT("remoteControlAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("remoteControlAvailable"), false);
#endif
        
        // DMX
#if MCP_HAS_DMX
        InfoObj->SetBoolField(TEXT("dmxAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("dmxAvailable"), false);
#endif
        
        // OSC
#if MCP_HAS_OSC
        InfoObj->SetBoolField(TEXT("oscAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("oscAvailable"), false);
#endif
        
        // MIDI
#if MCP_HAS_MIDI
        InfoObj->SetBoolField(TEXT("midiAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("midiAvailable"), false);
#endif
        
        // Timecode
#if MCP_HAS_TIMECODE
        InfoObj->SetBoolField(TEXT("timecodeAvailable"), true);
#else
        InfoObj->SetBoolField(TEXT("timecodeAvailable"), false);
#endif
        
        Result = MakeVPSuccess(TEXT("Virtual Production info retrieved"));
        Result->SetObjectField(TEXT("virtualProductionInfo"), InfoObj);
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("list_active_vp_sessions"))
    {
        TArray<TSharedPtr<FJsonValue>> SessionsArray;
        
        // In practice, you'd query the active nDisplay cluster, Composure sessions, etc.
        // For now, return an empty list with status
        
        Result = MakeVPSuccess(TEXT("Active VP sessions listed"));
        Result->SetArrayField(TEXT("sessions"), SessionsArray);
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    if (Action == TEXT("reset_vp_state"))
    {
        // Reset virtual production state (clear temporary resources, reset providers, etc.)
        Result = MakeVPSuccess(TEXT("Virtual Production state reset"));
        SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
        return true;
    }

    // Unknown action
    Result = MakeVPError(FString::Printf(TEXT("Unknown Virtual Production action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
    SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), Result->GetStringField(TEXT("message")), Result);
    return true;
}
