jack-webpeak - simple peak-signal meter for JACK with websocket support
=======================================================================

### Our Story So Far, Because I Find a Narrative Helps #####

In my adventures in working on [community radio](https://ktqa.org) projects, it became clear that a peak meter for JACK that worked via the web would be a neat thing to have.   Something more than an idiot light, something less than a full-blown useful meter.  After some deep searching I found an email thread where someone had a simple application called [jack-peak](http://gareus.org/gitweb/?p=jack-peak.git), with a tarball to it.  I downloaded it and started to dig in, but then other things prevailed and I promptly forgot about it, lost the tarball, and salted the earth.

Then a web-based meter went from "neat" to "pretty much necessary" and I had to go find it all over again.   I initially made some basic changes that allowed it to sit behind one of those websocketifier daemons.   But that had more moving parts than I liked and cluttered up my patch bay when more than one person was watching the meters.   Plus, I needed more experience working with JACK directly.

After looking around and trying to *make really sure* no one else had already solved this problem, I did some poking around on websocket libraries, dusted the code off, and knocked this out.   It is clearly a work in progress.

In addition to [JACK](https://jackaudio.org), it requires [wsServer](https://github.com/Theldus/wsServer) because it ~is the best solution~required me to think the least.

### How To Use #####

It will continue to work as `jack-peak` did on the command line, unless you hand it the `-w` argument.  At which point it will bind to that port on `localhost` and provide the output there.  `jack-webpeak` spinelessly refuses to bind to anything other than the loopback device as there's no methodology for authentication or SSL, and there are no plans to provide them.

To use this project, run it behind a proxy or a webserver capable of being one.   For this use case, I'm partial to [lighttpd](https://www.lighttpd.net/).

For more information, check out the `--help` or look at the man page.

### Credit, Bugs and Whatnot #####

The original `jack-peak` was a project long ago from [Robin Gareus](http://gareus.org/), and the way the thing works hasn't changed too much yet.   But if you run into problems with the code, file the bug report here rather than bothering him because he's got [other](https://ardour.org/), [more important](http://x42-plugins.com/x42/) things going on.  But a huge pile of thanks to the good Doctor for giving me a good starting point.

