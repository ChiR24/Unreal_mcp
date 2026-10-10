#include "MCP/Protocol/McpJsonRpc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#include "MCP/Protocol/McpJsonRpcImageContent.h"
#include "MCP/Protocol/McpJsonRpcReplyCompaction.h"

// Image helpers live in McpJsonRpcImageContent.cpp so this translation unit stays
// under the 250 pure-line ceiling; call sites below are unqualified.
using McpJsonRpcImage::AddImageContentIfPresent;
using McpJsonRpcImage::MakeToolTextData;
using McpJsonRpcReply::MakeCompactReply;

namespace
{
// -0 is valid JSON, but a client that reads numbers as JavaScript doubles cannot round-trip it and may reject the
// whole reply (rotators come out of quaternions as -0). Every zero is written as 0, as JSON.stringify does over stdio.
struct FMcpReplyPrintPolicy : TCondensedJsonPrintPolicy<TCHAR>
{
	static void WriteDouble(FArchive* Stream, double Value)
	{
		if (Value == 0.0) WriteChar(Stream, TEXT('0'));
		else TCondensedJsonPrintPolicy<TCHAR>::WriteDouble(Stream, Value);
	}
};
}

FMcpJsonRpcRequest FMcpJsonRpc::ParseRequest(const FString& Body)
{
	FMcpJsonRpcRequest Result;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	TSharedPtr<FJsonObject> Root;

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.ErrorType = EMcpJsonRpcError::ParseError;
		return Result;  // bValid = false, Id stays null per JSON-RPC 2.0
	}

	// Extract id early so it can be echoed in InvalidRequest errors
	TSharedPtr<FJsonValue> IdField = Root->TryGetField(TEXT("id"));
	if (IdField.IsValid() && (IdField->Type == EJson::Number || IdField->Type == EJson::String))
	{
		Result.Id = IdField;
		Result.bIsNotification = false;
	}

	// Validate jsonrpc field
	FString Version;
	if (!Root->TryGetStringField(TEXT("jsonrpc"), Version) || Version != TEXT("2.0"))
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// Method is required
	if (!Root->TryGetStringField(TEXT("method"), Result.Method))
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// Validate id type if present — null/bool/array/object are invalid per MCP spec
	if (IdField.IsValid() && !Result.Id.IsValid())
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// No id field = notification
	if (!IdField.IsValid())
	{
		Result.bIsNotification = true;
	}

	// Params is optional
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj)
	{
		Result.Params = *ParamsObj;
	}

	Result.bValid = true;
	return Result;
}

FString FMcpJsonRpc::BuildResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());

	if (Result.IsValid())
	{
		Root->SetObjectField(TEXT("result"), Result);
	}
	else
	{
		Root->SetObjectField(TEXT("result"), MakeShared<FJsonObject>());
	}

	return JsonToString(Root);
}

FString FMcpJsonRpc::BuildError(const TSharedPtr<FJsonValue>& Id, int32 Code,
	const FString& Message, const TSharedPtr<FJsonObject>& Data)
{
	auto ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), Code);
	ErrorObj->SetStringField(TEXT("message"), Message);
	if (Data.IsValid())
	{
		ErrorObj->SetObjectField(TEXT("data"), Data);
	}

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
	Root->SetObjectField(TEXT("error"), ErrorObj);

	return JsonToString(Root);
}

TSharedPtr<FJsonObject> FMcpJsonRpc::BuildToolResult(
	bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Data, const FString& ErrorCode)
{
	auto Result = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> Content;

	FString Text = bSuccess ? Message
		: ErrorCode.IsEmpty() ? FString::Printf(TEXT("Error: %s"), *Message)
		: FString::Printf(TEXT("Error [%s]: %s"), *ErrorCode, *Message);
	// What a client reads is the reply without its log-only fields (McpJsonRpcReplyCompaction.h);
	// Data itself stays whole for the image block below.
	const TSharedPtr<FJsonObject> Shown = Data.IsValid() ? MakeToolTextData(MakeCompactReply(Data)) : nullptr;
	// Success and failure both carry the receipt as text: a failure's errorCode,
	// suggestions[], executable nextCall and partial results live in Data, and a
	// client that renders only the text block needs them to recover. The first
	// line already says the message and the error code, so the copy below does not.
	if (Shown.IsValid())
	{
		TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->Values = Shown->Values;
		FString Field;
		if (Body->TryGetStringField(TEXT("message"), Field) && Field.Equals(Message, ESearchCase::CaseSensitive)) Body->RemoveField(TEXT("message"));
		if (Body->TryGetStringField(TEXT("errorCode"), Field) && Field.Equals(ErrorCode, ESearchCase::CaseSensitive)) Body->RemoveField(TEXT("errorCode"));
		Text += TEXT("\n\n") + JsonToString(Body);
	}

	auto TextContent = MakeShared<FJsonObject>();
	TextContent->SetStringField(TEXT("type"), TEXT("text"));
	TextContent->SetStringField(TEXT("text"), Text);
	Content.Add(MakeShared<FJsonValueObject>(TextContent));
	AddImageContentIfPresent(Data, Content);

	Result->SetArrayField(TEXT("content"), Content);
	if (Shown.IsValid())
	{
		// Same omission the text block gets. The image already travels once, as its own
		// image content block; repeating the base64 here shipped it twice and left every
		// client that renders structuredContent showing megabytes of unreadable text
		// beside the picture. The placeholder keeps the field's shape intact.
		Result->SetObjectField(TEXT("structuredContent"), Shown);
	}
	Result->SetBoolField(TEXT("isError"), !bSuccess);

	return Result;
}

FString FMcpJsonRpc::BuildProgressNotification(
	const TSharedPtr<FJsonValue>& ProgressToken, float Progress, float Total, const FString& Message)
{
	auto Params = MakeShared<FJsonObject>();
	if (ProgressToken.IsValid())
	{
		Params->SetField(TEXT("progressToken"), ProgressToken);
	}
	else
	{
		Params->SetStringField(TEXT("progressToken"), FString());
	}
	Params->SetNumberField(TEXT("progress"), static_cast<double>(Progress));
	Params->SetNumberField(TEXT("total"), static_cast<double>(Total));
	if (!Message.IsEmpty())
	{
		Params->SetStringField(TEXT("message"), Message);
	}

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetStringField(TEXT("method"), TEXT("notifications/progress"));
	Root->SetObjectField(TEXT("params"), Params);

	return JsonToString(Root);
}

FString FMcpJsonRpc::BuildNotification(
	const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetStringField(TEXT("method"), Method);
	Root->SetObjectField(TEXT("params"),
		Params.IsValid() ? Params : MakeShared<FJsonObject>());
	return JsonToString(Root);
}

FString FMcpJsonRpc::JsonToString(const TSharedPtr<FJsonObject>& Obj)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, FMcpReplyPrintPolicy>> Writer =
		TJsonWriterFactory<TCHAR, FMcpReplyPrintPolicy>::Create(&Output);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Output;
}
