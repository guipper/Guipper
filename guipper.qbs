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
        property bool kinectAvailable: {
            if (!qbs.targetOS.contains("linux")) return false;
            var process = new Process();
            try { return process.exec("pkg-config", ["--exists", "freenect2"], false) === 0; }
            catch (error) { return false; }
            finally { process.close(); }
        }

        Properties {
            condition: qbs.targetOS.contains("linux")
            cpp.dynamicLibraries: (outer || []).concat(["pulse", "pangocairo-1.0", "pango-1.0", "cairo", "fontconfig"])
        }
        Group {
            condition: qbs.targetOS.contains("macos")
            files: ['src/JPutils/macos/jp_screen_capture_macos.mm']
        }
        Properties {
            condition: qbs.targetOS.contains("macos")
            cpp.frameworks: (outer || []).concat(["CoreMedia", "CoreVideo"])
            cpp.weakFrameworks: (outer || []).concat(["ScreenCaptureKit"])
        }
        files: [
            'src/defines.h',
            'src/fix_line.cpp',
            'src/JPbox/jp_box.cpp',
            'src/JPbox/jp_box.h',
            'src/JPbox/jp_box_cam.cpp',
            'src/JPbox/jp_box_cam.h',
            'src/JPbox/jp_box_youtube.cpp',
            'src/JPbox/jp_box_text.cpp',
            'src/JPbox/jp_box_analogcolor.cpp',
            'src/JPbox/jp_box_youtube.h',
            'src/JPbox/jp_box_text.h',
            'src/JPbox/jp_box_analogcolor.h',
            'src/JPbox/jp_analogcolor_shaders.h',
            'src/JPgui/jp_hand_panel.cpp',
            'src/JPgui/jp_hand_panel.h',
            'src/JPutils/jp_hand_tracking.cpp',
            'src/JPutils/jp_hand_tracking.h',
            'src/JPutils/jp_crowd_tracking.h',
            'src/JPutils/jp_youtube.cpp',
            'src/JPutils/jp_text_render.cpp',
            'src/JPutils/jp_youtube.h',
            'src/JPutils/jp_text_render.h',
            'src/JPutils/jp_text_layout.h',
            'src/JPutils/jp_image_fixture.h',
            'src/JPbox/jp_box_screen.cpp',
            'src/JPbox/jp_box_screen.h',
            'src/JPutils/jp_screen_capture.h',
            'src/JPutils/jp_capture_resize.h',
            'src/JPutils/jp_screen_capture.cpp',
            'src/JPutils/jp_screen_capture_linux.cpp',
            'src/JPutils/jp_screen_capture_windows.cpp',
            'src/JPbox/jp_box_kinectparticles.cpp',
            'src/JPbox/jp_box_kinectparticles.h',
            'src/JPbox/jp_kinect_particles.h',
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
            'src/JPbox/jp_mapping_boolean.h',
			'src/JPbox/jp_graph_walk.cpp',
			'src/JPbox/jp_graph_walk.h',
			'src/JPbox/jp_quick_image.cpp',
			'src/JPbox/jp_quick_image.h',
            'src/JPbox/jp_box_spout.cpp',
            'src/JPbox/jp_box_spout.h',
            'src/JPbox/jp_box_video.cpp',
            'src/JPbox/jp_box_video.h',
            'src/JPbox/JPboxgroup.cpp',
            'src/JPbox/JPboxgroup.h',
            'src/JPbox/JPboxgroup_mapping_advanced.cpp',
			'src/JPbox/JPboxgroup_overlays.cpp',
			'src/JPbox/JPboxgroup_quick_images.cpp',
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
        of.pkgConfigs: kinectAvailable ? ["freenect2"] : []       // list of additional system pkgs to include
        of.includePaths: []     // include search paths
        of.cFlags: []           // flags passed to the c compiler
        of.cxxFlags: []         // flags passed to the c++ compiler
        of.linkerFlags: []      // flags passed to the linker
        of.defines: kinectAvailable ? ["KINECT2"] : []          // defines are passed as -D to the compiler
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
