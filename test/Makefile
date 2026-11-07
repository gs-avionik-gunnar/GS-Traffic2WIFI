SKETCH := ../GS-Traffic2WIFI
CXXFLAGS := -std=gnu++11 -O1 -g -Wall -Wextra -I$(SKETCH)
CORE := $(SKETCH)/Nmea.cpp $(SKETCH)/State.cpp $(SKETCH)/Gdl90.cpp $(SKETCH)/Simulator.cpp $(SKETCH)/Regions.cpp $(SKETCH)/Gfx.cpp $(SKETCH)/Qr.cpp $(SKETCH)/DialUi.cpp $(SKETCH)/DialFont.cpp $(SKETCH)/FlightLog.cpp $(SKETCH)/CallsignDb.cpp

all: run

test_core: test_main.cpp $(CORE)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined $^ -o $@

run: test_core
	./test_core

# Compile-only check of the Arduino glue against minimal stub headers (no ESP32 core needed).
syntax:
	for f in Settings FlightStore CallsignStore Bridge WebUi StatusLed App Regions DialHw Ble; do \
	  $(CXX) -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -Istubs -I$(SKETCH) $(SKETCH)/$$f.cpp || exit 1; \
	done
	$(CXX) -std=gnu++11 -fsyntax-only -Wall -Wno-unused-parameter -Istubs -I$(SKETCH) -x c++ $(SKETCH)/GS-Traffic2WIFI.ino
	$(CXX) -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -DCONFIG_IDF_TARGET_ESP32S3 -Istubs -I$(SKETCH) $(SKETCH)/Ble.cpp
	$(CXX) -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -DGS_BLE_ON_CLASSIC=0 -Istubs -I$(SKETCH) $(SKETCH)/Ble.cpp
	$(CXX) -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -DCONFIG_IDF_TARGET_ESP32S3 -DESP_ARDUINO_VERSION_MAJOR=3 -Istubs -I$(SKETCH) $(SKETCH)/DialHw.cpp
	$(CXX) -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -DCONFIG_IDF_TARGET_ESP32S3 -Istubs -I$(SKETCH) -x c++ $(SKETCH)/GS-Traffic2WIFI.ino 2>&1 | grep -v "^$$" || true

clean:
	rm -f test_core
