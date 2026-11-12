#pragma once

// OpenSSL for the bridge's TLS, included the one way that builds inside the engine: OpenSSL's ui.h declares a type
// named UI, which collides with the engine's UI namespace, and its headers want the Windows platform types.
#if WITH_SSL

#define UI OSSL_UI

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

#undef UI

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#endif
