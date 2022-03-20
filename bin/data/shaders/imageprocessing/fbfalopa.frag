#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float limitchroma;
uniform float limitburn;
uniform float dec;
uniform float fbst;
uniform float cfest;
void main()
{
		vec2 uv = gl_FragCoord.xy / resolution;
	//vec4 fin = texture2D(backbuffer,uv);

	vec4 cf = texture2D(texture1,vec2(uv.x,uv.y));
	vec4 cf2 = texture2D(texture1,vec2(1.-uv.y,1.-uv.x));

	vec2 puv = uv;

	float cfe = (cf.r+cf.g+cf.b)/3.;
	
	float mcfest = mapr(cfest,-.01,.01);
	puv-=vec2(0.5);
	puv*=scale(vec2(1.0-cfe*mcfest));
	puv+=vec2(0.5);
	vec4 prev = texture2D(feedback,puv);
    vec3 fin = cf.rgb+vec3(0.2);


  vec3 colchroma = vec3(.0,.0,.0);

  if(distance(colchroma,cf.rgb) > limitchroma){
       fin = cf.rgb;
  	}else{
  		fin = prev.rgb*fbst*1.01	;
  		fin = lm(fin,vec3(limitburn),vec3(dec));
  		}
	gl_FragColor = vec4(fin, 1.0);

}
