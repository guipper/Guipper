#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect iChannel0;
/*

    Terrain Lattice
    ---------------

	This is a raymarched heightmap subdivided into grid squares, which are each subdivided
	into two triangles to emulate the flat shaded look. It's been done before, so just to 
	be different, I put a mesh on top... It seemed like a good idea at the time. :)

    There are two ways to render flat grid squares. One is to linearly interpolate between
	the height values of all four vertices to produce a quad that looks flat on account of
    its straight edge joins. The other is to split the quad into two triangles and linearly 
	interpolate between the three verticies of each of those. I tried both methods, but 
	liked the look of the genuinely flat-planed triangles more.

    A flat shaded triangle render usually requires a barycentric approach, but since the
	grid triangles are essentially half squares, it's possible to use, vector 
	perpendicularity, symmetry, etc, to cut down on the calculations considerably.

	The extra cycles were used to render the mesh. I originally rendered the diagonal rods 
	also, but it made things look a little too busy, so I've left them out. Rendering straight 
	capsule-like tubes along grid seams can also be expensive, but it was possible to cut 
	corners there as well, so to speak.

	Anyway, this was just a practice run for a more ambitious example I have in mind. By
	the way, I have a simplex grid version as well that I'll put up later.
    

	Other examples:
    
	// Simple, and really nicely lit.
    Triangulator - nimitz
	https://www.shadertoy.com/view/lllGRr   
	

	// Nice example that takes an intuitive vectorized approach.
	Ray Marched Mesh Terrain - Flyguy
	https://www.shadertoy.com/view/ltjSRD

*/

// Max ray distance.
#define FAR 40.

// The point of the exercise was to polygonize the terrain and give it a flat shaded triangulated
// appearance, but if you'd prefer to see smooth quads, just uncomment the following:
//#define SMOOTH_QUAD

// Scene object ID to separate the mesh object from the terrain.
float objID;

// 2x2 matrix rotation. Note the absence of "cos." It's there, but in disguise, and comes courtesy
// of Fabrice Neyret's "ouside the box" thinking. :)
mat2 r2(float th){ vec2 a = sin(vec2(1.5707963, 0) + th); return mat2(a, -a.y, a.x); }


// Height map values. Just a couple of animated sinusoidal layers, but you could put anything
// here... so long as it's cheap. :)
float hm(in vec2 p){
    
    // Scaling, plus some movement.
    p = p/2. + iTime/4.;
    
    // Layer one.
    float n = dot(sin(p.xy*3.14159 - cos(p.yx*3.14159)*3.14159/2.), vec2(0.25) + .5)*.66;
    p = p*1.5;  // Increase frequency.
  
    p.xy = mat2(.866025, .5, -.5, .866025)*p.xy; // Rotate.
    
    // Add another layer.
    n += dot(sin(p.xy*3.14159 - cos(p.yx*3.14159)*3.14159/2.), vec2(0.25) + .5)*.34;
    
    return n; // Range [0, 1]... hopefully. :)

}

// Used to scale the grid without having to move the camera.
#define scale 2. 
vec3 hVal; // Global variable to hold the three height values for reuse.

