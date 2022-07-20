#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float energia1;
uniform float energia2;
uniform float ite1;

uniform float palette;
mat2 r2d(float a) {
	float c = cos(a), s = sin(a);
    return mat2(
        c, s,
        -s, c
    );
}
void main(){ 

    float rotTime = sin(iTime);
    
    vec3 color1 = vec3(0.8, 0.2, 0.);
    vec3 color2 = vec3(.0, 0.2, 0.8);
    
    vec2 uv = ( fragCoord -.5*iResolution.xy )/iResolution.y;

    vec3 destColor = vec3(5.0 * rotTime, .0, 0.5);
    float f = 0.;
    float maxIt = 18.0;
    vec3 shape = vec3(0.);
    for(float i = 0.0; i < maxIt; i++){
        float s = sin( i * cos(iTime*0.002+i*mapr(ite1,0.0,1.0))*0.05+0.15);
        float c = cos( i *(sin(iTime*0.002+i*mapr(ite1,0.325,0.375))*0.05+0.15));
        c += sin(iTime*.1);
        f = (.005) / abs(length(uv / vec2(c, s)) - 0.4);
		
		
		float aura = mapr(energia2,-200.,-50.);
        f += exp(aura*distance(uv, vec2(c,s)*0.5))*4.;
        // Mas Particulas
        f += exp(aura*distance(uv, vec2(c,s)*-0.5))*10.;
        // Circulito
        f += (.008) / abs(length(uv/2. / vec2(c/4. + sin(iTime*0.)*.1, s/4.)))*mapr(energia1,0.0,10.0);
        float idx = float(i)/ float(maxIt);
        idx = fract(idx*2.);
        vec3 colorX = mix(color1, color2,idx);
        shape += f * sin(colorX*.8);
        
        uv *= r2d(iTime*0.0001 + cos(i*10.-time)*f*2.1);
    }
    
    // vec3 shape = vec3(destColor * f);
    // Activar modo falopa fuerte
   //  shape = sin(shape*10.+time)*.5+.5;
	
	
	
	shape.rg *=rotate2d(palette*PI); 
	shape.bg *=rotate2d(palette*PI); 
	shape.rb *=rotate2d(palette*PI); 
	
	
	
    fragColor = vec4(shape,1.0);
}