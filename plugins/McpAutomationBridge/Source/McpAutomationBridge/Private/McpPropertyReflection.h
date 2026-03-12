// =============================================================================
// McpPropertyReflection.h
// =============================================================================
// Property reflection and JSON conversion utilities for MCP Automation Bridge.
//
// This file provides:
// - Export property values to JSON
// - Apply JSON values to properties
// - Type-safe property access helpers
// - Enum conversion utilities
//
// REFACTORING NOTES:
// - Extracted from McpAutomationBridgeHelpers.h for better organization
// - Reduces include dependency bloat for handlers that only need property access
// - Consolidates duplicated property conversion patterns across handlers
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"

// =============================================================================
// MCP Property Reflection Namespace
// =============================================================================
namespace McpPropertyReflection
{
    // =========================================================================
    // JSON Value Export (Property -> JSON)
    // =========================================================================

    /**
     * Convert a single Unreal property value from a container into a JSON value.
     * 
     * Supported property types:
     * - Strings (FStrProperty)
     * - Names (FNameProperty)
     * - Booleans (FBoolProperty)
     * - Numeric types (float, double, int32, int64, byte)
     * - Enums (returns name or numeric value)
     * - Object references (returns path string or null)
     * - Soft object/class references (returns soft path or null)
     * - Common structs (FVector, FRotator as arrays)
     * - Arrays (of supported inner types)
     * - Maps (with basic value types)
     * - Sets (of supported element types)
     *
     * @param TargetContainer Pointer to the container holding the property value
     * @param Property The property definition to export
     * @return JSON value representing the property, or null for unsupported types
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonValue> ExportPropertyToJsonValue(
        void* TargetContainer, 
        FProperty* Property);

    /**
     * Export all properties of a UObject to a JSON object.
     * @param Object The object to export
     * @param bIncludeTransient If true, include transient properties
     * @return JSON object with all property values
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportObjectToJson(
        UObject* Object, 
        bool bIncludeTransient = false);

    /**
     * Export specific properties of a UObject to a JSON object.
     * @param Object The object to export
     * @param PropertyNames Names of properties to export
     * @return JSON object with specified property values
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportPropertiesToJson(
        UObject* Object,
        const TArray<FName>& PropertyNames);

    // =========================================================================
    // JSON Value Import (JSON -> Property)
    // =========================================================================

    /**
     * Apply a JSON value to a reflected property on a target container.
     * 
     * Supports conversion from JSON to:
     * - Bool (from boolean, number, or "true"/"false" string)
     * - String/Name (from string)
     * - Numeric types (from number or numeric string)
     * - Enums (from string name or numeric value)
     * - Object references (from path string)
     * - Soft references (from path string or null)
     * - Structs (Vector/Rotator from array, or struct from JSON object)
     * - Arrays of supported types
     *
     * @param TargetContainer Pointer to the container to modify
     * @param Property The property to set
     * @param ValueField The JSON value to apply
     * @param OutError Receives error message on failure
     * @return true if successful
     */
    MCPAUTOMATIONBRIDGE_API bool ApplyJsonValueToProperty(
        void* TargetContainer,
        FProperty* Property,
        const TSharedPtr<FJsonValue>& ValueField,
        FString& OutError);

    /**
     * Apply multiple JSON values to properties of an object.
     * @param Object The object to modify
     * @param JsonValues Map of property names to JSON values
     * @param OutErrors Optional map to receive property-specific errors
     * @return Number of properties successfully set
     */
    MCPAUTOMATIONBRIDGE_API int32 ApplyJsonValuesToObject(
        UObject* Object,
        const TMap<FName, TSharedPtr<FJsonValue>>& JsonValues,
        TMap<FName, FString>* OutErrors = nullptr);

    /**
     * Apply properties from a JSON object to a UObject.
     * @param Object The object to modify
     * @param JsonObject The JSON object containing property values
     * @param OutErrors Optional map to receive property-specific errors
     * @return Number of properties successfully set
     */
    MCPAUTOMATIONBRIDGE_API int32 ApplyJsonObjectToObject(
        UObject* Object,
        const TSharedPtr<FJsonObject>& JsonObject,
        TMap<FName, FString>* OutErrors = nullptr);

    // =========================================================================
    // Property Type Utilities
    // =========================================================================

    /**
     * Get a human-readable type name for a property.
     */
    MCPAUTOMATIONBRIDGE_API FString GetPropertyTypeName(FProperty* Property);

    /**
     * Check if a property type is supported for JSON conversion.
     */
    MCPAUTOMATIONBRIDGE_API bool IsPropertyTypeSupported(FProperty* Property);

