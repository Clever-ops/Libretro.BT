CONFIG_DEFINES=''
INCLUDE_DIRS=''
LIBRARY_DIRS=''
MAKEFILE_DEFINES=''
PKG_CONF_USED=''

ASFLAGS="${ASFLAGS:-}"
CFLAGS="${CFLAGS:-}"
CXXFLAGS="${CXXFLAGS:-}"
LDFLAGS="${LDFLAGS:-}"
PREFIX="${PREFIX:-/usr/local}"
SHARE_DIR="${SHARE_DIR:-${PREFIX}/share}"

# add_define:
# $1 = MAKEFILE or CONFIG
# $2 = define
# $3 = value
add_define()
{ eval "${1}_DEFINES=\"\${${1}_DEFINES} $2=$3\""; }

# add_dirs:
# $1 = INCLUDE or LIBRARY
# $@ = include or library paths
add_dirs()
{	ADD="$1"; LINK="${1%"${1#?}"}"; shift
	while [ $# -gt 0 ]; do
		eval "${ADD}_DIRS=\"\${${ADD}_DIRS} -${LINK}${1}\""
		shift
	done
	eval "${ADD}_DIRS=\"\${${ADD}_DIRS# }\""
}

# check_compiler:
# $1 = language
# $2 = function in lib
check_compiler()
{	if [ "$1" = cxx ]; then
		COMPILER="$CXX"
		TEMP_CODE="$TEMP_CXX"
		TEST_C="extern \"C\" { void $2(void); } int main() { $2(); }"
	else
		COMPILER="$CC"
		TEMP_CODE="$TEMP_C"
		TEST_C="void $2(void); int main(void) { $2(); return 0; }"
	fi
}

# check_enabled:
# $1 = HAVE_$1 [Disabled feature]
# $2 = USER_$2 [Enabled feature]
# $3 = lib
# $4 = feature
# $5 = enable lib when true [checked only if non-empty]
check_enabled()
{	tmpvar="$(eval "printf %s \"\$HAVE_$1\"")"
	setval="$(eval "printf %s \"\$HAVE_$2\"")"

	if [ "$tmpvar" != 'no' ]; then
		if [ "$setval" != 'no' ] && [ "${5:-}" = 'true' ]; then
			eval "HAVE_$2=yes"
		fi
		return 0
	fi

	tmpval="$(eval "printf %s \"\$USER_$2\"")"

	if [ "$tmpval" != 'yes' ]; then
		if [ "$setval" != 'no' ]; then
			eval "HAVE_$2=no"
			if [ "${5:-}" != 'true' ]; then
				die : "Notice: $4 disabled, $3 support will also be disabled."
			fi
		fi
		return 0
	fi

	die 1 "Error: $4 disabled and forced to build with $3 support."
}

# check_lib:
# Compiles a simple test program to check if a library is available.
# $1 = language
# $2 = HAVE_$2
# $3 = lib
# $4 = function in lib [checked only if non-empty]
# $5 = extralibs [checked only if non-empty]
# $6 = headers [checked only if non-empty]
# $7 = include directory [checked only if non-empty]
# $8 = critical error message [checked only if non-empty]
check_lib()
{	tmpval="$(eval "printf %s \"\$HAVE_$2\"")"
	[ "$tmpval" = 'no' ] && return 0

	check_compiler "$1" "$4"

	if [ "$4" ]; then
		ECHOBUF="Checking function $4 in ${3% }"
		if [ "$6" ]; then
			printf %s\\n "$6" "int main(void) { void *p = (void*)$4; return 0; }" > "$TEMP_CODE"
		else
			printf %s\\n "$TEST_C" > "$TEMP_CODE"
		fi
	else
		ECHOBUF="Checking existence of ${3% }"
		printf %s\\n 'int main(void) { return 0; }' > "$TEMP_CODE"
	fi

	val="$2"
	lib="$3"
	include="${7:-}"
	error="${8:-}"
	answer='no'
	eval "set -- $INCLUDE_DIRS $LIBRARY_DIRS $5 $CFLAGS $LDFLAGS $3"
	"$COMPILER" -o "$TEMP_EXE" "$TEMP_CODE" "$@" >>config.log 2>&1 && answer='yes'

	if [ "$answer" = 'yes' ] && [ "$include" ]; then
		eval "set -- $INCLUDES"
		for dir do
			[ -d "/$dir/$include" ] && { eval "${val}_CFLAGS=\"-I/$dir/$include\""; break; }
		done
		[ -z "$(eval "printf %s \"\${${val}_CFLAGS}\"")" ] && answer='no'
	fi

	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_CODE" "$TEMP_EXE"

	if [ "$answer" = 'no' ]; then
		[ "$error" ] && die 1 "$error"
		setval="$(eval "printf %s \"\$USER_$val\"")"
		if [ "$setval" = 'yes' ]; then
			die 1 "Forced to build with library $lib, but cannot locate. Exiting ..."
		fi
	else
		eval "${val}_LIBS=\"$lib\""
		PKG_CONF_USED="$PKG_CONF_USED $val"
	fi

	return 0
}

