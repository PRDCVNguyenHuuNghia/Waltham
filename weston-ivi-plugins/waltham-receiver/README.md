# Waltham-receiver

waltham-receiver component is a receiver side implementation for using
Waltham protocol to obtain and process remote output received from
waltham-transmitter

This component is designed to be used for evaluating the functionalities of
waltham-transmitter plugin.

This component also acts as weston client application to display/handle various
requests from actual weston client at transmitter side.

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

###Build Steps


1. Prerequisite before building

    weston, wayland, waltham and gstreamer should be built and available.

2. In waltham-receiver directory, create build directory

        $cd waltham-receiver
        $mkdir build
        $cd build/

3. Run cmake

        $cmake ../
        $cmake --build .

4. waltham-receiver binary should be availaible in build directory

###Configure pipeline
You can use gstreamer pipeline as you want by configuring from "pipeline_receiver.cfg".
This file should be in the folder "/etc/xdg/weston/".

As an example, please refer to the example file named "pipeline_receiver_example*.cfg".

    -pipeline_receiver_example_general.cfg : Does not use any HW decoder.
    -pipeline_receiver_example_intel.cfg   : Use Intel's HW decoder.
    -pipeline_receiver_example_rcar.cfg    : Use Rcar's HW decoder.

Rename file as "pipeline_receiver.cfg" and put in correct place when you use them.

###Connection Establishment

1. Connect two board over ethernet.

2. Assign IP to both the boards and check if the simple ping works.

    For example:if transmitter IP: 192.168.2.51 and Waltham-Receiver IP: 192.168.2.52 then

    $ping 192.168.2.52 (you can also ping vice versa)

3. Make sure that IP address specified in the weston.ini under [transmitter-output] matches the Waltham-Receiver IP.

4. Make sure that IP address in pipeline.cfg on the transmitter side match the Waltham-Receiver IP.

###Basic test steps

1. Start weston at receiver side

    $weston &

2. Run waltham-receiver

    $waltham-receiver -p < port_number > -v &

3. Start weston with transmitter plugin at transmitter side, run application and put it on transmitter screen.You should see the application rendered on receiver display.

Connection established -receiver side logs:

````
set_sigint_handler >>>
 <<< set_sigint_handler
receiver_listen >>>
 <<< receiver_listen
watch_ctl >>>
 <<< watch_ctl
Waltham receiver listening on TCP port 34400...
receiver_mainloop >>>
receiver_flush_clients >>>
 <<< receiver_flush_clients
listen_socket_handle_data >>>
EPOLLIN evnet received.
receiver_accept_client >>>
client_create >>>
watch_ctl >>>
 <<< watch_ctl
Client 0xaaaadc4cde70 connected.
 <<< client_create
 <<< receiver_accept_client
 <<< listen_socket_handle_data
````
