// McpPinTypeParser.cpp — implementation. Filled progressively in Tasks 3-6.

#include "MCP/McpPinTypeParser.h"

#if WITH_EDITOR

#include "EdGraphSchema_K2.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Color.h"

bool FMcpPinTypeParser::Parse(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutError)
{
	const FString T = TypeString.TrimStartAndEnd();
	if (T.IsEmpty()) { OutError = TEXT("Empty type string"); return false; }

	OutPinType = FEdGraphPinType();
	OutPinType.ContainerType = EPinContainerType::None;

	// 1. Keyword scalar
	if (T == TEXT("Bool"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; return true; }
	if (T == TEXT("Byte"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;    return true; }
	if (T == TEXT("Int"))    { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;     return true; }
	if (T == TEXT("Int64"))  { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;   return true; }
	if (T == TEXT("Float"))  { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
	                           OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float; return true; }
	if (T == TEXT("Double")) { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
	                           OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double; return true; }
	if (T == TEXT("String")) { OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;  return true; }
	if (T == TEXT("Name"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;    return true; }
	if (T == TEXT("Text"))   { OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;    return true; }

	// 2. Builtin structs (TBaseStructure)
	auto MakeStruct = [&](UScriptStruct* SS) {
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = SS;
	};
	if (T == TEXT("Vector"))      { MakeStruct(TBaseStructure<FVector>::Get());      return true; }
	if (T == TEXT("Vector2D"))    { MakeStruct(TBaseStructure<FVector2D>::Get());    return true; }
	if (T == TEXT("Rotator"))     { MakeStruct(TBaseStructure<FRotator>::Get());     return true; }
	if (T == TEXT("Transform"))   { MakeStruct(TBaseStructure<FTransform>::Get());   return true; }
	if (T == TEXT("Color"))       { MakeStruct(TBaseStructure<FColor>::Get());       return true; }
	if (T == TEXT("LinearColor")) { MakeStruct(TBaseStructure<FLinearColor>::Get()); return true; }

	OutError = FString::Printf(TEXT("Unknown type keyword: '%s' (object/container/userdef parsing not yet implemented)"), *T);
	return false;
}

FString FMcpPinTypeParser::Serialize(const FEdGraphPinType& PinType, FString& OutWarning)
{
	OutWarning = TEXT("Not yet implemented");
	return TEXT("");
}

#endif // WITH_EDITOR
