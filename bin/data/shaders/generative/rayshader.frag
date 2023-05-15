#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


//const float PI = 3.141592654;
const float side = 0.3;
const float angle = PI*1.0/3.0;
const float sinA = 0.86602540378;
const float cosA = 0.5;
const vec3 zero = vec3(0.0);
const vec3 one = vec3(1.0);

vec4 rayColor(vec2 _fragToCenterPos, vec2 _fragCoord){
	float d = length(_fragToCenterPos);
	_fragToCenterPos = normalize(_fragToCenterPos);
		
	float multiplier = 0.0;
	const float loop = 60.0;
	const float dotTreshold = 0.90;
	const float timeScale = 0.75;
	const float fstep = 10.0;
	
	// generates "loop" directions, summing the "contribution" of the fragment to it. (fragmentPos dot direction)
	float c = 0.5/(d*d);
	float freq = 0.25;		
	for (float i = 1.0; i < loop; i++) {
		float attn = c;
		attn *= 1.85*(sin(i*0.3*iTime)*0.5+0.5);
		float t = iTime*timeScale - fstep*i;
		vec2 dir = vec2(cos(freq*t), sin(freq*t));
		float m = dot(dir, _fragToCenterPos);
		m = pow(abs(m), 4.0);
		m *= float((m) > dotTreshold);
		multiplier += 0.5*attn*m/(i);
	}

	float f = abs(cos(iTime/2.0));
	
	const vec4 rayColor = vec4(0.9, 0.7, 0.3, 1.0);
		
	float pat = abs(sin(10.0*mod(fragCoord.y*fragCoord.x, 1.5)));
	f += pat;
	vec4 color = f*multiplier*rayColor;
	return color;
}


vec4 drawRay(){
	float aspect = resolution.x / resolution.y;	
	vec3 pos = vec3(fragCoord.xy / resolution.xy, 1.0);
	pos.x *= aspect;
	
	vec2 fragToCenterPos = vec2(pos.x - 0.5*aspect, pos.y - 0.5);
	vec4 rayCol = rayColor(fragToCenterPos,fragCoord);
	  
	float u, v, w;
	float c = 0.0;	

	vec4 triforceColor = vec4(1.0);
	 return mix(rayCol, triforceColor, c);
}

void main() {

	fragColor = drawRay();
}