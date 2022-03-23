#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cant;
uniform float poly1_puntas;
uniform float poly1_size;
uniform float poly1_diffuse;
uniform float poly1_angle;
uniform float poly2_puntas;
uniform float polys_speed;
uniform float poly2_angle;
uniform float anglemulti;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
    float fix = resolution.x/resolution.y;
    uv.x*=fix;

    vec2 p = vec2(0.5*fix,0.5) - uv;
    float r = length(p);
    float a = atan(p.x,p.y);

    int cantidad = int(mapr(cant,1.0,100.0));//Defino la cantidad de iteraciones que tendra mi for
    int puntas1 = int(mapr(poly1_puntas,2.0,30.0));
	int puntas2 = int(mapr(poly2_puntas,2.0,30.0));
	
	if(mod(cantidad,2) == 0){
		cantidad++;
	}
	float mappoly1_size = mapr(poly1_size,0.00,0.2);
	float mappoly1_diffuse = mapr(poly1_diffuse,0.00,0.2);
	
	float angle1 = mapr(poly1_angle,0.0,PI*2);
	float angle2 = mapr(poly2_angle,0.0,PI*2);
	
	
	float speed1 = mapr(polys_speed,0.0,2.0);
	
	if(polys_speed > 0.48 && polys_speed < 0.52){
		speed1 = 0.0;
	}

	vec3 fin = vec3(0.0);//Defino un vec3 en el que ire sumando los circulos.

    vec2 uv2 = uv;
    for(int i =0; i< cantidad; i++){

        float index = i*PI*2.0/cantidad;
        index*=anglemulti;
        uv2-=vec2(0.5*fix,0.5);
        uv2 = scale(vec2(0.9))*uv2;
        uv2+=vec2(0.5*fix,0.5);

        vec3 col1 = vec3(1.0,0.0,0.0);
        vec3 col2 = vec3(0.0,0.0,1.0);

        vec3 colf = mix(col1,col2,(i+1)/float(cantidad));
        if(mod(i,2) == 0){
            fin+= poly(uv2,
			vec2(0.5*fix,0.5),
			mappoly1_size,
			mappoly1_size+mappoly1_diffuse,
			puntas1,
			angle1+speed1*time+index);
        }else{
			fin-= poly(uv2,
			vec2(0.5*fix,0.5),
			mappoly1_size,
			mappoly1_size+mappoly1_diffuse,
			puntas2,
			angle2-speed1*time+index);
          //  fin-= poly(uv2,vec2(0.5*fix,0.5),mappoly2_size,mappoly2_diffuse,puntas2,0.0);
        }

    }
    gl_FragColor = vec4(fin,1.0);
}









