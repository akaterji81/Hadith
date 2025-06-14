#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <QRCode.h>
#include "utility/EPD_7in5b_V2.h"   // E-paper display driver
#include "GUI_Paint.h"             // Paint functions for e-paper
#include "fonts.h"                 // Font definitions

// Color macros (if not defined in headers)
#ifndef WHITE
#define WHITE 0xFF
#endif
#ifndef BLACK
#define BLACK 0x00
#endif
#ifndef RED
#define RED 0xF0
#endif

void displayHadith();
bool fetchHadith(String &hadithText, String &hadithSource, String &englishNarrator, int &hadithNumber);
#include <ArduinoJson.h> // For parsing JSON from API

// Constants for Hadith text length
const int MIN_HADITH_LENGTH = 50; 
const int MAX_HADITH_LENGTH = 300; // User's previous limit

int drawWrappedText(const char* text, int x, int y, sFONT* font, UWORD background, UWORD foreground);

// Constants for deep sleep
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  3600         // Time ESP32 will go to sleep (in seconds) - 1 hour (3600s)

// WiFi credentials
const char* ssid = "ATYMK1";
const char* password = "a1b2c3d4e5f6";

// Hadith API endpoint with API key as query parameter
// Base URL - we'll add hadithNumber parameter dynamically
const char* hadith_api_base = "https://hadithapi.com/api/hadiths?apiKey=$2y$10$FkcEZULcgznqRMIa5j46PesgqPvoWl9bMAlCJcfMqtKNDekDgMO&limit=1&hadithNumber=";

// E-paper pins (for ESP32 E-Paper Driver Board Rev3)
#define EPD_CS   15
#define EPD_DC   27
#define EPD_RST  26
#define EPD_BUSY 25

// EPD object for 7.5" B V2
// No explicit object needed, Waveshare library uses C functions


// Frame buffer pointers
UBYTE *BlackImage = NULL;
UBYTE *RedImage = NULL;

