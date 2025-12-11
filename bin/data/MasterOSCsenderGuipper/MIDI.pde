
//int [] midi_number = {73,9,10,72,14,15,16,17,74,71,18,107,79,78,26,27,28}; //MAPEO PARA ORIGIN 37
//int [] midi_number = {0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23,24};// Y ESTE DE QUE CARAJO SERA NO !??!?!?! //NANOKONTROL 2 Para otra.
int [] midi_number = {16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7, 24}; //NANOKONTROL 2 PARA ESTE.



//Teclas especiales :
final int FLECHADER = 62; //SETEA LA VENTANA ABIERTA DE LA DERECHA.
final int FLECHAIZQ = 61; //SETEA LA VENTANA ABIERTA DE IZQUIERDA.
final int SETACTIVESHADER = 60; //EL RENDER DE SALIDA.
final int CYCLEONOFF = 46; //SI PRENDE EL CYCLEONOFF.



//ESTA ES LA DE LAS FLECHITAS DE LA ESQUINA SUPERIOR IZQUIERDA PARA QUE PASEN APENAS TOCAS DIRECTAMENTE :

final int FLECHADER2 = 59;
final int FLECHAIZQ2 = 58;


void noteOn(int channel, int pitch, int velocity) {
  // Receive a noteOn
  println();
  println("Note On:");
  println("--------");
  println("Channel:"+channel);
  println("Pitch:"+pitch);
  println("Velocity:"+velocity);
}

void noteOff(int channel, int pitch, int velocity) {
  // Receive a noteOff
  println();
  println("Note Off:");
  println("--------");
  println("Channel:"+channel);
  println("Pitch:"+pitch);
  println("Velocity:"+velocity);
}

void controllerChange(int channel, int number, int value) {
  // Receive a controllerChange
  println();
  println("Controller Change:");
  println("--------");
  println("Channel:"+channel);
  println("Number:"+number);
  println("Value:"+value);

  lastmidivalue = value;
  lastmidicontroller = number;
  for (int i = 0; i< midi_number.length; i++) {
    if (number == midi_number[i]) {
      println("MIDI NUMBER: " + i);
      String send = "/openguinumber/param"+str(i);
      
      float valuetosend = map(value, 0, 127, 0, 1);
      
      //ESTO ES PORQUE SE ROMPIO UN POTE EN MI NANOKONTROL Y SU VALOR SOLO VA DE O a 30:(
      if(number == 0){
        valuetosend = map(value, 0, 31, 0, 1);
      }
      sendToGuipper(valuetosend, send);
    }
  }

  if (number == FLECHADER && value == 127) {
    sendToGuipper(0, "/nextshader");
  }

  if (number == FLECHAIZQ && value == 127) {
    sendToGuipper(0, "/prevshader");
  }
  if (number == SETACTIVESHADER && value == 127) {
    sendToGuipper(0, "/setactiveshader");
  }

  if (number == CYCLEONOFF && value == 127) {
    sendToGuipper(0, "/setactivecycle");
  }


  if (number == FLECHADER2 && value == 127) {
    //sendToGuipper(0, "/nextshader");
    //sendToGuipper(0, "/setactiveshader");
    
    sendToGuipper(0, "/nextshader_gallerymode");
    
  }

  if (number == FLECHAIZQ2 && value == 127) {
    //sendToGuipper(0, "/prevshader");
    //sendToGuipper(0, "/setactiveshader");
    
    
      sendToGuipper(0, "/prevshader_gallerymode");
  }
  
  
  
}
