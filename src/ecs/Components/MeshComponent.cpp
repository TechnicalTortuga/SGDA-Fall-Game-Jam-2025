/*
MeshComponent.cpp - Pure data MeshComponent implementation

Since MeshComponent is now a pure data struct, this file contains only
the minimal implementation needed for the GetTypeName() method.

All mesh operations (creation, modification, rendering) are handled
by dedicated systems that reference the MeshComponent by entity ID.

This maintains clean separation of data and logic in our ECS architecture.
*/

#include "MeshComponent.h"

// Pure data struct - no implementation needed
// All mesh operations moved to MeshSystem
