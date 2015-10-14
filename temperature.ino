bool type_s = false;

void printAddress(byte *a) {
  for(int i = 0; i < 8; i++) {
    Serial.print(' ');
    Serial.print(a[i], HEX);
  }
}
boolean initTemperature(boolean debug) {
  // Get ds18b20 temperature sensor
  if ( !ds.search(addr)) {
    if(debug){
      Serial.println("No more addresses.");
      Serial.println();
    }
    ds.reset_search();
    delay(250);
    return false;
  }

  if(debug){
    Serial.print("ROM =");
    printAddress(addr);
    /*for(int i = 0; i < 8; i++) {
      Serial.print(' ');
      Serial.print(addr[i], HEX);
    }*/
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      if(debug){
        Serial.println("CRC is not valid!");
      }  
      return false;
  }
  
  if(debug){
    Serial.println();
  }

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      if(debug){
        Serial.println("  Chip = DS18S20");  // or old DS1820
      }
      type_s = true;
      return true;
    case 0x28:
      if(debug){
        Serial.println("  Chip = DS18B20");
      }
      type_s = false;
      return true;
    case 0x22:
      if(debug){
        Serial.println("  Chip = DS1822");
      }
      type_s = false;
      return true;
    default:
      if(debug){
        Serial.println("Device is not a DS18x20 family device.");
      }
      return false;
  }
  return false;
}

float getTemperature(boolean debug){
  if(debug) {
    Serial.print("Get last known addres: ");
    printAddress(addr);
    Serial.println("");
  }
  getTemperature(addr, debug);  
}

float getTemperature(byte *sensor, boolean debug)
{
  ds.reset();
  ds.select(sensor);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  ds.reset();
  ds.select(sensor);    
  ds.write(0xBE);         // Read Scratchpad

  byte data[12];
  for (int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  int16_t raw = (data[1] << 8) | data[0];

  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];

      if(debug){
        Serial.println("12 bit mode");
      }
    }else{
      if(debug){
        Serial.println("9 bit mode");
      }
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) {
      raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      if(debug){
        Serial.println("9 bit mode");
      }
    }
    else if (cfg == 0x20) {
      raw = raw & ~3; // 10 bit res, 187.5 ms
      if(debug){
        Serial.println("10 bit mode");
      }
    }
    else if (cfg == 0x40) {
      raw = raw & ~1; // 11 bit res, 375 ms
      if(debug){
        Serial.println("11 bit mode");
      }
    } else {
      //// default is 12 bit resolution, 750 ms conversion time
      if(debug){
        Serial.println("12 bit mode");
      }
    }
  }
  float celsius = (float)raw / 16.0;

  return celsius;
}
