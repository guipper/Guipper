#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float chroma_red;
uniform float chroma_green;
uniform float chroma_blue;
uniform float threshold;
uniform float fuerzadist;

void main() {
  vec3 col=texture2D(input_texture, gl_FragCoord.xy/resolution).rgb;
  vec3 col2 = vec3(chroma_red,chroma_green,chroma_blue);
	 
	
  vec3 fin = vec3(0.0);
  
  
  if(distance(col,col2) > threshold){
	fin = mix(vec3(0.0),col.rgb,distance(col,col2)*fuerzadist*5.0);
  }
 
  fragColor = vec4(fin,1.);

}
