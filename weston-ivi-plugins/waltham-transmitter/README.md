# Waltham Transmitter #

Waltham transmitter is a plugin for weston which uses
waltham IPC library to connect to remote and transmit
client buffer using gstreamer framework

Waltham Transmitter is divide in to two component:

1. Transmitter plugin: Provides API to create remote connections and push surfaces over the network and handles remote output and remote input.
2. waltham renderer  : The waltham renderer creates a buffer to be transmitted to other domain. The current implementation it uses gstreamer.

###Architecture

````
				ECU 1                                                                     ECU 2
              +-----------------------------------------------------+                    +----------------------------------------------+
              |        +-----------------+                          |                    |                                              |
              |        | IVI-Application |                          |                    |               +-----------+-----------+      |
              |        +-----------------+                          |                    |               | Gstreamer |           |      |
              |                 ^                                   |    Buffer   -----------------------> (Decode)  |           |      |
              |        wayland  |                         +----------------------/       |               +-----------+           |      |
              |                 v                         |         |    (Ethernet)      |               |     Waltham-receiver  |      |
              |   +----+---------------------+            |         |        ---------------------------->                       |      |
              |   |    |  Transmitter Plugin |<-----------------------------/            |               +-----------------------+      |
              |   |    |                     |            |         |  Waltham-Protocol  |                             ^                |
              |   |    | +-------------------+            |         |                    |                     wayland |                |
              |   |    | | waltham-renderer  |------------+         |                    |                             v                |
              |   |    | |(gstreamer Encode) |                      |                    |                 +---------------------+      |
              |   |    +-+-------------------+                      |                    |                 |                     |      |
              |   |                          |                      |                    |                 |       WESTON        |      |
              |   |         WESTON           |                      |                    |                 |                     |      |
              |   +------+-------------------+                      |                    |                 +----------------+----+      |
              |          |                                          |                    |                                  |           |
              |          v                                          |                    |                                  v           |
              |   +------------+                                    |                    |                            +----------+      |
              |   |  Display   |                                    |                    |                            |  Display |      |
              |   |            |                                    |                    |                            |          |      |
              |   +------------+                                    |                    |                            +----------+      |
              +-----------------------------------------------------+                    +----------------------------------------------+

````

###How to build

1. Prerequisite before building

    weston, wayland , gstreamer plugins and waltham should be built and available.

2. Get the source code from the repository.

    git clone https://gerrit.automotivelinux.org/gerrit/src/weston-ivi-plugins

3. Create build folder in weston-ivi-plugins

        $cd weston-ivi-plugins/
        $mkdir build/
        $cd build/

4. Run Cmake

        $cmake ../
        $cmake --build .

5. transmitter.so and waltham-renderer.so should be available in the build directory.

###How to configure weston.ini and gstreamer pipeline

1. weston.ini:

    To load transmitter plugin to weston, add 'transmitter.so' to the 'modules'
    key under '[core]', and make sure the 'shell' is 'ivi-shell.so'.

    The destination of remoting is configured in weston.ini.
    Add output name, server address, port number, output's width and height key
    under '[transmitter-output]'. You can speficy multiple [transmitter-output].

    In details, see 'weston.ini.transmitter'.

2. gstreamer pipeline:

    You can use gstreamer pipeline as you want by configuraing from "pipeline.cfg".This file should 
    be in the folder "/etc/xdg/weston/".

    As an example, please refer to the example file named "pipeline_example*.cfg".

    - pipeline_example_general.cfg : Does not use any HW encoder.
    - pipeline_example_intel.cfg   : Use Intel's HW encoder.
    - pipeline_example_rcar.cfg    : Use Rcar's HW encoder.

    Rename file as "pipeline.cfg" and put in correct place when you use them.

###Connection Establishment

1. Connect two board over ethernet.

2. Assign IP to both the boards and check if the simple ping works.

    For example:if transmitter IP: 192.168.2.51 and Waltham-Receiver IP: 192.168.2.52 then

    $ping 192.168.2.52 (you can also ping vice versa)

3. Make sure that IP address specified in the weston.ini under [transmitter-output] matches the Waltham-Receiver IP.

4. Make sure that IP address in pipeline.cfg on the transmitter side match the Waltham-Receiver IP.

###How to test

start weston with modified weston.ini mention above.
You can confirm the transmitter is loaded properly from
weston log as below.

````

    [07:14:22.958] Loading module '/usr/lib/weston/transmitter.so'
    [07:14:22.977] Registered plugin API 'transmitter_v1' of size 88
    [07:14:22.978] Registered plugin API 'transmitter_ivi_v1' of size 16
    [07:14:22.982] Loading module '/usr/lib/libweston-2/waltham-renderer.so'
    [07:14:23.032] Transmitter initialized.
    [07:14:23.032] Module '/usr/lib/libweston-2/waltham-renderer.so' already loaded
    [07:14:23.032] Transmitter weston_seat 0xaaaad0079e50
    [07:14:23.032] Transmitter created pointer=0xaaaad00977c0 for seat 0xaaaad0079e50
    [07:14:23.032] Transmitter created keyboard=0xaaaad0079fe0 for seat 0xaaaad0079e50
    [07:14:23.032] Transmitter created touch=0xaaaacffe1010 for seat 0xaaaad0079e50
````
Start remoting :

- Start an IVI application.
- Put surface on transmitter output using LayerManagerControl command

Example command:

    $EGLWLMockNavigation &
    $LaygeManagementControl get scene
      -> Please check connector name of transmitter output
    $layer-add-surfaces -d [transmitter output name] -s 1 -l 1

Weston log will indicate remoting has started:

````
    [07:16:39.043] surface ID 10
    [07:16:39.055] gst-setting are :-->
    [07:16:39.055] ip = 192.168.2.52
    [07:16:39.055] port = 34400
    [07:16:39.055] bitrate = 3000000
    [07:16:39.055] width = 800
    [07:16:39.055] height = 480
    [07:16:40.819] Parsing GST pipeline:appsrc name=src ! videoconvert ! video/x-raw,format=I420 ! omxh264enc bitrate=3000000 control-rate=2 ! rtph264pay ! udpsink name=sink host=192.168.2.52 port=34400 sync=false async=false
````
