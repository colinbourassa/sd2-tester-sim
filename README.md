# SD2 Tester Simulator

This Linux application is a simulator for the VxWorks-based SD2 Tester hardware, which would normally be connected to its Windows 9x host machine via a COM port (RS232). This simulator communicates with the SD2 Windows software (WSDC32.EXE) via a UNIX domain socket rather than a physical port. The intended usage is as follows:

 - Install WSDC32 in a Win9x VirtualBox guest OS.
 - In the VirtualBox settings for this guest, enable the COM1 serial port and set its Port Mode to "Host Pipe". Give it a path such as `/home/yourname/vbox-port`.
 - Start the VirtualBox machine and verify that the socket file given in the previous step was created.
 - Start sd2-tester-sim and point it to the same socket file. Click `Start listening`.
 - Start WSDC32.

At any point, you may save the simulator's virtual filesystem state to disk or load it from disk. This is useful because the SD2 system limits the total number of ECU modules that may be loaded at any given time, so it is helpful to be able to save state with all of the 550 Maranello modules loaded, for example. This alleviates the need to re-load the modules through the WSDC32 transfer process each time the simulator is restarted.

