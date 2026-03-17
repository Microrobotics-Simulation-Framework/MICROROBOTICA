# FindOpenUSD.cmake
# Custom find module for OpenUSD (Pixar Universal Scene Description).
#
# Handles both naming conventions:
#   - Pre-24.08: libusd.so, libsdf.so, libtf.so, etc.
#   - 24.08+:    libusd_usd.so, libusd_sdf.so, libusd_tf.so, etc.
#
# Sets:
#   OpenUSD_FOUND          - TRUE if core USD is found
#   OpenUSD_INCLUDE_DIRS   - Include directories
#   OpenUSD_LIBRARIES      - All found libraries to link against
#   OpenUSD_HAS_IMAGING    - TRUE if imaging libraries are available
#
# Searches:
#   PXR_ROOT environment variable
#   Common installation paths

set(_USD_SEARCH_PATHS
    $ENV{PXR_ROOT}/include
    $ENV{PXR_ROOT}
    /opt/usd/include
    /usr/local/include
    /usr/include
)

set(_USD_LIB_PATHS
    $ENV{PXR_ROOT}/lib
    /opt/usd/lib
    /usr/local/lib
    /usr/lib
)

# Search for pxr/usd/usd/api.h as the primary indicator
find_path(OpenUSD_INCLUDE_DIR
    NAMES pxr/usd/usd/api.h
    PATHS ${_USD_SEARCH_PATHS}
    DOC "OpenUSD include directory"
)

# Helper: find a USD library with both naming conventions
function(_find_usd_lib varname libname)
    find_library(${varname}
        NAMES usd_${libname} ${libname}
        PATHS ${_USD_LIB_PATHS}
        DOC "OpenUSD ${libname} library"
    )
endfunction()

# Core libraries (required)
set(_USD_CORE_LIBS usd usdGeom sdf tf gf vt ar plug)
set(OpenUSD_LIBRARIES "")
set(_USD_CORE_FOUND TRUE)

foreach(_lib IN LISTS _USD_CORE_LIBS)
    _find_usd_lib(_USD_LIB_${_lib} ${_lib})
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_LIBRARIES ${_USD_LIB_${_lib}})
    else()
        set(_USD_CORE_FOUND FALSE)
    endif()
endforeach()

# Imaging libraries (optional)
set(_USD_IMAGING_LIBS usdImaging hd hdSt usdImagingGL)
set(OpenUSD_HAS_IMAGING TRUE)

foreach(_lib IN LISTS _USD_IMAGING_LIBS)
    _find_usd_lib(_USD_LIB_${_lib} ${_lib})
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_LIBRARIES ${_USD_LIB_${_lib}})
    else()
        set(OpenUSD_HAS_IMAGING FALSE)
    endif()
endforeach()

# Additional useful libs (optional, add if found)
set(_USD_EXTRA_LIBS kind pcp work trace js arch)
foreach(_lib IN LISTS _USD_EXTRA_LIBS)
    _find_usd_lib(_USD_LIB_${_lib} ${_lib})
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_LIBRARIES ${_USD_LIB_${_lib}})
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenUSD
    REQUIRED_VARS OpenUSD_INCLUDE_DIR _USD_CORE_FOUND
)

if(OpenUSD_FOUND)
    set(OpenUSD_INCLUDE_DIRS ${OpenUSD_INCLUDE_DIR})
    if(OpenUSD_HAS_IMAGING)
        message(STATUS "OpenUSD imaging: available (UsdImagingGL, Hydra)")
    else()
        message(STATUS "OpenUSD imaging: not available — viewport will use software fallback")
    endif()
endif()

mark_as_advanced(OpenUSD_INCLUDE_DIR OpenUSD_LIBRARIES)
