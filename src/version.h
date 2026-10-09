#pragma once

// Einzige Stelle für die Firmware-Version (SemVer).
// Nach Änderung: ./build.sh  (synced README, baut, lädt hoch)
// GitHub legt ein Release nur an, wenn diese Version neu ist.

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 3
#define FW_VERSION_PATCH 1

#define FW_VERSION_STR "1.3.1"
