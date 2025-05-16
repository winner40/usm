file(REMOVE_RECURSE
  "libhook_test_preload_with_static.pdb"
  "libhook_test_preload_with_static.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/hook_test_preload_with_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
