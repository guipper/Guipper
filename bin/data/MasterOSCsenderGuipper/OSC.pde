void sendToGuipper(float _value, String _address) {
  /* in the following different ways of creating osc messages are shown by example */
  OscMessage myMessage = new OscMessage(_address);
  myMessage.add(_value); /* add an int to the osc message */
  /* send the message */
  oscP5.send(myMessage, toguipper);

}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {
  /* print the address pattern and the typetag of the received OscMessage */
  print(" String: "+theOscMessage.addrPattern());
  print(" Tipo de variable: "+theOscMessage.typetag());

  println(" Value " + theOscMessage.get(0).floatValue());
  if (theOscMessage.addrPattern().equals("/Note1")) {
    
    for (int i =0; i<2; i++) {
      if (theOscMessage.get(0).intValue() == notasOSC[i]) {
        println("NOTA : " + notasOSC[i]);
        pruebasuavizado[i] = 200;
      }
    }
    /* if(theOscMessage.get(0).intValue() == 36){
     pruebasuavizado = 255;
     }*/
    // println("VALOR"+ theOscMessage.get(0).intValue());
    //sendToGuipper(1.0, "/openguinumber/param0");
    //x =   theOscMessage.get(0).intValue();
    //y =   theOscMessage.get(1).intValue();
  } else {
    //println("NOTE1");
    //println("VALOR"+ theOscMessage.get(0).intValue());
    //  sendToGuipper(0.0, "/openguinumber/param0");
  }
}
