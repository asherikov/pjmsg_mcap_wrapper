/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

namespace pjmsg_mcap_wrapper
{
    class Message::Implementation
    {
    public:
        plotjuggler_msgs::msg::StatisticsNames names_;
        plotjuggler_msgs::msg::StatisticsValues values_;

        bool version_updated_;

    public:
        Implementation();
        void setVersion(const uint32_t version);
    };
}  // namespace pjmsg_mcap_wrapper
