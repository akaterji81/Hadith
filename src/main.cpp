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
void drawQRCode(String url, int x, int y, int size, UBYTE* image);
int drawWrappedText(const char* text, int x, int y, sFONT* font, UWORD background, UWORD foreground);

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

void setup() {
    Serial.begin(115200);
  // Initialize display hardware
  DEV_Module_Init();
    EPD_7IN5B_V2_Init();
    EPD_7IN5B_V2_Clear();

    // Create frame buffers
    UWORD Imagesize = ((EPD_7IN5B_V2_WIDTH % 8 == 0) ? (EPD_7IN5B_V2_WIDTH / 8 ) : (EPD_7IN5B_V2_WIDTH / 8 + 1)) * EPD_7IN5B_V2_HEIGHT;
    BlackImage = (UBYTE *)malloc(Imagesize);
    RedImage = (UBYTE *)malloc(Imagesize);
    Paint_NewImage(BlackImage, EPD_7IN5B_V2_WIDTH, EPD_7IN5B_V2_HEIGHT, 270, WHITE);
    Paint_NewImage(RedImage, EPD_7IN5B_V2_WIDTH, EPD_7IN5B_V2_HEIGHT, 270, WHITE);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    displayHadith();
}

void loop() {
    // Fetch and display hadith every 2 minutes
    displayHadith();
    const long interval = 60 * 60 * 1000; // 2 minutes in milliseconds
    for (int i = 0; i < interval / 1000; i++) {
        delay(1000); 
    }
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
            int narrator_y = source_y - 40;
            Paint_DrawString_EN(10, narrator_y, (char*)englishNarrator.c_str(), &Font24, WHITE, RED);
            Serial.println("[DEBUG] Drawing narrator at Y position: " + String(narrator_y));
        }
        
        // Draw the source at the bottom of the screen with Font32 in red
        Paint_DrawString_EN(10, source_y, (char*)hadithSource.c_str(), &Font32, WHITE, RED);
        
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
        int text_y = EPD_7IN5B_V2_WIDTH - text_margin - 10;    // From bottom edge
        
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
    // Try up to 5 times to get a valid hadith that fits within our length limit
    for (int attempt = 1; attempt <= 5; attempt++) {
        // Generate a random hadith number between 1 and 1000
        hadithNumber = random(1, 1001);
        String apiUrl = String(hadith_api_base) + String(hadithNumber);
        
        Serial.println("\n[DEBUG] Fetching hadith from API (attempt " + String(attempt) + ")");
        Serial.print("[DEBUG] API URL: ");
        Serial.println(apiUrl);
        
        HTTPClient http;
        http.begin(apiUrl);
        Serial.println("[DEBUG] Sending GET request...");
        int httpCode = http.GET();
        Serial.print("[DEBUG] HTTP Response code: ");
        Serial.println(httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("[DEBUG] Response size: " + String(payload.length()) + " bytes");
            
            // Simple string-based extraction instead of full JSON parsing
            int hadithStartPos = payload.indexOf("\"hadithEnglish\":");
            if (hadithStartPos > 0) {
                Serial.println("[DEBUG] Found hadithEnglish field at position " + String(hadithStartPos));
                
                // Find the start of the actual text - need to find the first quote after the colon
                hadithStartPos += 15; // Skip "hadithEnglish":
                int quotePos = payload.indexOf("\"", hadithStartPos);
                if (quotePos > hadithStartPos) {
                    hadithStartPos = quotePos + 1; // Position after the opening quote
                    
                    // Find the closing quote - need to be careful to find the right one
                    int hadithEndPos = -1;
                    int searchPos = hadithStartPos;
                    
                    // Search for the closing quote, skipping any escaped quotes
                    while (searchPos < payload.length()) {
                        int nextQuote = payload.indexOf("\"", searchPos);
                        if (nextQuote == -1) break;  // No more quotes found
                        
                        // Check if this quote is escaped (preceded by a backslash)
                        if (nextQuote > 0 && payload.charAt(nextQuote - 1) == '\\') {
                            // This is an escaped quote, skip it
                            searchPos = nextQuote + 1;
                        } else {
                            // This is the unescaped closing quote we're looking for
                            hadithEndPos = nextQuote;
                            break;
                        }
                    }
                    
                    if (hadithEndPos > hadithStartPos) {
                        // Extract the full hadith text including the first character
                        hadithText = payload.substring(hadithStartPos, hadithEndPos);
                        int originalLength = hadithText.length();
                        Serial.println("[DEBUG] Original hadith length: " + String(originalLength) + " characters");
                        
                        // Process the hadith text
                        // 1. Remove all backslashes and quotes
                        hadithText.replace("\\", "");
                        hadithText.replace("\"", " ");
                        
                        // 2. Handle Unicode escape sequences
                        hadithText.replace("ufdfa", "(PBUH)");
                        
                        // 3. Only replace escaped newlines, not all n's and r's
                        // We need to stop replacing all n's and r's as it removes these letters from words
                        // The backslashes were already removed in step 1, so we don't need to handle escaped chars
                        
                        // Print debug info
                        int processedLength = hadithText.length();
                        Serial.println("[DEBUG] Processed hadith length: " + String(processedLength) + " characters");
                        
                        // Check if the hadith is a good length for display (not too short, not too long)
                        if (processedLength < 50) {
                            Serial.println("[DEBUG] Hadith too short (" + String(processedLength) + " chars), trying another...");
                            http.end();
                            continue; // Try another hadith
                        }
                        
                        // Skip hadiths that are too long - don't even try to truncate them
                        // This ensures we only show hadiths that naturally fit on the screen
                        if (processedLength > 300) {
                            Serial.println("[DEBUG] Hadith too long (" + String(processedLength) + " chars), trying another...");
                            http.end();
                            continue; // Try another hadith
                        }
                        
                        // Print a preview of the processed hadith
                        int previewLength = min(100, processedLength);
                        Serial.println("[DEBUG] Processed hadith preview: '" + hadithText.substring(0, previewLength) + "'");
                        
                        // Extract the source information
                        int bookStartPos = payload.indexOf("\"bookName\":");
                        if (bookStartPos > 0) {
                            bookStartPos += 12; // Skip "bookName":"
                            int bookEndPos = payload.indexOf("\"", bookStartPos);
                            String book = payload.substring(bookStartPos, bookEndPos);
                            
                            int chapterStartPos = payload.indexOf("\"chapterEnglish\":");
                            if (chapterStartPos > 0) {
                                chapterStartPos += 17; // Skip "chapterEnglish":"
                                int chapterEndPos = payload.indexOf("\"", chapterStartPos);
                                String chapter = payload.substring(chapterStartPos, chapterEndPos);
                                
                                hadithSource = book + " - " + chapter;
                                Serial.println("[DEBUG] Extracted source: " + hadithSource);
                                
                                // Extract the narrator information
                                Serial.println("[DEBUG] Looking for englishNarrator field in API response");
                                
                                // Print a small portion of the payload for debugging
                                int previewLength = min(200, (int)payload.length());
                                Serial.println("[DEBUG] API response preview: " + payload.substring(0, previewLength) + "...");
                                
                                // Try different field names that might be in the API
                                int narratorStartPos = payload.indexOf("\"englishNarrator\":");
                                if (narratorStartPos <= 0) {
                                    // Try alternative field name
                                    Serial.println("[DEBUG] englishNarrator not found, trying narrator field");
                                    narratorStartPos = payload.indexOf("\"narrator\":");
                                }
                                
                                if (narratorStartPos > 0) {
                                    Serial.println("[DEBUG] Found narrator field at position " + String(narratorStartPos));
                                    
                                    // Determine field name length (either 18 for englishNarrator or 10 for narrator)
                                    int fieldLength = (payload.substring(narratorStartPos, narratorStartPos+10) == "\"narrator\"") ? 10 : 18;
                                    narratorStartPos += fieldLength; // Skip field name and colon
                                    
                                    int narratorEndPos = payload.indexOf('"', narratorStartPos);
                                    Serial.println("[DEBUG] Narrator end position: " + String(narratorEndPos));
                                    
                                    if (narratorEndPos > narratorStartPos) {
                                        englishNarrator = payload.substring(narratorStartPos, narratorEndPos);
                                        englishNarrator.replace("\\", ""); // Remove backslashes
                                        Serial.println("[DEBUG] Extracted narrator: '" + englishNarrator + "'");
                                    } else {
                                        englishNarrator = ""; // Empty if not found
                                        Serial.println("[DEBUG] Could not find end of narrator field");
                                    }
                                } else {
                                    // Use the book name as fallback narrator
                                    englishNarrator = "Narrated in " + book;
                                    Serial.println("[DEBUG] No narrator field found, using book name: '" + englishNarrator + "'");
                                }
                                
                                // Successfully extracted hadith and source
                                Serial.println("[DEBUG] Successfully extracted hadith #" + String(hadithNumber));
                                http.end();
                                return true;
                            }
                        }
                    }
                }
            }
            
            Serial.println("[DEBUG] Failed to extract hadith #" + String(hadithNumber) + ", trying another...");
        } else {
            Serial.println("[DEBUG] HTTP request failed for hadith #" + String(hadithNumber));
        }
        
        http.end();
    }
    
    Serial.println("[DEBUG] Failed to fetch a valid hadith after 5 attempts");
    return false;
}

