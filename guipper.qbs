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
            'src/JPbox/JPboxgroup.cpp',
            'src/JPbox/JPboxgroup.h',
            'src/JPbox/jp_box.cpp',
            'src/JPbox/jp_box.h',
            'src/JPbox/jp_box_cam.cpp',
            'src/JPbox/jp_box_cam.h',
            'src/JPbox/jp_box_framedifference.cpp',
            'src/JPbox/jp_box_framedifference.h',
            'src/JPbox/jp_box_image.cpp',
            'src/JPbox/jp_box_image.h',
            'src/JPbox/jp_box_ndi.cpp',
            'src/JPbox/jp_box_ndi.h',
            'src/JPbox/jp_box_preset.cpp',
            'src/JPbox/jp_box_preset.h',
            'src/JPbox/jp_box_shader.cpp',
            'src/JPbox/jp_box_shader.h',
            'src/JPbox/jp_box_spout.cpp',
            'src/JPbox/jp_box_spout.h',
            'src/JPbox/jp_box_video.cpp',
            'src/JPbox/jp_box_video.h',
            'src/JPgui/jp_bang.cpp',
            'src/JPgui/jp_bang.h',
            'src/JPgui/jp_complexslider.cpp',
            'src/JPgui/jp_complexslider.h',
            'src/JPgui/jp_controller.cpp',
            'src/JPgui/jp_controller.h',
            'src/JPgui/jp_knob.cpp',
            'src/JPgui/jp_knob.h',
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
            'src/JPutils/jp_parametergroup.cpp',
            'src/JPutils/jp_parametergroup.h',
            'src/main.cpp',
            'src/ofApp.cpp',
            'src/ofApp.h',
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
