#!/bin/env bash

set -e # Terminate the script when an error occurs

: ${fakeroot='.'}
: ${prefix_lib='lib'}
: ${prefix_include='include'}
: ${buildpath='build-Release'}

function err {
	echo "$@" 1>&2
	return 1
}

function verbose {
	local lvl="$1"
	shift
	echo " > ${lvl}${@}"
	"$@"
}


# Preliminar checks

if [[ ! -e "$buildpath" ]]; then
	err "Error: build directory `$buildpath` does not exist."
elif [[ ! -d "$buildpath" ]]; then
	err "Error: `$buildpath` is not a directory."
fi

if [[ -e "$fakeroot" && ! -d "$fakeroot" ]]; then
	err "Error: `$fakeroot` exists and is not a directory."
fi


# Generate variables for versioned binaries

version_M_m_p="$(head -n5 CMakeLists.txt | grep VERSION\ \" | sed -e 's/\s*VERSION "\(.\+\..\+\..\+\)"/\1/')"
version_M_m="$(echo -n "$version_M_m_p" | sed 's/\(.\+\..\+\)\..\+/\1/')"
version_M="$(echo -n "$version_M_m" | sed 's/\(.\+\)\..\+/\1/')"
[[ -z "$version_M_m_p" ]] && err 'Error: failed to detect the project'\''s version [major.minor.patch]'
[[ -z "$version_M_m" ]]   && err 'Error: failed to detect the project'\''s version [major.minor]'
[[ -z "$version_M" ]]     && err 'Error: failed to detect the project'\''s version [major]'
libapcf_so_M_m_p="$buildpath/lib-runtime/libapcf.so.$version_M_m_p"
libapcf_so_M_m_p_basename="libapcf.so.$version_M_m_p"
libapcf_so_M_m_basename="libapcf.so.$version_M_m"
libapcf_so_M_basename="libapcf.so.$version_M"
libapcf_so_basename="libapcf.so"
echo "Version: $version_M_m_p"


# Move files to the fakeroot

verbose '' install -d "$fakeroot/$prefix_include"
verbose '' install -d "$fakeroot/$prefix_lib"

verbose '' install -pt "$fakeroot/$prefix_include" "include/apcf.hpp"
verbose '' install -pt "$fakeroot/$prefix_include" "include/apcf_fwd.hpp"
verbose '' install -pt "$fakeroot/$prefix_include" "include/apcf_hierarchy.hpp"

verbose '' install -pt "$fakeroot/$prefix_lib" "$libapcf_so_M_m_p"
(
	verbose '' cd "$fakeroot/$prefix_lib"
	verbose '  ' ln -fsr "$libapcf_so_M_m_p_basename" "$libapcf_so_M_m_basename"
	verbose '  ' ln -fsr "$libapcf_so_M_m_p_basename" "$libapcf_so_M_basename"
	verbose '  ' ln -fsr "$libapcf_so_M_m_p_basename" "$libapcf_so_basename"
	verbose '  ' touch -c -r "$libapcf_so_M_m_p_basename" "$libapcf_so_M_m_basename"
	verbose '  ' touch -c -r "$libapcf_so_M_m_p_basename" "$libapcf_so_M_basename"
	verbose '  ' touch -c -r "$libapcf_so_M_m_p_basename" "$libapcf_so_basename"
)
