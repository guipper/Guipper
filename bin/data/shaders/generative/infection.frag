#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speed ;
uniform float flush ; 
uniform float sc1 ; 
uniform float speed2 ;

vec2 scale(vec2 uv, float s);
float sin2(float f);
float cos2(float f);
mat2 scale(vec2 _scale);
mat2 rotate2d(float _angle);

//float noise (in vec2 st,float fase);
//vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
//vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
//vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }
//float snoise(vec2 v);
//float random (in vec2 _st);
#define PI 3.14159265359
#define TWO_PI 6.28318530718

float sm(float v1,float v2,float val){return smoothstep(v1,v2,val);}

#define octaves 8

float noise2 (in vec2 st,float fase) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    
    float fase2 = fase;
    // Four corners in 2D of a tile
    float a = sin(random(i)*fase2);
    float b =  sin(random(i + vec2(1.0, 0.0))*fase2);
    float c =  sin(random(i + vec2(0.0, 1.0))*fase2);
    float d =  sin(random(i + vec2(1.0, 1.0))*fase2);

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}
float fbm2 (in vec2 st) {
    // Initial values
    float value = 0.0;
    float amplitude = 0.8;
    float frequency = 0.;
    vec2 shift = vec2(100);
    
    mat2 rot = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    
    // Loop of octaves
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise2(st,time*speed2*3.);
        
    
        st = rot * st * 2.0 + shift;
       
        amplitude *= .5;
    }
    return value;
}
void main(void)
{   
    vec2 uv = gl_FragCoord.xy / resolution;

    float fx = resolution.x/resolution.y;
    uv.x*= fx;
    
    vec3 dib = vec3(0.0);
    
    
    float fm1 = fbm2(uv*7.0*sc1)*1+0.0;
   
    //fm1 = smoothstep(0.1,0.9,abs(fm1));
    dib += vec3(abs(fm1));
    
    
   
    //dib= clamp(dib,0.0,1.0);
    vec3 col1 = vec3(0.2,0.4,0.4);
    vec3 col2 = vec3(0.9,0.8,0.5);
	
	col1 = vec3(0.2,0.4,0.4);
	col2 = vec3(0.9,0.8,0.55);
    vec3 colf = mix(col1,col2,fm1);  
    
    colf+=sin(colf*mapr(flush,20.0,50.0)+time*2.*speed)*0.1;
    
  
   // fragColor = vec4(dib,1.0);
    fragColor = vec4(colf,1.0);
}
