/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Sink class.
*/

#pragma once

#include <filesystem>
#include <vector>

#define PJMSG_MCAP_WRAPPER_PUBLIC __attribute__((visibility("default")))

namespace pjmsg_mcap_wrapper
{
  class PJMSG_MCAP_WRAPPER_PUBLIC Message
  {
  public:
    class Implementation;

  public:
    const std::unique_ptr<Implementation> pimpl_;

  public:
    Message();
    ~Message();

    std::vector<std::string>& names();
    std::vector<double>& values();
    std::string& name(const std::size_t index);
    double& value(const std::size_t index);

    void bumpVersion();
    void setVersion(const uint32_t version);
    [[nodiscard]] uint32_t getVersion() const;

    void setStamp(const uint64_t timestamp);
    [[nodiscard]] uint64_t getStamp() const;

    void reserve(const std::size_t size);
    void resize(const std::size_t size);
    [[nodiscard]] std::size_t size() const;
  };

  class PJMSG_MCAP_WRAPPER_PUBLIC Writer
  {
  protected:
    class Implementation;

  protected:
    const std::unique_ptr<Implementation> pimpl_;

  public:
    Writer();
    ~Writer();
    void initialize(const std::filesystem::path& filename, const std::string& topic_prefix);
    void flush();
    void write(const Message& message);
  };
} // namespace pjmsg_mcap_wrapper
