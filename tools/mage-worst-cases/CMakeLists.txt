add_mage_tool(
  mage-worst-cases
  SRCS
    mage-worst-cases.cpp
  LINK_LIBRARIES
    Mage::LLVMSupport
    Mage::MPFR
)
