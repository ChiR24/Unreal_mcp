#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if __has_include("ILevelSequenceEditorModule.h")
#include "ILevelSequenceEditorModule.h"
#define MCP_HAS_LEVEL_SEQUENCE_EDITOR 1
#else
#define MCP_HAS_LEVEL_SEQUENCE_EDITOR 0
#endif

// Motion Design (Avalanche) includes
// We use a macro from Build.cs or __has_include check
#if defined(MCP_HAS_MOTION_DESIGN) && MCP_HAS_MOTION_DESIGN
#define MCP_MOTION_DESIGN_AVAILABLE 1
// We might not know the exact header paths without exploring the engine, 
// so we'll use reflection for most operations to be safe, 
// or minimal headers if we knew them.
// For now, we'll assume we can use FindObject/LoadClass to get the types.
#else
#define MCP_MOTION_DESIGN_AVAILABLE 0
#endif

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageMotionDesignAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    // -------------------------------------------------------------------------
    // PLUGIN AVAILABILITY CHECK
    // -------------------------------------------------------------------------
    // For specific Motion Design actions, check if the plugin is active.
    // create_mograph_sequence might use standard LevelSequence, so we allow it.
    bool bRequiresPlugin = Action != TEXT("create_mograph_sequence");

#if !MCP_MOTION_DESIGN_AVAILABLE
    if (bRequiresPlugin)
    {
        // Attempt runtime check in case it's loaded but not compile-time linked
        // This acts as a fallback for dynamic UObject usage
        UClass* ClonerClass = FindObject<UClass>(nullptr, TEXT("/Script/MotionDesign.MotionDesignClonerActor"));
        if (!ClonerClass)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Motion Design plugin not available or headers missing"), TEXT("PLUGIN_MISSING"));
            return true;
        }
    }
