/**
    @file
    @author Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

#include "HeaderCdrAux.hpp"
#include "StatisticsNamesCdrAux.hpp"
#include "StatisticsValuesCdrAux.hpp"
#include "TimeCdrAux.hpp"

#define MCAP_IMPLEMENTATION
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_PUBLIC __attribute__((visibility("hidden")))
