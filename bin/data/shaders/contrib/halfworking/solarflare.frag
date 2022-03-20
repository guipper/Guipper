#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect iChannel0; 
void main()
{
    fragColor = vec4(0);
    //Centered coordinates.
    vec2 P = (gl_FragCoord.xy-.5*iResolution.xy)*2.2/iResolution.y;
    //Iterate for radius.
    for(float i = 1.;i<1.7;i+=.01)
    {
        //Calculate twisting sphere rays.
        vec3 R = vec3(P,sqrt(i-dot(P,P)));
        R.xy *= mat2(cos(i*.1),sin(i*.1),sin(i*.1),-cos(i*.1));
        //Add light rays.
    	fragColor += .025*pow(texture(iChannel0,R.xy/sqrt(R.z)-.1*iTime),vec4(i*4.-3.));
    }
    //Create edge glow and attenuation in space.
	fragColor *= vec4(.65,.4,.25,1)/(abs(dot(P,P)-1.)+.2),pow(1.-sqrt(max(1.-dot(P,P),0.)),2.);
    //Calculate disk distance for solar flares.
    float D = 1.5-length(P+P.y*.4+.1*cos(P.x*6.+.2*iTime));
    
    //Added center glow.
    fragColor += vec4(2,1,.5,0)*(exp(-dot(P,P)) +
	//Calculate the solar flares.
    .1*smoothstep(.8,1.,cos(iTime/8.+P.x+P.y*.4))*(cos(P.y*8.+iTime)*.3+.7)*exp(cos(P.x+P.y*.4+iTime)
    -10.*abs((cos(D*38.+P.x*17.+2.*iTime)*.1+.9)*D)));
}