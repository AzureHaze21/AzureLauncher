#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclure les en-têtes Windows rarement utilisés
// Fichiers d'en-tête Windows
#include <windows.h>

#include "../Blackbone/src/BlackBone/Process/Process.h"
#include "../Blackbone/src/BlackBone/Process/RPC/RemoteFunction.hpp"
#include "LauncherEmulator.hpp"
#include "Hook.h"
#include "Utils.hpp"