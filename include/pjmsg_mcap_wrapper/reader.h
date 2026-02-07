/**
    @file
    @author Alexander Sherikov, Damien SIX
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
*/

#pragma once

#include "message.h"

namespace pjmsg_mcap_wrapper
{
    class PJMSG_MCAP_WRAPPER_PUBLIC Reader
    {
    protected:
        class Implementation;

    protected:
        const std::unique_ptr<Implementation> pimpl_;

    public:
        Reader();
        ~Reader();

        void initialize(const std::filesystem::path &filename, const std::string &topic_prefix);

        /// Get next pjmsg_mcap_wrapper message
        bool next(Message &message);
    };
}  // namespace pjmsg_mcap_wrapper
