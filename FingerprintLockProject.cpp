// This Code was established by Valerio Job, Luca Faessler and Barththan Sivanantham as a Project for the
// Lecture "Computer Architecture" by Prof. Dr. Tschudin at University of Basel.

// This project consists of an Arduino UNO R4 WiFi, LCD 16x2 i2C display, fingerprint sensor R307S, 4x4 matrix keypad,
// 5V relay and a magnetic 12V lock.

// This code ensures the correct integration of all these components and offers implementation for storing, adding and 
// removing fingerprints as well as navigating in a menu.


#include <Wire.h>
#include <Waveshare_LCD1602_RGB.h>
#include <Adafruit_Fingerprint.h>
#include <DIYables_Keypad.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>

// Define hardware configuration
#define RELAY_PIN 13             // Pin connected to the relay
#define MASTER_PIN "9999"        // Default master PIN
#define OPEN_DURATION 5000       // Time (ms) for which the relay remains active
#define SSID "SSID"              // SSID for WiFi connection
#define PASSWORD "PASSWORD"      // Password for WiFi Connection

// Initialize LCD display (16x2)
Waveshare_LCD1602_RGB lcd(16, 2);

// Define keypad configuration
const int ROWS = 4;               // Number of rows in the keypad
const int COLS = 4;               // Number of columns in the keypad

// Key mapping for the 4x4 keypad
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// Pin connections for keypad rows and columns
byte pin_rows[ROWS] = {9, 8, 7, 6};   // Pins for rows
byte pin_column[COLS] = {5, 4, 3, 2}; // Pins for columns

// Initialize keypad
DIYables_Keypad keypad = DIYables_Keypad(makeKeymap(keys), pin_rows, pin_column, ROWS, COLS);

// Initialize fingerprint sensor
Adafruit_Fingerprint finger(&Serial1);

// Initialize NTP client for time synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600); // Timezone offset: +1 hour

// Global variables
String currentPin = MASTER_PIN; // Current PIN for authentication
bool inMenu = false;            // Menu mode flag
bool accessGranted = false;     // Access status flag

void setup() {
    // Configure relay pin and set to default state (HIGH - locked)
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    // Initialize serial communication
    Serial.begin(9600);
    Serial1.begin(57600); // For fingerprint sensor

    // Initialize LCD display
    lcd.init();
    lcd.setRGB(255, 255, 255); // Set LCD backlight color to white
    lcd.clear();

    // Connect to Wi-Fi
    connectWiFi();

    // Start NTP client
    timeClient.begin();

    // Display initialization message
    lcd.send_string("Initializing...");
    delay(2000);
    lcd.clear();
}

void loop() {
    // Update time from NTP server
    timeClient.update();
    String currentTime = timeClient.getFormattedTime();
    int currentHour = timeClient.getHours(); // Get the current hour

    // Display default screen if not in menu mode
    if (!inMenu) {
        lcd.setCursor(0, 0);
        lcd.send_string("* for Menu");
        lcd.setCursor(0, 1);
        lcd.send_string("# for Open ");
        lcd.send_string(currentTime.c_str());
    }

    // Read keypress from keypad
    char key = keypad.getKey();

    // Handle keypress actions
    if (key == '*') {
        // Enter menu mode
        lcd.clear();
        lcd.send_string("Enter PIN:");
        enterMenu();
    } else if (key == '#') {
        // Attempt to unlock
        if (currentHour >= 8 && currentHour < 20) {
            // Check fingerprint during allowed hours (8 AM - 8 PM)
            startFingerprint();
        } else {
            // Display time restriction message
            lcd.clear();
            lcd.send_string("  No Timeslot   ");
            lcd.setCursor(0, 1);
            lcd.send_string("Access: 8AM-8PM  ");
            delay(3000);
            lcd.clear();
            showDefaultScreen(); // Return to default screen
        }
    }
}

void connectWiFi() {
    // Connect to WiFi network
    WiFi.begin(SSID, PASSWORD);

    // Wait until the device is connected to the WiFi network
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lcd.setCursor(0, 0);
        lcd.send_string("Connecting WiFi...");
    }

    // Display connection success message
    lcd.clear();
    lcd.send_string("WiFi Connected");
    delay(2000);
    lcd.clear();
}

