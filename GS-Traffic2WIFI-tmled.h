
void tmshowleds(String target, int ledmode, int value)
{
  // Defining brightness
  int brightness = 1;
  brightness = preferences.getInt("tmbright", 0);

  uint32_t red = tmleds.Color(brightness, 0, 0);
  uint32_t green = tmleds.Color(0, brightness, 0);
  uint32_t blue = tmleds.Color(0, 0, brightness);
  uint32_t orange = tmleds.Color(brightness, brightness, 0);
  uint32_t white = tmleds.Color(brightness, brightness, brightness);
  uint32_t black = tmleds.Color(0, 0, 0);


  if ((ledmode == 3) && (target == "circle")) // Show Target in Circle - Value=Degree - OK-nontested
  {
    tmleds.fill(black, tmcirclestart, tmcirclecount);
    tmleds.show();
    int led = (value * tmcirclecount / 360) + tmcirclestart;
    tmleds.setPixelColor(led, orange);
    tmleds.show();
  }


  if (ledmode == 1) // Demo
  {
    // Spinn the circle
    for (int led = tmcirclestart; led <= tmcirclestart + tmcirclecount - 1; led++)
    {
      tmleds.setPixelColor(led, blue);
      if (led > tmcirclestart)
      {
        tmleds.setPixelColor(led - 1, red);
      }
      if (led > tmcirclestart + 1)
      {
        tmleds.setPixelColor(led - 2, green);
      }
      tmleds.show();
      delay(30);
    }
    tmleds.fill(black, 0, TMODLEDCOUNTER);
    tmleds.show();
    delay(200);

    tmleds.fill(black, 0, TMODLEDCOUNTER);
    tmleds.show();
    delay(50);
    
  }

 if (ledmode == 10) // Show all LEDs in red for Brightness-Config
  {
    tmleds.fill(red, 0, TMODLEDCOUNTER);
    tmleds.show();
  }

 if (ledmode == 11) // Turn off all LED for Brightness-Config
  {
    tmleds.fill(black, 0, TMODLEDCOUNTER);
    tmleds.show();
  }

  if (ledmode == 2) // Show Firmware on Circle
  {
    tmleds.fill(black, tmcirclestart, tmcirclecount - 1);
    tmleds.show();
    int ledfwcount = 1;
    for (int i = 0; i < versionmajor; i++)
    {
      tmleds.setPixelColor(tmcirclestart + i, red);
      ledfwcount = tmcirclestart + i;
    }
    tmleds.setPixelColor(ledfwcount + 1, green);
    for (int i = 0; i < versionminor; i++)
    {
      tmleds.setPixelColor(ledfwcount + 2 + i, red);
    }
    tmleds.show();
    delay(1000);
    tmleds.fill(black, 0, TMODLEDCOUNTER);
    tmleds.show();
  }
}

void showdemo()
{
  tmshowleds("", 1, 0);
  delay(300);
  tmshowleds("", 1, 0);
  delay(300);
  tmshowleds("circle", 3, 320);
  webserver.send(200, "text/html", "OK" );
}
