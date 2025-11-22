#pragma once

// Compatibility header for legacy includes.
// The original implementation has been split into dedicated headers:
//  - <session.h>  : Session definition
//  - <server.h>   : Server definition and start_server helper
//
// New code should prefer including <session.h> or <server.h> directly.

#include <session.h>
#include <server.h>
