add_custom_target(mage-all ALL)

add_custom_target(mage)
add_dependencies(mage-all mage)

add_custom_target(mage-archives)
add_dependencies(mage mage-archives)

add_custom_target(mage-bitcode-libraries)
add_dependencies(mage mage-bitcode-libraries)

add_subdirectory(Foo)