    /**
     * Get a property by name from an object's class.
     * @param Object The object to search
     * @param PropertyName The property name to find
     * @return The property, or nullptr if not found
     */
    inline FProperty* FindPropertyByName(UObject* Object, const FName& PropertyName)
    {
        if (!Object)
        {
            return nullptr;
        }
        
        UClass* Class = Object->GetClass();
        if (!Class)
        {
            return nullptr;
        }
        
        return Class->FindPropertyByName(PropertyName);
    }

    /**
     * Get the value of a property as a string.
     * @param Object The object containing the property
     * @param Property The property to read
     * @return String representation of the property value
     */
    MCPAUTOMATIONBRIDGE_API FString GetPropertyValueAsString(UObject* Object, FProperty* Property);

    /**
     * Set the value of a property from a string.
     * @param Object The object containing the property
     * @param Property The property to set
     * @param ValueString The string value to set
     * @param OutError Optional error message output
     * @return true if successful
     */
    MCPAUTOMATIONBRIDGE_API bool SetPropertyValueFromString(
        UObject* Object,
        FProperty* Property,
        const FString& ValueString,
        FString* OutError = nullptr);

    // =========================================================================
    // Enum Utilities
    // =========================================================================

    /**
     * Get all enum values as an array of strings.
     * @param Enum The enum to get values from
     * @return Array of enum value names
     */
    MCPAUTOMATIONBRIDGE_API TArray<FString> GetEnumValueNames(UEnum* Enum);

    /**
     * Convert an enum value to its string name.
     * @param Enum The enum type
     * @param Value The numeric enum value
     * @return The enum value name, or empty string if not found
     */
    MCPAUTOMATIONBRIDGE_API FString EnumValueToName(UEnum* Enum, int64 Value);

    /**
     * Convert an enum name string to its numeric value.
     * @param Enum The enum type
     * @param Name The enum value name (with or without enum prefix)
     * @param OutValue The numeric value output
     * @return true if conversion succeeded
     */
    MCPAUTOMATIONBRIDGE_API bool EnumNameToValue(UEnum* Enum, const FString& Name, int64& OutValue);

    // =========================================================================
    // Struct Utilities
    // =========================================================================

