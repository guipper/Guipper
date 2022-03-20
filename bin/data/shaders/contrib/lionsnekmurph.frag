#pragma include "../common.frag"
// Lion Snek Murph - Razult of imprivosed live kidding seshun an Twatch
// Thankx to alkama for the "draw a cat" suggestion
// LIVE SHADER CODING, SHADER SHOWDOWN STYLE, EVERY TUESDAYS 21:00 Uk time:
// https://www.twitch.tv/evvvvil_

// "Those who don't like football don't know the joy of the people" - Victor Hugo

vec2 z,e=vec2(.00035,-.00035); float b,t,tt,g,g2,bb,at; vec3 np,bp,pp,po,no,al,ld;//global vars. About as exciting as Football players discussing their taste in music
float bo(vec3 p,vec3 r){p=abs(p)-r;return max(max(p.x,p.y),p.z);} //box primitive function. Hey, did you know you shouldn't put people in a box? (Unless they fucking fit)
mat2 r2(float r){return mat2(cos(r),sin(r),-sin(r),cos(r));} //rotate function. Short and sweet, just like how acceptance speeches should be in Hollywood. Yeah Leo, it's pretty easy to lecture people from your Ivory tower.
vec2 fb( vec3 p, float s, float m, float b ) // fb "fucking bit" function make a base geometry which we use to make robot faces and outter geometry using more complex positions defined in mp
{ //fb just does a simple triple outlined box, really. Nothing is simple when you dip into triple brews though: try Triple Karmeliet beer and then we'll talk about how shit Budweiser is.
  vec2 h,t=vec2(bo(p,vec3(5,1,1.-b)),5);// Purple gradient box, you gotta start with the first stone and yes it is facing Mecca.
  h=vec2(bo(p,vec3(2.8,0.7,1.2-b)),6);// White box, because everybody loves the northern european socio political model, despite what the hippies in France are wearing or what the old greek ladies are drawing in the sand. (Reference to "Diogo Diogenes vs AlexanderDaGreat 2" - UFC Fight Night 666)
  t=t.x<h.x?t:h; //Merge purple and white geometries while retaining material ID. Sounds like social segregation to me, but who am I kidding? I'm the one buying expensive craft beers helping hipsters achieve "The Great Gentrification Swindle"
  h=vec2(bo(abs(p)-vec3(1,0,0),vec3(0.5,s,0.8-b)),m); //Small rectangles popping out of purple geometry, nice little added detail, like having a wifi connection on your sex toy
  t=t.x<h.x?t:h; //Merge small rectangles and the rest while retaining material ID
  h=vec2(bo(p,vec3(2.8,0.2,1.5-2.*sin(p.x*.1))),3); //Black box over white box, because white and black does a killer contrast. Yeah like killer whales or Newcastle United's home kit (not quite as deadly in attack since Alan Shearer left to swim in a pool of boring punditry)
  t=t.x<h.x?t:h; //Merge black box and the rest while retaining material ID
  t.x*=0.7; return t; //Tweak distance field to avoid artifact and return the whole shit
}
vec2 mp( vec3 p )
{ 
  p.xy*=r2(.3);//tilt the whole fucking scene a bit forward, make it more menacing, like a cross armed BTP officers fining skateboarders in the Corporation of London
  np=bp=pp=p; bp.xy+=vec2(3,12); bp.xy*=r2(5.5-sin(tt)*.2);pp.x=mod(pp.x+tt*5.,20.)-10.; //Setup new positions np, bp and pp used in kifs below and then  passed to fb to generate more geometries
  at=min(0.,(length(p+vec3(sin(tt*.5)*50.,-sin(p.y*.5+tt*5.)*5.,0))-30.)/30.); //spherical field displacement reverse attractor bullshit, name it whatever you wish, I'm way too busy counting the grains of rice in this bag.
  np.xy*=r2(mix(0.,0.8,bb)); //rotate shit to switch faces using bb variable
  for(int i=0;i<4;i++){ //main kifs loop cloning, pushing, pulling rotating each iteration, bit like a sex club but without the overweight bouncer trying to slot himself in.
    np=abs(np)-vec3(2,1.5-at*3.,1.6-at*5.); //abs sysmtery to push out and create more geometry each iteration, influenced by sphreical attractor above so we can push it bulbous. "A squid eating dough in a polyethylene bag is fast and bulbous, got me?"
    np.xz*=r2(.785*float(i)-cos(p.y*.3)*.1+at*.2); //rotate bit more each iteration and influenced by attractor and cos
    np.xy*=r2(.785*float(i)/2.); //rotate bit more each iteration along another axis to make it more face like
    np.xz+=cos(p.y*.4+1.5); // this tweaks face into cheekbones and shit like that. Man I have no idea I do these things under the influence and now I'm sober so none of this makes sense. I have no idea how you tea-total nerds do it.
    if(i<3){ //Bp is for the chin / jaw and we need less iteration this if makes sure of that, politely but with the kind of vigour that you would't wanna fuck with, especially if you are just a piece of code.
      bp=abs(bp)-vec3(1.5,0,1.-at*7.); //another boring kifs bullshit to make more geometry out of nothing, once again eluding everyone as to my lack of Math understanding and lack of giving a fuck about it.
      bp.xz*=r2(.785*float(i)-cos(p.y*.3)*.1); //Are we doing this again? Christ it seems as wasteful as giving ice creams to children. Yeah yeah you guessed it we rotate tiny bit each iter...
      bp.xy*=r2(.785*float(i)/2.); //This line of code cures Cancer, solves the conflicts in the Middle East and gives you a 4 hour erection. Not really, it's just same as above, bit of rotation each iter.
      bp.xz+=cos(p.y*.4+1.5); //again we curve things out into face-like-roundness
    }
    pp=abs(pp)-vec3(5,5.5,0);//Fucking hell one more kifs? One trick poney bullshit right there. Well fuck it I just needed one more kif for the structure at bottom and top
    pp.yz*=r2(0.45);//Rather dull simple shit here brohs...
    pp.xz*=r2(0.1); //...I've seen more exciting stuff happen at a funeral
  }  
  vec2 h,t=fb(np,1.2,3.,0.); //Now we have all the new positions, we call fucking bit function with the more complex position np to create faces
  h=fb(bp,3.+sin(bp.z*.5)*2.,6.,0.); t=t.x<h.x?t:h;//Then we call again fucking bit this time with bp complex position to create the bottom jaw
  h=fb(pp,30.,6.,0.); t=t.x<h.x?t:h; //Finally we call fucking bit a third time with pp complex position to create structure above and bellow
  h=vec2(length(abs(p-vec3(7,1,0))-vec3(0,0,5.-at*15.))-(1.2+sin(p.y+1.)+at*2.5),6);  //EYES - Because when the grim reaper rings the bell, I wanna be able to stare at the cruel bastard.
  h.x=min(h.x,length(abs(p.yz)-vec2(20.,0.))-0.5); // GLOWY TUBES above and bellow robot face. Everything looks better with glow sticks even overweight cyber-goth ravers.
  h.x*=0.3;  g+=(0.1-at*5.)/(0.1+h.x*h.x*(20.-abs(sin(p.x*.2+tt*3.))*18.)); t=t.x<h.x?t:h; //Make tubes glow and sweep da beam along z axis, doesn't quite solve the disparity between the rich and the poor in middle class England but it does brithen up this scene.
  h=vec2(length(cos(p*.5+np*.2-vec3(tt*5.0,0,0))),6);  //PARTICLES: Everybody loves glitter, makes turds look more polished and makes Boris Johnson social policies slightly less ugly to look at.
  h.x=max(h.x,length(p.xz)-(5.+sin(p.y*.1)*20.)); // Ah yes nice fucking bounding trick here broski, nothing clever but this limits the particles to sphere above head rather than them being everywhere.
  h.x*=0.6;  g+=0.1/(0.1+h.x*h.x*400.); t=t.x<h.x?t:h;  //Simple Balkhan glow on the particles (Thankx to Balkhan for the distance glow trick that I rinse just about every week, cheers Balkhan, you da man)
  h=vec2(0.7*(length(p-vec3(3,-3,0))-5.5+at*10.),3);t=t.x<h.x?t:h; // MIDDLE BALL Helps hide mess in middle of face, tones down geometry, adds simple focal point to composition. In other words: if you can't beat them, join them.
  pp=p+vec3(-6.5,-5,0);  //Building a new position pp to make the cat ears
  pp.xz=abs(pp.xz)-vec2(0,8.-at*15.);  //Fuck yeah Einstein! A cat has two ears, so we abs symetry the fucker to have two ears instead of one.
  pp.yz*=r2(0.75); pp.xz*=r2(0.75); //Rotate both ears almost 45 degree in both axis
  b=abs(sin(p.y*.1+3.))*1.7; // Taper the ears with this "b" variable. Wishful thinking: also use "b" variable to taper my wife's nagging.
  h=vec2(bo(pp,vec3(2.-b,10,3.-b)),5);  //CAT EARS, A feline ain't nothing without cat ears. This also applies to demoscener and shader queen Flopine, who also looks good with cat ears.
  h.x=abs(h.x)-.3; //Onion skin the fucking ears to get edges and create ear holes (not to be confused with bum holes)
  pp.xz*=r2(.7); //Seems like one more rotate, why? Not sure many synapses have been vaporized since the making of this shader. Ah yes this rotates the plane that cuts off the ears to reveal the onion skin edges. There, not such a wasteman afterall.
  h.x=max(h.x,pp.x-0.6);h.x*=0.6; //Cut ears with plane to reveal the onion skin edges, then tweak distance field to avoid artifact due to spherical attractor
  t=t.x<h.x?t:h; return t; // Add cat ears and return the whole shit
}
vec2 tr( vec3 ro, vec3 rd ) // main trace / raycast / raymarching loop function 
{
  vec2 h,t= vec2(.1); //Near plane because when it all started the hipsters still lived in Norwich and they only wore tweed.
  for(int i=0;i<128;i++){ //Main loop de loop 
    h=mp(ro+rd*t.x); //Marching forward like any good fascist army: without any care for culture theft. (get distance to geom)
    if(h.x<.0001||t.x>120.) break; //Conditional break we hit something or gone too far. Don't let the bastards break you down!
    t.x+=h.x;t.y=h.y; //Huge step forward and remember material id. Let me hold the bottle of gin while you count the colours.
  }
  if(t.x>120.) t.y=0.;//If we've gone too far then we stop, you know, like Alexander The Great did when he realised his wife was sexting some Turkish bloke. (10 points whoever gets the reference)
  return t;
}
#define a(d) clamp(mp(po+no*d).x/d,0.,1.)
#define s(d) smoothstep(0.,1.,mp(po+ld*d).x/d)
void main()//2 lines above are a = ambient occlusion and s = sub surface scattering
{
  vec2 uv=(fragCoord.xy/iResolution.xy-0.5)/vec2(iResolution.y/iResolution.x,1); //get UVs, nothing fancy, 
  tt=mod(iTime,62.8318);  //Time variable, modulo'ed to avoid ugly artifact. Imagine moduloing your timeline, you would become a cry baby straight after dying a bitter old man. Christ, that's some fucking life you've lived, Steve.
  bb=0.5+clamp(sin(tt*.5),-.5,.5);
  vec3 ro=mix(vec3(1),vec3(1,-1,-1),ceil(sin(tt*.2)))*vec3(30.+sin(tt*.2)*4.,sin(tt*.1)*3.,cos(tt*.2)*15.),//Ro=ray origin=camera position We build camera right here broski. Gotta be able to see, to peep through the keyhole.
  cw=normalize(vec3(0)-ro), cu=normalize(cross(cw,normalize(vec3(0,1,0)))),cv=normalize(cross(cu,cw)),
  rd=mat3(cu,cv,cw)*normalize(vec3(uv,.5)),co,fo;//rd=ray direction (where the camera is pointing), co=final color, fo=fog color
  ld=normalize(vec3(.2,.2,-.3)); //ld=light direction
  co=fo=vec3(.0,.05,.1)*(1.5-length(uv)-.5);//background is blueish with vignette and subtle vertical gradient based on ray direction y axis. It's dark like the heart of people from Norwich.
  z=tr(ro,rd);t=z.x; //Trace the trace in the loop de loop. Sow those fucking ray seeds and reap them fucking pixels.
  if(z.y>0.){ //Yeah we hit something, just like me stumbling home after 7 pints
    po=ro+rd*t; //Get ray pos, know where you at, be where you is.
    no=normalize(e.xyy*mp(po+e.xyy).x+e.yyx*mp(po+e.yyx).x+e.yxy*mp(po+e.yxy).x+e.xxx*mp(po+e.xxx).x); //Make some fucking normals. You do the maths while I count how many instances of Holly Willoughby there really is.
    al=vec3(.4-sin(po.y*.5)*.2,.1,.2); //al=albedo=base color, by default it's a gradient between purple and dark blue. 
    if(z.y<5.) al=vec3(0); //material ID < 5 makes it black
    if(z.y>5.) al=vec3(1); //material ID > 5 makes it white
    float dif=max(0.,dot(no,ld)), //Dumb as fuck diffuse lighting
    fr=pow(1.+dot(no,rd),4.), //Fr=fresnel which adds background reflections on edges to composite geometry better
    sp=pow(max(dot(reflect(-ld,no),-rd),0.),30.); //Sp=specular, stolen from Shane, below you will find: mix(vec3(.8),vec3(1),abs(rd))*al is a sick trick by crundle to tweak colour depending on ray direction
    co=mix(sp+al*(a(.1)*a(.5)+.2)*(dif+s(2.)*.55),fo,min(fr,.5)); //Building the final lighting result, compressing the fuck outta everything above into an RGB shit sandwich
    co=mix(fo,co,exp(-.00002*t*t*t)); //Fog soften things, but it won't stop Sophie's dildo battery from running out.
  }
  fragColor = vec4(pow(co+g*0.2*mix(vec3(.4,.3,.2),vec3(1,.4,.2),-at),vec3(.5)),1);// Naive gamma correction and glow applied at the end. Glow has a mix of colours to make it more interesting.
}

