#include "Gpio.hpp"

#include <fmt/format.h>
#include <fmt/core.h>
#include <condition_variable>

constexpr uint32_t SWITCH1_PIN = 32; // GPIO pin for Switch 1
constexpr uint32_t SWITCH2_PIN = 33; // GPIO pin for Switch 2
constexpr uint32_t BUTTON1_PIN = 34; // GPIO pin for Button 1
constexpr uint32_t BUTTON2_PIN = 35; // GPIO pin for Button 2
constexpr uint32_t BUTTON3_PIN = 36; // GPIO pin for Button 3
constexpr uint32_t BUTTON4_PIN = 37; // GPIO pin for Button 4

constexpr uint32_t LED2_PIN = 0;      // GPIO pin for LED 1
constexpr uint32_t LED3_PIN = 1;      // GPIO pin for LED 2
constexpr uint32_t LED4_PIN = 2;      // GPIO pin for LED 3
constexpr uint32_t RGB_LED1_PIN0 = 3; // GPIO pin for LED 4
constexpr uint32_t RGB_LED1_PIN1 = 4; // GPIO pin for LED 5
constexpr uint32_t RGB_LED1_PIN2 = 5; // GPIO pin for LED 6
constexpr uint32_t RGB_LED2_PIN0 = 6; // GPIO pin for LED 4
constexpr uint32_t RGB_LED2_PIN1 = 7; // GPIO pin for LED 5
constexpr uint32_t RGB_LED2_PIN2 = 8; // GPIO pin for LED 6

std::atomic<bool> keepBlinking{true};
std::atomic<bool> keepRGBRunning{true};

std::atomic<int> blinkInterval{200};
std::atomic<int> rgb1ColorIndex{0};
std::atomic<int> rgb2ColorIndex{0};

std::atomic<bool> rgb1NeedsUpdate{false};
std::atomic<bool> rgb2NeedsUpdate{false};

std::condition_variable rgbWakeupCondition;
std::mutex rgbWakeupMutex;

// Blink interval limits
constexpr int MIN_INTERVAL = 5;
constexpr int MAX_INTERVAL = 2000;
constexpr int INTERVAL_STEP = 10;

struct RGBColor
{
    bool r, g, b;
    const char *name;
};

const std::vector<RGBColor> rgbColors = {
    {false, false, false, "OFF"},
    {true, false, false, "BLUE"},
    {false, true, false, "GREEN"},
    {false, false, true, "RED"},
    {true, true, false, "CYAN"},
    {true, false, true, "MAGENTA"},
    {false, true, true, "YELLOW"},
    {true, true, true, "WHITE"}};

void SetRGBLED(Gpio::Gpio &gpio, int ledNumber, int colorIndex)
{
    if (colorIndex < 0 || colorIndex >= rgbColors.size())
    {
        return;
    }

    const RGBColor &color = rgbColors[colorIndex];

    if (ledNumber == 1)
    {
        gpio.SetOutput(RGB_LED1_PIN0, color.r ? Gpio::Value::HIGH : Gpio::Value::LOW);
        gpio.SetOutput(RGB_LED1_PIN1, color.g ? Gpio::Value::HIGH : Gpio::Value::LOW);
        gpio.SetOutput(RGB_LED1_PIN2, color.b ? Gpio::Value::HIGH : Gpio::Value::LOW);
    }
    else if (ledNumber == 2)
    {
        gpio.SetOutput(RGB_LED2_PIN0, color.r ? Gpio::Value::HIGH : Gpio::Value::LOW);
        gpio.SetOutput(RGB_LED2_PIN1, color.g ? Gpio::Value::HIGH : Gpio::Value::LOW);
        gpio.SetOutput(RGB_LED2_PIN2, color.b ? Gpio::Value::HIGH : Gpio::Value::LOW);
    }
}

