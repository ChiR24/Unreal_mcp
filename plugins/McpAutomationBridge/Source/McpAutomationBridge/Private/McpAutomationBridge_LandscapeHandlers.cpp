#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/World.h"
#include "EditorAssetLibrary.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleEditLandscape(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Dispatch to specific edit operations implemented below
    if (HandleModifyHeightmap(RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandlePaintLandscapeLayer(RequestId, Action, Payload, RequestingSocket)) return true;
    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateLandscape(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_landscape"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_landscape payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    // Parse inputs
    double X = 0.0, Y = 0.0, Z = 0.0;
    if (!Payload->TryGetNumberField(TEXT("x"), X)) X = 0.0;
    if (!Payload->TryGetNumberField(TEXT("y"), Y)) Y = 0.0;
    if (!Payload->TryGetNumberField(TEXT("z"), Z)) Z = 0.0;
    
    int32 ComponentsX = 8, ComponentsY = 8;
    Payload->TryGetNumberField(TEXT("componentsX"), ComponentsX);
    Payload->TryGetNumberField(TEXT("componentsY"), ComponentsY);
    
    int32 QuadsPerComponent = 63;
    Payload->TryGetNumberField(TEXT("quadsPerComponent"), QuadsPerComponent);
    
    int32 SectionsPerComponent = 1;
    Payload->TryGetNumberField(TEXT("sectionsPerComponent"), SectionsPerComponent);
    
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, X, Y, Z, ComponentsX, ComponentsY, QuadsPerComponent, SectionsPerComponent, MaterialPath, RequestingSocket]()
    {
        if (!GEditor || !GEditor->GetEditorWorldContext().World())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return;
        }
        
        UWorld* World = GEditor->GetEditorWorldContext().World();
        FVector Location(X, Y, Z);
        
        // Validate parameters
        if (ComponentsX < 1 || ComponentsX > 32 || ComponentsY < 1 || ComponentsY > 32)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("ComponentsX/Y must be between 1 and 32"), TEXT("INVALID_ARGUMENT"));
            return;
        }
        
        if (QuadsPerComponent != 7 && QuadsPerComponent != 15 && QuadsPerComponent != 31 && QuadsPerComponent != 63 && QuadsPerComponent != 127 && QuadsPerComponent != 255)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("QuadsPerComponent must be 7, 15, 31, 63, 127, or 255"), TEXT("INVALID_ARGUMENT"));
            return;
        }

        FScopedSlowTask SlowTask(3.0f, FText::FromString(TEXT("Creating landscape...")));
        SlowTask.MakeDialog();

        // Create landscape actor
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Spawning landscape actor")));
        
        ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), Location, FRotator::ZeroRotator);
        if (!Landscape)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn landscape actor"), TEXT("SPAWN_FAILED"));
            return;
        }

        // Configure landscape
        Landscape->SetActorLabel(FString::Printf(TEXT("Landscape_%dx%d"), ComponentsX, ComponentsY));
        Landscape->ComponentSizeQuads = QuadsPerComponent;
        Landscape->SubsectionSizeQuads = QuadsPerComponent / SectionsPerComponent;
        Landscape->NumSubsections = SectionsPerComponent;

        // Load material if specified
        if (!MaterialPath.IsEmpty())
        {
            UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (Mat)
            {
                Landscape->LandscapeMaterial = Mat;
            }
        }

        // Initialize components
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Initializing landscape components")));
        
        // Create flat heightmap data (all at Z level)
        const int32 VertX = ComponentsX * QuadsPerComponent + 1;
        const int32 VertY = ComponentsY * QuadsPerComponent + 1;
        
        TArray<uint16> HeightArray;
        HeightArray.SetNumUninitialized(VertX * VertY);
        for (int32 YIdx = 0; YIdx < VertY; ++YIdx)
        {
            for (int32 XIdx = 0; XIdx < VertX; ++XIdx)
            {
                HeightArray[YIdx * VertX + XIdx] = 32768; // 0cm elevation
            }
        }
        
        // Import the heightmap (UE 5.6+ API)
        const int32 InMinX = 0;
        const int32 InMinY = 0;
        const int32 InMaxX = ComponentsX * QuadsPerComponent;
        const int32 InMaxY = ComponentsY * QuadsPerComponent;
        const int32 NumSubsections = SectionsPerComponent;
        const int32 SubsectionSizeQuads = QuadsPerComponent / FMath::Max(1, SectionsPerComponent);
        
        // UE 5.6+ Import API expects TMap<FGuid, TArray<uint16>> for heightmap data
        FGuid HeightmapGuid = FGuid::NewGuid();
        TMap<FGuid, TArray<uint16>> ImportHeightData;
        ImportHeightData.Add(HeightmapGuid, MoveTemp(HeightArray));
        
        // Import layers info must be a TMap as well
        TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportLayerInfos;
        
        // Edit layers (last parameter) uses TArrayView in UE 5.6+
        TArray<FLandscapeLayer> EditLayers;
        
        Landscape->Import(
            HeightmapGuid,
            InMinX, InMinY,
            InMaxX, InMaxY,
            NumSubsections,
            SubsectionSizeQuads,
            ImportHeightData,
            nullptr,
            ImportLayerInfos,
            ELandscapeImportAlphamapType::Additive,
            TArrayView<const FLandscapeLayer>(EditLayers)
        );

        // Finalize
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Finalizing landscape")));
        Landscape->RecreateCollisionComponents();
        Landscape->PostEditChange();

        // Build response
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
        Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
        Resp->SetNumberField(TEXT("componentsX"), ComponentsX);
        Resp->SetNumberField(TEXT("componentsY"), ComponentsY);
        Resp->SetNumberField(TEXT("quadsPerComponent"), QuadsPerComponent);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Landscape created successfully"), Resp, FString());
    });

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_landscape requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleModifyHeightmap(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("modify_heightmap"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("modify_heightmap payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString LandscapePath;
    if (!Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath) || LandscapePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("landscapePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* HeightDataArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("heightData"), HeightDataArray) || !HeightDataArray || HeightDataArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("heightData array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Copy height data for async task
    TArray<uint16> HeightValues;
    for (const TSharedPtr<FJsonValue>& Val : *HeightDataArray)
    {
        if (Val.IsValid() && Val->Type == EJson::Number)
        {
            HeightValues.Add(static_cast<uint16>(FMath::Clamp(Val->AsNumber(), 0.0, 65535.0)));
        }
    }

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, LandscapePath, HeightValues, RequestingSocket]()
    {
        ALandscape* Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
        if (!Landscape)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load landscape"), TEXT("LOAD_FAILED"));
            return;
        }

        ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
        if (!LandscapeInfo)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Landscape has no info"), TEXT("INVALID_LANDSCAPE"));
            return;
        }

        FScopedSlowTask SlowTask(2.0f, FText::FromString(TEXT("Modifying heightmap...")));
        SlowTask.MakeDialog();

        // Get landscape dimensions
        int32 MinX, MinY, MaxX, MaxY;
        if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get landscape extent"), TEXT("INVALID_LANDSCAPE"));
            return;
        }

        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Writing heightmap data")));

        int32 SizeX = (MaxX - MinX + 1);
        int32 SizeY = (MaxY - MinY + 1);

        // Validate height data size
        if (HeightValues.Num() != SizeX * SizeY)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Height data size mismatch. Expected %d x %d = %d values, got %d"), 
                    SizeX, SizeY, SizeX * SizeY, HeightValues.Num()),
                TEXT("INVALID_ARGUMENT"));
            return;
        }

        // Write heightmap data in a single call (UE 5.6 API)
        FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
        LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightValues.GetData(), SizeX, /*InCalcNormals*/ true);

        // Flush and rebuild
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Rebuilding collision")));
        LandscapeEdit.Flush();
        Landscape->RecreateCollisionComponents();
        Landscape->PostEditChange();

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
        Resp->SetNumberField(TEXT("modifiedVertices"), HeightValues.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Heightmap modified successfully"), Resp, FString());
    });

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("modify_heightmap requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandlePaintLandscapeLayer(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("paint_landscape_layer"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("paint_landscape_layer payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString LandscapePath;
    if (!Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath) || LandscapePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("landscapePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("layerName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Paint region (optional - if not specified, paint entire landscape)
    int32 MinX = -1, MinY = -1, MaxX = -1, MaxY = -1;
    const TSharedPtr<FJsonObject>* RegionObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj)
    {
        (*RegionObj)->TryGetNumberField(TEXT("minX"), MinX);
        (*RegionObj)->TryGetNumberField(TEXT("minY"), MinY);
        (*RegionObj)->TryGetNumberField(TEXT("maxX"), MaxX);
        (*RegionObj)->TryGetNumberField(TEXT("maxY"), MaxY);
    }

    double Strength = 1.0;
    Payload->TryGetNumberField(TEXT("strength"), Strength);
    Strength = FMath::Clamp(Strength, 0.0, 1.0);

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, LandscapePath, LayerName, MinX, MinY, MaxX, MaxY, Strength, RequestingSocket]()
    {
        ALandscape* Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
        if (!Landscape)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load landscape"), TEXT("LOAD_FAILED"));
            return;
        }

        ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
        if (!LandscapeInfo)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Landscape has no info"), TEXT("INVALID_LANDSCAPE"));
            return;
        }

        // Find or create the layer
        ULandscapeLayerInfoObject* LayerInfo = nullptr;
        for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
        {
            if (Layer.LayerName == FName(*LayerName))
            {
                LayerInfo = Layer.LayerInfoObj;
                break;
            }
        }

        if (!LayerInfo)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Layer '%s' not found. Create layer first using landscape editor."), *LayerName),
                TEXT("LAYER_NOT_FOUND"));
            return;
        }

        FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Painting landscape layer...")));
        SlowTask.MakeDialog();

        // Get paint region
        int32 PaintMinX = MinX, PaintMinY = MinY, PaintMaxX = MaxX, PaintMaxY = MaxY;
        if (PaintMinX < 0 || PaintMaxX < 0)
        {
            LandscapeInfo->GetLandscapeExtent(PaintMinX, PaintMinY, PaintMaxX, PaintMaxY);
        }

        // Paint the layer (UE 5.6 API expects rectangle writes)
        FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
        const uint8 PaintValue = static_cast<uint8>(Strength * 255.0);
        const int32 RegionSizeX = (PaintMaxX - PaintMinX + 1);
        const int32 RegionSizeY = (PaintMaxY - PaintMinY + 1);
        TArray<uint8> AlphaData;
        AlphaData.Init(PaintValue, RegionSizeX * RegionSizeY);
        
        LandscapeEdit.SetAlphaData(LayerInfo, PaintMinX, PaintMinY, PaintMaxX, PaintMaxY, AlphaData.GetData(), RegionSizeX);

        LandscapeEdit.Flush();
        Landscape->PostEditChange();

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
        Resp->SetStringField(TEXT("layerName"), LayerName);
        Resp->SetNumberField(TEXT("strength"), Strength);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Layer painted successfully"), Resp, FString());
    });

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("paint_landscape_layer requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