void startFingerprint() {
    // Prompt user to place their finger
    lcd.clear();
    lcd.send_string("Place Finger...");

    // Timeout for fingerprint scanning
    unsigned long start = millis();
    while (true) {
        if (finger.getImage() == FINGERPRINT_OK) {
            // Check if the fingerprint matches a stored template
            if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerFastSearch() == FINGERPRINT_OK) {
                lcd.clear();
                lcd.send_string("Access Granted");
                openLock();
                return;
            } else {
                lcd.clear();
                lcd.send_string("No Match Found");
                delay(2000);
                lcd.clear();
                return;
            }
        }

        // Exit if the timeout is exceeded
        if (millis() - start > 5000) {
            lcd.clear();
            lcd.send_string("Timeout");
            delay(2000);
            lcd.clear();
            return;
        }
    }
}

void openLock() {
    // Activate the relay to unlock the door
    digitalWrite(RELAY_PIN, LOW);
    lcd.clear();
    lcd.send_string("Access Granted");

    // Keep the lock open for the specified duration
    delay(OPEN_DURATION);

    // Deactivate the relay to lock the door
    digitalWrite(RELAY_PIN, HIGH);
    lcd.clear();
    showDefaultScreen(); // Return to the default screen
}

void enterMenu() {
    inMenu = true; // Enable menu mode
    lcd.clear();
    lcd.send_string("Enter PIN:");
    String enteredPin = "";

    while (true) {
        lcd.setCursor(0, 1);

        // Display the entered PIN as asterisks
        String pinDisplay = "";
        for (unsigned int i = 0; i < enteredPin.length(); i++) {
            pinDisplay += '*';
        }
        lcd.send_string(pinDisplay.c_str());

        char key = keypad.getKey();

        // Append numeric keypress to entered PIN
        if (key >= '0' && key <= '9') {
            if (enteredPin.length() < 16) { // Limit PIN length to 16 characters
                enteredPin += key;
            }
        } else if (key == 'B') {
            // Remove the last character from the entered PIN
            if (enteredPin.length() > 0) {
                enteredPin.remove(enteredPin.length() - 1);
            }
        } else if (key == 'C') {
            // Clear the entered PIN
            enteredPin = "";
        } else if (key == '#') {
            // Verify the entered PIN
            if (enteredPin == currentPin) {
                lcd.clear();
                lcd.send_string("Menu Access");
                delay(2000);
                lcd.clear();
                showMenu();
                return;
            } else {
                lcd.clear();
                lcd.send_string("Wrong PIN");
                delay(2000);
                lcd.clear();
                inMenu = false;
                return;
            }
        } else if (key == '*') {
            // Exit menu mode
            lcd.clear();
            inMenu = false;
            return;
        }
    }
}

void showDefaultScreen() {
    // Display the default screen with options for menu and door access
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.send_string("* for Menu");
    lcd.setCursor(0, 1);
    lcd.send_string("# for Open ");
    lcd.send_string(timeClient.getFormattedTime().c_str());
}

void showMenu() {
    // Display the main menu options
    lcd.clear();
    lcd.send_string("1: Fingerprint");
    lcd.setCursor(0, 1);
    lcd.send_string("2: Change PIN");

    while (true) {
        char key = keypad.getKey();
        if (key == '1') {
            fingerprintMenu();
            break;
        } else if (key == '2') {
            changePin();
            break;
        } else if (key == '*') {
            lcd.clear();
            inMenu = false;
            break;
        }
    }
}

void fingerprintMenu() {
    // Display fingerprint-related options
    lcd.clear();
    lcd.send_string("1: Add Finger");
    lcd.setCursor(0, 1);
    lcd.send_string("2: Remove Finger");

    while (true) {
        char key = keypad.getKey();
        if (key == '1') {
            addFingerprint();
            break;
        } else if (key == '2') {
            removeFingerprint();
            break;
        } else if (key == '*') {
            lcd.clear();
            inMenu = false;
            break;
        }
    }
}

