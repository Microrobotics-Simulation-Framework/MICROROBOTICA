# FindOpenUSD.cmake
# Custom find module for OpenUSD (Pixar Universal Scene Description).
#
# Sets:
#   OpenUSD_FOUND          - TRUE if core USD is found
#   OpenUSD_INCLUDE_DIRS   - Include directories
#   OpenUSD_LIBRARIES      - Core libraries to link against
#   OpenUSD_HAS_IMAGING    - TRUE if imaging libraries are available
#   OpenUSD_IMAGING_LIBS   - Imaging libraries (usdImaging, etc.)
#
# Searches:
#   PXR_ROOT environment variable
#   Common installation paths

# Search for pxr/usd/usd/api.h as the primary indicator
find_path(OpenUSD_INCLUDE_DIR
    NAMES pxr/usd/usd/api.h
    PATHS
        $ENV{PXR_ROOT}/include
        $ENV{PXR_ROOT}
        /opt/usd/include
        /usr/local/include
        /usr/include
    DOC "OpenUSD include directory"
)

# Core libraries (required)
set(_USD_CORE_LIBS usd usdGeom sdf tf gf vt ar plug)
set(OpenUSD_LIBRARIES "")

foreach(_lib IN LISTS _USD_CORE_LIBS)
    find_library(_USD_LIB_${_lib}
        NAMES ${_lib}
        PATHS
            $ENV{PXR_ROOT}/lib
            /opt/usd/lib
            /usr/local/lib
            /usr/lib
        DOC "OpenUSD ${_lib} library"
    )
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_LIBRARIES ${_USD_LIB_${_lib}})
    endif()
endforeach()

# Imaging libraries (optional)
set(_USD_IMAGING_LIBS usdImaging hd hdSt)
set(OpenUSD_IMAGING_LIBS "")
set(OpenUSD_HAS_IMAGING TRUE)

foreach(_lib IN LISTS _USD_IMAGING_LIBS)
    find_library(_USD_LIB_${_lib}
        NAMES ${_lib}
        PATHS
            $ENV{PXR_ROOT}/lib
            /opt/usd/lib
            /usr/local/lib
            /usr/lib
        DOC "OpenUSD ${_lib} library"
    )
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_IMAGING_LIBS ${_USD_LIB_${_lib}})
    else()
        set(OpenUSD_HAS_IMAGING FALSE)
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenUSD
    REQUIRED_VARS OpenUSD_INCLUDE_DIR OpenUSD_LIBRARIES
)

if(OpenUSD_FOUND)
    set(OpenUSD_INCLUDE_DIRS ${OpenUSD_INCLUDE_DIR})
    if(OpenUSD_HAS_IMAGING)
        message(STATUS "OpenUSD imaging libraries found")
        list(APPEND OpenUSD_LIBRARIES ${OpenUSD_IMAGING_LIBS})
    else()
        message(STATUS "OpenUSD imaging libraries not found — viewport will use software fallback")
    endif()
endif()

mark_as_advanced(OpenUSD_INCLUDE_DIR OpenUSD_LIBRARIES OpenUSD_IMAGING_LIBS)