    /**
     * Convert a Vector to a JSON array [x, y, z].
     */
    inline TSharedPtr<FJsonValue> VectorToJsonValue(const FVector& Vector)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.Z));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /**
     * Convert a JSON array to a Vector.
     * @param JsonArray The JSON array [x, y, z]
     * @param OutVector The output vector
     * @return true if conversion succeeded
     */
    inline bool JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FVector& OutVector)
    {
        if (JsonArray.Num() < 3)
        {
            return false;
        }
        
        OutVector.X = JsonArray[0]->AsNumber();
        OutVector.Y = JsonArray[1]->AsNumber();
        OutVector.Z = JsonArray[2]->AsNumber();
        return true;
    }

    /**
     * Convert a Rotator to a JSON array [pitch, yaw, roll].
     */
    inline TSharedPtr<FJsonValue> RotatorToJsonValue(const FRotator& Rotator)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /**
     * Convert a JSON array to a Rotator.
     */
    inline bool JsonArrayToRotator(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FRotator& OutRotator)
    {
        if (JsonArray.Num() < 3)
        {
            return false;
        }
        
        OutRotator.Pitch = JsonArray[0]->AsNumber();
        OutRotator.Yaw = JsonArray[1]->AsNumber();
        OutRotator.Roll = JsonArray[2]->AsNumber();
        return true;
    }

    /**
     * Convert a Color to a JSON object {r, g, b, a}.
     */
    inline TSharedPtr<FJsonObject> ColorToJson(const FColor& Color)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("r"), Color.R);
        Obj->SetNumberField(TEXT("g"), Color.G);
        Obj->SetNumberField(TEXT("b"), Color.B);
        Obj->SetNumberField(TEXT("a"), Color.A);
        return Obj;
    }

    /**
     * Convert a JSON object to a Color.
     */
    inline bool JsonToColor(const TSharedPtr<FJsonObject>& Obj, FColor& OutColor)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double R = 255.0, G = 255.0, B = 255.0, A = 255.0;
        Obj->TryGetNumberField(TEXT("r"), R);
        Obj->TryGetNumberField(TEXT("g"), G);
        Obj->TryGetNumberField(TEXT("b"), B);
        Obj->TryGetNumberField(TEXT("a"), A);
        
        OutColor = FColor(
            FMath::Clamp(static_cast<uint8>(R), 0, 255),
            FMath::Clamp(static_cast<uint8>(G), 0, 255),
            FMath::Clamp(static_cast<uint8>(B), 0, 255),
            FMath::Clamp(static_cast<uint8>(A), 0, 255)
        );
        return true;
    }

    /**
     * Convert a LinearColor to a JSON object {r, g, b, a}.
     */
    inline TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("r"), Color.R);
        Obj->SetNumberField(TEXT("g"), Color.G);
        Obj->SetNumberField(TEXT("b"), Color.B);
        Obj->SetNumberField(TEXT("a"), Color.A);
        return Obj;
    }

    /**
     * Convert a JSON object to a LinearColor.
     */
    inline bool JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, FLinearColor& OutColor)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
        Obj->TryGetNumberField(TEXT("r"), R);
        Obj->TryGetNumberField(TEXT("g"), G);
        Obj->TryGetNumberField(TEXT("b"), B);
        Obj->TryGetNumberField(TEXT("a"), A);
        
        OutColor = FLinearColor(R, G, B, A);
        return true;
    }

    // =========================================================================
    // Container Property Utilities
    // =========================================================================

    /**
     * Check if a property is an array type.
     */
    inline bool IsArrayProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FArrayProperty>();
    }

    /**
     * Check if a property is a map type.
     */
    inline bool IsMapProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FMapProperty>();
    }

    /**
     * Check if a property is a set type.
     */
    inline bool IsSetProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FSetProperty>();
    }

    /**
     * Get the inner element property of an array property.
     */
    inline FProperty* GetArrayInnerProperty(FArrayProperty* ArrayProp)
    {
        return ArrayProp ? ArrayProp->Inner : nullptr;
    }

    /**
     * Get the element count of an array property.
     */
    MCPAUTOMATIONBRIDGE_API int32 GetArrayPropertyCount(void* Container, FArrayProperty* ArrayProp);

    /**
     * Export an array property to a JSON array.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(
        void* Container,
        FArrayProperty* ArrayProp);

    /**
     * Import a JSON array into an array property (replaces existing elements).
     */
    MCPAUTOMATIONBRIDGE_API bool ImportJsonToArray(
        void* Container,
        FArrayProperty* ArrayProp,
        const TArray<TSharedPtr<FJsonValue>>& JsonArray,
        FString& OutError);

    // =========================================================================
    // Output Capture Utility
    // =========================================================================

    /**
     * Captures log output written to GLog into an in-memory list of lines.
     * Attach as an FOutputDevice to collect serialized log messages.
     */
    struct FMcpOutputCapture : public FOutputDevice
    {
        TArray<FString> Lines;

        /**
         * Capture a log line, trim trailing newlines, and append to Lines.
         */
        virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
        {
            if (!V)
            {
                return;
            }
            
            FString S(V);
            while (S.EndsWith(TEXT("\n")))
            {
                S.RemoveAt(S.Len() - 1);
            }
            Lines.Add(S);
        }

        /**
         * Get all captured lines and clear the internal buffer.
         */
        TArray<FString> Consume()
        {
            TArray<FString> Tmp = MoveTemp(Lines);
            Lines.Empty();
            return Tmp;
        }
    };

    // =========================================================================
    // JSON String Extraction Utility
    // =========================================================================

    /**
     * Extract all top-level JSON objects from a string that may contain
     * mixed text and JSON content.
     *
     * @param In The input string that may contain JSON objects
     * @return Array of complete top-level JSON object strings
     */
    inline TArray<FString> ExtractTopLevelJsonObjects(const FString& In)
    {
        TArray<FString> Results;
        int32 Depth = 0;
        int32 Start = INDEX_NONE;
        
        for (int32 i = 0; i < In.Len(); ++i)
        {
            const TCHAR C = In[i];
            if (C == '{')
            {
                if (Depth == 0)
                {
                    Start = i;
                }
                Depth++;
            }
            else if (C == '}')
            {
                Depth--;
                if (Depth == 0 && Start != INDEX_NONE)
                {
                    Results.Add(In.Mid(Start, i - Start + 1));
                    Start = INDEX_NONE;
                }
            }
        }
        
        return Results;
    }

    /**
     * Convert a string to its UTF-8 hexadecimal representation for debugging.
     */
    inline FString HexifyUtf8(const FString& In)
    {
        FTCHARToUTF8 Converter(*In);
        const uint8* Bytes = reinterpret_cast<const uint8*>(Converter.Get());
        int32 Len = Converter.Length();
        
        FString Hex;
        Hex.Reserve(Len * 2);
        for (int32 i = 0; i < Len; ++i)
        {
            Hex += FString::Printf(TEXT("%02x"), Bytes[i]);
        }
        
        return Hex;
    }

} // namespace McpPropertyReflection