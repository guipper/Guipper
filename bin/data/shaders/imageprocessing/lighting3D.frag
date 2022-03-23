#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float height;
uniform float light_dir_x;
uniform float light_dir_y;
uniform float light_dir_z;
uniform float ambient;
uniform float diffuse;
uniform float specular_red;
uniform float specular_green;
uniform float specular_blue;

uniform float specular_size;
uniform sampler2D texture1;
vec2 d = vec2(1.,0.);

float bri(vec2 p) {
	return length(texture(texture1, p/resolution))*mapr(height,-300,300.);
}

vec3 normal(vec2 z) {
	return normalize(cross(
	vec3(d.x,0.,bri(z-d.xy)-bri(z+d.xy)),
	vec3(0.,d.x,bri(z-d.yx)-bri(z+d.yx))));
}

void main()
{
	vec3 t1 =  texture(texture1, gl_FragCoord.xy/resolution).rgb;
	vec2 z = gl_FragCoord.xy;

	vec3 n= normal(z-d.xy)+normal(z+d.xy);
	     n+=normal(z-d.yx)+normal(z+d.yx);
		   n/=4.;

	vec3 lightdir=normalize(vec3(light_dir_x-.5,light_dir_y-.5,light_dir_z));

	float diff=max(0.,dot(n,lightdir))*diffuse;
	float spec_red  =pow(max(0.,dot(reflect(-n,vec3(0.,0.,-1.)),lightdir)),1.+specular_size*100.)*specular_red;
	float spec_green=pow(max(0.,dot(reflect(-n,vec3(0.,0.,-1.)),lightdir)),1.+specular_size*100.)*specular_green;
	float spec_blue =pow(max(0.,dot(reflect(-n,vec3(0.,0.,-1.)),lightdir)),1.+specular_size*100.)*specular_blue;

	
	
	
	t1*=diff*2.+ambient*2.;
	t1+=vec3(spec_red,spec_green,spec_blue)*2.;

	gl_FragColor = vec4(t1,1.);
}
