#include <DFRobot_SHT20.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SD.h>
#include <RtcDS1302.h>

#define SD_CS_PIN 10
#define RESET_BUTTON_PIN 2
#define LED_PIN1 3
#define LED_PIN2 4

DFRobot_SHT20 sht20(&Wire, SHT20_I2C_ADDR);
LiquidCrystal_I2C lcd(0x27, 16, 2);
File dataFile;
ThreeWire myWire(7, 6, 8); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

unsigned long loggingTime = 0;
const unsigned long loggingInterval = 600000; // 600000 for 10 mins

void setup()
{
    Serial.begin(9600);
    while (!Serial);

    Serial.println("Initializing SD card...");

    if (!initializeSDCard()) 
    {
        Serial.println("Initialization failed!");
        dataerror();
        return;
    }
    Serial.println("Initialization done.");

    sht20.initSHT20();
    lcd.init();
    lcd.backlight();
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    delay(100);
    sht20.checkSHT20();

    // Initialize DS1302 RTC
    Rtc.Begin();

    // Set the compiled time to RTC if necessary
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    if (!Rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected())
    {
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled)
    {
        Rtc.SetDateTime(compiled);
    }
}

void loop()
{
    // Retrieve the current date and time from the DS1302 RTC
    RtcDateTime now = Rtc.GetDateTime();

    // Format the timestamp
    char timestamp[20];
    snprintf(timestamp, sizeof(timestamp), "%02d-%02d-%04d %02d:%02d:%02d",
             now.Day(), now.Month(), now.Year(),
             now.Hour(), now.Minute(), now.Second());

    // Read temperature and humidity from the SHT20 sensor
    float temperature = sht20.readTemperature();
    float humidity = sht20.readHumidity();

    // Display sensor data on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print("%");

    // Check for reset button press
    if (digitalRead(RESET_BUTTON_PIN) == LOW)
    {
        Serial.println("Reset button pressed, restarting...");
        delay(1000); // Debounce delay
        asm volatile("  jmp 0"); // Jump to address 0, effectively restarting the Arduino
    }

    // Check if it's time to log data
    if (millis() - loggingTime >= loggingInterval)
    {
        // Open the data file in write mode
        if (!SD.begin(SD_CS_PIN))
        {
            Serial.println("SD card initialization failed!");
            dataerror();
        }
        else
        {
            dataFile = SD.open("data.csv", FILE_WRITE);

            // If the file is available, write to it
            if (dataFile)
            {
                // Check if we need to write the header
                if (dataFile.size() == 0)
                {
                    writeHeader(dataFile);
                }

                // Write timestamp along with data
                dataFile.print(timestamp);
                dataFile.print(",");
                dataFile.print(temperature);
                dataFile.print(",");
                dataFile.println(humidity);
                dataFile.close();
                Serial.println("Data written to SD card.");
                datasent();
            }
            else
            {
                Serial.println("Error opening data file.");
                dataerror();
            }
        }
        loggingTime = millis();
    }
}

// Function to write the header to the CSV file
void writeHeader(File& file)
{
    // Define the header line
    const char* header = "Timestamp,Temperature(C),Humidity(%)\n";
    
    // Write the header line to the file
    file.print(header);
}


bool initializeSDCard()
{
    for (int i = 0; i < 10; i++)
    {
        if (SD.begin(SD_CS_PIN))
        {
            return true;
        }
        delay(500); // Wait for 500 milliseconds before retrying
    }
    return false;
}

void datasent()
{
    digitalWrite(LED_PIN1, HIGH);
    delay(5000);
    digitalWrite(LED_PIN1, LOW);
}

void dataerror()
{
    digitalWrite(LED_PIN2, HIGH);
    delay(5000);
    digitalWrite(LED_PIN2, LOW);
}
