#version 330
in vec4 particleColor;
in float sourceLayer;
in float surfaceCoverage;
out vec4 outputColor;
uniform int surfaceGrains;
uniform float plumeSoftness;
void main() {
    vec2 p=gl_PointCoord*2.-1.;float r=dot(p,p);
    bool detail=sourceLayer>.5 && surfaceGrains==0;
    if(r>1. && !detail)discard;
    // Overlapping pixel footprints avoid the pinholes of circular sprites
    // in the current surface. Detached particles remain circular grains.
    // Constant coverage matters: equal-depth splats cannot fill a faded edge
    // after the first splat has written depth, which otherwise draws a grid.
    float edge=sourceLayer>.5?1.:1.-smoothstep(.65,1.,r);
    if(sourceLayer<.5){
        edge=1.-smoothstep(mix(.9,.15,plumeSoftness),1.,r);
        edge*=mix(1.,exp(-r),plumeSoftness);
    }
    // Lit sphere sprites keep a readable silhouette without tessellating 50k spheres.
    vec3 normal=vec3(p,sqrt(max(0.,1.-r)));
    float light=sourceLayer>.5 ? 1. : .7+.3*max(0.,dot(normal,normalize(vec3(-.4,-.5,1.))));
    outputColor=vec4(particleColor.rgb*light,particleColor.a*edge*surfaceCoverage);
}