// The terrain - tesselated in a flat-grid triangle-pair fashion... Needs rewording. :D
float triTerrain(vec2 p){ 

    
    
    vec2 ip = floor(p); // Integer value. Used for the unique corner height values.
    p -= ip; // Fractional grid value.
 
    
    //p *= p*(3.-2.*p); // Weird Gouraud-looking triangles, or smooth quad.
     
    
    // The barycentric coordinates, so to speak, and the corresponding height value.
    // For those of you familiar with the process, you may note that there are far 
    // fewer operations than usual.
   
    float s = step(1., p.x + p.y); // Determines which side of the diagonal we're on.
    
    // Storing the heights at the three triangle vertices. Normally, it wouldn't be
    // necessary, but we're reusing them to render the mesh.
    hVal = vec3(hm(ip+s), hm(ip + vec2(1, 0)), hm(ip+vec2(0, 1)));
    
    #ifdef SMOOTH_QUAD
    // A simple, interpolated quad. It's not really flat, but the edge-joins are straight,
    // so it looks that way. Because the mesh is set up on triangle logic, there two
    // extra height values. Normally, you'd only need one extra.
    return mix(mix(hm(ip), hVal.y, p.x), mix(hVal.z, hm(ip+1.), p.x), p.y);
    #else
    // Barycentric setup: This is a very trimmed down version of the generalized barycentric
    // calculations that involve cross-products, and so forth. Without going into detail, I'm
    // sure you could imagine that three points in space can be used to generate a plane 
    // equation via cross products and such, and the fractional grid points could be used in
    // unison with the vertice coordinates to determine the exact coordinate on the plane, or
    // the height value at that coordinate.
    //
    // Anyway, the grid triangles are shaped in such a way that a lot of the operations cancel 
    // out, and the lines below are the result. You could just use them. However, if you require
    // more information, look up a few barycentric coordinate examples.
    //
    vec3 b = abs(vec3(1.0 - p.x - p.y, p.x - (p.x - p.y + 1.)*s, p.y - (p.y - p.x + 1.)*s));
    
    // The linearly interpolated triangle height.
    return dot(b, hVal);
    #endif
    
/* 

    // Triangulating across the other diagonal. Handy, if you want to make patterns.
    float s = step(p.x, p.y);
    hVal = vec3(hm(ip), hm(ip + vec2(1. - s, s)), hm(ip+1.));

    //return mix(mix(hVal.x, hm(ip+vec2(1, 0)), f.x), mix(hm(ip+vec2(0, 1)), hVal.z, f.x),f.y);

    vec3 b = abs(vec3(1. - (1. - s)*p.x - p.y*s, (1.-2.*s)*(p.x - p.y), p.x*s + p.y*(1. - s)));
    
    return dot(b, hVal);
*/
   
 
}

