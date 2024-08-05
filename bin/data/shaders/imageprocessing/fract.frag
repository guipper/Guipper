#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float fractx;
uniform float fracty;
uniform float speedx;
uniform float speedy;

void main() {
  vec2 uv = gl_FragCoord.xy / resolution;

  vec2 puv = gl_FragCoord.xy / resolution;
  /*puv = fract(puv
			  *mapr(fractst,1.0,20.0));*/

  float mapspeedx = mapr(speedx, -1.0, 1.0);
  float mapspeedy = mapr(speedy, -1.0, 1.0);
  float mfractx = mapr(fractx, 1.0, 40.0);
  float mfracty = mapr(fracty, 1.0, 40.0);

  puv = fract(vec2(puv.x * mfractx + mapspeedx * time, puv.y * mfracty + mapspeedy * time));

  puv *= resolution;
  vec4 t1 = texture(texture1, puv / resolution);
  vec3 fin = t1.rgb;

  fragColor = vec4(fin, 1.0);
}
