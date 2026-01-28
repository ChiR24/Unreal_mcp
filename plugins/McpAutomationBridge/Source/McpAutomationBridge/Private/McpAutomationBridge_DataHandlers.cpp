// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 31: Data & Persistence Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/DataTableFactory.h"
#include "Factories/DataAssetFactory.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead

// Data Assets & Tables
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Engine/CurveTable.h"
#include "Curves/RichCurve.h"
#include "Curves/SimpleCurve.h"

// Save Game
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// Gameplay Tags
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"

// Config
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

// Blueprint Creation for SaveGame
#include "Kismet2/KismetEditorUtilities.h"

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageDataAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_data"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_data")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_data payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  if (SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action_type"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Data action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // DATA ASSETS
  // ========================================================================
  if (LowerSub == TEXT("create_data_asset")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Normalize path
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      FString AssetName = FPackageName::GetShortName(AssetPath);
      
      UPackage* Package = CreatePackage(*AssetPath);
      if (Package) {
        Package->FullyLoad();
        
        // Use our generic data asset class
        UMcpGenericDataAsset* NewAsset = NewObject<UMcpGenericDataAsset>(
            Package, *AssetName, RF_Public | RF_Standalone);
        
        if (NewAsset) {
          // Set optional properties
          FString ItemName;
          if (Payload->TryGetStringField(TEXT("itemName"), ItemName)) {
            NewAsset->ItemName = ItemName;
          }
          FString Description;
          if (Payload->TryGetStringField(TEXT("description"), Description)) {
            NewAsset->Description = Description;
          }
          
          FAssetRegistryModule::AssetCreated(NewAsset);
          NewAsset->MarkPackageDirty();
          McpSafeAssetSave(NewAsset);
          
          bSuccess = true;
          Message = TEXT("Data asset created");
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create data asset object");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create package");
        ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // CREATE PRIMARY DATA ASSET
  // ========================================================================
  else if (LowerSub == TEXT("create_primary_data_asset")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString PrimaryAssetType;
    Payload->TryGetStringField(TEXT("primaryAssetType"), PrimaryAssetType);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      FString AssetName = FPackageName::GetShortName(AssetPath);
      UPackage* Package = CreatePackage(*AssetPath);
      
      if (Package) {
        Package->FullyLoad();
        
        // Create a generic primary data asset
        UMcpGenericDataAsset* NewAsset = NewObject<UMcpGenericDataAsset>(
            Package, *AssetName, RF_Public | RF_Standalone);
        
        if (NewAsset) {
          if (!PrimaryAssetType.IsEmpty()) {
            NewAsset->Properties.Add(TEXT("PrimaryAssetType"), PrimaryAssetType);
          }
          
          FAssetRegistryModule::AssetCreated(NewAsset);
          NewAsset->MarkPackageDirty();
          McpSafeAssetSave(NewAsset);
          
          bSuccess = true;
          Message = TEXT("Primary data asset created");
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create primary data asset");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create package");
        ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // GET DATA ASSET INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_data_asset_info")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
      if (Asset) {
        UDataAsset* DataAsset = Cast<UDataAsset>(Asset);
        if (DataAsset) {
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("className"), DataAsset->GetClass()->GetName());
          
          // If it's our generic type, include properties
          UMcpGenericDataAsset* GenericAsset = Cast<UMcpGenericDataAsset>(DataAsset);
          if (GenericAsset) {
            Resp->SetStringField(TEXT("itemName"), GenericAsset->ItemName);
            Resp->SetStringField(TEXT("description"), GenericAsset->Description);
            
            TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
            for (const auto& Pair : GenericAsset->Properties) {
              PropsObj->SetStringField(Pair.Key, Pair.Value);
            }
            Resp->SetObjectField(TEXT("properties"), PropsObj);
          }
          
          bSuccess = true;
          Message = TEXT("Data asset info retrieved");
        } else {
          bSuccess = false;
          Message = TEXT("Asset is not a data asset");
          ErrorCode = TEXT("INVALID_ASSET_TYPE");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET DATA ASSET PROPERTY
  // ========================================================================
  else if (LowerSub == TEXT("set_data_asset_property")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    FString PropertyValue;
    Payload->TryGetStringField(TEXT("value"), PropertyValue);
    
    if (AssetPath.IsEmpty() || PropertyName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and propertyName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UMcpGenericDataAsset* Asset = LoadObject<UMcpGenericDataAsset>(nullptr, *AssetPath);
      if (Asset) {
        if (PropertyName == TEXT("itemName")) {
          Asset->ItemName = PropertyValue;
        } else if (PropertyName == TEXT("description")) {
          Asset->Description = PropertyValue;
        } else {
          Asset->Properties.Add(PropertyName, PropertyValue);
        }
        
        Asset->MarkPackageDirty();
        McpSafeAssetSave(Asset);
        
        bSuccess = true;
        Message = TEXT("Property set successfully");
      } else {
        bSuccess = false;
        Message = TEXT("Data asset not found or not compatible type");
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CREATE DATA TABLE
  // ========================================================================
  else if (LowerSub == TEXT("create_data_table")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowStructPath;
    Payload->TryGetStringField(TEXT("rowStructPath"), RowStructPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      FString AssetName = FPackageName::GetShortName(AssetPath);
      UPackage* Package = CreatePackage(*AssetPath);
      
      if (Package) {
        Package->FullyLoad();
        
        UDataTable* NewTable = NewObject<UDataTable>(Package, *AssetName, RF_Public | RF_Standalone);
        
        if (NewTable) {
          // Try to find the row struct
          if (!RowStructPath.IsEmpty()) {
            UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *RowStructPath);
            if (!RowStruct) {
              RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
            }
            if (RowStruct) {
              NewTable->RowStruct = RowStruct;
            }
          }
          
          FAssetRegistryModule::AssetCreated(NewTable);
          NewTable->MarkPackageDirty();
          McpSafeAssetSave(NewTable);
          
          bSuccess = true;
          Message = TEXT("Data table created");
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create data table");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create package");
        ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // ADD DATA TABLE ROW
  // ========================================================================
  else if (LowerSub == TEXT("add_data_table_row")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowName;
    Payload->TryGetStringField(TEXT("rowName"), RowName);
    
    if (AssetPath.IsEmpty() || RowName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and rowName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table && Table->RowStruct) {
        // Create new row data
        uint8* RowData = (uint8*)FMemory::Malloc(Table->RowStruct->GetStructureSize());
        Table->RowStruct->InitializeStruct(RowData);
        
        // Set row values from payload if provided
        const TSharedPtr<FJsonObject>* RowValues = nullptr;
        if (Payload->TryGetObjectField(TEXT("rowData"), RowValues) && RowValues) {
          for (TFieldIterator<FProperty> It(Table->RowStruct); It; ++It) {
            FProperty* Prop = *It;
            FString PropName = Prop->GetName();
            
            if ((*RowValues)->HasField(PropName)) {
              FString ValueStr = (*RowValues)->GetStringField(PropName);
              void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
              Prop->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None);
            }
          }
        }
        
        Table->AddRow(FName(*RowName), RowData, Table->RowStruct);
        Table->MarkPackageDirty();
        McpSafeAssetSave(Table);
        
        // Free temporary data
        Table->RowStruct->DestroyStruct(RowData);
        FMemory::Free(RowData);
        
        bSuccess = true;
        Message = TEXT("Row added to data table");
        Resp->SetStringField(TEXT("rowName"), RowName);
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found or has no row struct");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // REMOVE DATA TABLE ROW
  // ========================================================================
  else if (LowerSub == TEXT("remove_data_table_row")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowName;
    Payload->TryGetStringField(TEXT("rowName"), RowName);
    
    if (AssetPath.IsEmpty() || RowName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and rowName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table) {
        Table->RemoveRow(FName(*RowName));
        Table->MarkPackageDirty();
        McpSafeAssetSave(Table);
        
        bSuccess = true;
        Message = TEXT("Row removed from data table");
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET DATA TABLE ROW
  // ========================================================================
  else if (LowerSub == TEXT("get_data_table_row")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowName;
    Payload->TryGetStringField(TEXT("rowName"), RowName);
    
    if (AssetPath.IsEmpty() || RowName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and rowName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table && Table->RowStruct) {
        uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
        if (RowData) {
          TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
          
          for (TFieldIterator<FProperty> It(Table->RowStruct); It; ++It) {
            FProperty* Prop = *It;
            FString PropName = Prop->GetName();
            FString ValueStr;
            void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
            Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
            RowObj->SetStringField(PropName, ValueStr);
          }
          
          Resp->SetObjectField(TEXT("rowData"), RowObj);
          Resp->SetStringField(TEXT("rowName"), RowName);
          bSuccess = true;
          Message = TEXT("Row retrieved");
        } else {
          bSuccess = false;
          Message = FString::Printf(TEXT("Row '%s' not found"), *RowName);
          ErrorCode = TEXT("ROW_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET DATA TABLE ROWS
  // ========================================================================
  else if (LowerSub == TEXT("get_data_table_rows")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table && Table->RowStruct) {
        TArray<TSharedPtr<FJsonValue>> RowsArray;
        TArray<FName> RowNames = Table->GetRowNames();
        
        for (const FName& Name : RowNames) {
          uint8* RowData = Table->FindRowUnchecked(Name);
          if (RowData) {
            TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
            RowObj->SetStringField(TEXT("_rowName"), Name.ToString());
            
            for (TFieldIterator<FProperty> It(Table->RowStruct); It; ++It) {
              FProperty* Prop = *It;
              FString PropName = Prop->GetName();
              FString ValueStr;
              void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
              Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
              RowObj->SetStringField(PropName, ValueStr);
            }
            
            RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
          }
        }
        
        Resp->SetArrayField(TEXT("rows"), RowsArray);
        Resp->SetNumberField(TEXT("rowCount"), RowsArray.Num());
        bSuccess = true;
        Message = TEXT("Rows retrieved");
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // IMPORT DATA TABLE CSV
  // ========================================================================
  else if (LowerSub == TEXT("import_data_table_csv")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString CsvContent;
    Payload->TryGetStringField(TEXT("csvContent"), CsvContent);
    FString CsvFilePath;
    Payload->TryGetStringField(TEXT("csvFilePath"), CsvFilePath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      // Load CSV from file if path provided
      if (CsvContent.IsEmpty() && !CsvFilePath.IsEmpty()) {
        FFileHelper::LoadFileToString(CsvContent, *CsvFilePath);
      }
      
      if (CsvContent.IsEmpty()) {
        bSuccess = false;
        Message = TEXT("csvContent or csvFilePath is required");
        ErrorCode = TEXT("MISSING_CSV_DATA");
      } else {
        UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
        if (Table && Table->RowStruct) {
          TArray<FString> Problems = Table->CreateTableFromCSVString(CsvContent);
          
          if (Problems.Num() == 0) {
            Table->MarkPackageDirty();
            McpSafeAssetSave(Table);
            bSuccess = true;
            Message = TEXT("CSV imported successfully");
          } else {
            bSuccess = false;
            Message = FString::Printf(TEXT("Import had %d problems: %s"), Problems.Num(), *Problems[0]);
            ErrorCode = TEXT("IMPORT_PROBLEMS");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Data table not found or has no row struct");
          ErrorCode = TEXT("TABLE_NOT_FOUND");
        }
      }
    }
  }
  // ========================================================================
  // EXPORT DATA TABLE CSV
  // ========================================================================
  else if (LowerSub == TEXT("export_data_table_csv")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table) {
        FString CsvContent = Table->GetTableAsCSV();
        
        if (!OutputPath.IsEmpty()) {
          FFileHelper::SaveStringToFile(CsvContent, *OutputPath);
          Message = FString::Printf(TEXT("CSV exported to %s"), *OutputPath);
        } else {
          Resp->SetStringField(TEXT("csvContent"), CsvContent);
          Message = TEXT("CSV content retrieved");
        }
        bSuccess = true;
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // EMPTY DATA TABLE
  // ========================================================================
  else if (LowerSub == TEXT("empty_data_table")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath);
      if (Table) {
        Table->EmptyTable();
        Table->MarkPackageDirty();
        McpSafeAssetSave(Table);
        
        bSuccess = true;
        Message = TEXT("Data table emptied");
      } else {
        bSuccess = false;
        Message = TEXT("Data table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CREATE CURVE TABLE
  // ========================================================================
  else if (LowerSub == TEXT("create_curve_table")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString CurveType;
    Payload->TryGetStringField(TEXT("curveType"), CurveType);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      FString AssetName = FPackageName::GetShortName(AssetPath);
      UPackage* Package = CreatePackage(*AssetPath);
      
      if (Package) {
        Package->FullyLoad();
        
        UCurveTable* NewTable = NewObject<UCurveTable>(Package, *AssetName, RF_Public | RF_Standalone);
        
        if (NewTable) {
          FAssetRegistryModule::AssetCreated(NewTable);
          NewTable->MarkPackageDirty();
          McpSafeAssetSave(NewTable);
          
          bSuccess = true;
          Message = TEXT("Curve table created");
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create curve table");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create package");
        ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
      }
    }
  }
  // ========================================================================
  // ADD CURVE ROW
  // ========================================================================
  else if (LowerSub == TEXT("add_curve_row")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowName;
    Payload->TryGetStringField(TEXT("rowName"), RowName);
    
    if (AssetPath.IsEmpty() || RowName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and rowName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UCurveTable* Table = LoadObject<UCurveTable>(nullptr, *AssetPath);
      if (Table) {
        FRichCurve& NewCurve = Table->AddRichCurve(FName(*RowName));
        
        // Add keys if provided
        const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
        if (Payload->TryGetArrayField(TEXT("keys"), Keys) && Keys) {
          for (const auto& KeyVal : *Keys) {
            const TSharedPtr<FJsonObject>* KeyObj = nullptr;
            if (KeyVal->TryGetObject(KeyObj) && KeyObj) {
              double Time = 0.0, Value = 0.0;
              (*KeyObj)->TryGetNumberField(TEXT("time"), Time);
              (*KeyObj)->TryGetNumberField(TEXT("value"), Value);
              NewCurve.AddKey(static_cast<float>(Time), static_cast<float>(Value));
            }
          }
        }
        
        Table->MarkPackageDirty();
        McpSafeAssetSave(Table);
        
        bSuccess = true;
        Message = TEXT("Curve row added");
        Resp->SetStringField(TEXT("rowName"), RowName);
      } else {
        bSuccess = false;
        Message = TEXT("Curve table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET CURVE VALUE
  // ========================================================================
  else if (LowerSub == TEXT("get_curve_value")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString RowName;
    Payload->TryGetStringField(TEXT("rowName"), RowName);
    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);
    
    if (AssetPath.IsEmpty() || RowName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and rowName are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UCurveTable* Table = LoadObject<UCurveTable>(nullptr, *AssetPath);
      if (Table) {
        FRealCurve* Curve = Table->FindCurve(FName(*RowName), TEXT("GetCurveValue"), false);
        if (Curve) {
          float Value = Curve->Eval(static_cast<float>(Time));
          Resp->SetNumberField(TEXT("value"), Value);
          Resp->SetNumberField(TEXT("time"), Time);
          bSuccess = true;
          Message = TEXT("Curve value retrieved");
        } else {
          bSuccess = false;
          Message = FString::Printf(TEXT("Curve row '%s' not found"), *RowName);
          ErrorCode = TEXT("ROW_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Curve table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // IMPORT CURVE TABLE CSV
  // ========================================================================
  else if (LowerSub == TEXT("import_curve_table_csv")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString CsvContent;
    Payload->TryGetStringField(TEXT("csvContent"), CsvContent);
    FString CsvFilePath;
    Payload->TryGetStringField(TEXT("csvFilePath"), CsvFilePath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      if (CsvContent.IsEmpty() && !CsvFilePath.IsEmpty()) {
        FFileHelper::LoadFileToString(CsvContent, *CsvFilePath);
      }
      
      if (CsvContent.IsEmpty()) {
        bSuccess = false;
        Message = TEXT("csvContent or csvFilePath is required");
        ErrorCode = TEXT("MISSING_CSV_DATA");
      } else {
        UCurveTable* Table = LoadObject<UCurveTable>(nullptr, *AssetPath);
        if (Table) {
          TArray<FString> Problems = Table->CreateTableFromCSVString(CsvContent, ERichCurveInterpMode::RCIM_Linear);
          
          if (Problems.Num() == 0) {
            Table->MarkPackageDirty();
            McpSafeAssetSave(Table);
            bSuccess = true;
            Message = TEXT("CSV imported to curve table successfully");
          } else {
            bSuccess = false;
            Message = FString::Printf(TEXT("Import had %d problems"), Problems.Num());
            ErrorCode = TEXT("IMPORT_PROBLEMS");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Curve table not found");
          ErrorCode = TEXT("TABLE_NOT_FOUND");
        }
      }
    }
  }
  // ========================================================================
  // EXPORT CURVE TABLE CSV
  // ========================================================================
  else if (LowerSub == TEXT("export_curve_table_csv")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      UCurveTable* Table = LoadObject<UCurveTable>(nullptr, *AssetPath);
      if (Table) {
        FString CsvContent = Table->GetTableAsCSV();
        
        if (!OutputPath.IsEmpty()) {
          FFileHelper::SaveStringToFile(CsvContent, *OutputPath);
          Message = FString::Printf(TEXT("Curve table CSV exported to %s"), *OutputPath);
        } else {
          Resp->SetStringField(TEXT("csvContent"), CsvContent);
          Message = TEXT("Curve table CSV content retrieved");
        }
        bSuccess = true;
      } else {
        bSuccess = false;
        Message = TEXT("Curve table not found");
        ErrorCode = TEXT("TABLE_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SAVE GAME OPERATIONS
  // ========================================================================
  else if (LowerSub == TEXT("create_save_game_blueprint")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      AssetPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
      
      FString AssetName = FPackageName::GetShortName(AssetPath);
      FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
      
      UPackage* Package = CreatePackage(*AssetPath);
      if (Package) {
        Package->FullyLoad();
        
        // Create a SaveGame blueprint
        UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
            USaveGame::StaticClass(),
            Package,
            FName(*AssetName),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass()
        );
        
        if (NewBP) {
          FAssetRegistryModule::AssetCreated(NewBP);
          NewBP->MarkPackageDirty();
          McpSafeAssetSave(NewBP);
          
          bSuccess = true;
          Message = TEXT("SaveGame blueprint created");
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create SaveGame blueprint");
          ErrorCode = TEXT("CREATION_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create package");
        ErrorCode = TEXT("PACKAGE_CREATION_FAILED");
      }
    }
  }
  else if (LowerSub == TEXT("save_game_to_slot")) {
    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 UserIndex = 0;
    if (double UserIndexD = 0.0; Payload->TryGetNumberField(TEXT("userIndex"), UserIndexD)) {
      UserIndex = static_cast<int32>(UserIndexD);
    }
    FString SaveGameClass;
    Payload->TryGetStringField(TEXT("saveGameClass"), SaveGameClass);
    
    if (SlotName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("slotName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Determine which SaveGame class to use
      UClass* SaveClass = USaveGame::StaticClass();
      if (!SaveGameClass.IsEmpty()) {
        UClass* CustomClass = FindObject<UClass>(nullptr, *SaveGameClass);
        if (CustomClass && CustomClass->IsChildOf(USaveGame::StaticClass())) {
          SaveClass = CustomClass;
        }
      }
      
      // Create a save game object
      USaveGame* SaveObj = UGameplayStatics::CreateSaveGameObject(SaveClass);
      if (SaveObj) {
        // Set data from payload if provided
        const TSharedPtr<FJsonObject>* DataObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("data"), DataObj) && DataObj) {
          // Iterate through JSON fields and apply to SaveGame properties
          for (const auto& Pair : (*DataObj)->Values) {
            FString PropName = Pair.Key;
            const TSharedPtr<FJsonValue>& JsonVal = Pair.Value;
            
            FProperty* Property = SaveObj->GetClass()->FindPropertyByName(*PropName);
            if (Property) {
              FString ApplyError;
              ApplyJsonValueToProperty(SaveObj, Property, JsonVal, ApplyError);
            }
          }
        }
        
        bool bSaved = UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, UserIndex);
        if (bSaved) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Game saved to slot '%s'"), *SlotName);
          Resp->SetStringField(TEXT("slotName"), SlotName);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to save game to slot");
          ErrorCode = TEXT("SAVE_FAILED");
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create save game object");
        ErrorCode = TEXT("CREATION_FAILED");
      }
    }
  }
  else if (LowerSub == TEXT("load_game_from_slot")) {
    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 UserIndex = 0;
    if (double UserIndexD = 0.0; Payload->TryGetNumberField(TEXT("userIndex"), UserIndexD)) {
      UserIndex = static_cast<int32>(UserIndexD);
    }
    
    if (SlotName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("slotName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
      if (LoadedGame) {
        bSuccess = true;
        Message = FString::Printf(TEXT("Game loaded from slot '%s'"), *SlotName);
        Resp->SetStringField(TEXT("slotName"), SlotName);
        Resp->SetStringField(TEXT("className"), LoadedGame->GetClass()->GetName());
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Failed to load game from slot '%s'"), *SlotName);
        ErrorCode = TEXT("LOAD_FAILED");
      }
    }
  }
  else if (LowerSub == TEXT("delete_save_slot")) {
    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 UserIndex = 0;
    if (double UserIndexD = 0.0; Payload->TryGetNumberField(TEXT("userIndex"), UserIndexD)) {
      UserIndex = static_cast<int32>(UserIndexD);
    }
    
    if (SlotName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("slotName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
      if (bDeleted) {
        bSuccess = true;
        Message = FString::Printf(TEXT("Save slot '%s' deleted"), *SlotName);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Failed to delete save slot '%s'"), *SlotName);
        ErrorCode = TEXT("DELETE_FAILED");
      }
    }
  }
  else if (LowerSub == TEXT("does_save_exist")) {
    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 UserIndex = 0;
    if (double UserIndexD = 0.0; Payload->TryGetNumberField(TEXT("userIndex"), UserIndexD)) {
      UserIndex = static_cast<int32>(UserIndexD);
    }
    
    if (SlotName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("slotName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
      Resp->SetBoolField(TEXT("exists"), bExists);
      Resp->SetStringField(TEXT("slotName"), SlotName);
      bSuccess = true;
      Message = FString::Printf(TEXT("Save slot '%s' %s"), *SlotName, bExists ? TEXT("exists") : TEXT("does not exist"));
    }
  }
  else if (LowerSub == TEXT("get_save_slot_names")) {
    int32 UserIndex = 0;
    Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);
    
    // Get save game directory
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
    TArray<FString> SaveFiles;
    IFileManager::Get().FindFiles(SaveFiles, *(SaveDir / TEXT("*.sav")), true, false);
    
    TArray<TSharedPtr<FJsonValue>> SlotNames;
    SlotNames.Reserve(SaveFiles.Num());
    for (const FString& File : SaveFiles) {
      FString SlotName = FPaths::GetBaseFilename(File);
      SlotNames.Add(MakeShared<FJsonValueString>(SlotName));
    }
    
    Resp->SetArrayField(TEXT("slotNames"), SlotNames);
    Resp->SetNumberField(TEXT("count"), SlotNames.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d save slots"), SlotNames.Num());
  }
  // ========================================================================
  // GAMEPLAY TAGS
  // ========================================================================
  else if (LowerSub == TEXT("create_gameplay_tag") || LowerSub == TEXT("add_native_gameplay_tag")) {
    FString TagName;
    Payload->TryGetStringField(TEXT("tagName"), TagName);
    FString Comment;
    Payload->TryGetStringField(TEXT("comment"), Comment);
    
    if (TagName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("tagName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Native gameplay tags can ONLY be added during engine initialization (module startup).
      // Once the engine is fully initialized (including in the editor), calling AddNativeGameplayTag
      // will trigger an ensure condition and crash. This is a fundamental limitation of UE5.
      // 
      // Proper ways to add gameplay tags:
      // 1. Add them during module startup using FGameplayTagNativeAdder
      // 2. Add them to Config/DefaultGameplayTags.ini
      // 3. Use the Project Settings > Gameplay Tags UI
      //
      // Since we're running in the editor (post-initialization), we cannot add native tags.
      bSuccess = false;
      Message = FString::Printf(TEXT("Cannot create native gameplay tag '%s' - engine is already initialized. "
                                     "Native tags can only be added during module startup. "
                                     "Use Project Settings > Gameplay Tags, or add to DefaultGameplayTags.ini instead."), *TagName);
      ErrorCode = TEXT("NATIVE_TAGS_EDITOR_RESTRICTION");
      Resp->SetBoolField(TEXT("editorRestriction"), true);
      Resp->SetStringField(TEXT("alternative"), TEXT("Add tags via Project Settings > Gameplay Tags or Config/DefaultGameplayTags.ini"));
    }
  }
  else if (LowerSub == TEXT("request_gameplay_tag")) {
    FString TagName;
    Payload->TryGetStringField(TEXT("tagName"), TagName);
    
    if (TagName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("tagName is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
      FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);
      
      if (Tag.IsValid()) {
        bSuccess = true;
        Message = TEXT("Tag found");
        Resp->SetStringField(TEXT("tagName"), Tag.ToString());
        Resp->SetBoolField(TEXT("valid"), true);
      } else {
        bSuccess = true; // Not an error, just tag doesn't exist
        Message = TEXT("Tag not found");
        Resp->SetBoolField(TEXT("valid"), false);
      }
    }
  }
  else if (LowerSub == TEXT("check_tag_match")) {
    FString TagToCheck;
    Payload->TryGetStringField(TEXT("tagToCheck"), TagToCheck);
    FString TagToMatch;
    Payload->TryGetStringField(TEXT("tagToMatch"), TagToMatch);
    bool bExactMatch = false;
    Payload->TryGetBoolField(TEXT("exactMatch"), bExactMatch);
    
    if (TagToCheck.IsEmpty() || TagToMatch.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("tagToCheck and tagToMatch are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
      FGameplayTag Tag1 = Manager.RequestGameplayTag(FName(*TagToCheck), false);
      FGameplayTag Tag2 = Manager.RequestGameplayTag(FName(*TagToMatch), false);
      
      bool bMatches = bExactMatch ? Tag1.MatchesTagExact(Tag2) : Tag1.MatchesTag(Tag2);
      
      Resp->SetBoolField(TEXT("matches"), bMatches);
      Resp->SetStringField(TEXT("tagToCheck"), TagToCheck);
      Resp->SetStringField(TEXT("tagToMatch"), TagToMatch);
      bSuccess = true;
      Message = bMatches ? TEXT("Tags match") : TEXT("Tags do not match");
    }
  }
  else if (LowerSub == TEXT("create_tag_container")) {
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    
    if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray && TagsArray->Num() > 0) {
      FGameplayTagContainer Container;
      UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
      
      for (const auto& TagVal : *TagsArray) {
        FString TagStr;
        if (TagVal->TryGetString(TagStr)) {
          FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagStr), false);
          if (Tag.IsValid()) {
            Container.AddTag(Tag);
          }
        }
      }
      
      TArray<TSharedPtr<FJsonValue>> AddedTags;
      AddedTags.Reserve(Container.Num());
      for (const FGameplayTag& Tag : Container) {
        AddedTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
      }
      
      Resp->SetArrayField(TEXT("tags"), AddedTags);
      Resp->SetNumberField(TEXT("count"), Container.Num());
      bSuccess = true;
      Message = FString::Printf(TEXT("Container created with %d tags"), Container.Num());
    } else {
      bSuccess = false;
      Message = TEXT("tags array is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    }
  }
  else if (LowerSub == TEXT("add_tag_to_container") || LowerSub == TEXT("remove_tag_from_container") || LowerSub == TEXT("has_tag")) {
    // These operations typically work on runtime containers
    // For now, return info that these are runtime operations
    bSuccess = true;
    Message = TEXT("Tag container operations are runtime-only. Use in gameplay code.");
    Resp->SetBoolField(TEXT("runtimeOnly"), true);
  }
  else if (LowerSub == TEXT("get_all_gameplay_tags")) {
    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
    FGameplayTagContainer AllTags;
    Manager.RequestAllGameplayTags(AllTags, true);
    
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    TagsArray.Reserve(AllTags.Num());
    for (const FGameplayTag& Tag : AllTags) {
      TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    
    Resp->SetArrayField(TEXT("tags"), TagsArray);
    Resp->SetNumberField(TEXT("count"), TagsArray.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Retrieved %d gameplay tags"), TagsArray.Num());
  }
  // ========================================================================
  // CONFIG OPERATIONS
  // ========================================================================
  else if (LowerSub == TEXT("read_config_value")) {
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    FString ConfigFile;
    Payload->TryGetStringField(TEXT("configFile"), ConfigFile);
    
    if (Section.IsEmpty() || Key.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("section and key are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      // Determine config file
      FString Filename = GGameIni;
      if (!ConfigFile.IsEmpty()) {
        if (ConfigFile.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) {
          Filename = GEngineIni;
        } else if (ConfigFile.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) {
          Filename = GEditorIni;
        } else if (ConfigFile.Equals(TEXT("Input"), ESearchCase::IgnoreCase)) {
          Filename = GInputIni;
        }
      }
      
      FString Value;
      if (GConfig->GetString(*Section, *Key, Value, Filename)) {
        Resp->SetStringField(TEXT("value"), Value);
        Resp->SetStringField(TEXT("section"), Section);
        Resp->SetStringField(TEXT("key"), Key);
        bSuccess = true;
        Message = TEXT("Config value retrieved");
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Config value not found: [%s] %s"), *Section, *Key);
        ErrorCode = TEXT("VALUE_NOT_FOUND");
      }
    }
  }
  else if (LowerSub == TEXT("write_config_value")) {
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    FString Value;
    Payload->TryGetStringField(TEXT("value"), Value);
    FString ConfigFile;
    Payload->TryGetStringField(TEXT("configFile"), ConfigFile);
    
    if (Section.IsEmpty() || Key.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("section and key are required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      FString Filename = GGameIni;
      if (!ConfigFile.IsEmpty()) {
        if (ConfigFile.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) {
          Filename = GEngineIni;
        } else if (ConfigFile.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) {
          Filename = GEditorIni;
        } else if (ConfigFile.Equals(TEXT("Input"), ESearchCase::IgnoreCase)) {
          Filename = GInputIni;
        }
      }
      
      GConfig->SetString(*Section, *Key, *Value, Filename);
      GConfig->Flush(false, Filename);
      
      bSuccess = true;
      Message = TEXT("Config value written");
      Resp->SetStringField(TEXT("section"), Section);
      Resp->SetStringField(TEXT("key"), Key);
    }
  }
  else if (LowerSub == TEXT("get_config_section")) {
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    FString ConfigFile;
    Payload->TryGetStringField(TEXT("configFile"), ConfigFile);
    
    if (Section.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("section is required");
      ErrorCode = TEXT("MISSING_PARAMETER");
    } else {
      FString Filename = GGameIni;
      if (!ConfigFile.IsEmpty()) {
        if (ConfigFile.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) {
          Filename = GEngineIni;
        } else if (ConfigFile.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) {
          Filename = GEditorIni;
        }
      }
      
      TArray<FString> SectionStrings;
      if (GConfig->GetSection(*Section, SectionStrings, Filename)) {
        TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
        for (const FString& Line : SectionStrings) {
          FString LKey, LValue;
          if (Line.Split(TEXT("="), &LKey, &LValue)) {
            SectionObj->SetStringField(LKey, LValue);
          }
        }
        Resp->SetObjectField(TEXT("values"), SectionObj);
        Resp->SetStringField(TEXT("section"), Section);
        Resp->SetNumberField(TEXT("count"), SectionStrings.Num());
        bSuccess = true;
        Message = TEXT("Section retrieved");
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Section '%s' not found"), *Section);
        ErrorCode = TEXT("SECTION_NOT_FOUND");
      }
    }
  }
  else if (LowerSub == TEXT("flush_config")) {
    FString ConfigFile;
    Payload->TryGetStringField(TEXT("configFile"), ConfigFile);
    
    FString Filename = GGameIni;
    if (!ConfigFile.IsEmpty()) {
      if (ConfigFile.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) {
        Filename = GEngineIni;
      } else if (ConfigFile.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) {
        Filename = GEditorIni;
      }
    }
    
    GConfig->Flush(false, Filename);
    bSuccess = true;
    Message = TEXT("Config flushed to disk");
  }
  else if (LowerSub == TEXT("reload_config")) {
    FString ConfigFile;
    Payload->TryGetStringField(TEXT("configFile"), ConfigFile);
    
    FString Filename = GGameIni;
    if (!ConfigFile.IsEmpty()) {
      if (ConfigFile.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) {
        Filename = GEngineIni;
      } else if (ConfigFile.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) {
        Filename = GEditorIni;
      }
    }
    
    // Clear cached values and reload from disk
    // First, unload the file if it's already loaded
    if (FConfigFile* ConfigFilePtr = GConfig->Find(Filename)) {
      // Force reload by reading the file again
      ConfigFilePtr->Read(Filename);
      bSuccess = true;
      Message = TEXT("Config reloaded from disk");
    } else {
      // File not loaded yet, just report success (will load on next access)
      bSuccess = true;
      Message = TEXT("Config not currently loaded, will load on next access");
    }
  }
  // ========================================================================
  // UNKNOWN ACTION
  // ========================================================================
  // UNKNOWN ACTION
  // Return false to allow other handlers to try (dispatch fall-through)
  else {
    return false;
  }

  // Fallback return - should never reach here but required for compiler
  return true;

#else
  // Non-editor build - send error and return true to indicate we handled it
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("manage_data requires editor build."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif // WITH_EDITOR
}
