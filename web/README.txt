jack-peak - web interface example
---------------------------------

This is a simple PHP/XHTML/JavaScript/CSS example exposing jack peak data
via a web interface. It is meant for inspiration and to demonstrate the idea.


1) Setup website

Make this folder accessible via a PHP-5 capable webserver.

2) [optional] configure PHP site

edit meterpeak.php - set path to METERFILE (default /tmp/peaks.json)
edit index.php - set maximum channel-count to display (default: 8)

3) Set permissions

jack-peak and meterpeak.php need to be able to write to $config['meter_file']
(default '/tmp/peaks.json'). The reason for this is that the file will be
locked during reading and writing to prevent partial reads/writes.

This script assumes you are the user who runs jackd and jack-peak. The
METERFILE will be owned by this user.

3a) if you have sudo permissions

  METERFILE=/tmp/peaks.json
  WWWUSER=www-data
  touch $METERFILE
  chmod g+w $METERFILE
  sudo chgrp $WWWUSER $METERFILE

3b) if you don't have sudo permissions

  METERFILE=/tmp/peaks.json
  touch $METERFILE
  chmod 0666 $METERFILE

4) launch jack-peak

  METERFILE=/tmp/peaks.json
  PORTS=system:capture_1 system:capture_2
  jack-peak -i 200 -j -p -f $METERFILE $PORTS

-i 200 : IEC-268-16 scale, 200px high
-j     : JSON output
-p     : include peak-hold data
-f ..  : write to file

