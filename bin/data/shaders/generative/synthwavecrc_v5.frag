#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
const vec3 BG = vec3(0, 0, 0);
const float EPS = 0.08;
const float FOVH = 20.0;
const float D = 0.1;
const float MAX_DIST = 80;
const int MAX_ITER = 300;
const float AMBIENT = 0.4;
const int FSAA = 3;
const float GRID_DIM = 1.6;
const bool JUMP = false;


const vec3 col1 = vec3(0.0,0.9,.2)*.2;

float speedx = 0.5;
float speedy = 0.49;

float animationspeed1 = 0.16;
float iterations = 0.5;
float formuparam = 0.87;
float volsteps = 0.48;
float stepsize = 0.24;
float zoom = 0.08;
float tile = 0.8;
float brightness = 0.41;
float darkmatter = 0.82;
float distfading = 0.31;
float saturation = 0.42;


uniform float alphalogos1;
uniform float alphalogos2;
uniform float logoprincipaly;
float jTime;
float g = 0.0;
uniform float speed;

uniform sampler2D tx1;
uniform sampler2D tx2;

float sq(float x) {
    return x*x;
}

// From https://www.shadertoy.com/view/Msf3WH
vec2 hashv( vec2 p )
{
	p = vec2( dot(p,vec2(127.1,311.7)), dot(p,vec2(269.5,183.3)) );
	return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}
float noise2( in vec2 p )
{
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;

	vec2  i = floor( p + (p.x+p.y)*K1 );
    vec2  a = p - i + (i.x+i.y)*K2;
    float m = step(a.y,a.x); 
    vec2  o = vec2(m,1.0-m);
    vec2  b = a - o + K2;
	vec2  c = a - 1.0 + 2.0*K2;
    vec3  h = max( 0.5-vec3(dot(a,a), dot(b,b), dot(c,c) ), 0.0 );
	vec3  n = h*h*h*h*vec3( dot(a,hashv(i+0.0)), dot(b,hashv(i+o)), dot(c,hashv(i+1.0)));
    return dot( n, vec3(70.0) );
}

// From https://www.shadertoy.com/view/WttXWX
uint hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float hash(vec2 vf) {
    uint offset = vf.x < 0.0 ? 13u : 0u;
    uvec2 vi = uvec2(abs(vf));
    return float(hash(vi.x + (vi.y<<16) + offset)) / float( 0xffffffffU );
}

float vertex_height(vec2 vf) {
    float offset = - 2.0 + 2.0*smoothstep(1., 8., abs(vf.x));
    float amplitude = mix(0.0, 2.5, smoothstep(1., 14., abs(vf.x)));
    return amplitude * hash(vf) + offset + (JUMP ? 0.2*abs(sin(4.*iTime)) : 0.);
}

// Adapted from https://www.shadertoy.com/view/tsScRK
float height_and_color_map(vec2 proj){
    proj /= GRID_DIM;
    proj.x -= 0.5;
    vec2 d = fract(proj);
    proj -= d;
    bool gd = dot(d,vec2(1))<1.;
    vec2 pt = proj + (gd ? 0. : 1.);
    float n2 = vertex_height(proj+vec2(1,0));
    float n3 = vertex_height(proj+vec2(0,1));
    float nt = vertex_height(pt);
    float nmid = mix(n2,n3,d.y);
    float nv, dx;
    if(gd) {
        nv = mix(nt,n3,d.y);
        dx = d.x/(1.-d.y);
	} else {
        nv = mix(n2,nt,d.y);
        dx = (1.-d.x)/(d.y);
	}
    return mix(nv,nmid,dx);
}


