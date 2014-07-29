# Once done these will be defined:
#
#  ZNC_FOUND
#  ZNC_INCLUDE_DIRS
#  ZNC_LIBRARIES
#  ZNC_DEFINITIONS

if(ZNC_INCLUDE_DIRS AND ZNC_LIBRARIES AND ZNC_DEFINITIONS)
	set(ZNC_FOUND TRUE)
else()
	find_package(PkgConfig QUIET)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(ZNC_PKG QUIET znc)
	endif()

	find_path(ZNC_INCLUDE_DIR
		NAMES
			znc/zncconfig.h
			znc/main.h
			znc/Modules.h
		HINTS
			${ZNC_PKG_INCLUDE_DIRS})

	find_library(ZNC_LIB
		NAMES
			znc
		HINTS
			${ZNC_PKG_LIBRARY_DIRS})

	if(NOT ZNC_LIB)
		unset(ZNC_LIB CACHE)
	endif()

	if(NOT ZNC_PKG_CFLAGS_OTHER)
		set(ZNC_PKG_CFLAGS_OTHER "-Wall -W -Wno-unused-parameter -Woverloaded-virtual -Wshadow -pthread -fvisibility=hidden -fPIC -include znc/zncconfig.h")
	endif()

	set(ZNC_DEFINITIONS ${ZNC_PKG_CFLAGS_OTHER} CACHE STRING "ZNC module compile options")
	set(ZNC_INCLUDE_DIRS ${ZNC_INCLUDE_DIR} CACHE PATH "ZNC include dir")
	set(ZNC_LIBRARIES ${ZNC_LIB} CACHE STRING "ZNC module library (optional)")

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(ZNC DEFAULT_MSG
		ZNC_INCLUDE_DIR ZNC_DEFINITIONS)
	mark_as_advanced(ZNC_INCLUDE_DIR ZNC_LIB)
endif()
