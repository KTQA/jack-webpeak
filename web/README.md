jack-webpeak - implementation example
=====================================

This is an update to the original jack-peak web example, done in pure javascript using websockets.  As a result there is no need for PHP in this example, though we will use the PHP web server for convenience.

As before, it is meant for inspiration and to demonstrate the idea.  More work will need to be done to use it in a project.


### 1) Configure Javascript #####

Edit `config.js` for the number of ports you'll be monitoring, and what the eventual URL is for the websocket server.  In this example, we'll be monitoring the stereo inputs from the sound card.

    var ports = 2;
    var wsurl = "ws://localhost:18000";

### 2) Start Webserver #####

You can use any webserver you like, but I've got PHP handy, and this is a non-production example, so let's do it like this:

    $ php -S 0.0.0.0:8000

The web server can run as any user.

### 3) Run The Thing #####

Okay, now we have to start `jack-webpeak` with settings the website will understand.  To function, `jack-webpeak` has to run as the user that is running JACK.   So, we run the command:

    $ jack-webpeak -w 18000 -i 200 -j -p system:capture_1 system:capture_2

Notes on options:
  * -w 18000 - run in websocket mode, bound to 127.0.0.1 on port 18000
  * -i 200   - IEC-268-16 scale, 200px high
  * -j       - JSON output
  * -p       - include peak-hold data

### 4) See The Thing #####

At this point, you should be able check our your [local website](http://localhost:8000) and see the thing in operation.

#### But... it only works on localhost! #######

Yes, this is by design.

`jack-webpeak` isn't really a solution for anything by itself, but just a *part* of your balanced JACK breakfast.  See the main README for more information.
