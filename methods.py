import os
import warnings

def get_library_path(env, name):
	return "{}/{}".format(env.lib_folder, name)

def add_shared_library(env, name, sources, **args):
	library = env.SharedLibrary(env.get_library_path(name), sources, **args)
	env.NoCache(library)
	return library

def add_library(env, name, sources, **args):
	library = env.Library(env.get_library_path(name), sources, **args)
	env.NoCache(library)
	return library

def build_thirdparty_library(env, name, search_sources, includes, defines):
	base_dir = os.path.join("#thirdparty", name)
	includes = [os.path.join(base_dir, folder) for folder in includes]

	env.Append(CPPDEFINES=defines)
	env.Append(CPPPATH=includes)

	sources = []
	for src in search_sources:
		sources += env.Glob(os.path.join(base_dir, src))

	lib = env.add_library(name, sources)
	env.Prepend(LIBS=[lib])

def init_env(env, lib_folder):
	env.__class__.add_shared_library = add_shared_library
	env.__class__.add_library = add_library
	env.__class__.build_thirdparty_library = build_thirdparty_library
	env.__class__.get_library_path = get_library_path
	env.lib_folder = lib_folder


