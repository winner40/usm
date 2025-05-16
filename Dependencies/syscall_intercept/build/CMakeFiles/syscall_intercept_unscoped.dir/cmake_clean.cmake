file(REMOVE_RECURSE
  "libsyscall_intercept_unscoped.a"
  "libsyscall_intercept_unscoped.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM C)
  include(CMakeFiles/syscall_intercept_unscoped.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
