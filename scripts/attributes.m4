# Functions to check for attributes support in compiler

AC_DEFUN([CC_ATTRIBUTE_INTERNAL], [
	AC_CACHE_CHECK([if compiler supports __attribute__((visibility("internal")))],
		[cc_cv_attribute_internal],
		[AC_COMPILE_IFELSE([AC_LANG_SOURCE([
			void __attribute__((visibility("internal"))) internal_function() { }
			])],
			[cc_cv_attribute_internal=yes],
			[cc_cv_attribute_internal=no])
		])
	
	if test "x$cc_cv_attribute_internal" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_INTERNAL], 1, [Define this if the compiler supports the internal visibility attribute])
		$1
	else
		true
		$2
	fi
])

AC_DEFUN([CC_ATTRIBUTE_VISIBILITY], [
        AC_LANG_PUSH(C++)

        tmp_CXXFLAGS=$CXXFLAGS
        CXXFLAGS="$CXXFLAGS -fvisibility=hidden"

	AC_CACHE_CHECK([if compiler supports __attribute__((visibility("default")))],
		[cc_cv_attribute_visibility],
		[AC_COMPILE_IFELSE([AC_LANG_SOURCE([
			void __attribute__((visibility("default"))) visibility_function() { }
			])],
			[cc_cv_attribute_visibility=yes],
			[cc_cv_attribute_visibility=no])
		])
	
        CXXFLAGS=$tmp_CXXFLAGS
        AC_LANG_POP(C++)

	if test "x$cc_cv_attribute_visibility" = "xyes"; then
		AC_DEFINE([SUPPORT_ATTRIBUTE_VISIBILITY], 1, [Define this if the compiler supports the visibility attributes.])
                CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
		$1
	else
                true
		$2
	fi

])
