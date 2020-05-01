This is just the c source and the hex for the ATtiny85 used in this project.

This translates/forms the NMEA $GPRMC frame from an EM-406a GPS receiver for an old Kenwood TH-D7 two way handheld radio for using the GPS data in APRS.
This is necessary because the radio is very picky and expects a certain format of the frame although compliant with the NMEA standard.

This program took me several days to get it working although it is not very complicated as I do not see myself as a programmer and I do not write programs very often.
The base for the program and the idea is from ham radio operator TA1MD who wrote a program for a PIC microcontroller and had a different input format.

The basic function of the program is collecting the wanted data from the $GPRMC frame with software UART, forms it, recalculates the checksum and sends the new frame over software UART.
