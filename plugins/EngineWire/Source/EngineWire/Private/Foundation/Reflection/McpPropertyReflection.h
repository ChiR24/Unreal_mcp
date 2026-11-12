#pragma once

#include "CoreMinimal.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace McpPropertyReflection
{
ENGINEWIRE_API TSharedPtr<FJsonValue> ExportPropertyToJsonValue(void* TargetContainer, FProperty* Property);
// The same, or the property's export text when it has no JSON form (a delegate): a read never comes back empty.
ENGINEWIRE_API TSharedPtr<FJsonValue> ExportPropertyToJsonValueOrText(void* TargetContainer, FProperty* Property);
// The echo of a write: a struct, or an array of them, shows its editable fields only (see the .cpp).
ENGINEWIRE_API TSharedPtr<FJsonValue> ExportWrittenValueToJson(void* TargetContainer, FProperty* Property);
ENGINEWIRE_API TSharedPtr<FJsonObject> ExportPropertiesToJson(UObject* Object, const TArray<FName>& PropertyNames);
// A broad export walks every reflected property, so a large CDO answered with
// a payload the gateway then refused as RESULT_TOO_LARGE - advice the caller
// cannot act on, because inspect_cdo has no narrowing parameter other than the
// targeted propertyNames path this deliberately leaves alone.
static constexpr int32 McpMaxBoundedExportProperties = 200;
ENGINEWIRE_API TSharedPtr<FJsonObject> ExportObjectToJsonBounded(UObject* Object, bool bIncludeTransient = false, int32 MaxProperties = McpMaxBoundedExportProperties);
ENGINEWIRE_API bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError);
ENGINEWIRE_API FString GetPropertyTypeName(FProperty* Property);
ENGINEWIRE_API FString GetPropertyValueAsString(UObject* Object, FProperty* Property);
ENGINEWIRE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp);

// False with OutError ("<Label> must be between 1 and 1024, the engine's limit; got 0") when Number is outside the
// ClampMin and ClampMax the Details panel holds Property to; a property with neither takes any value. The engine
// checks them nowhere else, so a value written past them is stored as sent.
inline bool CheckRange(const FProperty* Property, double Number, const FString& Label, FString& OutError)
{
    const FString Min = Property->GetMetaData(TEXT("ClampMin"));
    const FString Max = Property->GetMetaData(TEXT("ClampMax"));
    if ((Min.IsEmpty() || Number >= FCString::Atod(*Min)) && (Max.IsEmpty() || Number <= FCString::Atod(*Max)))
    {
        return true;
    }
    const FString Range = Min.IsEmpty() ? TEXT("at most ") + Max
                        : Max.IsEmpty() ? TEXT("at least ") + Min
                                        : FString::Printf(TEXT("between %s and %s"), *Min, *Max);
    OutError = FString::Printf(TEXT("%s must be %s, the engine's limit; got %g"), *Label, *Range, Number);
    return false;
}

}