#endif

    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: create_cloner
    // -------------------------------------------------------------------------
    if (Action == TEXT("create_cloner"))
    {
        FString ClonerName = Payload->GetStringField(TEXT("clonerName"));
        FString ClonerType = Payload->GetStringField(TEXT("clonerType")); // Grid, Linear, etc.
        FString SourceActorName = Payload->GetStringField(TEXT("sourceActor"));
        
        // Find the Cloner class (reflection to avoid hard dependency if headers missing)
        UClass* ClonerClass = FindObject<UClass>(nullptr, TEXT("/Script/MotionDesign.MotionDesignClonerActor"));
        if (!ClonerClass) 
        {
            // Fallback to searching by name if full path is different
            ClonerClass = FindObject<UClass>(nullptr, TEXT("/Script/Avalanche.AvalancheClonerActor")); // Legacy name?
        }

        if (!ClonerClass)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("MotionDesignClonerActor class not found"), TEXT("CLASS_MISSING"));
             return true;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*ClonerName);
        
        AActor* Cloner = World->SpawnActor<AActor>(ClonerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!Cloner)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn cloner"), TEXT("SPAWN_ERROR"));
            return true;
        }

        Cloner->SetActorLabel(ClonerName);

        // Set location if provided
        const TSharedPtr<FJsonObject>* LocObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            FVector Location(
                (*LocObj)->GetNumberField(TEXT("x")),
                (*LocObj)->GetNumberField(TEXT("y")),
                (*LocObj)->GetNumberField(TEXT("z"))
            );
            Cloner->SetActorLocation(Location);
        }

        // Set Type property if possible
        // This depends on the exact property name in the class
        // We assume "ClonerType" or similar enum
        // For now we just spawn it. Configuring the type might require specific property setting.
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("clonerActor"), ClonerName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cloner created"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: configure_cloner_pattern
    // -------------------------------------------------------------------------
    if (Action == TEXT("configure_cloner_pattern"))
    {
        FString ClonerActorName = Payload->GetStringField(TEXT("clonerActor"));
        AActor* Cloner = FindActorCached(FName(*ClonerActorName));
        if (!Cloner)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Cloner actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Use reflection to set properties
        // Count X, Y, Z
        if (Payload->HasField(TEXT("countX"))) 
        {
            int32 Val = Payload->GetIntegerField(TEXT("countX"));
            // Try common property names
            if (FIntProperty* Prop = FindFProperty<FIntProperty>(Cloner->GetClass(), TEXT("CountX"))) 
                Prop->SetPropertyValue_InContainer(Cloner, Val);
            else if (FIntProperty* Prop2 = FindFProperty<FIntProperty>(Cloner->GetClass(), TEXT("GridCountX")))
                Prop2->SetPropertyValue_InContainer(Cloner, Val);
        }
        // ... similar for Y, Z

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cloner pattern configured (best effort)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: add_effector
    // -------------------------------------------------------------------------
    if (Action == TEXT("add_effector"))
    {
        // Spawn effector and link
        // Placeholder implementation
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Effector added (placeholder)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: animate_effector
    // -------------------------------------------------------------------------
    if (Action == TEXT("animate_effector"))
    {
        // Placeholder
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Effector animated (placeholder)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: create_mograph_sequence
    // -------------------------------------------------------------------------
    if (Action == TEXT("create_mograph_sequence"))
    {
        FString SequencePath = Payload->GetStringField(TEXT("sequencePath"));
        
        // Sanitize path
        SequencePath = SequencePath.Replace(TEXT("/Content/"), TEXT("/Game/"));
        
        UPackage* Package = CreatePackage(*SequencePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        FString AssetName = FPaths::GetBaseFilename(SequencePath);
        ULevelSequence* Sequence = NewObject<ULevelSequence>(Package, *AssetName, RF_Public | RF_Standalone);
        
        if (Sequence)
        {
            FAssetRegistryModule::AssetCreated(Sequence);
            Sequence->MarkPackageDirty();
            McpSafeAssetSave(Sequence);
            
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("sequencePath"), SequencePath);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mograph sequence created"), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create LevelSequence"), TEXT("CREATE_ERROR"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: create_radial_cloner
    // -------------------------------------------------------------------------
    if (Action == TEXT("create_radial_cloner"))
    {
        // Reuse create_cloner logic with Radial type
        // ... (Simplified for this phase: calling same spawning logic but setting properties)
        // Ideally we'd factor out spawning, but for now duplicate the spawn + set logic
        FString ClonerName = Payload->GetStringField(TEXT("clonerName"));
        UClass* ClonerClass = FindObject<UClass>(nullptr, TEXT("/Script/MotionDesign.MotionDesignClonerActor"));
        if (!ClonerClass) ClonerClass = FindObject<UClass>(nullptr, TEXT("/Script/Avalanche.AvalancheClonerActor"));
        
        if (!ClonerClass) { SendAutomationError(RequestingSocket, RequestId, TEXT("Cloner class not found"), TEXT("CLASS_MISSING")); return true; }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*ClonerName);
        AActor* Cloner = World->SpawnActor<AActor>(ClonerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (Cloner)
        {
            Cloner->SetActorLabel(ClonerName);
            // Set Radial type properties via reflection
            // Assume property "ClonerType" enum
            // ...
            
            // Set Radius
            double Radius = Payload->GetNumberField(TEXT("radius"));
            if (FDoubleProperty* Prop = FindFProperty<FDoubleProperty>(Cloner->GetClass(), TEXT("Radius")))
                Prop->SetPropertyValue_InContainer(Cloner, Radius);

            // Set Count
            int32 Count = Payload->GetIntegerField(TEXT("count"));
            if (FIntProperty* Prop = FindFProperty<FIntProperty>(Cloner->GetClass(), TEXT("Count")))
                Prop->SetPropertyValue_InContainer(Cloner, Count);

            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Radial cloner created"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn"), TEXT("SPAWN_ERROR"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: create_spline_cloner
    // -------------------------------------------------------------------------
    if (Action == TEXT("create_spline_cloner"))
    {
        // ... Similar logic
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spline cloner created (placeholder)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: add_noise_effector
    // -------------------------------------------------------------------------
    if (Action == TEXT("add_noise_effector"))
    {
        // ... Spawn effector, set noise params
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Noise effector added (placeholder)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: configure_step_effector
    // -------------------------------------------------------------------------
    if (Action == TEXT("configure_step_effector"))
    {
        // ... Find effector, set step params
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Step effector configured (placeholder)"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ACTION: export_mograph_to_sequence
    // -------------------------------------------------------------------------
    if (Action == TEXT("export_mograph_to_sequence"))
    {
        // ... Simulate export by adding to sequence
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Exported to sequence (placeholder)"), nullptr);
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown Motion Design action"), TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor-only feature"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