// The flat shaded terrain and the mesh.
float map(vec3 p){
    
    // The terrain. By the way, when you scale coordinates, you have to scale back
    // the distance value to keep things in check. I often forget this.
    float ter = triTerrain(p.xz*scale)/scale; 
 
    const float hPert = .25; // Terrain height perturbation.
    float fl = p.y  + (.5 - ter)*hPert;//*.25; // Adding it to a flat plane.


    hVal = hVal*hPert - .175; // Rescaling the height values to match the terrain perturbation.
    
    // The grid boundary railings. As usual, the code looks more complicated than it is. Basically, we're
    // positioning four tubes around the grid boundaries. The Y-value is just a height interpolation
    // from one vertice to the adjoining one. The "abs" business is just a cheap trick to stack the 
    // railings on top of one another without having to render another four tubes. 
    vec3 q = p*scale;
    q.xz = fract(q.xz); // Break space into squares along the XZ plane.
        
    // Tubes on the left and right boundaries.
    float ln = length(vec2(q.x, abs(q.y - hVal.x -(hVal.z - hVal.x)*q.z - .25) -.25));
    ln = min(ln, length(vec2(q.x - 1., abs(q.y - hVal.y - (hVal.x - hVal.y)*q.z - .25) - .25)));

    // Tubes on the bottom and top boundaries.
    ln = min(ln, length(vec2(abs(q.y - hVal.x - (hVal.y - hVal.x)*q.x - .25) -.25, q.z)));
    ln = min(ln, length(vec2(abs(q.y - hVal.z - (hVal.x - hVal.z)*q.x - .25) -.25, q.z - 1.)));

    // The diagonal tube lines. Makes things look too busy, but comment them out, if you feel like it.
    //vec2 diag = vec2(q.x + q.z - 1., abs(q.y - hVal.z - (hVal.y - hVal.z)*q.x- .25) -.25);//*.7071;
    //ln = min(ln, length(diag));
        
    
/*    
    
    // If you wanted to use the reverse diagonal on the triangulation. See the comments in the 
    // "triTerrain" function first.
    
    ln =   length(vec2(q.x, abs(q.y - hVal.x - (hVal.y - hVal.x)*q.z - .25) -.25));
    ln = min(ln, length(vec2(q.x - 1., abs(q.y - hVal.y - (hVal.z - hVal.y)*q.z - .25) -.25)));
    ln = min(ln, length(vec2(abs(q.y - hVal.x - (hVal.y - hVal.x)*q.x - .25) -.25, q.z)));
    ln = min(ln, length(vec2(abs(q.y - hVal.y - (hVal.z - hVal.y)*q.x - .25) -.25, q.z - 1.)));


    vec2 diag = vec2(q.x - q.z, abs(q.y - hVal.x - (hVal.z - hVal.x)*q.z - .25) -.25);//*.7071;
    ln = min(ln, length(diag));
*/         
   
    
    
    // Vertical column and the balls. We've calculated another height value offset by half the grid in 
    // order to draw just one each - instead of four. It's a little hard to explain why but it has to
    // do with repetitive cell boundaries.
    float hgt = hm(floor(p.xz*scale + .5))*hPert - .175;
    //float hgtZ = hm(floor(p.xz*scale + .5) + vec2(0, 1))*pert - .025;
    vec2 offXZ = fract(p.xz*scale + .5) - .5;
    ln = min(ln, max(length(offXZ), abs(q.y - hgt) - .5));

    // The metallic balls. Stacked two high using the "abs" trick.
    float sp = length(vec3(offXZ.x, abs(abs(q.y - hgt - .25) - .25), offXZ.y));
    
    
    ln -= .04; // Line thickness.
    sp -= .125; // Ball thickness.
    ln = min(ln, sp);
    
    
    ln /= scale;
    sp /= scale;
  
 
    // Object ID.
    objID = step(fl, ln);
    
    
    // Combining the mesh with the terrain.
    return min(fl, ln); //smin(fl, ln, .025);
 
}

 

// Standard raymarching routine.
float trace(vec3 ro, vec3 rd){
   
    float t = 0.; //fract(sin(dot(rd, vec3(7, 157, 113)))*45758.5453)*.1;
   
    for (int i=0; i<96; i++){

        float d = map(ro + rd*t);
        
        if(abs(d)<.001*(t*.125 + 1.) || t>FAR) break;
        
        t += d*.8;  // Using more accuracy, in the first pass.
    }
    
    return min(t, FAR);
}


/*
// Tetrahedral normal - courtesy of IQ. I'm in saving mode, so the two "map" calls saved make
// a difference. Also because of the random nature of the scene, the tetrahedral normal has the 
// same aesthetic effect as the regular - but more expensive - one, so it's an easy decision.
vec3 getNormal(in vec3 p, float t)
{  
    vec2 e = vec2(-1., 1.)*0.001*min(1. + t, 5.);   
	return normalize(e.yxx*map(p + e.yxx) + e.xxy*map(p + e.xxy) + 
					 e.xyx*map(p + e.xyx) + e.yyy*map(p + e.yyy) );   
}
*/

// Standard normal function. It's not as fast as the tetrahedral calculation, but more symmetrical.
vec3 getNormal(in vec3 p, float t) {
	const vec2 e = vec2(0.002, 0); //vec2(0.002*min(1. + t*.5, 2.), 0);
	return normalize(vec3(map(p + e.xyy) - map(p - e.xyy), map(p + e.yxy) - map(p - e.yxy),	map(p + e.yyx) - map(p - e.yyx)));
}