std::string ReadRGBColorName(Gpio::Gpio &gpio, int ledNumber)
{
    bool r, g, b;

    if (ledNumber == 1)
    {
        r = (gpio.GetPinValue(RGB_LED1_PIN0) == Gpio::Value::HIGH);
        g = (gpio.GetPinValue(RGB_LED1_PIN1) == Gpio::Value::HIGH);
        b = (gpio.GetPinValue(RGB_LED1_PIN2) == Gpio::Value::HIGH);
    }
    else if (ledNumber == 2)
    {
        r = (gpio.GetPinValue(RGB_LED2_PIN0) == Gpio::Value::HIGH);
        g = (gpio.GetPinValue(RGB_LED2_PIN1) == Gpio::Value::HIGH);
        b = (gpio.GetPinValue(RGB_LED2_PIN2) == Gpio::Value::HIGH);
    }
    else
    {
        return "INVALID_LED";
    }

    for (const auto &color : rgbColors)
    {
        if (color.r == r && color.g == g && color.b == b)
        {
            return fmt::format("{}", color.name);
        }
    }

    return "UNKNOWN";
}

void ButtonPressCallback(unsigned int pin, Gpio::Value value, std::chrono::steady_clock::time_point timestamp)
{
    if (value == Gpio::Value::HIGH)
    {
        int currentInterval = blinkInterval.load();

        if (pin == BUTTON1_PIN)
        {
            int newInterval = std::max(MIN_INTERVAL, currentInterval - INTERVAL_STEP);
            blinkInterval.store(newInterval);
            fmt::print("[BUTTON1] Speed UP! New interval: {}ms\n", newInterval);
        }

        else if (pin == BUTTON2_PIN)
        {
            int newInterval = std::min(MAX_INTERVAL, currentInterval + INTERVAL_STEP);
            blinkInterval.store(newInterval);
            fmt::print("[BUTTON2] Speed DOWN! New interval: {}ms\n", newInterval);
        }
        else if (pin == BUTTON3_PIN)
        {
            int currentColor = rgb1ColorIndex.load();
            int newColor = (currentColor + 1) % rgbColors.size();
            rgb1ColorIndex.store(newColor);
            rgb1NeedsUpdate.store(true);

            rgbWakeupCondition.notify_one();
        }
        else if (pin == BUTTON4_PIN)
        {
            int currentColor = rgb2ColorIndex.load();
            int newColor = (currentColor + 1) % rgbColors.size();
            rgb2ColorIndex.store(newColor);
            rgb2NeedsUpdate.store(true);

            rgbWakeupCondition.notify_one();
        }
    }
}

