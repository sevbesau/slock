slock - simple screen locker
============================
simple screen locker utility for X. 


Requirements
------------
In order to build slock you need the Xlib header files.

Configuration
-------------
Edit config.mk to match your local setup (slock is installed into
the /usr/local namespace by default).

icon_path must be a full path that points to a png.
An example [icon](icon.png) is provided.

![Example1](img/example1.png)
![Example2](img/example2.png)
![Example3](img/example3.png)

Installation
------------
Enter the following command to build and install slock
(if necessary as root):

    make clean install


Running slock
-------------
Simply invoke the 'slock' command. To get out of it, enter your password.
