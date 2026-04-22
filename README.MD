### MeteoGNSS
This is a project to use ModuleGNSS from M5Stack to generate data for a boat.

It is able to generate:

- Pressure data (Just actual pressure)
- GNSS data (Position, COG, SOG)
- Magnetic heading data 

We don't generate Attitude data as this is in another project.

The data is : 

- Presented in the screen
- Sent to signalK, NMEA0183 and N2K

What to send is selectable so no need to send another GPS position but... May be interesting
in case of something going bad.

Temperature is processed but for the moment we must connect an external sensor as the internal one
measures internal temperature of the device.

A possibility is add an evaluator that sends an alarm when pressure goes down fast....

