// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabListing
{
/**
 * The page-side function that reads what a listing says about itself: publisher, category, rating,
 * licenses, price and publication date.
 *
 * One definition, interpolated into the listing details and the search, so a search hit and its details
 * report the same fact the same way. It reads only fields the listing already carries and never fetches
 * anything, and what it writes is labels and numbers: no URL and no account field.
 */
const TCHAR* Script();
} // namespace McpFabListing
