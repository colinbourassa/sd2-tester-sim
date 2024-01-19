# SD2 Tester Simulator

This Linux application is a simulator for the VxWorks-based SD2 Tester hardware, which would normally be connected to its Windows 9x host machine via a COM port (RS232). This simulator communicates with the SD2 Windows software (WSDC32.EXE) via a UNIX domain socket rather than a physical port. The intended usage is as follows:

 - Install WSDC32 in a Win9x VirtualBox guest OS.
 - In the VirtualBox settings for this guest, enable the COM1 serial port and set its Port Mode to "Host Pipe". Give it a path such as `/home/yourname/vbox-port`.
 - Start the VirtualBox machine and verify that the socket file given in the previous step was created.
 - Start sd2-tester-sim and point it to the same socket file. Click `Connect`, then `Start listening`. For the time being, the stdout for this tool should be going somewhere visible to the user, as it includes important diagnostic information.
 - Start WSDC32.

