// RiftSerializer/include/RiftSerializer/RiftSerializer.h
//
// This is the master header for the RiftSerializer library.
// Including this file will bring in all core components for building
// and accessing serialized RiftSerializer data structures.

#pragma once

// Core utilities and definitions
#include "include/Common/Common.h"

// Fundamental binary format types (headers, offset table entries)
#include "include/Types/Types.h"

// Type traits for serialization behavior (POD, fixed-size, variable-size)
#include "Include/Traits/Traits.h"

// Zero-copy accessor templates and view classes
#include "include/Accessor/Accessor.h"

// Runtime buffer builder for serialization
#include "include/Builder/Builder.h"

// Note: Generated schema headers (e.g., RiftSerializer/Generated/Entity_State.h)
// are separate and should be included individually as needed, or through a
// central generated "all_schemas.h" if your engine structure permits.
