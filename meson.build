project('grapher', ['cpp'],
	default_options : [
		'cpp_std=c++17',
	]
)


# ###################
comp = meson.get_compiler('cpp').get_id()
opt = get_option('optimization')

args = []
largs = []

if comp != 'msvc'
    args += [
		'-Wall', # warnings can be nice
		
		'-Wno-unused-variable',      # i want to be able to not use variables
		'-Wno-unused-but-set-variable',
		'-Wno-unused-function',      # i want to be able to not use functions
		'-Wno-tautological-compare', # i want to be able to disable ifs
		
		'-Wno-sign-compare', # appears in stb image
		
		'-Wno-non-template-friend', # not sure what this is for, but my code works fine with this warning
		
		# need Win7 support for some APIs (currently tracy)
		'-DWINVER=0x0601',
		'-D_WIN32_WINNT=0x0601',
	]
	
	if opt != '0'
		args += [
			'-msse2',
			#'-ffast-math', does not allow use of NaNs and Infs, which I commonly use
			'-fno-math-errno',
			
			'-fwhole-program',
			'-ffunction-sections',
		]
		largs += [
			'-Wl,-gc-sections', # does not work, gcc exe still 2MB for no reason
		]
	endif
endif

# ###################

com = '../common'

glfw_inc = com/ 'glfw-3.3.5.bin.WIN64/include'
glfw_lib = com/ 'glfw-3.3.5.bin.WIN64'/ (comp == 'msvc' ? 'lib-mingw-w64/libglfw3dll.a' : 'lib-vc2019/glfw3dll.lib')

common_incdirs = [
	com,
	com/ 'dear_imgui',
	com/ 'dear_imgui_custom',
	com/ 'kisslib/tracy',
	glfw_inc,
]

common_sources = [
	com/ 'kisslib/string.cpp',
	com/ 'kisslib/file_io.cpp',
	com/ 'kisslib/random.cpp',
	com/ 'kisslib/timer.cpp',
	com/ 'kisslib/read_directory.cpp',
	com/ 'kisslib/stb_image.cpp',
	com/ 'kisslib/stb_truetype.cpp',
	com/ 'kisslib/stb_rect_pack.cpp',
	
	com/ 'glad/glad.c',
	
	com/ 'opengl_text.cpp',
	com/ 'opengl.cpp',
	com/ 'engine.cpp',
	
	com/ 'dear_imgui/imgui.cpp',
	com/ 'dear_imgui/imgui_widgets.cpp',
	com/ 'dear_imgui/imgui_widgets.cpp',
	com/ 'dear_imgui/imgui_draw.cpp',
	com/ 'dear_imgui/misc/cpp/imgui_stdlib.cpp',
	com/ 'dear_imgui/imgui_demo.cpp',
	com/ 'dear_imgui_custom/imgui_impl_glfw.cpp',
	com/ 'dear_imgui_custom/imgui_impl_opengl3.cpp',
]

if get_option('tracy') == true
	add_project_arguments('-DTRACY_ENABLE', language : 'cpp')
	common_sources += com/ 'kisslib/tracy/TracyClient.cpp'
	
	largs += '-lAdvapi32'
	largs += '-lDbghelp'
	largs += '-lWs2_32'
endif

add_project_arguments('-DOPT='+ get_option('optimization'), language : 'cpp')
add_project_arguments('-DDEBUG='+ (get_option('debug') ? '1':'0'), language : 'cpp')

common_dep = declare_dependency(
	sources             : common_sources,
	include_directories : common_incdirs,
)

sources = [
	'grapher.cpp',
]

exe = executable('grapher',
	sources             : sources,
	objects             : glfw_lib,
	dependencies        : common_dep,
	
	cpp_args            : args,
	link_args           : largs
)