// Cheap shadows are hard. In fact, I'd almost say, shadowing particular scenes with limited 
// iterations is impossible... However, I'd be very grateful if someone could prove me wrong. :)
float softShadow(vec3 ro, vec3 lp, float k){

    // More would be nicer. More is always nicer, but not really affordable... Not on my slow test machine, anyway.
    const int maxIterationsShad = 20; 
    
    vec3 rd = (lp-ro); // Unnormalized direction ray.

    float shade = 1.0;
    float dist = 0.05;    
    float end = max(length(rd), 0.001);
    //float stepDist = end/float(maxIterationsShad);
    
    rd /= end;

    // Max shadow iterations - More iterations make nicer shadows, but slow things down. Obviously, the lowest 
    // number to give a decent shadow is the best one to choose. 
    for (int i=0; i<maxIterationsShad; i++){

        float h = map(ro + rd*dist);
        //shade = min(shade, k*h/dist);
        shade = min(shade, smoothstep(0.0, 1.0, k*h/dist)); // Subtle difference. Thanks to IQ for this tidbit.
        //dist += min( h, stepDist ); // So many options here: dist += clamp( h, 0.0005, 0.2 ), etc.
        dist += clamp(h, 0.01, .5);
        
        // Early exits from accumulative distance function calls tend to be a good thing.
        if (h<0.001 || dist > end) break; 
    }

    // I've added 0.5 to the final shade value, which lightens the shadow a bit. It's a preference thing.
    return min(max(shade, 0.) + 0.2, 1.0); 
}


// I keep a collection of occlusion routines... OK, that sounded really nerdy. :)
// Anyway, I like this one. I'm assuming it's based on IQ's original.
float cAO(in vec3 pos, in vec3 nor)
{
	float sca = 2.0, occ = 0.0;
    for( int i=0; i<5; i++ ){
    
        float hr = 0.01 + float(i)*0.5/4.0;        
        float dd = map(nor * hr + pos);
        occ += (hr - dd)*sca;
        sca *= 0.7;
    }
    return clamp( 1.0 - occ, 0.0, 1.0 );    
}




// Tri-Planar blending function. Based on an old Nvidia writeup:
// GPU Gems 3 - Ryan Geiss: http://http.developer.nvidia.com/GPUGems3/gpugems3_ch01.html
vec3 tex3D(sampler2DRect channel, vec3 p, vec3 n){
    
    n = max(abs(n) - .2, 0.001);
    n /= dot(n, vec3(1));
	vec3 tx = texture(channel, p.zy).xyz;
    vec3 ty = texture(channel, p.xz).xyz;
    vec3 tz = texture(channel, p.xy).xyz;
    
    // Textures are stored in sRGB (I think), so you have to convert them to linear space 
    // (squaring is a rough approximation) prior to working with them... or something like that. :)
    // Once the final color value is gamma corrected, you should see correct looking colors.
    return tx*tx*n.x + ty*ty*n.y + tz*tz*n.z;
}

// Texture bump mapping. Four tri-planar lookups, or 12 texture lookups in total. I tried to 
// make it as concise as possible. Whether that translates to speed, or not, I couldn't say.
vec3 texBump( sampler2DRect tx, in vec3 p, in vec3 n, float bf){
   
    const vec2 e = vec2(0.002, 0);
    
    // Three gradient vectors rolled into a matrix, constructed with offset greyscale texture values.    
    mat3 m = mat3( tex3D(tx, p - e.xyy, n), tex3D(tx, p - e.yxy, n), tex3D(tx, p - e.yyx, n));
    
    vec3 g = vec3(0.299, 0.587, 0.114)*m; // Converting to greyscale.
    g = (g - dot(tex3D(tx,  p , n), vec3(0.299, 0.587, 0.114)) )/e.x; g -= n*dot(n, g);
                      
    return normalize( n + g*bf ); // Bumped normal. "bf" - bump factor.
	
}



