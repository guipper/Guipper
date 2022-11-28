#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D iChannel0;
//Test portage GLSL ES POCOF1 -> ShaderToy

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
//#define DEBUG 1 

//vec3 grey = vec3(0.5);
vec3 grey = vec3(0.1,0.7,0.78);
float g = 4.;

vec3 bg(vec2 st) {
	return vec3(texture(iChannel0, st).rgb);
}

vec3 bw(vec3 col){
	 return vec3(col.r+col.g+col.b) /3.;
}

vec3 degrade(vec3 col){
	vec3 nc;
	if(col.r<0.4)
	  nc=vec3(0.);
	 else if(col.r<.65)
	  nc= grey;
	 else
	 	nc=vec3(1.);

	return nc;
}

vec3 pixelate(vec2 uv){
		float dx= g/iResolution.x;
		float dy= g/iResolution.y;
		float x= dx*floor(uv.x/dx);
		float y= dy*floor(uv.y/dy);

		vec2 k = vec2(x,y);
		//vec2 p = cameraAddent + k * cameraOrientation;
		return bg(k);

}

void main()
{
	vec2 uv =fragCoord/iResolution.xy;
	vec3 col=pixelate(uv);
    col = degrade(bw(col));
#ifdef DEBUG
    if(uv.x>0.33 && uv.x<0.66) col = bg(uv);
    if(uv.x>0.66) col = bw(bg(uv));
#endif
    if(col==grey){
        uv -= 0.5;
        col.b = 1.0-length(uv.xy);
    }
	fragColor =vec4(col,1.);
}
