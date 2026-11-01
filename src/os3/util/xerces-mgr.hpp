#pragma once

namespace os3::util
{
  class xerces_mgr
  {
  public:
    xerces_mgr();
    ~xerces_mgr();

    xerces_mgr(const xerces_mgr&)            = delete; // Disable copying
    xerces_mgr& operator=(const xerces_mgr&) = delete;
    xerces_mgr(xerces_mgr&&)                 = delete; // Disable moving (to satisfy Rule of Five)
    xerces_mgr& operator=(xerces_mgr&&)      = delete;
  };
} // namespace os3::util
