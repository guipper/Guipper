import oscP5.*;
import netP5.*;
import themidibus.*; //Import the library
import ddf.minim.*; //Librería de sonido.
import controlP5.*;


//SONIDO
Minim minim;
AudioInput in;


MidiBus myBus; // The MidiBus
OscP5 oscP5;
NetAddress toguipper;
NetAddress tomaestrodeletras;

ControlP5 cp5;


//OSC DESDE ABLETON AL GUIPPER ESTE SERIA COMO EL SENDER QUE HACE EL CALCULO :
/*********************************/

int [] notasOSC = {36, 42};
float [] pruebasuavizado = {0, 0};
/********************************/




int lastmidivalue;
int lastmidicontroller;

float suavizado;
float micfinal;
float suavizadoadd;
float suavizadomult;
float recorridospeed;
Recorrido recorrido;


boolean guipperActive = true;


void setup() {
  oscP5 = new OscP5(this, 6000);//RECIEVER


  toguipper = new NetAddress("127.0.0.1", 5080); //SENDER AL GUIPPER
  tomaestrodeletras  = new NetAddress("127.0.0.1", 4050); //SENDER AL MAESTRO DE LETRAS

  size(1200, 600);
  background(0);

  MidiBus.list(); // List all available Midi devices on STDOUT. This will show each device's index and name.

  myBus = new MidiBus(this, 1, 4); // Create a new MidiBus with no input device and the default Java Sound Synthesizer as the output device.

  iniciarMinim();
  initcp5();

  //init_arduino();
  recorrido = new Recorrido();
}


void draw() {
  myBus.sendNoteOn(1, 60, 100);

  // in.mix.get(250);
  float freq = abs(in.mix.get(250)*1024);
  if (freq > micfinal) {
    micfinal = freq;
  } else {
    micfinal-=suavizado;
  }
  micfinal = constrain(micfinal, 0, 1024);

  textAlign(CENTER, CENTER);
  background(0);
  textSize(18);

  float x = width/2;
  float y = 50;
  float sepy = 50;

  text("LAST MIDI VALUE: "+lastmidivalue, x, y);
  text("LAST MIDI CONTROLLER: "+lastmidicontroller, x, y+sepy);

  //drawArduino();
  // updateArduino();

  float rectalto = 230;
  float rectancho = 40;
  float rectx = 300;
  float recty = rectalto/2+20 ;

  float mapmicf = map(micfinal, 0, 1024, 0, 1) + suavizadoadd;
  mapmicf = constrain(mapmicf, 0, 1);
  rectMode(CENTER);
  noFill();
  strokeWeight(3);
  stroke(255);
  rect(rectx, recty, rectancho, rectalto);
  fill(255);
  rectMode(CORNER);
  rect(rectx-rectancho/2,
    recty - mapmicf*rectalto + rectalto/2,
    rectancho,
    mapmicf*rectalto+suavizadoadd*rectalto);
  /// println("freq" + freq);

  if (cp5.get(Textfield.class, "sendmic1").getText().length() > 0) {
    sendToGuipper(mapmicf, cp5.get(Textfield.class, "sendmic1").getText());
  }
  if (cp5.get(Textfield.class, "sendmic2").getText().length() > 0) {
    sendToGuipper(mapmicf, cp5.get(Textfield.class, "sendmic2").getText());
  }
  if (cp5.get(Textfield.class, "sendmic3").getText().length() > 0) {
    sendToGuipper(mapmicf, cp5.get(Textfield.class, "sendmic3").getText());
  }

  if (recorrido.speed > 0.1) {
    sendToGuipper(map(recorrido.pos.x, 0, width, 0, 1), cp5.get(Textfield.class, "sendposx").getText());
    sendToGuipper(map(recorrido.pos.y, 0, height, 0, 1), cp5.get(Textfield.class, "sendposy").getText());
  }
  //sendToGuipper(mapmicf+suavizadoadd, cp5.get(Textfield.class, "sendmic4").getText());

  //lastmidicontroller

  recorrido.run();

  String standart [] = {"/circuloresolution_nico/size", "/circuloresolution_nico/size2"};
  for (int i=0; i<2; i++) {
    pruebasuavizado[i]-=10;
    if (pruebasuavizado[i] > 1) {
      //pruebasuavizado[i]-=10;
      sendToGuipper(map(pruebasuavizado[i], 0, 255, 0, 1), "/openguinumber/param"+str(i));
      //  sendToGuipper(map(pruebasuavizado[i], 0, 255, 0, 1), standart[i] );
    }
  }
}
void keyPressed() {
  if (key == 'k') {
    recorrido.puntos.clear();
    recorrido.ppos =0;
  }
  if (key == 'a') {
    recorrido.agregarpunto();
  }


  if (key == 'q') {
    guipperActive =!guipperActive;
  }
}
