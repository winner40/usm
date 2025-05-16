file(REMOVE_RECURSE
  "libsyscall_intercept.a"
  "libsyscall_intercept.pdb"
  "syscall_intercept_scoped.o"
  "syscall_intercept_unscoped.o"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/syscall_intercept_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