# check_pkgconf:
# If available uses $PKG_CONF_PATH to find a library.
# $1 = HAVE_$1
# $2 = package
# $3 = version [checked only if non-empty]
# $4 = critical error message [checked only if non-empty]
check_pkgconf()
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	eval "TMP_$1=\$tmpval"
	[ "$tmpval" = 'no' ] && return 0

	ECHOBUF="Checking presence of package $2"
	[ "$3" ] && ECHOBUF="$ECHOBUF >= ${3##* }"

	[ "$PKG_CONF_PATH" = "none" ] && {
		eval "HAVE_$1=no"
		eval "${1#HAVE_}_VERSION=0.0"
		printf %s\\n "$ECHOBUF ... no"
		return 0
	}

	val="$1"
	pkg="$2"
	err="$4"
	answer='no'
	version='no'

	eval "set -- ${3:-0.0}"
	for ver do
		if $PKG_CONF_PATH --atleast-version="$ver" "$pkg"; then
			answer='yes'
			version="$("$PKG_CONF_PATH" --modversion "$pkg")"
			eval "${val}_CFLAGS=\"$("$PKG_CONF_PATH" "$pkg" --cflags)\""
			eval "${val}_LIBS=\"$("$PKG_CONF_PATH" "$pkg" --libs)\""
			eval "${val#HAVE_}_VERSION=\"$ver\""
			break
		fi
	done

	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$ECHOBUF ... $version"
	if [ "$answer" = 'no' ]; then
		[ "$err" ] && die 1 "$err"
		setval="$(eval "printf %s \"\$USER_$val\"")"
		if [ "$setval" = 'yes' ]; then
			die 1 "Forced to build with package $pkg, but cannot locate. Exiting ..."
		fi
	else
		PKG_CONF_USED="$PKG_CONF_USED $val"
	fi
}

# check_header:
# $1 = HAVE_$1
# $@ = header files
check_header()
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	[ "$tmpval" = 'no' ] && return 0
	rm -f -- "$TEMP_C"
	val="$1"
	header="$2"
	shift
	for head do
		CHECKHEADER="$head"
		printf %s\\n "#include <$head>" >> "$TEMP_C"
	done
	printf %s\\n "int main(void) { return 0; }" >> "$TEMP_C"
	answer='no'
	eval "set -- $INCLUDE_DIRS"
	"$CC" -o "$TEMP_EXE" "$TEMP_C" "$@" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "Checking presence of header file $CHECKHEADER ... $answer"
	rm -f -- "$TEMP_C" "$TEMP_EXE"
	setval="$(eval "printf %s \"\$USER_$val\"")"
	if [ "$setval" = 'yes' ] && [ "$answer" = 'no' ]; then
		die 1 "Build assumed that $header exists, but cannot locate. Exiting ..."
	fi
}

# check_macro:
# $1 = HAVE_$1
# $2 = macro name
# $3 = header name [included only if non-empty]
check_macro()
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	[ "$tmpval" = 'no' ] && return 0
	if [ "${3}" ]; then
		ECHOBUF="Checking presence of predefined macro $2 in $3"
		header_include="#include <$3>"
	else
		ECHOBUF="Checking presence of predefined macro $2"
		header_include=""
	fi
	cat << EOF > "$TEMP_C"
$header_include
#ifndef $2
#error $2 is not defined
#endif
int main(void) { return 0; }
EOF
	answer='no'
	val="$1"
	macro="$2"
	eval "set -- $CFLAGS $INCLUDE_DIRS"
	"$CC" -o "$TEMP_EXE" "$TEMP_C" "$@" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_C" "$TEMP_EXE"
	setval="$(eval "printf %s \"\$USER_$val\"")"
	if [ "$setval" = 'yes' ] && [ "$answer" = 'no' ]; then
		die 1 "Build assumed that $macro is defined, but it's not. Exiting ..."
	fi
}

# check_switch:
# $1 = language
# $2 = HAVE_$2
# $3 = switch
# $4 = critical error message [checked only if non-empty]
check_switch()
{	check_compiler "$1" ''

	ECHOBUF="Checking for availability of switch $3 in $COMPILER"
	printf %s\\n 'int main(void) { return 0; }' > "$TEMP_CODE"
	answer='no'
	"$COMPILER" -o "$TEMP_EXE" "$TEMP_CODE" "$3" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$2=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_CODE" "$TEMP_EXE"
	[ "$answer" = 'no' ] && {
		[ "$4" ] && die 1 "$4"
	}
}

