#pragma include "../common.frag"
vec2 cmult(vec2 a, vec2 b)
{
	vec2 p;
	p[0]=a[0]*b[0]-a[1]*b[1];
	p[1]=a[0]*b[1]+a[1]*b[0];
	return p;
}
uniform float shading;
uniform float bailout;
uniform float iterations;
uniform float speed;
float shade=.1;
float zoom=1.;
float X=-.61;
float Y=.4234;
float depthCull=.1;
vec2 offset=vec2(.5,.3);


void main()
{
	vec2 uv = fragCoord.xy / iResolution.x;
	float maptime = iTime * speed;
	zoom +=-1.5+sin(maptime*.1)*1.6-1.6;
   // zoom +=-1.5+sin(iTime*.03)*.15;
	
	
    offset.x +=sin(maptime*.4)*.05;
    offset.y +=cos(maptime*.6)*.05;
	shade +=.2+cos(maptime*40.)*.002;
	shade*=mapr(shading,0.0,3.0);
    vec2 position = uv-offset;//gl_TexCoord[0].xy - offset;
	position = position * zoom;
	vec2 mouse=vec2(X,Y);
    mouse.x +=sin(maptime*.4)*.01+.01;
    mouse.y +=cos(maptime*.3)*.01+.01;

	vec2 c, c0, d;
	float v;
	
	c = vec2(position);
	c0 = mouse;
	
	vec2 f = position.xy;
	for(int i=0; i<int(iterations*150.); i++) {
		d = cmult(c, c);
		c = d + c0;
		v = ((c.x*c.x)) + (c.y*c.y) / sin(length(c.x )*4.);
		if (v > mapr(bailout,0.0,10.0)) break;
	}
	//vec4 tex=texture(texture, c);
	float l=sin(( c.y*2.)*.23);
	
	//l +=sin(c.y *1.2)*.5;
	//l +=sin(c.y *.2)*.1;
	c.x+=sin(c.y*2.);
	c.y+=sin(c.x*20.)*.1;
	c.x+=sin(c.y*3.);
	c.y+=sin(c.x*5.);
	
	float rand = mod(fract(sin(dot(2.5*uv,vec2(12.9898,100.233+iTime)))*43758.5453),.35);

	vec4 color;
	if(v>depthCull*20.){

	color = vec4(rand+vec3(sin((c.y*4.)*.4+sin(c.x*10.)*3.)+.2,length(-1.1-c*.1),sin(c.y))*pow(v,-shade-.23),1.)*vec4(vec3(.6),1.);
	}
	
	else{
	v +=sin(c.x*20.)*.01;
	v +=sin(c.y*120.)*.02;
	v +=sin(c.y*20.)*.1;
	v +=sin(c.x*4.)*.2;
	v +=sin(c.x*7.)*.2;
	v=clamp(v,0.,1.);
	color = vec4((rand*.75)+vec3(pow(v+.3,-.75)),1.)*vec4(.3,.5,1.,1.);
	}
	
	//gl_FragColor=color;


	fragColor =color;
}