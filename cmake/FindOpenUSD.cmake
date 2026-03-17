# FindOpenUSD.cmake
# Custom find module for OpenUSD (Pixar Universal Scene Description).
#
# Sets:
#   OpenUSD_FOUND          - TRUE if OpenUSD is found
#   OpenUSD_INCLUDE_DIRS   - Include directories
#   OpenUSD_LIBRARIES      - Libraries to link against
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
        /usr/local/include
        /usr/include
        /opt/usd/include
        /opt/openusd/include
    DOC "OpenUSD include directory"
)

# Find key libraries
set(_USD_LIBS usd usdGeom usdImaging sdf tf gf vt ar plug)
set(OpenUSD_LIBRARIES "")

foreach(_lib IN LISTS _USD_LIBS)
    find_library(_USD_LIB_${_lib}
        NAMES ${_lib}
        PATHS
            $ENV{PXR_ROOT}/lib
            /usr/local/lib
            /usr/lib
            /opt/usd/lib
            /opt/openusd/lib
        DOC "OpenUSD ${_lib} library"
    )
    if(_USD_LIB_${_lib})
        list(APPEND OpenUSD_LIBRARIES ${_USD_LIB_${_lib}})
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenUSD
    REQUIRED_VARS OpenUSD_INCLUDE_DIR OpenUSD_LIBRARIES
)

if(OpenUSD_FOUND)
    set(OpenUSD_INCLUDE_DIRS ${OpenUSD_INCLUDE_DIR})
endif()

mark_as_advanced(OpenUSD_INCLUDE_DIR OpenUSD_LIBRARIES)
