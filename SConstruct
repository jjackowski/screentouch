# This file is part of the Screentouch project. It is subject to the GPLv3
# license terms in the LICENSE file found in the top-level directory of this
# distribution and at
# https://github.com/jjackowski/screentouch/blob/master/LICENSE.
# No part of the Screentouch project, including this file, may be copied,
# modified, propagated, or distributed except according to the terms
# contained in the LICENSE file.
# 
# Copyright (C) 2018  Jeff Jackowski

import os
import platform

#####
# setup the build options
buildopts = Variables('localbuildconfig.py')
buildopts.Add(BoolVariable('debug', 'Produce a debugging build', True))
buildopts.Add('CCDBGFLAGS',
	'The flags to use with the compiler for debugging builds.',
	'-g -fno-common -Og')
buildopts.Add('CCOPTFLAGS',
	'The flags to use with the compiler for optimized non-debugging builds.',
	'-O2 -ffunction-sections -fno-common')
buildopts.Add('LINKDBGFLAGS',
	'The flags to use with the linker for debugging builds.',
	'')
buildopts.Add('LINKOPTFLAGS',
	'The flags to use with the linker for optimized builds.',
	'-Wl,--gc-sections')
# Currently, only Boost header files, not the library binaries, are needed.
buildopts.Add(PathVariable('BOOSTINC',
	'The directory containing the Boost header files, or "." for the system default.',
	'.')) #, PathVariable.PathAccept))
buildopts.Add(PathVariable('EVDEVINC',
	'The libevdev include path.',
	'/usr/include/libevdev-1.0', PathVariable.PathAccept))

puname = platform.uname()

#####
# create the template build environment
env = Environment(variables = buildopts,
	PSYS = puname[0].lower(),
	PARCH = puname[4].lower(),
	BOOSTABI = '',
	#CC = 'distcc armv6j-hardfloat-linux-gnueabi-gcc',
	#CXX = 'distcc armv6j-hardfloat-linux-gnueabi-g++',
	# include paths
	CPPPATH = [
		#'.',  # this path looks surprised or confused
		#'..',
		'$BOOSTINC',
		'#/.',
		'$EVDEVINC'
	],
	# options passed to C and C++ (?) compiler
	CCFLAGS = [
		# flags always used with compiler
	],
	# options passed to C++ compiler
	CXXFLAGS = [
		'-std=c++17'
	],
	# macros
	CPPDEFINES = [
	],
	# options passed to the linker; using gcc to link
	LINKFLAGS = [
		# flags always used with linker
	],
	LIBPATH = [
		'$BOOSTLIB',
	],
	LIBS = [  # required libraries
		'pthread',
		'evdev',
	]
)


#####
# Debugging build enviornment
dbgenv = env.Clone(LIBS = [ ])  # no libraries; needed for library check
dbgenv.AppendUnique(
	CCFLAGS = '$CCDBGFLAGS',
	LINKFLAGS = '$LINKDBGFLAGS',
	BINAPPEND = '-dbg',
	BUILDTYPE = 'dbg'
)

#####
# extra cleaning
if GetOption('clean'):
	Execute(Delete(Glob('*~') + [
		'config.log',
	] ))

#####
# configure the build
else:
	#####
	# Configuration for Boost libraries
	conf = Configure(dbgenv, config_h = 'BuildConfig.h',
		conf_dir = env.subst('.conf/${PSYS}-${PARCH}')) #, help=False)

# add back the libraries
dbgenv['LIBS'] = env['LIBS']

#####
# Optimized build enviornment
optenv = env.Clone()
optenv.AppendUnique(
	CCFLAGS = '$CCOPTFLAGS',
	LINKFLAGS = '$LINKOPTFLAGS',
	BINAPPEND = '',
	BUILDTYPE = 'opt'
)

envs = [ dbgenv, optenv ]

for env in envs:
	# build code
	code = SConscript('SConscript', exports = 'env', duplicate=0,
		variant_dir = env.subst('bin/${PSYS}-${PARCH}-${BUILDTYPE}'))
	env.Clean(code, env.subst('bin/${PSYS}-${PARCH}-${BUILDTYPE}'))
	Alias(env['BUILDTYPE'], code)

Default('dbg')

#####
# setup help text for the build options
Help(buildopts.GenerateHelpText(dbgenv))
#if GetOption('help'):
#	print 'Build target aliases:'
#	print '  libs-dbg    - All libraries; debugging build. This is the default.'
#	print '  libs        - Same as libs-dbg'
#	print '  samples-dbg - All sample programs; debugging build.'
#	print '  samples     - Same as samples-dbg.'
#	print '  tests-dbg   - All unit test programs; debugging build.'
#	print '  tests       - Same as tests-dbg.'
