#pragma include "../common.frag"

mat2 rotate(float a)
{
    return mat2(cos(a), sin(a), -sin(a), cos(a));
}

float pattern(vec2 p)
{
    p *= 15.0;
    return abs(sin(p.x) + sin(p.y));
}

float boxmap(vec3 p)
{
    p *= 0.3;
    vec3 m = pow(abs(normalize(p)), vec3(20));
    vec3 a = vec3(pattern(p.yz),pattern(p.zx),pattern(p.xy));
	return dot(a,m)/(m.x+m.y+m.z);
}

vec3 smin(vec3 a, vec3 b)
{
    float k = 0.08;
	vec3 h = clamp( 0.5 + 0.5*(b-a)/k, 0.0, 1.0 );
	return mix( b, a, h ) - k*h*(1.0-h);
}

vec3 sabs(vec3 p)
{
 	return  p - 2.0 * smin(vec3(0), p);
}

float map(in vec3 p)
{
    float s = 3.7;
    float amp = 1.0/s;
    float c = 0.5;
    p = sabs(mod(p, c * 2.0) - c);
    float de = 50.;
    for(int i=0; i<3; i++){
        p.xy *= rotate(0.4+sin(iTime*0.2+ 0.3*sin(iTime*0.4))*0.2);
        p.yz *= rotate(0.4+sin(iTime*0.3+ 0.5*sin(iTime*0.5))*0.2);
        p = sabs(p);
        p *= s;
        p -= vec3(0.2*p.z, 0.6*p.x, 0.4) * (s - 1.0);
	    de = abs(length(p*amp) - 0.2) ;
        amp /= s;
    }
    return de + boxmap(p) * 0.02 - 0.01;
}

vec3 calcNormal(vec3 p){
  vec2 e = vec2(1, -1) * 0.001;
  return normalize(
    e.xyy*map(p+e.xyy)+e.yyx*map(p+e.yyx)+
    e.yxy*map(p+e.yxy)+e.xxx*map(p+e.xxx)
  );
}

vec3 doColor(vec3 p){
	return vec3(0.2,0.9,0.2) * boxmap(p);
}

void main()
{
    vec2 p = (fragCoord * 2.0 - iResolution.xy) / iResolution.y;
    vec3 ro = vec3(0.2, 0.1, 0.5)+iTime*0.1;
    vec3 rd = normalize(vec3(p, 2));
    rd.xz *= rotate(sin(iTime*0.3)*0.6);
    rd.yz *= rotate(sin(iTime*0.2)*0.6);
    rd.xy *= rotate(sin(iTime*0.05));
    vec3 col = mix(
        vec3(0.3, 0.7, 0.8),
        vec3(0.1, 0.1, 0.2),
        smoothstep(0.3, 2.5, length(p)));
    float t = 0.1, d;
 	for(int i = 0; i < 100; i++)
  	{
        d = map(ro + rd * t);
    	t += 0.1 * d;
    	if(d < 0.001 || t > 5.0) break;
  	}
  	if(d < 0.001)
  	{
	  	vec3 p = ro + rd * t;
	 	vec3 nor = calcNormal(p);
    	vec3 li = normalize(vec3(1));
        vec3 c = doColor(p);
        c *= clamp(dot(nor, li), 0.3, 1.0);
        c *= max(0.5 + 0.5 * nor.y, 0.0);
        c += pow(clamp(dot(reflect(normalize(p - ro), nor), li), 0.0, 1.0), 20.0);
        c.x +=1.0- exp(-t*t*0.15);
        c = clamp(c,0.0,1.0);
        col = mix(col,c,exp(-t*t*0.6));
    }
  	col = pow(col, vec3(0.8));
    col= mix(col, vec3(col.x), clamp(sin(iTime*0.5 + sin(iTime*0.2)*0.5)*2.0-1.0, 0.0, 1.0));
    fragColor = vec4(col, 1.0);
}