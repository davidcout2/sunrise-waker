# Hardware selection and integration
The choice of the LED strip was made to ensure compatibility with the ESP32's voltage output, simplifying the circuit design and allowing for optimal performance in driving the LEDs. 
A 1 meter 5-volt RGB were therefore used (p = 2 W). During the initial testing phase, attempts were made to connect the LED strip directly to the ESP32 microcontroller. 
However, we observed that the light intensity of the LEDs was insufficient, indicating that the output current of the ESP32 was too low to drive the LED strip effectively.

To address this issue and achieve the desired light intensity for the sunrise simulation, a MOSFET (Metal-Oxide-Semiconductor Field-Effect Transistor) was chosen as an additional 
hardware component.

First, we need to define the voltage required to switch on/off the mosfet. Since we work with 5V strip LED, the voltage to switch the mosfet on ($`𝑉_GS(𝑡ℎ)(max)`$) 
should be less than 5V. To switch it off, we simply need a low voltage ($`𝑉_GS(𝑡ℎ)(min)`$).

Secondly, a MOSFET should be chosen with an ID rating higher than the required current for the LED strip in order to ensure sufficient current handling. 
Here we need an Id > 400mA (we used 1/3 of a the 6W led strip with a voltage of 5V).

Thirdly, we took the RDson into account. We tried to choose a Mosfet with a low resistance but didn’t controlled if the heat dissipation were big enough. 
Since the leds doesn’t require much current, we indeed consider that there are no risk to produce more heat than the dissipation capacity.

The last point where to choose a mosfet with a VDS > 5V.
With those criteria, we decided to use the IRFZ44N Mosfet(VDS = 55V, Id = 49A, Rds(on) = 22 mΩ, $`𝑉_GS(𝑡ℎ)(min)=2𝑉`$, $`𝑉_GS(𝑡ℎ)(max)=4V`$).

After conducting online research, we encountered conflicting information regarding the requirement for a gate resistor in MOSFET applications. 
We struggled to find a clear method for determining the appropriate resistance value. As a result, we decided to follow a suggestion from a forum post and opted to use a 42Ω resistor.

Here is a sketch of the hardware used for the LED part:

<img width="775" height="430" alt="image" src="https://github.com/user-attachments/assets/7bdf1f38-df96-4267-bd9b-e35510261188" />
<p><em>Figure 1: Sketch of the components and the wires used for the LED functionality</em></p>


We also used a touchscreen, so that the user could set manually the waking time. At the begin of the project, we wanted to use an ESP32 board with an integrated 240*320 2.8 inch LCD TFT touchscreen produced by Sunton [1]. But we noticed, that a pin was not working correctly and that there were no 5V pins. 
So we tried with a separate display. We choose a $`480*320`$ 4.0 inch LCD TFT touchscreen [2]. We used the following image for the wiring: 

<img width="450" height="300" alt="image-1" src="https://github.com/user-attachments/assets/7c7793ab-96d9-4b23-b0e9-c5e9ca3013f0" />
<p><em>Figure 2: Image of the wiring between the ESP32 and the touchscreen [3] </em></p>

We also used a PCB to solder the ESP32, the resistances and the mosfets on it. We also used led strip jumpers, heat shrink tube to avoid short circuit and stackable long legs female header to make the ESP32 removable. 

Pictures of the system can be found in the Appendix 1-3. 

# ESP32 programming and configuration 
The programming were done with the Arduino IDE. The implementation was done in the order of the following subchapter. 
## Sunrise simulation 
We tried to simulate the sunrise through the light intensity and the color shift. We first tried to pick the RGB colors on youtube videos and model those colors as time function. The result were not good enough. So we finetuned the rgb colors to find suitable functions. 

<img width="387" height="230" alt="image" src="https://github.com/user-attachments/assets/b94feb2a-5c97-4aad-a973-f2d8dc53c639" />
<p><em>Table 1: RGB values used for the colour functions modeling </em></p>

The above table show the chosen RGB values as a function of time. We then used Excel to compute polynomial functions that get close to those values. The resulting functions are shown in the following figure. 

<img width="730" height="423" alt="image" src="https://github.com/user-attachments/assets/d03cbe0c-2cc6-48bc-8874-385ac2335812" />
<p><em>Figure 3: Figure representing the colour functions (dotted curves) computed from the picked RGB colours(points)</em></p>

In the implementation, we simply lightended the LED strip according to the above functions and with the following considerations: 
•	The function to light up the LED accepts values up to 255. For higher values, we simply light up the LED of the corresponding color with a value of 255.
•	We decided that the sunrise should take up to 30 minutes. If the time is exceeded, we also light up the LED of the corresponding color with a value of 255.

The following method shows the computation of the blue colour in function of time.
<img width="470" height="77" alt="image" src="https://github.com/user-attachments/assets/e3f96b98-6337-4011-897c-72fa45bdad28" />
<p><em>Figure 4: Method used to compute the blue colour in function of time </em></p>

## Time  
The current time is obtained from the "ch.pool.ntp.org" NTP server. We apply a timezone correction of +2 hours using the configTime method and stored the time as a tm struct.
The waking time is stored in minutes, calculated as 60 * hours + minutes. 
## GUI for the touchscreen 
We were inspired by an alarm clock GUI project [4]. The Idee were to display an analog clock on the left of the screen and a button to set the start of the sunrise and display it on the right side of the screen as we can see on the following figure. 

<img width="303" height="202" alt="image" src="https://github.com/user-attachments/assets/a821658d-215f-4a86-89cb-03bd0ae89a35" />
<p><em>Figure 5: View of the home screen on the touch screen</em></p> 

The implementation of the analog clock is pretty simple. A circle is displayed. Numbers are then displayed over it. Their position are computed trigonometrically with a radius and an angle. The display of the clock hands follows the same computation principle. 2 lines are displayed and their positions are computed trigonometrically with their length and an angle that is calculated according to the actual time.  

The following method is used for computing the end coordinate. 
<img width="470" height="60" alt="image" src="https://github.com/user-attachments/assets/c27de3aa-f270-40d3-8b4d-522b89398450" />
<p><em>Figure 6: Method used to compute the coordinates of the end point of a rotated line </em></p>

By clicking on the "Set Waker" button, a new screen is displayed where the user can set the waking time. Upon saving the set time, it is published to the ESP32 time topic, allowing the Flutter app to synchronize its waking time accordingly. The layout of this screen is shown in the following figure. 

<img width="302" height="201" alt="image" src="https://github.com/user-attachments/assets/24962f09-0265-4fc6-bf75-fbd3cdcf8f37" />
<p><em>Figure 7: View of the screen to set the waking time on the touch screen </em></p>

# Appendix

<p><em>Picture of the whole hardware:</em></p>
<img width="473" height="315" alt="image" src="https://github.com/user-attachments/assets/1343b281-646e-4291-9abc-22e7c7f7322c" />

<p><em>Picture of the LED strip panel</em></p>
<img width="473" height="315" alt="image" src="https://github.com/user-attachments/assets/3248878b-cf5b-45b5-a34d-050759f8ce43" />

<p><em>Picture of the PCB with the wires drawed in black</em></p>
<img width="473" height="368" alt="image" src="https://github.com/user-attachments/assets/e62fabf9-eced-41b3-af3e-3ed46600d207" />
