#pragma include "../common.frag"
vec3 LIGHT = normalize(vec3(-0.3,0.2,-0.1));


uniform float FULL_SIZE ;
uniform float EDGE_SIZE ;
uniform float PAIR_SIZE ;
uniform float rotx ;
uniform float roty ;
uniform float rotz ;
vec3 n3(vec3 n)
{
 	return fract(cos(dot(n,vec3(813,12,376)))*vec3(901.81,827.46,615.79));   
}
vec3 model(vec3 p)
{	

	p.xy*=rotate2d(rotx);
	p.xz*=rotate2d(roty);
	p.yz*=rotate2d(rotz);
    float A = p.z/3.0+iTime*0.25;
    vec3 R = vec3(cos(A),sin(A),0);
    vec3 C = vec3(mod(p.xy+8.,16.)-8.+R.yx*vec2(1,-1),fract(p.z)-0.5);
    
    float H = min(length(C.xy+R.xy*FULL_SIZE*4.),length(C.xy-R.xy*FULL_SIZE*4.))*0.5-EDGE_SIZE;
    float P = max(length(vec2(dot(C.xy,R.yx*vec2(1,-1)),C.z))-PAIR_SIZE,length(C.xy)-FULL_SIZE*4.);
    
    float T = FULL_SIZE*4.+0.01+2.*EDGE_SIZE-length(C.xy);
    return vec3(min(H,P),T,P);  
}
vec3 normal(vec3 p)
{
 	vec2 N = vec2(-0.04, 0.04);

 	return normalize(model(p+N.xyy).x*N.xyy+model(p+N.yxy).x*N.yxy+
                     model(p+N.yyx).x*N.yyx+model(p+N.xxx).x*N.xxx);
}
vec4 raymarch(vec3 p, vec3 d)
{
    vec4 M = vec4(p+d*2.0,0);
 	for(int i = 0; i<100;i++)
    {
        float S = model(M.xyz).x;
    	M += vec4(d,1) * S;
        if (S<0.01 || M.w>50.0) break;
    }
    return M;
}
vec3 sky(vec3 d)
{
    float L = dot(d,LIGHT);
 	return vec3(0.3,0.5,0.6)-0.3*(-L*0.5+0.5)+exp2(32.0*(L-1.0));   
}
vec3 color(vec3 p, vec3 d)
{
    vec2 M = model(p).yz;
    float A = atan(mod(p.y+8.,16.)-8.,8.-mod(p.x+8.,16.));
    float T1 = ceil(fract(cos(floor(p.z)*274.63))-0.5);
    float T2 = sign(fract(cos(floor(p.z-80.0)*982.51))-0.5);
    float T3 = T2*sign(cos(p.z/3.0+iTime*0.25+A));

    float L = dot(normal(p),LIGHT)*0.5+0.5;
    float R = max(dot(reflect(d,normal(p)),LIGHT),0.0);
    vec3 C = mix(mix(vec3(0.9-0.8*T3,0.9-0.6*T3,T3),vec3(1.0-0.6*T3,0.2+0.8*T3,0.1*T3),T1),vec3(0.2),step(0.01,M.y));
 	C = mix(C,vec3(0.2,0.5,1.0),step(0.01,-M.x));
    return	C*L+pow(R,16.0);
}
void main()
{
    vec2 A = vec2(rotx,roty) / iResolution.xy * vec2(0,1) * 3.1416;
    vec3 D = vec3(cos(A.x)*sin(A.y),sin(A.x)*sin(A.y),cos(A.y));
    D = mix(vec3(1,0,0),D,ceil((A.x+A.y)/10.0));
    vec3 P = D*12.0-vec3(0,0,iTime*2.0);
    
	//P.x+=time ;
	
	
    vec3 X = normalize(-D);
    vec3 Y = normalize(cross(X,vec3(0,0,1)));
    vec3 Z = normalize(cross(X,Y));
    
	vec2 UV = (fragCoord.xy - iResolution.xy * 0.5) / iResolution.y;
    vec3 R = normalize(mat3(X,Y,Z) * vec3(1,UV));
    
    vec4 M = raymarch(P,R);
    vec3 C = mix(color(M.xyz,R),sky(R),smoothstep(0.5,1.0,M.w/50.0));
	fragColor = vec4(C,1);
}