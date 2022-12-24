class Recorrido {

  PVector pos ;
  float size = 50;
  boolean agarrado;
  boolean ismoving ; 

  int ppos = 0;
  float move = 0;

  String name;

  float minpan = -1, maxpan = 1;
  float mindb = -20, maxdb = 6;

  //PUNTOS: 
  float psize = 20; //Size de los puntos;
  boolean pagarrado = false;
  ArrayList<PVector> puntos;
  float speed = 0.05;
  int Npagarrado = -1;

  Recorrido() {

    puntos = new ArrayList<PVector>();
    pos = new PVector(width/2, height/2);
    PVector [] initpoints = {new PVector(width/2, height/2), 
      new PVector(random(width), random(height)), 
      new PVector(random(width), random(height)), 
      new PVector(random(width), random(height)), 
      new PVector(random(width), random(height)), 
      new PVector(random(width), random(height))
    };

    for (int i =0; i<initpoints.length; i++) {
      puntos.add(initpoints[i]);
      //println("Pan P"+i+": "+ map(initpoints[i].x, 0, width, -1, 1));
      print("Pan P"+i+": "+ map(initpoints[i].x, 0, width, -100, 100) + "%");
      println(" Vol P"+i+": "+ map(initpoints[i].y, 0, height, -20, 6) + "Db");
    }
    println(" ");
  }

  void run() {
    display();
    renderPuntos();
    moversobrelospuntos();
  }
  void agregarpunto() {
    puntos.add(new PVector(mouseX, mouseY));
  }
  void display() {

    strokeWeight(5);
    stroke(255);
    fill(255);

    if (overCircle(pos.x, pos.y, size)) {
      fill(255);
      if (mousePressed) {
        agarrado = true;
        fill(255, 0, 0);
      }
    }

    if (!mousePressed) {
      agarrado = false;
    }

    if (agarrado) {
      pos.x = mouseX;
      pos.y = mouseY;
    }

    ellipse(pos.x, pos.y, size, size);
    textAlign(CENTER, CENTER);
  }

  void renderPuntos() {
    strokeWeight(3);
    stroke(255);
    for (int i=puntos.size()-1; i>=0; i--) {
      if (i > 0) {
        line(puntos.get(i-1).x, puntos.get(i-1).y, puntos.get(i).x, puntos.get(i).y);
      }
    }


    for (int i=puntos.size()-1; i>=0; i--) {
      if (overCircle(puntos.get(i).x, puntos.get(i).y, psize)) {
        if (mousePressed) {
          pagarrado = true;
          Npagarrado = i;
          fill(255, 255, 0);
        } else {
          fill(220, 220, 0);
        }
      } else {
        fill(180, 180, 0);
      }
      ellipse(puntos.get(i).x, puntos.get(i).y, psize, psize);
      fill(255);
      textSize(18);
      text("N: " + i, puntos.get(i).x, puntos.get(i).y+25);
    }

    if (pagarrado && Npagarrado != -1) {
      puntos.get(Npagarrado).x = mouseX;  
      puntos.get(Npagarrado).y = mouseY;
    }

    if (!mousePressed) {
      pagarrado = false;
      Npagarrado = -1;
    }
  }

  void moversobrelospuntos() {
    if (puntos.size() > 1) {
      move+=recorridospeed;  
      //println("PPOS : " + ppos);
      //println("move : " + move);
      if (move <= 1) {
        if (ppos == puntos.size()-1) {
          pos.x = puntos.get(ppos).x * (1- move) + puntos.get(0).x * move;
          pos.y = puntos.get(ppos).y * (1- move) + puntos.get(0).y * move;
        } else {
          pos.x = puntos.get(ppos).x * (1- move) + puntos.get(ppos+1).x * move;
          pos.y = puntos.get(ppos).y * (1- move) + puntos.get(ppos+1).y * move;
        }
      }
      if (move > 1) {
        move =0;
        ppos++;
        if (ppos == puntos.size()) {
          ppos = 0;
        }
      }
    }
  }

  //Si no toca ningun punto devuelve -1, si no, el punto que toca
  /*int puntoquetoca() {
   for (int i =0; i<f3.puntos.size(); i++) {
   if (overCircle(f3.puntos.get(i).x, f3.puntos.get(i).y, f3.psize)) {
   return i;
   }
   }
   return -1;
   }*/
}

boolean overRect_corner(float x, float y, float w, float h) {
  if (mouseX > x && mouseX < x+w && mouseY > y && mouseY < y+h) {
    return true ;
  } else {
    return false;
  }
}

boolean overCircle(float x, float y, float diameter) {
  float disX = x - mouseX;
  float disY = y - mouseY;
  if (sqrt(sq(disX) + sq(disY)) < diameter/2 ) {
    return true;
  } else {
    return false;
  }
}