// Helper function to shuffle an array (Fisher-Yates)
static void shuffleArray(int arr[], int n) {
    if (n <= 0) return;
    for (int i = n - 1; i > 0; i--) {
        int j = random(0, i + 1); // random() is exclusive for the upper bound in Arduino, so i+1 is correct for 0 to i inclusive
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void setup() {
    Serial.begin(115200);
    // delay(1000); // Optional: For serial monitor to connect during debugging
    Serial.println("\nWoke up / Booting...");

    // Initialize display hardware
    DEV_Module_Init();
    EPD_7IN5B_V2_Init();
    EPD_7IN5B_V2_Clear(); // Clear the physical display screen

    // Create frame buffers
    UWORD Imagesize = ((EPD_7IN5B_V2_WIDTH % 8 == 0) ? (EPD_7IN5B_V2_WIDTH / 8 ) : (EPD_7IN5B_V2_WIDTH / 8 + 1)) * EPD_7IN5B_V2_HEIGHT;
    // Ensure buffers are allocated only if they haven't been (e.g. if RTC mem was used, though not here)
    // For standard deep sleep with reset, malloc will run each time.
    if (BlackImage == NULL) BlackImage = (UBYTE *)malloc(Imagesize);
    if (RedImage == NULL) RedImage = (UBYTE *)malloc(Imagesize);

    if (BlackImage == NULL || RedImage == NULL) {
        Serial.println("Failed to allocate memory for frame buffers!");
        // Potentially try to display an error if enough of the system is up
        // For now, just go back to sleep.
        Serial.println("Going to sleep due to memory allocation error...");
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
        return; // Should not be reached
    }
    
    Paint_NewImage(BlackImage, EPD_7IN5B_V2_WIDTH, EPD_7IN5B_V2_HEIGHT, 270, WHITE);
    Paint_NewImage(RedImage, EPD_7IN5B_V2_WIDTH, EPD_7IN5B_V2_HEIGHT, 270, WHITE);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    int wifi_retries = 0;
    const int max_wifi_retries = 20; // Try for 10 seconds (20 * 500ms)
    while (WiFi.status() != WL_CONNECTED && wifi_retries < max_wifi_retries) {
        delay(500);
        Serial.print(".");
        wifi_retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        displayHadith(); // This function fetches and displays the Hadith
    } else {
        Serial.println("\nFailed to connect to WiFi.");
        // Optionally display a "No Wi-Fi" message on the e-paper
        Paint_SelectImage(BlackImage); // Ensure black image is selected for text
        Paint_Clear(WHITE);            // Clear buffer before drawing error
        Paint_DrawString_EN(10, EPD_7IN5B_V2_WIDTH/2 - 20, (char*)"Failed to connect to WiFi.", &Font32, WHITE, BLACK);
        EPD_7IN5B_V2_Display(BlackImage, RedImage); // Display the error message
        DEV_Delay_ms(2000); // Show error for a bit if display was updated
    }

    // Disconnect Wi-Fi and turn off radio to save power
    Serial.println("Disconnecting WiFi...");
    WiFi.disconnect(true); // true = also erase WiFi credentials from RAM
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected and radio off.");

    // Put the e-paper display to sleep
    Serial.println("Putting e-paper display to sleep.");
    EPD_7IN5B_V2_Sleep();
    
    // Release resources used by display module (EPD_7IN5B_V2_Sleep() handles display power down)
    Serial.println("Display module put to sleep.");

    // Configure deep sleep
    Serial.println("Configuring deep sleep for " + String(TIME_TO_SLEEP) + " seconds.");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Going to sleep now...");
    Serial.flush(); // Ensure all serial messages are sent
    esp_deep_sleep_start();

    // Code below esp_deep_sleep_start() will not be executed on a normal wake-up cycle
}

void loop() {
  // This function will not be executed when deep sleep is enabled,
  // as the ESP32 resets and runs setup() on each wake-up.
  // Kept empty for this reason.
}

void displayHadith() {
    Serial.println("[DEBUG] Displaying hadith...");
    String hadithText = "";
    String hadithSource = "";
    String englishNarrator = "";  // Add variable for narrator
    int hadithNumber = 0;
    
    // Clear both image buffers
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_SelectImage(RedImage);
    Paint_Clear(WHITE);
    
    if (fetchHadith(hadithText, hadithSource, englishNarrator, hadithNumber)) {
        Serial.println("[DEBUG] Successfully fetched hadith");
        Serial.println("[DEBUG] Hadith length: " + String(hadithText.length()));
        Serial.println("[DEBUG] Source length: " + String(hadithSource.length()));
        Serial.println("[DEBUG] Narrator length: " + String(englishNarrator.length()));
        Serial.println("[DEBUG] Narrator content: '" + englishNarrator + "'");
        
        // Calculate available space
        int total_height = EPD_7IN5B_V2_WIDTH; // In portrait mode, WIDTH is the height (800)
        int title_height = 100;  // Increased space for title including margin
        int source_height = 50;  // Space for source including margin
        int available_height = total_height - title_height - source_height;
        
        // Draw the title with Font48
        Paint_SelectImage(BlackImage);
        Paint_DrawString_EN(10, 20, (char*)"Hourly Hadith:", &Font48, WHITE, BLACK);
        
        // Draw the hadith text with Font32 - limit to available height
        int hadith_start_y = title_height;
        int hadith_end_y = hadith_start_y + available_height;
        
        // Draw the hadith text and get the final position
        int final_text_y = drawWrappedText(hadithText.c_str(), 10, hadith_start_y, &Font32, WHITE, BLACK);
        Serial.println("[DEBUG] Final text Y position: " + String(final_text_y));
        
        // Calculate source position - always at the bottom of the screen
        int source_y = total_height - source_height;
        Serial.println("[DEBUG] Source Y position: " + String(source_y));
        
        // Draw the narrator above the source if available
        
        Paint_SelectImage(RedImage);
        if (englishNarrator.length() > 0) {
            // Draw narrator 40px above the source
            int narrator_y = hadith_start_y - 20;
            Paint_DrawString_EN(10, narrator_y, (char*)englishNarrator.c_str(), &Font16, WHITE, RED);
            Serial.println("[DEBUG] Drawing narrator at Y position: " + String(narrator_y));
        }
        
        // Draw the source at the bottom of the screen with Font16 in red
        Paint_DrawString_EN(10, source_y, (char*)hadithSource.c_str(), &Font16, WHITE, RED);
        
        // Generate and draw QR code with Arabic text
        // Extract just the Arabic text by finding it in the hadith string
        int arabicStart = hadithText.indexOf("\"hadithArabic\":") + 15;  // Skip past "hadithArabic":"
        int arabicEnd = hadithText.indexOf('"', arabicStart);
        
        // Debug: Check if we found the Arabic text section
        Serial.println("[DEBUG] Arabic text search - Start index: " + String(arabicStart) + ", End index: " + String(arabicEnd));
        
        // Use a simple string for QR code to avoid memory issues
        // The crash is likely related to complex string handling with Arabic text
        String qrContent = "Hadith #" + String(hadithNumber);
        
        Serial.println("[DEBUG] Using simple hadith number for QR code: " + qrContent);
        
        // Also log the Arabic text for debugging only
        if (arabicStart > 15 && arabicEnd > arabicStart) {
            Serial.println("[DEBUG] Found Arabic text section, but not using it in QR code");
        } else {
            Serial.println("[DEBUG] Arabic text section not found in response");
        }
        
        Serial.println("[DEBUG] Final QR code content length: " + String(qrContent.length()) + " characters");
        
        // Instead of QR code, just write the hadith number in the bottom right corner
        
        // Position for the hadith number text
        int text_margin = 30;   // Margin from edges
        
        // Calculate position in bottom right corner
        // EPD_7IN5B_V2_HEIGHT = 480 (width in portrait mode)
        // EPD_7IN5B_V2_WIDTH = 800 (height in portrait mode)
        int text_x = EPD_7IN5B_V2_HEIGHT - text_margin - 110;  // From right edge
        int text_y = EPD_7IN5B_V2_WIDTH - text_margin - 5;    // From bottom edge
        
        // Draw the hadith number text in black
        Paint_SelectImage(BlackImage);
        String numberText = "Hadith #" + String(hadithNumber);
        Paint_DrawString_EN(text_x, text_y, (char*)numberText.c_str(), &Font16, WHITE, BLACK);
        
        // Display the full image
        EPD_7IN5B_V2_Display(BlackImage, RedImage);
        
        Serial.println("[DEBUG] Display updated");
    } else {
        Serial.println("[DEBUG] Failed to fetch hadith");
        Paint_SelectImage(RedImage);
        Paint_DrawString_EN(10, 80, (char*)"Error fetching hadith", &Font24, WHITE, RED);
        EPD_7IN5B_V2_Display(BlackImage, RedImage);
    }
}


bool fetchHadith(String &hadithText, String &hadithSource, String &englishNarrator, int &hadithNumber) {
    for (int attempt = 1; attempt <= 10; attempt++) {
        // Generate a random hadith number. The API seems to use 1-based indexing.
        // Max hadith number observed in example was 7563, but it's safer to use a known range or test API limits.
        // For now, using the user's last range.
        hadithNumber = random(1, 7563); 
        String apiUrl = String(hadith_api_base) + String(hadithNumber);
        
        Serial.println("\n[DEBUG] Fetching hadith from API (attempt " + String(attempt) + " for hadithNumber: " + String(hadithNumber) + ")");
        Serial.println("[DEBUG] API URL: " + apiUrl);

        HTTPClient http;
        http.begin(apiUrl);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Allow redirects
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.printf("[DEBUG] HTTP GET... code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = http.getString();
                StaticJsonDocument<8192> doc; // Using StaticJsonDocument, likely ArduinoJson v6
                DeserializationError error = deserializeJson(doc, payload);

                if (error) {
                    Serial.print(F("[ERROR] deserializeJson() failed: "));
                    Serial.println(error.f_str());
                    http.end();
                    delay(500); // Wait before next attempt
                    continue; // Try next API call attempt
                }

                if (!doc["hadiths"].is<JsonObject>() || !doc["hadiths"]["data"].is<JsonArray>()) {
                    Serial.println(F("[ERROR] JSON structure missing 'hadiths' or 'data' array, or 'hadiths' is not an object."));
                    http.end();
                    delay(500); // Wait before next attempt
                    continue;
                }

                JsonArray hadithsInPayload = doc["hadiths"]["data"].as<JsonArray>();
                int numInPayload = hadithsInPayload.size();
                Serial.println("[DEBUG] Found " + String(numInPayload) + " hadith entries in this payload.");

                if (numInPayload > 0) {
                    int indices[numInPayload];
                    for (int i = 0; i < numInPayload; i++) indices[i] = i;
                    shuffleArray(indices, numInPayload); // Shuffle to try them in random order

                    for (int i = 0; i < numInPayload; i++) {
                        JsonObject hadithObject = hadithsInPayload[indices[i]];
                        
                        if (hadithObject["hadithEnglish"].isNull()) {
                            Serial.println("[DEBUG] Skipping entry " + String(indices[i]) + ", hadithEnglish is null or missing.");
                            continue;
                        }
                        hadithText = String(hadithObject["hadithEnglish"].as<const char*>());
                        
                        // Basic text processing: remove \" and \n, \r. More robust processing might be needed.
                        hadithText.replace("\\\"", "\""); // Replace escaped quotes with actual quotes
                        hadithText.replace("\\n", "\n");   // Replace escaped newlines
                        hadithText.replace("\\r", "");    // Remove escaped carriage returns
                        // Add other specific replacements if needed, e.g., for ufdfa (ﷺ)
                        hadithText.replace("ufdfa", "(PBUH)"); // Example from memory

                        // Filter out non-ASCII characters to remove Arabic and other symbols
                        String filteredHadithText = "";
                        for (int k = 0; k < hadithText.length(); k++) {
                            char c = hadithText.charAt(k);
                            if (c >= ' ' && c <= '~') { // Keep only printable ASCII
                                filteredHadithText += c;
                            }
                        }
                        hadithText = filteredHadithText; // Replace original with filtered

                        int currentLength = hadithText.length();
                        Serial.println("[DEBUG] Trying entry " + String(indices[i]) + " from payload, processed length: " + String(currentLength));

                        if (currentLength >= MIN_HADITH_LENGTH && currentLength <= MAX_HADITH_LENGTH) {
                            Serial.println("[DEBUG] Suitable hadith found in payload!");
                            
                            // Narrator
                            if (!hadithObject["englishNarrator"].isNull()) {
                                englishNarrator = String(hadithObject["englishNarrator"].as<const char*>());
                            } else {
                                englishNarrator = ""; // Default to empty if null or missing
                            }
                            
                            // Source (Book Name - Chapter English)
                            String bookName = "Unknown Book";
                            if (hadithObject["book"].is<JsonObject>() && !hadithObject["book"]["bookName"].isNull()) {
                                bookName = String(hadithObject["book"]["bookName"].as<const char*>());
                            }
                            String chapterEnglish = "Unknown Chapter";
                            if (hadithObject["chapter"].is<JsonObject>() && !hadithObject["chapter"]["chapterEnglish"].isNull()) {
                                chapterEnglish = String(hadithObject["chapter"]["chapterEnglish"].as<const char*>());
                            }
                            hadithSource = bookName + " - " + chapterEnglish;

                            Serial.println("[DEBUG] Final Hadith Text Preview: '" + hadithText.substring(0, min(100, (int)hadithText.length())) + "...'");
                            Serial.println("[DEBUG] Final Narrator: " + englishNarrator);
                            Serial.println("[DEBUG] Final Source: " + hadithSource);
                            
                            http.end();
                            return true; // Suitable hadith found and processed
                        } else {
                            Serial.println("[DEBUG] Hadith length (" + String(currentLength) + ") not suitable. Min: " + String(MIN_HADITH_LENGTH) + ", Max: " + String(MAX_HADITH_LENGTH));
                        }
                    }
                    Serial.println("[DEBUG] No suitable hadith found in the current payload's entries after checking all.");
                } else {
                    Serial.println("[DEBUG] No hadith entries found in 'data' array of the payload.");
                }
            } else {
                Serial.printf("[DEBUG] HTTP GET... failed, server error code: %d, message: %s\n", httpCode, http.errorToString(httpCode).c_str());
            }
        } else {
            Serial.printf("[DEBUG] HTTP GET... failed, client/connection error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
        delay(500); // Wait a bit before retrying API call
    }
    Serial.println("[ERROR] Failed to fetch a suitable hadith after all " + String(10) + " attempts.");
    return false;
}

int drawWrappedText(const char* text, int x, int y, sFONT* font, UWORD background, UWORD foreground) {
    // Calculate display parameters with more conservative width estimation
    // For Font32, each character is approximately 16-20 pixels wide in portrait mode
    int char_width = 16;  // Conservative estimate for Font32
    int max_width = EPD_7IN5B_V2_HEIGHT - x - 10; // Display width in portrait mode (480px), with margins
    int line_height = font->Height + 20; // Extra spacing between lines
    int current_y = y;
    
    // Create a copy of the text we can modify
    char* text_copy = (char*)malloc(strlen(text) + 1);
    if (text_copy == NULL) return y; // Memory allocation failed, return original y
    strcpy(text_copy, text);
    
    // Split text into paragraphs first (handle newlines)
    char* paragraph_context;
    char* paragraph = strtok_r(text_copy, "\n", &paragraph_context);
    
    while (paragraph != NULL) {
        // Now process each paragraph word by word
        char* word_context;
        char* word_copy = strdup(paragraph); // Make a copy for word tokenization
        if (word_copy == NULL) {
            free(text_copy);
            return y; // Memory allocation failed, return original y
        }
        
        char* word = strtok_r(word_copy, " ", &word_context);
        char line[256] = ""; // Current line being built
        
        while (word != NULL) {
            // Check if adding this word would exceed the line width
            int current_line_width = strlen(line) * char_width;
            int word_width = strlen(word) * char_width;
            
            // If adding word + space would exceed max width, start a new line
            // Also check if the word is just a single letter - if so, try to keep it with the previous word
            if ((current_line_width > 0) && (current_line_width + word_width + char_width > max_width) && !(strlen(word) == 1)) {
                // Current line is full, print it and start a new line
                Paint_DrawString_EN(x, current_y, line, font, background, foreground);
                current_y += line_height; // Move to next line with consistent spacing
                strcpy(line, word); // Start new line with current word
            } else {
                // Word fits on current line
                if (strlen(line) > 0) {
                    // Not the first word on the line, add a space first
                    strcat(line, " ");
                }
                strcat(line, word);
            }
            
            // Get next word
            word = strtok_r(NULL, " ", &word_context);
        }
        
        // Print any remaining text in this paragraph
        if (strlen(line) > 0) {
            Paint_DrawString_EN(x, current_y, line, font, background, foreground);
            current_y += line_height;
        }
        
        // Move to next paragraph
        paragraph = strtok_r(NULL, "\n", &paragraph_context);
        free(word_copy);
    }
    
    // Clean up
    free(text_copy);
    
    // Return the final y-position (after the last line)
    return current_y;
}
