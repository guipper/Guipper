//int [] midi_number = {73,9,10,72,14,15,16,17,74,71,18,107,79,78,26,27,28}; //MAPEO PARA ORIGIN 37
//int [] midi_number = {0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23,24};// Y ESTE DE QUE CARAJO SERA NO !??!?!?! //NANOKONTROL 2 Para otra.

class MidiKeymap {
  final int[] sliderCc;
  private final HashMap<String, Integer> keysByName = new HashMap<String, Integer>();

  MidiKeymap(int[] _sliderCc) {
    sliderCc = _sliderCc;
  }

  void setKey(String keyName, int midiNumber) {
    keysByName.put(keyName, midiNumber);
  }

  int getKey(String keyName) {
    if (keysByName.containsKey(keyName)) {
      return keysByName.get(keyName);
    }
    return -1;
  }
}

MidiKeymap midiKeymap = buildMpkMini2Keymap();

MidiKeymap buildMpkMini2Keymap() {
  MidiKeymap keymap = new MidiKeymap(new int[]{1, 2, 3, 4, 8, 7, 6, 5, 20, 21, 22, 23, 19, 18, 17, 16}); //MPKmini II PARA ESTE.

  keymap.setKey("nextShader", 48); //SETEA LA VENTANA ABIERTA DE LA DERECHA.
  keymap.setKey("prevShader", 49); //SETEA LA VENTANA ABIERTA DE IZQUIERDA.
  keymap.setKey("setActiveShader", 44); //EL RENDER DE SALIDA.
  keymap.setKey("cycleOnOff", 51); //SI PRENDE EL CYCLEONOFF.

  //ESTA ES LA DE LAS FLECHITAS DE LA ESQUINA SUPERIOR IZQUIERDA PARA QUE PASEN APENAS TOCAS DIRECTAMENTE :
  keymap.setKey("nextShaderGallery", 46);
  keymap.setKey("prevShaderGallery", 47);

  return keymap;
}

// MIDI debug / learn mode (for new devices)
boolean midiDebugEnabled = true;
boolean midiLearnMode = true;
ArrayList<Integer> learnedCC = new ArrayList<Integer>();
ArrayList<Integer> learnedNotes = new ArrayList<Integer>();

int getMappedSliderIndex(int number) {
  for (int i = 0; i < midiKeymap.sliderCc.length; i++) {
    if (number == midiKeymap.sliderCc[i]) {
      return i;
    }
  }
  return -1;
}

float mapControllerValueToUnitRange(int number, int value) {
  // Legacy fix for broken hardware potentiometer behavior
  if (number == 0) {
    return map(value, 0, 31, 0, 1);
  }
  return map(value, 0, 127, 0, 1);
}

void sendMappedSliderIfAny(int number, int value) {
  int mappedIndex = getMappedSliderIndex(number);
  if (mappedIndex == -1) {
    return;
  }

  println("MIDI NUMBER: " + mappedIndex);
  String send = "/openguinumber/param" + str(mappedIndex);
  float valuetosend = mapControllerValueToUnitRange(number, value);
  sendToGuipper(valuetosend, send);
}

void handleActionButtons(int number, int value) {
  // Different controllers send "pressed" as 127, 1 or another positive value.
  if (value <= 0) {
    return;
  }

  if (number == midiKeymap.getKey("nextShader")) {
    sendToGuipper(0, "/nextshader");
  }

  if (number == midiKeymap.getKey("prevShader")) {
    sendToGuipper(0, "/prevshader");
  }

  if (number == midiKeymap.getKey("setActiveShader")) {
    sendToGuipper(0, "/setactiveshader");
  }

  if (number == midiKeymap.getKey("cycleOnOff")) {
    sendToGuipper(0, "/setactivecycle");
  }

  if (number == midiKeymap.getKey("nextShaderGallery")) {
    sendToGuipper(0, "/nextshader_gallerymode");
  }

  if (number == midiKeymap.getKey("prevShaderGallery")) {
    sendToGuipper(0, "/prevshader_gallerymode");
  }
}

void registerLearnedCC(int number) {
  if (!learnedCC.contains(number)) {
    learnedCC.add(number);
    printMidiLearnSummary();
  }
}

void registerLearnedNote(int pitch) {
  if (!learnedNotes.contains(pitch)) {
    learnedNotes.add(pitch);
    printMidiLearnSummary();
  }
}

String formatIntArray(ArrayList<Integer> values) {
  String out = "";
  for (int i = 0; i < values.size(); i++) {
    out += values.get(i);
    if (i < values.size()-1) out += ", ";
  }
  return out;
}

void printMidiLearnSummary() {
  println();
  println("=== MIDI LEARN SUMMARY ===");
  println("CC learned order   : [" + formatIntArray(learnedCC) + "]");
  println("NOTE learned order : [" + formatIntArray(learnedNotes) + "]");
  println("COPY slider array  : new int[]{" + formatIntArray(learnedCC) + "};");
  println("==========================");
}


void noteOn(int channel, int pitch, int velocity) {
  if (midiDebugEnabled) {
    println("[MIDI][NOTE ON] ch=" + channel + " pitch=" + pitch + " vel=" + velocity);
  }
  if (midiLearnMode && velocity > 0) {
    registerLearnedNote(pitch);
  }

  // Pads/buttons frequently arrive as NOTE ON instead of CC.
  handleActionButtons(pitch, velocity);
}

void noteOff(int channel, int pitch, int velocity) {
  if (midiDebugEnabled) {
    println("[MIDI][NOTE OFF] ch=" + channel + " pitch=" + pitch + " vel=" + velocity);
  }
}

void controllerChange(int channel, int number, int value) {
  int mappedIndex = getMappedSliderIndex(number);
  if (midiDebugEnabled) {
    println("[MIDI][CC] ch=" + channel + " cc=" + number + " val=" + value + " mappedIndex=" + mappedIndex);
  }
  if (midiLearnMode) {
    registerLearnedCC(number);
  }

  lastmidivalue = value;
  lastmidicontroller = number;
  sendMappedSliderIfAny(number, value);
  handleActionButtons(number, value);
}
