#include "os3/util/xerces-mgr.hpp"
#include "os3/util/x-str.hpp"
#include <fmt/base.h>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLException.hpp>

namespace os3::util
{
  xerces_mgr::xerces_mgr()
  {
    try
    {
      xercesc::XMLPlatformUtils::Initialize();
    }
    catch (const xercesc::XMLException& e)
    { // ugly, but it is rare
      fmt::print("Internal error: Error initializing Xerces: '{}'\n", to_utf8(e.getMessage()));
      throw;
    }
  }

  xerces_mgr::~xerces_mgr() { xercesc::XMLPlatformUtils::Terminate(); }
} // namespace os3::util
