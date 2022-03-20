
#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float saturation;
uniform float brightness;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 uv2 = gl_FragCoord.xy ;
	
	vec4 t1 =  texture2D(textura1, uv2/resolution);	
	
	vec3 rgbahsb = rgb2hsb(t1.rgb);
	
	vec3 fin = hsb2rgb(vec3(fract(rgbahsb.r),
							rgbahsb.g*saturation*2.,
							rgbahsb.b*brightness*2.));
	
	gl_FragColor = vec4(fin,1.0); 
}