// Improved text wrapping function with consistent spacing and proper word wrapping
// Returns the y-position after the last line of text
// Function to draw a QR code on the e-paper display
void drawQRCode(String url, int x, int y, int size, UBYTE* image) {
    // Skip QR code generation if URL is empty or too short
    if (url.length() < 5) {
        Serial.println("[DEBUG] Skipping QR code - URL too short");
        return;
    }
    
    Serial.print("[DEBUG] QR Code URL: ");
    Serial.println(url);
    
    // Create the QR code
    QRCode qrcode;
    
    // Use version 3 for better compatibility (same as working example)
    const int qrVersion = 3;
    
    // Calculate the size of the QR code data buffer
    uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
    
    // Initialize the QR code with MEDIUM error correction for better reliability
    qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_MEDIUM, url.c_str());
    
    // Smaller module size for more compact QR code
    const int moduleSize = 4;
    
    // Calculate QR code size in pixels
    const int qrPixelSize = qrcode.size * moduleSize;
    
    Serial.printf("[DEBUG] QR Code size: %d modules, %d pixels\n", qrcode.size, qrPixelSize);
    
    // Select the image buffer
    Paint_SelectImage(image);
    
    // Create a white background with adequate quiet zone
    const int quietZone = 20; // Standard quiet zone (about 4 modules)
    Paint_DrawRectangle(x - quietZone, y - quietZone, 
                        x + qrPixelSize + quietZone, 
                        y + qrPixelSize + quietZone, 
                        WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    // Draw the QR code modules - simple approach that works reliably
    for (uint8_t qrY = 0; qrY < qrcode.size; qrY++) {
        for (uint8_t qrX = 0; qrX < qrcode.size; qrX++) {
            if (qrcode_getModule(&qrcode, qrX, qrY)) {
                // Draw a filled square for each black module
                Paint_DrawRectangle(
                    x + qrX * moduleSize,
                    y + qrY * moduleSize,
                    x + (qrX + 1) * moduleSize - 1,
                    y + (qrY + 1) * moduleSize - 1,
                    BLACK,
                    DOT_PIXEL_1X1,
                    DRAW_FILL_FULL
                );
            }
        }
    }
    
    // Print QR code to serial for debugging
    Serial.println("[DEBUG] QR Code pattern:");
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            Serial.print(qrcode_getModule(&qrcode, x, y) ? "██" : "  ");
        }
        Serial.println();
    }
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
