// entity.hpp — CS2 entity model (bridges to entities.hpp for ODR safety).
//
// NOTE: This header is DEPRECATED in favor of entities.hpp which provides
// the same interface with the same namespace. Including both headers will
// cause ODR violations. Use entities.hpp for new code.
//
// Reference entities.hpp for the full interface.

#pragma once

#include "real/cs2/entities.hpp"

// The types and functions previously declared here are now in entities.hpp.
// This header exists solely to prevent compilation errors in code that
// includes "real/cs2/entity.hpp". It re-exports entities.hpp.