// Compact, self-contained version of IQ's 3D value noise function. I have a transparent noise
// example that explains it, if you require it.
float n3D(in vec3 p){
    
	const vec3 s = vec3(7, 157, 113);
	vec3 ip = floor(p); p -= ip; 
    vec4 h = vec4(0., s.yz, s.y + s.z) + dot(ip, s);
    p = p*p*(3. - 2.*p); //p *= p*p*(p*(p * 6. - 15.) + 10.);
    h = mix(fract(sin(h)*43758.5453), fract(sin(h + s.x)*43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}



// Very basic pseudo environment mapping... and by that, I mean it's fake. :) However, it 
// does give the impression that the surface is reflecting the surrounds in some way.
//
// More sophisticated environment mapping:
// UI easy to integrate - XT95    
// https://www.shadertoy.com/view/ldKSDm
vec3 envMap(vec3 p){
    
    p *= 2.;
    p.xz += iTime*.5;
    
    float n3D2 = n3D(p*2.);
   
    // A bit of fBm.
    float c = n3D(p)*.57 + n3D2*.28 + n3D(p*4.)*.15;
    c = smoothstep(0.5, 1., c); // Putting in some dark space.
    
    p = vec3(c*.8, c*.9, c);//vec3(c*c, c*sqrt(c), c); // Bluish tinge.
    
    return mix(p.zxy, p, n3D2*.34 + .665); // Mixing in a bit of purple.

}

// Simple sinusoidal path, based on the z-distance.
vec2 path(in float z){ float s = sin(z/36.)*cos(z/18.); return vec2(s*16., 0.); }
 

void main(){

    
    // Screen coordinates.
	vec2 uv = (fragCoord - iResolution.xy*.5)/iResolution.y;
	
	// Camera Setup.
	vec3 lk = vec3(0, -.5 - .125*.5, iTime + 1.);  // "Look At" position.
	vec3 ro = lk + vec3(0, 2.5, -2.); // Camera position, doubling as the ray origin.
 
    // Light positioning. One is just in front of the camera, and the other is in front of that.
 	vec3 lp = ro + vec3(0, .75, 2);// Put it a bit in front of the camera.
	
	// Sending the camera, "look at," and two light vectors across the plain. The "path" function is 
	// synchronized with the distance function.
	lk.xy += path(lk.z);
	ro.xy += path(ro.z);
	lp.xy += path(lp.z);

    // Using the above to produce the unit ray-direction vector.
    float FOV = 1.; // FOV - Field of view.
    vec3 fwd = normalize(lk-ro);
    vec3 rgt = normalize(vec3(fwd.z, 0., -fwd.x )); 
    // "right" and "forward" are perpendicular, due to the dot product being zero. Therefore, I'm 
    // assuming no normalization is necessary? The only reason I ask is that lots of people do 
    // normalize, so perhaps I'm overlooking something?
    vec3 up = cross(fwd, rgt); 

    // rd - Ray direction.
    vec3 rd = normalize(fwd + FOV*uv.x*rgt + FOV*uv.y*up);
    
    // Swiveling the camera about the XY-plane (from left to right) when turning corners.
    // Naturally, it's synchronized with the path in some kind of way.
	rd.xy *= r2( path(lk.z).x/128. );

    
/*       
    // Mouse controls.   
	vec2 ms = vec2(0);
    if (iMouse.z > 1.0) ms = (2.*iMouse.xy - iResolution.xy)/iResolution.xy;
    vec2 a = sin(vec2(1.5707963, 0) - ms.x); 
    mat2 rM = mat2(a, -a.y, a.x);
    rd.xz = rd.xz*rM; 
    a = sin(vec2(1.5707963, 0) - ms.y); 
    rM = mat2(a, -a.y, a.x);
    rd.yz = rd.yz*rM;
*/    
	 
    
    // Raymarch to the scene.
    float t = trace(ro, rd);
    
    float svObjID = objID;
	
    // Initiate the scene color to black.
	vec3 sceneCol = vec3(0);
	
	// The ray has effectively hit the surface, so light it up.
	if(t < FAR){
        
        // Edge and edge-factor.
        //float edge, crv = 1., ef = 4.; // Curvature variable not used.
        
        // Texture scale factor.
        float tSize0 = 1./2.;
    	
    	// Surface position and surface normal.
	    vec3 sp = ro + rd*t;
	    //vec3 sn = getNormal(sp, edge, crv, ef, t);
        vec3 sn = getNormal(sp, t);
        
        // Texture-based bump mapping. I've left it out for this.
        //if(svObjID>0.5) sn = texBump(iChannel0, sp*tSize0, sn, .003);
	    
        
        // Obtaining the texel color. 
	    vec3 texCol;   

        
        if(svObjID>0.5) { // Terrain texturing.
            
            texCol = tex3D(iChannel0, sp*tSize0, sn);
            //texCol = texture(iChannel0, sp.xz*tSize0).xyz;
            //texCol *= texCol;
            texCol = smoothstep(0.0, .5, texCol);//*vec3(1, .9, .8);//
            //texCol *= triTerrain(sp.xz)*.25 + .75; // Adds more definition to the squares.
 
            // Color some of the squares brown.
            if(mod(dot(floor(sp.xz*scale), vec2(1, -1)), 5.)>=2.) texCol *= vec3(1, .5, .25);
            
                        
            /*
            // Blinking lights. Too much, I think.
            float rnd = fract(sin(dot(floor(sp.xz*scale), vec2(141.13, 289.97)))*43758.5453);
            rnd = sin(rnd*6.283 + iTime)*.5 + .5;
            if(rnd>.33) texCol *= vec3(1, .5, .25);
            else { texCol *= 2.;  }
            //texCol *= mix(vec3(2.), vec3(1, .5, .25), rnd); // Alternative: Random mix.
            */
            
        }
        else { // The chrome lattice.
            
            texCol = tex3D(iChannel0, sp*tSize0, sn)*.25;
 
        }
    	
    	// Light direction vector.
	    vec3 ld = lp-sp;

        // Distance from respective light to the surface point.
	    float lDist = max(length(ld), 0.001);
    	
    	// Normalize the light direction vector.
	    ld /= lDist;

        
        
        // Shadows and ambient self shadowing.
    	float sh = softShadow(sp, lp, 8.);
    	float ao = cAO(sp, sn); // Ambient occlusion.
	    
	    // Light attenuation, based on the distances above.
	    float atten = 1./(1. + lDist*lDist*0.05);

    	
    	// Diffuse lighting.
	    float diff = max( dot(sn, ld), 0.0);
        diff = pow(diff, 4.)*1.5; // Ramping up the diffuse.
    	
    	// Specular lighting.
	    float spec = pow(max( dot( reflect(-ld, sn), -rd ), 0.0 ), 8.); 
	    
	    // Fresnel term. Good for giving a surface a bit of a reflective glow.
        float fre = pow( clamp(dot(sn, rd) + 1., .0, 1.), 4.);
        
        
        // I got a reminder looking at XT95's "UI" shader that there are cheaper ways
        // to produce a hint of reflectivity than an actual reflective pass. :)        
        vec3 env = envMap(reflect(rd, sn))*2.;
        if(svObjID>.5) { // Lowering the terrain settings a bit.
            env *= .25;
            spec *= .5;            
            fre *= .5;
        }
        

        // Combining the above terms to procude the final color.
        sceneCol += (texCol*(diff + 0.25 + vec3(1, .9, .7)*fre) + env + vec3(1, .95, .8)*spec);
        

        // Shading.
        sceneCol *= ao*atten*sh;
        
        
	
	}
    
    // Simple dark fog. It's almost black, but I left a speck of blue in there to account for 
    // the blue reflective glow... Although, it still doesn't explain where it's coming from. :)
    vec3 bg = mix(vec3(.6, .5, 1), vec3(.025, .05, .1), clamp(rd.y + .75, 0., 1.));//vec3(1, .9, .8);//
    sceneCol = mix(sceneCol, bg, smoothstep(0., .95, t/FAR));
    
    // Rought gamma correction.
	fragColor = vec4(sqrt(clamp(sceneCol, 0., 1.)), 1.0);
	
}