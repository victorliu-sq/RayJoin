function(check_required_package package_name)
  find_package(${package_name} REQUIRED)

  if (NOT ${package_name}_FOUND)
    message(FATAL_ERROR "${package_name} not found, please install the ${package_name} library")
  else ()
    message("${package_name} is found.")
  endif ()
endfunction()