void addFingerprint() {
    lcd.clear();
    lcd.send_string("Assigning ID...");
    delay(1000);

    // Find next free ID
    int fingerID = -1;
    for (int id = 0; id <= 127; id++) {
        if (finger.loadModel(id) != FINGERPRINT_OK) {
            fingerID = id;
            break;
        }
    }

    // If no free ID is found
    if (fingerID == -1) {
        lcd.clear();
        lcd.send_string("No Free IDs");
        delay(2000);
        lcd.clear();
        fingerprintMenu();
        return;
    }

    lcd.clear();
    lcd.send_string("Place Finger");

    // Fingerprint read and store
    if (fingerEnroll(fingerID)) {
        lcd.clear();
        lcd.send_string("Finger Added");
        lcd.setCursor(0, 1);
        lcd.send_string("ID: ");
        lcd.send_string(String(fingerID).c_str());
        delay(3000);
        lcd.clear();
        fingerprintMenu(); // Back to Fingerprint screen
    } else {
        lcd.clear();
        lcd.send_string("Enroll Failed");
        delay(2000);
        lcd.clear();
        fingerprintMenu(); // Back to Fingerprint screen
    }
}


bool fingerEnroll(int id) {
    // Enroll a fingerprint for the given ID
    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            lcd.clear();
            lcd.send_string("Scan Finger");
        } else {
            lcd.clear();
            lcd.send_string("Scan Finger");
            lcd.setCursor(0, 1);
            lcd.send_string("Again");
        }

        // Wait until a finger is detected
        while (finger.getImage() != FINGERPRINT_OK) {
            // Stay here until a finger is detected
        }

        if (finger.image2Tz(i + 1) != FINGERPRINT_OK) {
            lcd.clear();
            lcd.send_string("Error Capturing");
            delay(2000);
            return false; // Error transferring the fingerprint image
        }

        if (i == 0) {
            lcd.clear();
            lcd.send_string("Remove Finger");
            delay(2000); // Allow time to remove the finger
        }
    }

    // Create and store the fingerprint model
    if (finger.createModel() != FINGERPRINT_OK) {
        lcd.clear();
        lcd.send_string("Error Creating");
        delay(2000);
        return false; // Error creating the model
    }

    if (finger.storeModel(id) != FINGERPRINT_OK) {
        lcd.clear();
        lcd.send_string("Store Failed");
        delay(2000);
        return false; // Error storing the model
    }

    return true; // Successfully stored
}

void removeFingerprint() {
    // Display loading message while IDs are being fetched
    lcd.clear();
    lcd.send_string("Loading IDs...");

    // Create a list of occupied fingerprint IDs
    int occupiedIDs[128];
    int occupiedCount = 0;

    for (int id = 0; id < 128; id++) {
        if (finger.loadModel(id) == FINGERPRINT_OK) {
            occupiedIDs[occupiedCount++] = id;
        }
    }

    if (occupiedCount == 0) {
        lcd.clear();
        lcd.send_string("No Finger");
        delay(2000);
        lcd.clear();
        fingerprintMenu(); // Back to Menu
        return;
    }

    int currentIndex = 0;
    bool screenNeedsUpdate = true;

    // Display initial screen
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.send_string("ID:");
    lcd.setCursor(0, 1);
    lcd.send_string("Nav: A/B    -> #");

    while (true) {
        if (screenNeedsUpdate) {
            // Update only the ID display
            lcd.setCursor(3, 0);
            lcd.send_string("   "); // Clear old ID
            lcd.setCursor(3, 0);
            lcd.send_string(String(occupiedIDs[currentIndex]).c_str());
            screenNeedsUpdate = false;
        }

        char key = keypad.getKey();
        if (key == 'B') {
            // Navigate to the previous ID
            currentIndex = (currentIndex - 1 + occupiedCount) % occupiedCount;
            screenNeedsUpdate = true;
        } else if (key == 'A') {
            // Navigate to the next ID
            currentIndex = (currentIndex + 1) % occupiedCount;
            screenNeedsUpdate = true;
        } else if (key == '#') {
            // Confirm deletion of the selected ID
            int idToDelete = occupiedIDs[currentIndex];
            lcd.clear();
            lcd.send_string("Delete ID:");
            lcd.setCursor(0, 1);
            lcd.send_string(String(idToDelete).c_str());
            lcd.send_string("?");
            lcd.setCursor(0, 1);
            lcd.send_string("No: *  |  Yes: #");

            // Wait for confirmation or cancellation
            while (true) {
                char confirmKey = keypad.getKey();
                if (confirmKey == '#') {
                    // Confirm deletion
                    lcd.clear();
                    lcd.send_string("Deleting ID:");
                    lcd.setCursor(0, 1);
                    lcd.send_string(String(idToDelete).c_str());
                    delay(2000);

                    if (finger.deleteModel(idToDelete) == FINGERPRINT_OK) {
                        lcd.clear();
                        lcd.send_string("ID ");
                        lcd.send_string(String(idToDelete).c_str());
                        lcd.send_string(" Deleted");
                        delay(2000);
                        lcd.clear();
                        

                        // Remove the ID from the list
                        for (int i = currentIndex; i < occupiedCount - 1; i++) {
                            occupiedIDs[i] = occupiedIDs[i + 1];
                        }
                        occupiedCount--;

                        fingerprintMenu();
                        return;

                        if (occupiedCount == 0) {
                            // No more IDs left
                            lcd.clear();
                            lcd.send_string("No Finger");
                            delay(2000);
                            lcd.clear();
                            fingerprintMenu();
                            return;
                        }

                        // Adjust the index to show the next ID
                        currentIndex %= occupiedCount;
                        screenNeedsUpdate = true;
                        break;
                    } else {
                        lcd.clear();
                        lcd.send_string("Delete Failed");
                        delay(2000);
                        screenNeedsUpdate = true;
                        break;
                    }
                } else if (confirmKey == '*') {
                    // Cancel deletion
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.send_string("ID:");
                    lcd.setCursor(0, 1);
                    lcd.send_string("Nav: A/B    -> #");
                    screenNeedsUpdate = true;
                    break;
                }
            }
        } else if (key == '*') {
            // Exit the removal menu
            lcd.clear();
            showMenu();
            return;
        }
    }
}