void RGBControlThread(Gpio::Gpio &gpio)
{
    SetRGBLED(gpio, 1, 0);
    SetRGBLED(gpio, 2, 0);

    int lastRGB1Color = 0;
    int lastRGB2Color = 0;

    while (keepRGBRunning.load())
    {
        bool updated = false;

        // Check for RGB1 updates
        if (rgb1NeedsUpdate.load() || rgb1ColorIndex.load() != lastRGB1Color)
        {
            int newColor = rgb1ColorIndex.load();
            SetRGBLED(gpio, 1, newColor);
            lastRGB1Color = newColor;
            rgb1NeedsUpdate.store(false);
            updated = true;

            std::string actualColor = ReadRGBColorName(gpio, 1);
            fmt::print("[RGB1] Color changed to: {}\n", actualColor);
        }

        // Check for RGB2 updates
        if (rgb2NeedsUpdate.load() || rgb2ColorIndex.load() != lastRGB2Color)
        {
            int newColor = rgb2ColorIndex.load();
            SetRGBLED(gpio, 2, newColor);
            lastRGB2Color = newColor;
            rgb2NeedsUpdate.store(false);
            updated = true;

            std::string actualColor = ReadRGBColorName(gpio, 2);
            fmt::print("[RGB2] Color changed to: {}\n", actualColor);
        }

        // If no updates needed, SLEEP UNTIL WOKEN UP by button press!
        if (!updated)
        {
            std::unique_lock<std::mutex> lock(rgbWakeupMutex);
            // Wait for notification OR timeout (for graceful shutdown)
            rgbWakeupCondition.wait_for(lock, std::chrono::milliseconds(1000), []()
                                        {
                // Wake up if:
                // 1. Button pressed (rgb1NeedsUpdate or rgb2NeedsUpdate set)
                // 2. Thread should stop (!keepRGBRunning)
                return rgb1NeedsUpdate.load() || rgb2NeedsUpdate.load() || !keepRGBRunning.load(); });
        }
        else
        {
            // Small delay after update to avoid busy loop if multiple updates come quickly
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    SetRGBLED(gpio, 1, 0);
    SetRGBLED(gpio, 2, 0);
}

void BlinkGreenLeds(Gpio::Gpio &gpio)
{
    const std::vector<uint32_t> ledPins = {LED2_PIN, LED3_PIN, LED4_PIN};

    for (uint32_t pin : ledPins)
    {
        gpio.SetOutput(pin, Gpio::Value::LOW);
    }

    while (keepBlinking.load())
    {
        int currentInterval = blinkInterval.load();

        for (size_t i = 0; i < ledPins.size() && keepBlinking.load(); ++i)
        {
            uint32_t currentPin = ledPins[i];

            gpio.SetOutput(currentPin, Gpio::Value::HIGH);
            std::this_thread::sleep_for(std::chrono::milliseconds(currentInterval));

            if (!keepBlinking.load())
                break;

            gpio.SetOutput(currentPin, Gpio::Value::LOW);
            std::this_thread::sleep_for(std::chrono::milliseconds(currentInterval));
        }

        if (keepBlinking.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    for (uint32_t pin : ledPins)
    {
        gpio.SetOutput(pin, Gpio::Value::LOW);
    }
}

int main()
{
    std::thread blinkThread;
    std::thread rgbThread;

    try
    {
        Gpio::Gpio gpio("/dev/gpiochip0");

        Gpio::InterruptConfig buttonConfig;
        buttonConfig.edgeType = Gpio::EdgeType::RISING;
        buttonConfig.debouncePeriod = std::chrono::milliseconds(50);
        buttonConfig.callback = ButtonPressCallback;
        buttonConfig.enabled = true;

        gpio.EnableInterrupt(BUTTON1_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON2_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON3_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON4_PIN, buttonConfig);

        fmt::print("\n=== Button Controls ===\n");
        fmt::print("BUTTON1 (Pin {}): Increase blink speed\n", BUTTON1_PIN);
        fmt::print("BUTTON2 (Pin {}): Decrease blink speed\n", BUTTON2_PIN);
        fmt::print("BUTTON3 (Pin {}): Change RGB LED1 color\n", BUTTON3_PIN);
        fmt::print("BUTTON4 (Pin {}): Change RGB LED2 color\n", BUTTON4_PIN);

        blinkThread = std::thread(BlinkGreenLeds, std::ref(gpio));
        rgbThread = std::thread(RGBControlThread, std::ref(gpio));

        fmt::print("Press Enter to exit...\n\n");
        std::cin.get();

        keepBlinking.store(false);
        if (blinkThread.joinable())
        {
            blinkThread.join();
        }

        keepRGBRunning.store(false);

        // Wake up RGB thread so it can exit gracefully
        rgbWakeupCondition.notify_one();

        if (rgbThread.joinable())
        {
            rgbThread.join();
        }

        gpio.DisableInterrupt(BUTTON1_PIN);
        gpio.DisableInterrupt(BUTTON2_PIN);
        gpio.DisableInterrupt(BUTTON3_PIN);
        gpio.DisableInterrupt(BUTTON4_PIN);
        // gpio.DisableAllInterrupts();
    }
    catch (const std::exception &e)
    {
        fmt::print("Error: {}\n", e.what());
        keepBlinking.store(false);
        keepRGBRunning.store(false);

        // Wake up RGB thread for cleanup
        rgbWakeupCondition.notify_one();

        if (blinkThread.joinable())
        {
            blinkThread.join();
        }

        if (rgbThread.joinable())
        {
            rgbThread.join();
        }
        return 1;
    }

    return 0;
}