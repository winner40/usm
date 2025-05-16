file(REMOVE_RECURSE
  "libhook_test_preload_with_shared.pdb"
  "libhook_test_preload_with_shared.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/hook_test_preload_with_shared.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
