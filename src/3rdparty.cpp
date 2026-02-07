/**
    @file
    @author Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "3rdparty.h"

#define MCAP_IMPLEMENTATION
#pragma GCC diagnostic push
/// @todo presumably GCC bug
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <mcap/reader.hpp>
#include <mcap/writer.hpp>
#pragma GCC diagnostic pop

#include "HeaderCdrAux.ipp"
#include "StatisticsNamesCdrAux.ipp"
#include "StatisticsValuesCdrAux.ipp"
#include "TimeCdrAux.ipp"

