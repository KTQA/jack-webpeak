<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<?php

define('BASE_URL', '');
define('NL', "\n");

$config['channels_count']=8;
$config['show_readme']=true;

?>
<head>
  <title>JACK Peak-Meter</title>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
  <link rel="stylesheet" href="<?=BASE_URL?>static/style.css" type="text/css" media="all"/>
  <script type="text/javascript" src="<?=BASE_URL?>static/script.js"></script>
</head>
<div class="container">
<h1 class="center">Jack-Peak</h1>
 <div class="center">
  <button id="btn_peakrun" onclick="peakmeter_start();"><img src="static/meter_on.png" alt="start" /></button>
  <button id="btn_peakstop" onclick="peakmeter_stop();" disabled="disabled"><img src="static/meter_off.png" alt="stop" /></button>
 </div>
 <div class="meterbox">
<?php

echo '  <div class="meters" style="width:'.(37*$config['channels_count']).'px;">'.NL;
for ($i=0; $i<$config['channels_count']; $i++) {
	echo '  <div class="meter" id="meterbg'.$i.'" style="background-color:#4d4d4d;">';
	echo '<img src="static/iec268-scale.png" alt=""/>';
	echo '<div class="meterscale" id="peakmeter'.$i.'" style="height:0px;"></div>';
	echo '<div class="meterpeak" id="maxmeter'.$i.'" style="top:0px;"></div>';
	echo '</div>'.NL;
}
echo '  </div>'.NL;

?>
 </div>
 <div class="clearer"></div>
<?php
if ($config['show_readme']) {
	echo '<h1 class="center">README.txt</h1>';
	echo ' <div class="doc">'.NL;
	echo '  <pre>'.NL;
	readfile('README.txt');
	echo '  </pre>'.NL;
	echo ' </div>'.NL;
}
?>
</div>
<div class="footer">
<a href="http://gareus.org/oss/jack-peak">jack-peak</a>
&copy; 2012 Robin Gareus
<a href="https://www.gnu.org/licenses/gpl-2.0.html">GPLv2</a>
</div>

</body>
</html>
