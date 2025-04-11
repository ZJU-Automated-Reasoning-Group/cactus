# Configuration script for APRON
# The "./configure" does not set all the variables properly (e.g. MLGMPIDL_PREFIX)
#
# Variables to define:
#
#     APRON_SOURCEDIR       Source directory of apron (contains "Makefile.config.model")
#     APRON_INSTALL_PREFIX  Installation prefix of apron
#     APRON_HAS_SHARED      Set to 1 if shared library is enabled, empty otherwise
#     PPL_INSTALL_PREFIX    Installation prefix of PPL
#     PPL_ENABLED           Set to 1 if PPL is enabled, empty otherwise

file(READ "${APRON_SOURCEDIR}/Makefile.config.model" MAKEFILE_CONFIG)

# file(READ) reads the file as one single string (safer way to deal with ';' characters...)
# so we cannot use ^ (line beginning) in regexes, but must search for '\n' instead

string(REGEX REPLACE "\nAPRON_PREFIX *= *[^\n]*\n" "\nAPRON_PREFIX=${APRON_INSTALL_PREFIX}\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nPPL_PREFIX *= *[^\n]*\n" "\nPPL_PREFIX=${PPL_INSTALL_PREFIX}\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nMLGMPIDL_PREFIX *= *[^\n]*\n" "\nMLGMPIDL_PREFIX=${APRON_INSTALL_PREFIX}\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\n *#? *HAS_PPL *= *[^\n]*\n" "\nHAS_PPL=${PPL_ENABLED}\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nOCAMLFIND *= *[^\n]*\n" "\nOCAMLFIND=\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nHAS_SHARED *= *[^\n]*\n" "\nHAS_SHARED=${APRON_HAS_SHARED}\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nHAS_OCAML *= *[^\n]*\n" "\nHAS_OCAML=\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")
string(REGEX REPLACE "\nHAS_OCAMLOPT *= *[^\n]*\n" "\nHAS_OCAMLOPT=\n" MAKEFILE_CONFIG "${MAKEFILE_CONFIG}")

file(WRITE "${APRON_SOURCEDIR}/Makefile.config" "${MAKEFILE_CONFIG}")
