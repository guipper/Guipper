import qbs
import qbs.Process
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import "../../../libs/openFrameworksCompiled/project/qtcreator/ofApp.qbs" as ofApp

Project{
    property string of_root: "../../.."

    ofApp {
        name: { return FileInfo.baseName(sourceDirectory) }

        files: [
            'src/defines.h',
            'src/fix_line.cpp',
            'src/JPbox/jp_box.cpp',
            'src/JPbox/jp_box.h',
            'src/JPbox/jp_box_cam.cpp',
            'src/JPbox/jp_box_cam.h',
            'src/JPbox/jp_box_pointercloud.cpp',
            'src/JPbox/jp_box_pointercloud.h',
            'src/JPbox/jp_box_framedifference.cpp',
            'src/JPbox/jp_box_framedifference.h',
            'src/JPbox/jp_box_image.cpp',
            'src/JPbox/jp_box_image.h',
            'src/JPbox/jp_box_kinect2.cpp',
            'src/JPbox/jp_box_kinect2.h',
            'src/JPbox/jp_box_ndi.cpp',
            'src/JPbox/jp_box_ndi.h',
            'src/JPbox/jp_box_preset.cpp',
            'src/JPbox/jp_box_preset.h',
            'src/JPbox/jp_box_sequencer.cpp',
            'src/JPbox/jp_box_sequencer.h',
            'src/JPbox/jp_box_shader.cpp',
            'src/JPbox/jp_box_shader.h',
            'src/JPbox/jp_box_shader_mapping.cpp',
            'src/JPbox/jp_box_spout.cpp',
            'src/JPbox/jp_box_spout.h',
            'src/JPbox/jp_box_video.cpp',
            'src/JPbox/jp_box_video.h',
            'src/JPbox/JPboxgroup.cpp',
            'src/JPbox/JPboxgroup.h',
            'src/JPbox/JPboxgroup_mapping_advanced.cpp',
            'src/JPgui/jp_bang.cpp',
            'src/JPgui/jp_bang.h',
            'src/JPgui/jp_complexslider.cpp',
            'src/JPgui/jp_complexslider.h',
            'src/JPgui/jp_controller.cpp',
            'src/JPgui/jp_controller.h',
            'src/JPgui/jp_exposebutton.cpp',
            'src/JPgui/jp_exposebutton.h',
            'src/JPgui/jp_button.h',
            'src/JPgui/jp_knob.cpp',
            'src/JPgui/jp_knob.h',
            'src/JPgui/jp_screen.h',
            'src/JPgui/jp_shader_editor.cpp',
            'src/JPgui/jp_shader_editor.h',
            'src/JPgui/jp_surfacestack.h',
            'src/JPgui/jp_slider.cpp',
            'src/JPgui/jp_slider.h',
            'src/JPgui/jp_toogle.cpp',
            'src/JPgui/jp_toogle.h',
            'src/JPgui/jp_tooglelist.cpp',
            'src/JPgui/jp_tooglelist.h',
            'src/JPutils/jp_constants.cpp',
            'src/JPutils/jp_constants.h',
            'src/JPutils/jp_dragobject.cpp',
            'src/JPutils/jp_dragobject.h',
            'src/JPutils/jp_fbohandler.cpp',
            'src/JPutils/jp_fbohandler.h',
            'src/JPutils/jp_fileloader.cpp',
            'src/JPutils/jp_fileloader.h',
            'src/JPutils/jp_midi_keymap.cpp',
            'src/JPutils/jp_midi_keymap.h',
            'src/JPutils/jp_audio.cpp',
            'src/JPutils/jp_audio_analyzer.cpp',
            'src/JPutils/jp_audio_analyzer.h',
            'src/JPutils/jp_audio_queue.h',
            'src/JPutils/jp_persistence_test.cpp',
            'src/JPutils/jp_persistence_test.h',
            'src/JPutils/jp_uishot.cpp',
            'src/JPutils/jp_uishot.h',
            'src/JPutils/jp_shader_globals.cpp',
            'src/JPutils/jp_shader_globals.h',
            'src/JPutils/jp_audio.h',
            'src/JPutils/jp_help_content.h',
            'src/JPutils/jp_pointer.h',
            'src/JPutils/jp_parametergroup.cpp',
            'src/JPutils/jp_parametergroup.h',
            'src/JPutils/jp_textfield.h',
            'src/JPutils/jp_tooltip.h',
            'src/JPutils/TransitionSR.cpp',
            'src/JPutils/TransitionSR.h',
            'src/main.cpp',
            'src/ofApp.cpp',
            'src/ofApp.h'
        ]

        of.addons: [
            'ofxNDI',
            'ofxOsc',
        ]

        // additional flags for the project. the of module sets some
        // flags by default to add the core libraries, search paths...
        // this flags can be augmented through the following properties:
        of.pkgConfigs: []       // list of additional system pkgs to include
        of.includePaths: []     // include search paths
        of.cFlags: []           // flags passed to the c compiler
        of.cxxFlags: []         // flags passed to the c++ compiler
        of.linkerFlags: []      // flags passed to the linker
        of.defines: []          // defines are passed as -D to the compiler
                                // and can be checked with #ifdef or #if in the code
        of.frameworks: []       // osx only, additional frameworks to link with the project
        of.staticLibraries: []  // static libraries
        of.dynamicLibraries: [] // dynamic libraries

        // other flags can be set through the cpp module: http://doc.qt.io/qbs/cpp-module.html
        // eg: this will enable ccache when compiling
        //
        // cpp.compilerWrapper: 'ccache'

        Depends{
            name: "cpp"
        }

        // common rules that parse the include search paths, core libraries...
        Depends{
            name: "of"
        }

        // dependency with the OF library
        Depends{
            name: "openFrameworks"
        }
    }

    property bool makeOF: true  // use makfiles to compile the OF library
                                // will compile OF only once for all your projects
                                // otherwise compiled per project with qbs
    

    property bool precompileOfMain: false  // precompile ofMain.h
                                           // faster to recompile when including ofMain.h 
                                           // but might use a lot of space per project

    references: [FileInfo.joinPaths(of_root, "/libs/openFrameworksCompiled/project/qtcreator/openFrameworks.qbs")]
}
