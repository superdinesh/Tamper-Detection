#include <EEPROM.h>

// Pin Definitions
#define RED_LED_PIN 5
#define GREEN_LED_PIN 6
#define ORANGE_LED_PIN 9
#define LDR_PIN A1

// EEPROM Addresses
#define KEY_START_ADDR 10     // Address for the saved key
#define STATE_ADDR 0          // Address to save the device state

// Reserved Commands
const String LOCK_COMMAND = "LOCKDEVI";
const String UNLOCK_COMMAND = "UNLOCKDE";
const String HEAR_MODE_COMMAND = "HEARMODE";
const String CLEAR_EEPROM_COMMAND = "CLEAREEP";
const String RESET_COMMAND = "RESETMOD";
const String LED_STATUS_COMMAND = "LEDSTATUS";
const String DEBUG_COMMAND = "DEBUG_wallence";

// Device States
enum DeviceState {
  ORANGE, // Default state
  GREEN,  // Locked
  RED     // Tampered
};

// Global Variables
String currentKey = "12345678"; // Default key
bool isTampered = false;
bool isLocked = false;         // Initial state is unlocked
bool inHearMode = false;       // Hear mode flag
bool unlockModeActive = false; // Flag to indicate if unlock mode is active
String lastIRCommand = "";
bool waitingForKey = false;    // State to check if we are waiting for the unlock key

// Threshold for light detection
const int LDR_THRESHOLD = 800;

DeviceState currentState = ORANGE;

void setup() {
  Serial.begin(115200);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(ORANGE_LED_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  // Load previous state and key from EEPROM
  loadDeviceState();

  // Apply the last saved state
  applyState(isLocked ? GREEN : ORANGE);  // Start with the appropriate state

  Serial.println("Device Initialized. Enter 'DEBUG_wallence' to start debug mode.");
}

void loop() {
  // Monitor the LDR sensor for tampering only if locked
  if (isLocked) {
    monitorTampering();
  }

  // Check for debug command through serial monitor
  if (Serial.available()) {
    String inputCommand = Serial.readStringUntil('\n');
    inputCommand.trim();

    if (inputCommand == DEBUG_COMMAND) {
      debugMode();
    } else {
      processCommand(inputCommand);
    }
  }
}

// Monitor the LDR sensor for tampering
void monitorTampering() {
  int ldrValue = analogRead(LDR_PIN);
  if (ldrValue < LDR_THRESHOLD && !isTampered) {
    isTampered = true;
    setDeviceState(RED);
    Serial.println("Tampering detected!");
  }
}

// Debug Mode
void debugMode() {
  Serial.println("Debug Mode Activated.");
  Serial.println("Select Input Method:");
  Serial.println("1: Simulate IR Input (Enter IR_CURRENT_DATA)");
  Serial.println("2: Direct Command Input (Enter command such as LOCKDEVI, UNLOCKDE, etc.)");
  while (true) {
    if (Serial.available()) {
      String debugChoice = Serial.readStringUntil('\n');
      debugChoice.trim();

      if (debugChoice == "1") {
        simulateIRInput();
        break;
      } else if (debugChoice == "2") {
        serialCommandInput();
        break;
      } else {
        Serial.println("Invalid choice. Please enter '1' or '2'.");
      }
    }
  }
}

// Simulate IR input (IR_CURRENT_DATA)
void simulateIRInput() {
  Serial.println("IR Input Simulation Activated.");
  Serial.println("Enter IR_CURRENT_DATA command to simulate IR input.");
  
  while (true) {
    if (Serial.available()) {
      String irCommand = Serial.readStringUntil('\n');
      irCommand.trim();

      if (irCommand == "IR_CURRENT_DATA") {
        String simulatedIRCommand = "LOCKDEVI"; // Example IR Command
        processCommand(simulatedIRCommand);
        break;
      }
    }
  }
}

// Process commands entered through the serial monitor
void serialCommandInput() {
  Serial.println("Direct Command Input Activated.");
  Serial.println("Enter a command (e.g., LOCKDEVI, UNLOCKDE, etc.):");

  while (true) {
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      processCommand(command);
      break;
    }
  }
}

