#pragma include "../common.frag"
//uniform float time;


//#define PI 3.14159265359
#define PI2 6.28318530718

vec4 buildings(vec2 pos){
	vec4 col = vec4(0.0);
    
    float roof = 0.1;
    float bx = pos.x * 20.0;
    float x = 0.05 * floor(bx - 4.0);
    // BX = Position relative to building left
    bx = mod(bx,1.0);
  
    // Build pseudorandom rooftop line
    roof += 0.06 * cos(x * 2.0);
	roof += 0.1 * cos(x * 23.0);
	roof += 0.02 * cos(x * 722.0 );
	roof += 0.03 * cos(x * 1233.0 );
	
    roof += 0.06;
    
    if(pos.y < roof && pos.y > 0.0 && bx > 0.1 * cos(20.0 * pos.x)){
    	col.b += 0.4;
        
        // Draw windows
        float window = abs(sin(200.0 * pos.y));
        window *= abs(sin(20.0 * bx));
        
        // type 1 window
        if(mod(2023.0 * x,2.0) < 0.5){
          	window = floor(1.3 * window);
        	col.rgb += 1.5 * window * vec3(0.9,0.9,0.9);
        }
        // type 2 window
        else if(mod(2983.0 * x,2.0) < 1.3){
        	col.rb += window;
        }
        else {
            if(window > 0.5){
            	col.rg += 0.8;
           	}
        }
      	col.a = 1.0;
    }

    return col;
}

vec4 train(vec2 pos){
	vec4 col = vec4(0.0);
    
    float base = 0.01 * cos(pos.x * 7.0 + time * PI2) + 0.02;
    

    col.r += base;
   	
    if(pos.y > 0.0){
        pos.y -= base;
        // track
        if(pos.y < 0.01){
            // Actual track
            if(pos.y > -0.005){
                col.rg += 0.1;
                col.a = 1.0;
            }
            // supports
            else if(cos(4.0 * (pos.x * 7.0 + time * PI2)) < -0.8){
            	col.rg += 0.1;
                col.a = 1.0;
            }
            
        }
        // train
        else if(pos.y < 0.04 && pos.x < 0.3){
            bool in_y_range = pos.y < 0.02 || pos.y > 0.03;
              	
            if(pos.x < -0.01){
                // Delimit wagons
                if(pos.x > -0.02 || cos(pos.x * 100.0) < 0.9){

                    if(pos.y < 0.018 && pos.y > 0.014 && pos.x < -0.02){
                        col.r += 1.0;	
                        col.a = 1.0;
                    }

                    // windows
                    else if(!in_y_range && pos.x > 0.01){
                        col.a = 0.0;
                    } else if(in_y_range || (cos(pos.x * 400.0) < 0.0) ){
                        col.rgb += 0.5;
                        col.a = 1.0;
                    }
                }
            } else if (pos.x < 0.01) {
                // Front of the train
                // function: 1-x^3
                // Make a suitable x and y axis to plot the function
                float xx = (pos.x + 0.01)/ 0.02;
                float yy = (pos.y - 0.02) / 0.02;
                float func = 1.0 - pow(xx, 3.0);
                
                if(yy < func){
                	col.rgb += 0.5;
                    col.a = 1.0;
                }
            }
        }
    }
    
    return col;
}

vec4 stars(vec2 pos){
	vec4 col = vec4(0.0);
  	float threshold = -0.999;
	pos *= 1.0;
  	if(1.0 * cos(pos.x * 1000.0) + 1.0 * cos(30000.0 * pos.x + cos(10000.0 * pos.y)) < threshold){
      	if(cos(pos.y * 100.0 + 10000.0 * cos(pos.x * 10.0)) < threshold){
      		col += 1.0;
          	col.a = 1.0;
        }
    }
  	return col;
}

void mainImage( out vec4 fragColor, in vec2 gl_FragCoord.xy )
{
	vec2 uv = gl_FragCoord.xy.xy / iResolution.xy;
    
    
	vec4 col = vec4(0.0);
    
    vec2 pos = uv - vec2(0.5);
    
    pos.y += 0.3;
    
    vec2 water = vec2(0.0);
    
    // Offset for water waves
    water.x += 0.003 * cos(pos.y * 150.0 + time * PI2);
    water.x += 0.001 * cos(pos.y * 250.0 +  time * PI2);
    water.x += 0.001 * cos(pos.y * 4050.0 + time * PI2);
    
    col += 0.6 * buildings(pos + vec2(time * 0.2, 0.0)); // Buildings
    col += 0.3 * buildings(pos * vec2(1.0, -1.0) + vec2(time * 0.2, 0.0) + water); // Reflection of buildings
    
    vec4 t = train(pos);
    col = t.a * t + (1.0 - t.a) * col; // Train
    t = train(pos * vec2(1.0, -1.0) + water); // Reflection of train
    t.a *= 0.3;
    col = t.a * t + (1.0 - t.a) * col;
    
    
    // Sky + water color
  	if(col.a < 0.1){
        if(pos.y < 0.0){
            col.b += 0.2;
        } else {
            col.rgb += vec3(0.1,0.1,0.3);
            col += stars(pos);
        }
    }
  
    // Plane
    if(pos.x < -0.1 && pos.y > 0.6 && pos.y < 0.602){
    	col.rgb += 0.22 +
            0.1 * cos(pos.x + iTime) + 
            0.03 * cos(pos.x * 200.0 + 10.0 * iTime);
    }
    
    float d = distance(pos,vec2(-0.1, 0.6));
    
    if(d < 0.005){
    	col.r += (1.0 - d/0.01) * (pow(cos(time * PI),30.0)  + 0.5);
    	col.b += (1.0 - d/0.01) * (pow(cos(time * PI + 1.0),30.0)  + 0.5);
    }
    
    col.a = 1.0;
    
	fragColor = col;
}


void main(){
    vec2 uv = gl_FragCoord.xy.xy * iResolution.xy;
    vec4 col;

    mainImage(col, uv);

    fragColor = col;
}
