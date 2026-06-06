#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include <type_traits>
#include <utility>

#if WITH_EDITOR
#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) &&                               \
    (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#endif
#else
#include "SubobjectDataSubsystem.h"
#endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif

#if WITH_EDITOR && MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
namespace McpAutomationBridge {
template <typename, typename = void> struct THasK2Add : std::false_type {};

template <typename T>
struct THasK2Add<T, std::void_t<decltype(std::declval<T>().K2_AddNewSubobject(
                        std::declval<FAddNewSubobjectParams>()))>>
    : std::true_type {};

template <typename, typename = void> struct THasAdd : std::false_type {};

template <typename T>
struct THasAdd<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(
                      std::declval<FAddNewSubobjectParams>()))>>
    : std::true_type {};

template <typename, typename = void> struct THasAddTwoArg : std::false_type {};

template <typename T>
struct THasAddTwoArg<
    T, std::void_t<decltype(std::declval<T>().AddNewSubobject(
           std::declval<FAddNewSubobjectParams>(), std::declval<FText &>()))>>
    : std::true_type {};

template <typename, typename = void>
struct THandleHasIsValid : std::false_type {};

template <typename T>
struct THandleHasIsValid<T, std::void_t<decltype(std::declval<T>().IsValid())>>
    : std::true_type {};

template <typename, typename = void> struct THasRename : std::false_type {};

template <typename T>
struct THasRename<
    T, std::void_t<decltype(std::declval<T>().RenameSubobjectMemberVariable(
           std::declval<UBlueprint *>(), std::declval<FSubobjectDataHandle>(),
           std::declval<FName>()))>> : std::true_type {};

template <typename, typename = void> struct THasK2Remove : std::false_type {};

template <typename T>
struct THasK2Remove<
    T,
    std::void_t<decltype(std::declval<T>().K2_RemoveSubobject(
        std::declval<UBlueprint *>(), std::declval<FSubobjectDataHandle>()))>>
    : std::true_type {};

template <typename, typename = void> struct THasRemove : std::false_type {};

template <typename T>
struct THasRemove<T, std::void_t<decltype(std::declval<T>().RemoveSubobject(
                         std::declval<UBlueprint *>(),
                         std::declval<FSubobjectDataHandle>()))>>
    : std::true_type {};

template <typename, typename = void>
struct THasDeleteSubobject : std::false_type {};

template <typename T>
struct THasDeleteSubobject<
    T, std::void_t<decltype(std::declval<T>().DeleteSubobject(
           std::declval<const FSubobjectDataHandle &>(),
           std::declval<const FSubobjectDataHandle &>(),
           std::declval<UBlueprint *>()))>> : std::true_type {};

template <typename, typename = void> struct THasK2Attach : std::false_type {};

template <typename T>
struct THasK2Attach<
    T, std::void_t<decltype(std::declval<T>().K2_AttachSubobject(
           std::declval<UBlueprint *>(), std::declval<FSubobjectDataHandle>(),
           std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

template <typename, typename = void> struct THasAttach : std::false_type {};

template <typename T>
struct THasAttach<T, std::void_t<decltype(std::declval<T>().AttachSubobject(
                         std::declval<FSubobjectDataHandle>(),
                         std::declval<FSubobjectDataHandle>()))>>
    : std::true_type {};
} // namespace McpAutomationBridge
#endif