# check_val:
# Uses check_pkgconf to find a library and falls back to check_lib if false.
# $1 = language
# $2 = HAVE_$2
# $3 = lib
# $4 = include directory [checked only if non-empty]
# $5 = package
# $6 = version [checked only if non-empty]
# $7 = critical error message [checked only if non-empty]
# $8 = force check_lib when true [checked only if non-empty]
check_val()
{	check_pkgconf "$2" "$5" "${6:-}" "${7:-}"
	[ "$PKG_CONF_PATH" = "none" ] || [ "${8:-}" = true ] || return 0
	tmpval="$(eval "printf %s \"\$HAVE_$2\"")"
	oldval="$(eval "printf %s \"\$TMP_$2\"")"
	if [ "$tmpval" = 'no' ] && [ "$oldval" != 'no' ]; then
		eval "HAVE_$2=auto"
		check_lib "$1" "$2" "$3" '' '' '' "$4" ''
	fi
}

create_config_header()
{   outfile="$1"; shift

	printf %s\\n "Creating config header: $outfile"
	name="$(printf %s "QB_${outfile}__" | tr '.[a-z]' '_[A-Z]')"

	{	printf %s\\n "#ifndef $name" "#define $name" '' \
			"#define PACKAGE_NAME \"$PACKAGE_NAME\""

		while [ $# -gt 0 ]; do
			case "$(eval "printf %s \"\$HAVE_$1\"")" in
				'yes')
					n='0'
					c89_build="$(eval "printf %s \"\$C89_$1\"")"
					cxx_build="$(eval "printf %s \"\$CXX_$1\"")"

					if [ "$c89_build" = 'no' ]; then
						n=$(($n+1))
						printf %s\\n '#if __cplusplus || __STDC_VERSION__ >= 199901L'
					fi

					if [ "$cxx_build" = 'no' ]; then
						n=$(($n+1))
						printf %s\\n '#ifndef CXX_BUILD'
					fi

					printf %s\\n "#define HAVE_$1 1"

					while [ $n != '0' ]; do
						n=$(($n-1))
						printf %s\\n '#endif'
					done
				;;
				'no') printf %s\\n "/* #undef HAVE_$1 */";;
			esac
			shift
		done

		eval "set -- $CONFIG_DEFINES"
		for VAR do
			printf %s\\n "#define ${VAR%%=*} ${VAR#*=}"
		done

		printf %s\\n '#endif'
	} > "$outfile"
}

create_config_make()
{	outfile="$1"; shift

	printf %s\\n "Creating make config: $outfile"

	{	if [ "$HAVE_CC" = 'yes' ]; then
			printf %s\\n "CC = $CC"

			if [ "${CFLAGS}" ]; then
				printf %s\\n "CFLAGS = $CFLAGS"
			fi
		fi

		if [ "$HAVE_CXX" = 'yes' ]; then
			printf %s\\n "CXX = $CXX"

			if [ "${CXXFLAGS}" ]; then
				printf %s\\n "CXXFLAGS = $CXXFLAGS"
			fi
		fi

		printf %s\\n "WINDRES = $WINDRES" \
			"MOC = $MOC" \
			"ASFLAGS = $ASFLAGS" \
			"LDFLAGS = $LDFLAGS" \
			"INCLUDE_DIRS = $INCLUDE_DIRS" \
			"LIBRARY_DIRS = $LIBRARY_DIRS" \
			"PACKAGE_NAME = $PACKAGE_NAME" \
			"BUILD = $BUILD" \
			"PREFIX = $PREFIX"

		while [ $# -gt 0 ]; do
			case "$(eval "printf %s \"\$HAVE_$1\"")" in
				'yes')
					n='0'
					c89_build="C89_$1"
					cxx_build="CXX_$1"

					for build in "$c89_build" "$cxx_build"; do
						if [ "$(eval "printf %s \"\$$build\"")" = 'no' ]; then
							n=$(($n+1))
							printf %s\\n "ifneq (\$(${build%%_*}_BUILD),1)"
						fi
					done

					printf %s\\n "HAVE_$1 = 1"

					while [ $n != '0' ]; do
						n=$(($n-1))
						printf %s\\n 'endif'
					done
				;;
				'no') printf %s\\n "HAVE_$1 = 0";;
			esac

			case "$PKG_CONF_USED" in
				*$1*)
					FLAG="$(eval "printf %s \"\$$1_CFLAGS\"")"
					LIBS="$(eval "printf %s \"\$$1_LIBS\"")"
					[ "${FLAG}" ] && printf %s\\n "$1_CFLAGS = ${FLAG%"${FLAG##*[! ]}"}"
					[ "${LIBS}" ] && printf %s\\n "$1_LIBS = ${LIBS%"${LIBS##*[! ]}"}"
				;;
			esac
			shift
		done
		eval "set -- $MAKEFILE_DEFINES"
		for VAR do
			printf %s\\n "${VAR%%=*} = ${VAR#*=}"
		done

	} > "$outfile"
}

. qb/config.libs.sh
