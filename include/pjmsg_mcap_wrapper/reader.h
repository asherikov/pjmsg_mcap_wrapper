/**
    @file
    @author  Damien SIX
*/

#pragma once

#include "pjmsg_mcap_wrapper/all.h"

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
    void initialize(const std::filesystem::path& filename);
    bool read(Message& message);
    [[nodiscard]] bool hasNext() const;
  };
} // namespace pjmsg_mcap_wrapper