// Process commands
void processCommand(const String& command) {
  if (isTampered) {
    if (command == CLEAR_EEPROM_COMMAND) {
      clearEEPROM();
      return;
    }
    if (command == RESET_COMMAND) {
      resetDevice();
      return;
    }
    Serial.println("Device tampered! Only CLEAR_EEPROM and RESET commands are allowed.");
    return;
  }

  if (inHearMode) {
    processHearMode(command);
    return;
  }

  if (command == LOCK_COMMAND) {
    if (isLocked) {
      Serial.println("Device is already locked.");
    } else {
      lockDevice();
    }
    return;
  }

  if (command == UNLOCK_COMMAND) {
    if (!isLocked) {
      Serial.println("Device is already unlocked.");
    } else {
      activateUnlockMode();
    }
    return;
  }

  if (command == LED_STATUS_COMMAND) {
    checkLEDStatus();
    return;
  }

  if (waitingForKey) {
    if (command.length() == 8) {
      unlockDevice(command);
    } else {
      Serial.println("Incorrect key length. Key must be 8 characters.");
    }
    return;
  }

  if (command == HEAR_MODE_COMMAND && !isLocked) {
    activateHearMode();
    return;
  }

  if (command == CLEAR_EEPROM_COMMAND) {
    clearEEPROM();
    return;
  }

  if (command == RESET_COMMAND) {
    resetDevice();
    return;
  }

  Serial.println("Invalid or unsupported command.");
}

// Lock the device
void lockDevice() {
  isLocked = true;
  isTampered = false;
  setDeviceState(GREEN);
  Serial.println("Device locked.");
}

// Save new key in hear mode
void processHearMode(const String& command) {
  if (command.length() == 8) {
    if (isReservedCommand(command)) {
      Serial.println("Error: Key cannot be a reserved command (LOCKDEVI, HEARMODE, UNLOCKDE).");
    } else {
      saveKeyToEEPROM(command);
      currentKey = command;
      Serial.println("Key updated successfully.");
      inHearMode = false;
    }
  } else {
    Serial.println("Invalid key length. Key must be 8 characters.");
  }
}

// Activate hear mode
void activateHearMode() {
  inHearMode = true;
  Serial.println("Hear mode activated. Send an 8-character key.");
}

// Activate unlock mode
void activateUnlockMode() {
  waitingForKey = true;
  Serial.println("Unlock mode activated. Send the 8-character unlock key.");
}

// Unlock the device
void unlockDevice(const String& key) {
  if (key == currentKey) {
    isLocked = false;
    isTampered = false;
    waitingForKey = false;
    setDeviceState(ORANGE);
    Serial.println("Device unlocked.");
  } else {
    Serial.println("Incorrect key. Unlock failed. Please send UNLOCKDE again with the correct key.");
  }
}

// Check LED status
void checkLEDStatus() {
  Serial.println("LED Status:");
  Serial.print("Green LED: ");
  Serial.println(currentState == GREEN ? "ON" : "OFF");
  Serial.print("Red LED: ");
  Serial.println(currentState == RED ? "ON" : "OFF");
  Serial.print("Orange LED: ");
  Serial.println(currentState == ORANGE ? "ON" : "OFF");
}

// Clear specific EEPROM addresses (key and state)
void clearEEPROM() {
  for (int i = 0; i < 8; i++) {
    EEPROM.write(KEY_START_ADDR + i, 0xFF);
  }
  EEPROM.write(STATE_ADDR, 0xFF);
  Serial.println("Specific EEPROM addresses cleared.");
}

// Reset device to default state
void resetDevice() {
  clearEEPROM();
  currentKey = "12345678";
  saveKeyToEEPROM(currentKey);
  setDeviceState(ORANGE);
  Serial.println("Device reset to default state.");
}

// Save key to EEPROM
void saveKeyToEEPROM(const String& key) {
  for (int i = 0; i < 8; i++) {
    EEPROM.write(KEY_START_ADDR + i, key[i]);
  }
}

// Load device state and key from EEPROM
void loadDeviceState() {
  uint8_t savedState = EEPROM.read(STATE_ADDR);
  if (savedState > RED) {
    setDeviceState(ORANGE);
  } else {
    applyState((DeviceState)savedState);
  }
  currentKey = loadKeyFromEEPROM();
}

// Load key from EEPROM
String loadKeyFromEEPROM() {
  String key = "";
  for (int i = 0; i < 8; i++) {
    char ch = EEPROM.read(KEY_START_ADDR + i);
    key += (ch != 0xFF) ? ch : '\0';
  }
  return key == "" ? "12345678" : key;
}

// Set device state and persist it in EEPROM
void setDeviceState(DeviceState state) {
  currentState = state;
  EEPROM.write(STATE_ADDR, state);
  applyState(state);
}

// Apply state to LEDs
void applyState(DeviceState state) {
  digitalWrite(GREEN_LED_PIN, state == GREEN ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, state == RED ? HIGH : LOW);
  digitalWrite(ORANGE_LED_PIN, state == ORANGE ? HIGH : LOW);
}

// Check for reserved commands
bool isReservedCommand(const String& command) {
  return command == LOCK_COMMAND || command == UNLOCK_COMMAND || command == HEAR_MODE_COMMAND;
}
