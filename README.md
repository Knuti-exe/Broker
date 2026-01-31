# MQTT Broker on an Old Smartphone

In the IoT world, MQTT services are practically unavoidable—and I think most developers would agree. To send and receive data between clients and sensors, we need an MQTT Broker to act as a mediator (or "middleman").
When deciding which device should host my broker, I considered three options:
  1. Cloud Broker: A solid choice, but I wanted to keep my MQTT architecture local.
  2. Dedicated PC/Server: Running a full OS (like Ubuntu Server) just for this would consume too much electricity.
  3. My daily smartphone: The idea was to run the broker in the background 24/7.

The third option seemed perfect at first. I didn't mind the slightly higher battery drain, but I quickly hit a wall: my phone's UI is extremely aggressive toward background services. No matter how many optimizations I disabled, Android kept killing the broker process.

Then, a new idea popped into my head out of nowhere...

### Turning an old smartphone into a dedicated Broker

I found an old Huawei in my drawer. It was still working, though the touch screen was only partially operational. It runs Android 4.4.4, but for a simple broker, that’s more than enough. To set it up, I used USB debugging and a desktop application to control the screen remotely.

The main challenges were:
  1. Finding a compatible MQTT Broker app for such an old Android version.
  2. Configuring the service for 24/7 stability.
  3. Keeping the phone plugged in without damaging the battery (avoiding constant 100% charge).

I won’t name the specific app I used, as there are many solutions available for this kind of setup. The first two points were easy, but the third—power management—was tricky. Since I couldn't root the phone to limit charging via software, I came up with a hardware workaround:
<img width="933" height="645" alt="obraz" src="https://github.com/user-attachments/assets/22be66c4-cd1b-4427-8077-2b548618b397" />
*Schematic made in [Fritzing]*

VIN is connected our wall charger (5V DC ~5W) and left USB port to our server (smartphone running broker service). It works in pretty simple way:
  1. Smartphone publicate its battery percentage to topic "broker/battery", while ESP32 subscribes to that topic,
  2. If battery status changes (while discharging or charging the battery), it sends that information, so ESP will know, if battery is running low or not,
  3. If battery reaches 30%, then microcontroller turns MOSFET to charge smartphone.
  4. If battery reaches 80%, it turns down charging and smartphone starts to run on its own battery.

And that is unique way to reuse old devices into usefull server :)


# TODO
- [x] README file
- [ ] Interactive choice WiFi and logging in
- [ ] Implement light-sleep mode
