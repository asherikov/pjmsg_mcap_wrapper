/**
    @file
    @author  Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
*/

#pragma once

#include "message.h"

namespace pjmsg_mcap_wrapper
{
    class PJMSG_MCAP_WRAPPER_PUBLIC Writer
    {
    public:
        struct PJMSG_MCAP_WRAPPER_PUBLIC Parameters
        {
            enum class PJMSG_MCAP_WRAPPER_PUBLIC Compression
            {
                NONE,
                ZSTD
            } compression_ = Compression::NONE;

            Parameters(){};
        };

    protected:
        class Implementation;

    protected:
        const std::unique_ptr<Implementation> pimpl_;

    public:
        Writer();
        ~Writer();
        void initialize(
                const std::filesystem::path &filename,
                const std::string &topic_prefix,
                const Parameters &params = Parameters{});
        void flush();
        void write(const Message &message);
    };
}  // namespace pjmsg_mcap_wrapper
