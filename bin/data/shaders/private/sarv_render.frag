#version 330
in vec3 tint;
flat in int roundPoint;
out vec4 fragColor;
void main(){
 float light=1.;
 if(roundPoint!=0){vec2 q=gl_PointCoord*2.-1.;float r=dot(q,q);if(r>1.)discard;vec3 n=vec3(q,sqrt(1.-r));light=.15+.85*max(0.,dot(n,normalize(vec3(-.5,-.5,1))));}
 fragColor=vec4(tint*light,1);
}