float roadLines(vec2 pos) {
    float roadWidth = 0.3; // Ancho de la carretera
    float lineOffset = 0.1; // Separación entre líneas
    float lineOpacity = 1.0; // Opacidad de las líneas
    float lineThickness = 0.002; // Grosor de las líneas
    
    // Obtener la altura del terreno en la posición actual
    float terrainHeight = height_and_color_map(pos);
    
    // Definir la posición y el grosor de la línea de la carretera
    float roadY = 0.0; // Altura de la línea de la carretera
    float roadThickness = 0.001; // Grosor de la línea de la carretera
    
    // Calcular el ángulo de inclinación del terreno
    float slopeAngle = atan(length(vec2(dFdx(terrainHeight), dFdy(terrainHeight))));
    
    // Calcular la intensidad de la línea de la carretera basada en el ángulo de inclinación
    float lines = smoothstep(0.0, 0.1, slopeAngle); // Ajusta el valor según el ángulo deseado
    
    // Aplicar el grosor y la opacidad de la línea de la carretera
    lines *= lineOpacity * smoothstep(-lineThickness, 0.0, abs(pos.y - roadY) - roadThickness / 2.0);
    
    return lines;
}
// Signed distance function that defines the scene.
float sdf(in vec3 pos ) {    
    return pos.y*1.5 - height_and_color_map(pos.xz)*2.;
}
vec3 generateStarnest(){
	//get coords and direction
	vec2 uv = gl_FragCoord.xy/resolution.xy-vec2(.5,.5);
	
	uv.y = 1.0-uv.y;
	uv.y*=resolution.y/resolution.x;
	vec3 dir=vec3(uv*zoom*5.0,1.);

	float a1=.5+1.0/resolution.x*2.;
	float a2=.8+1.0/resolution.y*2.;
	mat2 rot1=mat2(cos(a1),sin(a1),-sin(a1),cos(a1));
	mat2 rot2=mat2(cos(a2),sin(a2),-sin(a2),cos(a2));
	dir.xz*=rot1;
	dir.xy*=rot2;
	vec3 from=vec3(1.,.5,0.5);
	from+=vec3(time*mapr(speedx,-0.05,0.05),time*mapr(speedy,-0.05,0.05),-2.);
	
	
	
	//from.xz*=rot1;
	//from.xy*=rot2;
	
	//volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	
	
	int mite = int(floor(mapr(iterations,10.0,25.0)));
	int mvolsteps = int(floor(mapr(volsteps,0.0,20.0)));
	float mbri = mapr(brightness,0.0,0.0030);
	float mdarkmatter = mapr(darkmatter,0.0,10.0);
	for (int r=0; r<mvolsteps; r++) {
		vec3 p=from+s*dir*.5;
		p = abs(vec3(tile)-mod(p,vec3(tile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<mite; i++) { 
			p=abs(p)/dot(p,p)-formuparam; // the magic formula
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,mdarkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*mbri*fade;
		//v*=vec3(0.0,0.9,1.3);	// coloring based on distance
		fade*=mapr(distfading,0.2,.5); // distance fading
		s+=stepsize;
	
		
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	v = mix(v,vec3(0.0,9.9,9.),0.5)*.8;
	
	
	vec3 c1 = vec3(28./255., 128./255., 183./255.);
	vec3 c2 = vec3(1.);
	
	return v*0.01;	
}



// Ray marching engine.
void rayMarcher(in vec3 cameraPos, in vec3 lookDir, in vec2 screenDim, in vec2 uv,
                inout vec3 col) {
    vec3 up = vec3(0, 1., 0);
    vec3 lookPerH = normalize(cross(lookDir, up));
    vec3 lookPerV = normalize(cross(-lookDir, lookPerH));
    vec3 screenCenter = cameraPos + lookDir;
    vec3 screenPos = screenCenter + 0.5 * screenDim.x * uv.x * lookPerH
                     + 0.5 * screenDim.y * uv.y * lookPerV;
    
    vec3 rayDir = normalize(screenPos - cameraPos);
    
    float t = 0.0;
    float dist;
    vec3 pos;
    int iter;
	

	
    do {
        pos = cameraPos + t * rayDir;
        dist = sdf(pos);
        t += 0.5*dist;
        iter++;
		g++;
		//g = (g+1) * t*20.;
    } while(iter < MAX_ITER && dist < MAX_DIST && dist > EPS);
    
    if(dist <= EPS) {
        vec3 new_col = vec3(0, 0, 0);
        
        vec3 nml = normalize(vec3(
            dist - sdf(pos - vec3(EPS, 0, 0)),
            dist - sdf(pos - vec3(0, EPS, 0)),
            dist - sdf(pos - vec3(0, 0, EPS))
        ));
    
        float dx = mod(pos.x-GRID_DIM/2.0, GRID_DIM);
        dx = min(dx, GRID_DIM-dx);
        float dz = mod(pos.z, GRID_DIM);
        dz = min(dz, GRID_DIM-dz);
        float col_t = smoothstep(4., 7., abs(pos.x));
        //vec3 col_dark = mix(vec3(0.1, 0.5, 0.3), vec3(0.1, 0.4, 0.3), col_t);
        //vec3 col_bright = mix(vec3(0., 0.2, 0.), vec3(0., 0.9, 0.), col_t);
        
		vec3 col_dark = mix(vec3(0., 85.0/255., 88.0/255.)*.65, vec3(0., 85.0/255., 88.0/255.)*.15, col_t);
        vec3 col_bright = mix(vec3(0., 0.9, 0.), vec3(0., 0.9, 0.), col_t);
        
		
		vec3 lightDir = vec3(0.1, -0.3, 1);
        new_col = mix(new_col, col_dark, clamp(dot(-lightDir, nml), .2, 0.5));
        new_col = mix(col_bright, new_col, smoothstep(0.005, 0.01, min(dx, dz)));
        
		
		vec3 c1 = vec3(0.0,0.8,0.5);
		//new_col*=3.5;
        //new_col = mix(new_col, vec3(0,0,0), smoothstep(15., 30., cameraPos.z - pos.z));
        //float roadLinesIntensity = roadLines(pos.xz);
		//new_col = mix(new_col,vec3(0.5,0.0,0.0),roadLinesIntensity)*2.;
		
		col = mix(new_col+g*g*0.0000065*c1, col, smoothstep(20.,950., cameraPos.z - pos.z)) ;
		
		 // Calcular las líneas de la carretera
		//float roadLinesIntensity = roadLines(pos.xz);
	
	
		 // Mezclar las líneas de la carretera con el color del terreno
		//col = mix(col, vec3(1.0), roadLinesIntensity);
	
	
	}
}

vec4 sampleColor(in vec2 sampleCoord )
{
    float aspectRatio = iResolution.x / iResolution.y;
    float screenWidth = 2.0 * D * atan(0.5 * FOVH);

    vec3 cameraPos = vec3(0, 0, -2.*iTime);
    vec3 lookDir = vec3(0, 0.01, -D);
    vec2 screenDim = vec2(screenWidth, screenWidth / aspectRatio);

    // Normalized pixel coordinates (from -1 to 1)
    vec2 uv = 2.0 * sampleCoord / iResolution.xy - 1.0;
    uv.y*=-1.;
    // Pixel coordinates fixed with correct aspect ratio
    vec2 uvf = 2.0* (sampleCoord - iResolution.xy / 2.0) / iResolution.y;
    vec3 col = BG;
    
    // Stars
    //float noise2Val = noise2(30.0 * uvf+time);
    //float whiteIntensity = smoothstep(0.8, 1.0, noise2Val);
    //col = mix(col1, vec3(0., 0.9, 0.), whiteIntensity);
    col = mix(vec3(165./255.,250./255.,92./255.)*.01,generateStarnest(),1.);
    // Sun
    const vec2 sun_c = vec2(0, 0.25);
    const float sun_r = 0.4;
    if(length(uvf-sun_c) < sun_r) {
        vec3 sun_col = mix(vec3(1, .4, .8), vec3(1, 1, .3), (uv.y-sun_c.y+sun_r)/(2.*sun_r));
        
		
		const float BAND_HEIGHT = 0.02;
        float dy = mod(uvf.y, BAND_HEIGHT) / BAND_HEIGHT;
        
		//col = mix(sun_col, vec3(0,0,0), smoothstep(0.25, 0.35, abs(dy-0.5)));
	}
	
	//g*=-1.*cameraPos.z*2.;
	//g*= smoothstep(0.2,0.3,cameraPos.x)*5000.;
    // Grid
    rayMarcher(cameraPos, lookDir, screenDim, uv, col);
    // Output to screen
    //return vec4(col+g*g*.0000016*(vec3(165./255.,250./255.,92./255.)*0.2), 1.0);
	
	
	
	return vec4(col+g*g*.0000*(vec3(165./255.,220./255.,92./255.)*0.2), 1.0);
}

void main() {
    vec4 colSum = vec4(0);
    for(int i = 0; i < FSAA; i++) {
        for(int j = 0; j < FSAA; j++) {	
			vec2 fc = vec2(float(i) / float(FSAA), float(j) / float(FSAA));
			//fc.y = 
            colSum += sampleColor(fragCoord + fc);
        }
    }
	
	vec2 uv2 = gl_FragCoord.xy / resolution;
	vec2 uv3 = gl_FragCoord.xy / resolution;
	
	//Reajustar en web ?
	uv2-=vec2(0.5);
	uv2*=scale(vec2(3.5));
	uv2*=scale(vec2(2.1,0.6));
	uv2+=vec2(0.5);
	uv2.y+=0.35+mapr(logoprincipaly,-1.,1.);
	
	uv3-=vec2(0.5);
	uv3*=scale(vec2(7.0));
	
	uv3*=scale(vec2(0.3,0.275));
	uv3+=vec2(0.5);
	uv3.y-=0.50;
	vec4 img = texture2D(tx1,uv2);
	vec4 img2 = texture2D(tx2,uv3);
	
	img.rgb *= img.a;
	
	img2.rgb *= img2.a;
	
	
	//colSum+=img.rgba;
	
	vec4 cglow = vec4(155.,237.,92.,1.0);
	
	
	
	vec2 uv4 = gl_FragCoord.xy / resolution;
	
	
	uv4-=vec2(0.5);
	uv4*=scale(vec2(7.5));
	uv4*=scale(vec2(1.7,0.5));
	uv4+=vec2(0.5);
	uv4.x+=5;
	uv4.y-=0.3;
	
	
	vec2 uv5 = gl_FragCoord.xy / resolution;
	
	
	uv5-=vec2(0.5);
	uv5*=scale(vec2(7.5));
	uv5*=scale(vec2(1.7,0.5));
	uv5+=vec2(0.5);
	uv5.x-=5;
	uv5.y-=0.3;
	
	vec4 logoder = texture2D(tx1,uv4);
	vec4 logoizq = texture2D(tx1,uv5);
	
	vec4 fin = colSum / colSum.w*4.71;
	//fin = mix(fin,mix(fin,img,.7),img.a);
	
	fin = mix(fin,mix(fin,img2,.9	),img2.a*alphalogos2);
	fin = mix(fin,mix(fin,img,.8),img.a);
	
	fin = mix(fin,mix(fin,logoder,.9),logoder.a*alphalogos1);
	fin = mix(fin,mix(fin,logoizq,.9),logoizq.a*alphalogos1);
	//fin = mix(fin,mix(fin,logoder,1.),img2.a);
    fragColor = fin ;
}