void changePin() {
    // Change the PIN after verifying the old PIN
    lcd.clear();
    lcd.send_string("Old PIN:");
    String oldPin = "";

    while (true) {
        lcd.setCursor(0, 1);

        // Display the entered old PIN as asterisks
        String pinDisplay = "";
        for (unsigned int i = 0; i < oldPin.length(); i++) {
            pinDisplay += '*';
        }
        lcd.send_string(pinDisplay.c_str());

        char key = keypad.getKey();
        if (key >= '0' && key <= '9') {
            if (oldPin.length() < 16) { // Limit PIN length to 16 characters
                oldPin += key;
            }
        } else if (key == 'B') {
            // Remove the last character from the old PIN
            if (oldPin.length() > 0) {
                oldPin.remove(oldPin.length() - 1);
            }
        } else if (key == 'C') {
            // Clear the entered old PIN
            oldPin = "";
        } else if (key == '#') {
            // Verify the old PIN
            if (oldPin == currentPin) {
                lcd.clear();
                lcd.send_string("New PIN:");
                String newPin = "";

                while (true) {
                    lcd.setCursor(0, 1);

                    // Display the entered new PIN as asterisks
                    pinDisplay = "";
                    for (unsigned int i = 0; i < newPin.length(); i++) {
                        pinDisplay += '*';
                    }
                    lcd.send_string(pinDisplay.c_str());

                    char key = keypad.getKey();
                    if (key >= '0' && key <= '9') {
                        if (newPin.length() < 16) { // Limit PIN length to 16 characters
                            newPin += key;
                        }
                    } else if (key == 'B') {
                        // Remove the last character from the new PIN
                        if (newPin.length() > 0) {
                            newPin.remove(newPin.length() - 1);
                        }
                    } else if (key == 'C') {
                        // Clear the entered new PIN
                        newPin = "";
                    } else if (key == '#') {
                        // Save the new PIN and exit
                        currentPin = newPin;
                        lcd.clear();
                        lcd.send_string("PIN Changed");
                        delay(2000);
                        lcd.clear();
                        inMenu = false;
                        return;
                    } else if (key == '*') {
                        // Cancel PIN change and exit
                        lcd.clear();
                        return;
                    }
                }
            } else {
                // Old PIN verification failed
                lcd.clear();
                lcd.send_string("Wrong Old PIN");
                delay(2000);
                lcd.clear();
                return;
            }
        } else if (key == '*') {
            // Cancel the operation and exit
            lcd.clear();
            return;
        }
    }
}
