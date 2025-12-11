#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


precision mediump float;
//vec3 verdejpupper(){return vec3(0.0,1.0,0.8);}

// we need the sketch resolution to perform some calculations

uniform float pantalla;

#define iTime time
#define iResolution resolution

#define PI 3.14159265359
#define TWO_PI 6.28318530718

#define OCTAVES 8


float mapr2(float _value,float _low2,float _high2) {
	float val = _low2 + (_high2 - _low2) * (_value - 0.) / (1.0 - 0.);
    //float val = 0.1;
	return val;
} 
mat2 scale2(vec2 _scale2){
    return mat2(_scale2.x,0.0,
                0.0,_scale2.y);
}
mat2 rotate2d2(float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
}

vec2 scale2(vec2 uv, float s);

mat2 scale2(vec2 _scale2);
mat2 rotate2d2(float _angle); 


float line( vec2 p, vec2 a, vec2 b, float r ){
  vec2 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}

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


vec3 visual2(vec2 uv){
	
	vec3 c1 = vec3(1.0,0.0,0.0);
	vec3 c2 = vec3(0.0,0.0,0.0);
	
	vec3 dib = vec3(0.0);
	
	vec2 uv2 = gl_FragCoord.xy / resolution;
	vec2 uv3 = gl_FragCoord.xy / resolution;
	int cnt = 2;
	
	
	for(int i=0; i<cnt; i++){	
	
		float nseed = uv2.y + i*100.;
		float sp = time * (float(i+1.)*4.)*.1;
		float seed = float(i*10000.+1.)+time*1.;
		float ns = noise2(vec2(uv2.x*2.+sp*.4,uv2.y),seed)*.1;
		uv2.y +=ns;
		
		float fix = resolution.x/resolution.y;
		float ef = smoothstep(0.49,0.51,uv2.y);
		
		ef *= smoothstep(0.94,0.96,sin(uv2.y*80.)*.5+.5);
		
		
		float msk = smoothstep(0.6,0.8,uv3.y);
		ef*= msk;
		//dib+= mix(c2,c1,ns);
		dib+= mix(c2,c1,ef);
		//dib = vec3(msk);
	}
	
	
	
	
	
	
	return dib;
	
	
}

void main(){	
	vec2 uv = gl_FragCoord.xy / resolution;
	uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x/resolution.y;
	uv.x*=fix;
	vec3 c1 = vec3(1.0,0.0,0.0);
	vec3 c2 = vec3(0.0);
		

	fragColor = vec4(visual2(uv),1.0); 
	
}









