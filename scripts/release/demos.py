"""Original MIT demo assets; generated into staging, never into bin/data."""
from pathlib import Path
import struct
import zlib

VERTEX = '''#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
void main(){gl_Position=modelViewProjectionMatrix*position;}
'''
SHADER = '''#version 150
uniform float time; // @internal
uniform vec2 resolution;
uniform vec4 audio_bands; // @internal
// Rings across the radius. Exponential, because the interesting part is the
// low end: linear spacing spends most of the slider on rings too fine to read.
uniform float density = 0.25;
// Rings per second. 0 freezes the animation, which is how you get a still.
uniform float speed = 0.16;
// 0 is the plain sine gradient, 1 is hard-edged printed rings.
uniform float sharpness = 0.0;
// How much the bass drives the brightness. 0 ignores the audio input entirely.
uniform float audio_amount = 0.4;
uniform float red = 0.8; // @color r tint
uniform float green = 0.3; // @color g tint
uniform float blue = 0.1; // @color b tint
out vec4 fragColor;
void main(){
 vec2 uv=(gl_FragCoord.xy-resolution*.5)/max(resolution.y,1.);
 float phase=(length(uv)*2.*pow(64.,density)-time*speed*2.)*6.2831853;
 // Fade to the flat mean once a ring gets thinner than a couple of pixels,
 // rather than letting it break into moire that crawls with the animation.
 // Matters at high density in the small preview and on low-res outputs.
 float wave=.5+.5*sin(phase)*(1.-smoothstep(1.,3.1415927,fwidth(phase)));
 // Squares up the sine without moving the midpoint, so sharpness 0 is exactly
 // the soft gradient and 1 is a hard ring. The flat mean above stays flat.
 float edge=mix(.5,.01,sharpness);
 wave=mix(wave,smoothstep(.5-edge,.5+edge,wave),sharpness);
 fragColor=vec4(vec3(red,green,blue)*wave*(1.-audio_amount+audio_amount*audio_bands.x),1.);
}
'''
MIX = '''#version 150
uniform sampler2DRect source;
uniform sampler2DRect overlay;
uniform float amount = 0.5;
out vec4 fragColor;
void main(){fragColor=mix(texture(source,gl_FragCoord.xy),texture(overlay,gl_FragCoord.xy),amount);}
'''
def generate(data: Path):
    shaders=data/'shaders/Getting Started'; shaders.mkdir(parents=True,exist_ok=True)
    (shaders/'rings.frag').write_text(SHADER)
    (shaders/'mix.frag').write_text(MIX)
    examples=data/'savefiles/examples'; examples.mkdir(parents=True,exist_ok=True)
    def box(name, shader, x):
        return f'<box><nombre>{name}</nombre><x>{x}</x><y>160</y><directory>shaders/Getting Started/{shader}.frag</directory><onoff>1</onoff></box>'
    rings=box('Rings','rings',150)
    (examples/'01-generative.xml').write_text('<guipper_format>1</guipper_format><activerender>0</activerender>'+rings)
    (examples/'02-audio.xml').write_text('<guipper_format>1</guipper_format><activerender>0</activerender>'+rings)
    # Original RGB test image; generated without external media or font assets.
    width,height=320,180
    scanlines=b''.join(b'\0'+b''.join(bytes((int(x*255/(width-1)),int(y*255/(height-1)),80)) for x in range(width)) for y in range(height))
    def chunk(kind,payload):
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b'')
    (data/'image').mkdir(exist_ok=True)
    (data/'image/demo-gradient.png').write_bytes(image)
    imagebox='<box><nombre>Image</nombre><x>150</x><y>400</y><directory>image/demo-gradient.png</directory><onoff>1</onoff></box>'
    mix=box('Mix','mix',450).replace('</box>','<fboslinks><source>Rings</source><overlay>Image</overlay></fboslinks></box>')
    (examples/'03-mix.xml').write_text('<guipper_format>1</guipper_format><activerender>2</activerender>'+rings+imagebox+mix)
    (data/'savefiles/data.xml').write_text((examples/'01-generative.xml').read_text())
