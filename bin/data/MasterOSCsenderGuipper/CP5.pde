void initcp5() {

  PFont font = createFont("arial", 20);
  cp5 = new ControlP5(this);



  float slider_x = 20;
  float slider_y = 20;
  float slider_sepyy = 40;
  int slider_w = 200;
  int slider_h = 30;

  cp5.addSlider("suavizado")
    .setPosition(slider_x, slider_y)
    .setRange(1, 30)
    .setSize(slider_w, slider_h)
    ;
  cp5.addSlider("suavizadoadd")
    .setPosition(slider_x, slider_y+=slider_sepyy)
    .setRange(0, 1)
    .setSize(slider_w, slider_h)
    ;
  cp5.addSlider("suavizadomult")
    .setPosition(slider_x, slider_y+=slider_sepyy)
    .setRange(0, 2)
    .setValue(1)
    .setSize(slider_w, slider_h)
    ;
  cp5.addTextfield("sendmic1")
    .setPosition(slider_x, slider_y+=slider_sepyy)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setFocus(true)
    .setColor(color(255, 0, 0))
    ;
  cp5.addTextfield("sendmic2")
    .setPosition(slider_x, slider_y+=slider_sepyy)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setFocus(true)
    .setColor(color(255, 0, 0))
    ;
  cp5.addTextfield("sendmic3")
    .setPosition(slider_x, slider_y+=slider_sepyy)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setFocus(true)
    .setColor(color(255, 0, 0))
    ;


  /****************************************************************ANIMACION *********************************/  /////////////////////////////////
  float slider_y2 = 20;
  cp5.addSlider("recorridospeed")
    .setPosition(width-slider_w*1.5, slider_y2)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setRange(0.0, 0.1)
    ;

  cp5.addTextfield("sendposx")
    .setPosition(width-slider_w*1.5, slider_y2+=slider_sepyy)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setFocus(true)
    .setColor(color(255, 0, 0))
     .setText("/circuloresolution/posx");
    ;
  cp5.addTextfield("sendposy")
    .setPosition(width-slider_w*1.5, slider_y2+=slider_sepyy)
    .setSize(slider_w, slider_h)
    .setFont(font)
    .setFocus(true)
    .setColor(color(255, 0, 0))
    .setText("/circuloresolution/posy");
    ;
  /*  cp5.addTextfield("sendmic4")
   .setPosition(slider_x, slider_y+=slider_sepyy)
   .setSize(slider_w, slider_h)
   .setFont(font)
   .setFocus(true)
   .setColor(color(255, 0, 0))
   ;*/
}


/*public void input(String theText) {
 // automatically receives results from controller input
 println("a textfield event for controller 'input' : "+theText);
 }*/
